import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

RUNNER = Path(__file__).resolve().with_name('run_checks.py')
FAKE_TEST = '''#!/usr/bin/env python3
import os,sys,time
from pathlib import Path
assert not any(k.startswith('GTEST_') for k in os.environ)
if '--gtest_list_tests' in sys.argv:
 print('ESM4Fixture.\\n  Works')
else:
 mode=os.environ.get('SKILL_TEST_CASE','good')
 if mode=='exit':sys.exit(7)
 if mode=='timeout':time.sleep(10)
 target=next(x.split('xml:',1)[1] for x in sys.argv if x.startswith('--gtest_output='))
 case='<testcase classname="ESM4Fixture" name="Works" status="run" result="completed">'+('<skipped/>' if mode=='skip' else '')+'</testcase>'
 Path(target).write_text('<testsuites tests="'+('0' if mode=='empty' else '1')+'">'+('' if mode=='empty' else case)+'</testsuites>')
 if mode=='drift':Path('marker').write_text('changed')
'''


class RunnerTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='openmw-skill-test-')
        self.addCleanup(self.tmp.cleanup)
        root = Path(self.tmp.name)
        self.repo = root / 'repo'
        (self.repo / 'components/esm4').mkdir(parents=True)
        (self.repo / 'marker').write_text('initial')
        (self.repo / '.gitignore').write_text('build/\nevidence/\n')
        self.env = os.environ.copy()
        self.env.update(GIT_AUTHOR_NAME='Skill test', GIT_AUTHOR_EMAIL='test@example.invalid',
                        GIT_COMMITTER_NAME='Skill test', GIT_COMMITTER_EMAIL='test@example.invalid')
        for command in [['git', 'init', '-q'], ['git', 'add', '.'], ['git', '-c', 'commit.gpgsign=false', 'commit', '-qm', 'fixture']]:
            subprocess.run(command, cwd=self.repo, env=self.env, check=True)
        commands = root / 'bin'
        commands.mkdir()
        cmake = commands / 'cmake'
        cmake.write_text('#!/bin/sh\nexit "${SKILL_BUILD_EXIT:-0}"\n')
        cmake.chmod(0o755)
        self.env.update(PATH=str(commands) + os.pathsep + self.env['PATH'], GTEST_FILTER='Unexpected.*',
                        GTEST_TOTAL_SHARDS='3', GTEST_SHARD_INDEX='1')
        for directory in ['build', 'build/m15-sanitize']:
            build = self.repo / directory
            build.mkdir(parents=True, exist_ok=True)
            (build / 'CMakeCache.txt').write_text(f'CMAKE_HOME_DIRECTORY:INTERNAL={self.repo}\n'
                                               'CMAKE_CXX_FLAGS:STRING=-fsanitize=address,undefined\n')
            exe = build / 'components-tests'
            exe.write_text(FAKE_TEST)
            exe.chmod(0o755)

    def run_case(self, name, mode='sanitize', timeout=10):
        self.env['SKILL_TEST_CASE'] = name
        result = subprocess.run([sys.executable, str(RUNNER), '--repo', str(self.repo), '--output',
                                 f'evidence/{name}', '--mode', mode, '--timeout', str(timeout)],
                                env=self.env, text=True, capture_output=True)
        path = self.repo / f'evidence/{name}/verification.json'
        return result, json.loads(path.read_text()) if path.exists() else None

    def test_clean_run_clears_inherited_filter_and_sharding(self):
        result, report = self.run_case('good')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(report['passed'])
        self.assertEqual(report['results']['sanitize']['executed'], 1)

    def test_rejects_skips_empty_results_nonzero_exit_and_source_drift(self):
        for name in ['skip', 'empty', 'exit', 'drift']:
            with self.subTest(name=name):
                result, report = self.run_case(name)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertFalse(report['passed'])
                self.assertIn('error', report)

    def test_refuses_existing_evidence(self):
        self.run_case('good')
        result, report = self.run_case('good')
        self.assertEqual(result.returncode, 2)
        self.assertTrue(report['passed']) # Original successful evidence survived.

    def test_preserves_build_failure(self):
        self.env['SKILL_BUILD_EXIT'] = '9'
        result, report = self.run_case('build-failure', 'components')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report['commands'][0]['returncode'], 9)

    def test_timeout_is_recorded(self):
        result, report = self.run_case('timeout', timeout=1)
        self.assertEqual(result.returncode, 1)
        self.assertTrue(report['commands'][-1]['timed_out'])


if __name__ == '__main__':
    unittest.main()
