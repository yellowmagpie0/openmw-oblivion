#!/usr/bin/env python3
"""Build/test an existing OpenMW configuration and retain fresh, verified evidence."""
import argparse
import hashlib
import json
import os
import signal
from pathlib import Path
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

from verify_gtest import verify


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def source_state(repo, output):
    head = git(repo, 'rev-parse', 'HEAD').decode().strip()
    h = hashlib.sha256(head.encode() + git(repo, 'diff', 'HEAD', '--binary'))
    for raw in sorted(git(repo, 'ls-files', '--others', '--exclude-standard', '-z').split(b'\0')):
        if not raw:
            continue
        path = repo / os.fsdecode(raw)
        if path.is_relative_to(output):
            continue
        h.update(raw + b'\0')
        h.update(os.readlink(path).encode() if path.is_symlink() else path.read_bytes())
    return {'head': head, 'source_fingerprint': h.hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path.cwd())
    parser.add_argument('--output', type=Path, required=True, help='New evidence directory; never reused')
    parser.add_argument('--mode', action='append', choices=['components', 'sanitize', 'engine', 'python'], required=True)
    parser.add_argument('--build-dir', type=Path, default=Path('build'))
    parser.add_argument('--sanitize-dir', type=Path, default=Path('build/m15-sanitize'))
    parser.add_argument('--filter', help='Explicit GoogleTest selection; sanitizer default is ESM4*, others *')
    parser.add_argument('--jobs', type=int, default=6)
    parser.add_argument('--timeout', type=int, default=3600, help='Seconds per subprocess')
    args = parser.parse_args()
    if args.jobs < 1 or args.timeout < 1 or len(args.mode) != len(set(args.mode)):
        parser.error('positive jobs/timeout and unique modes required')
    repo = args.repo.resolve()
    output = (repo / args.output).resolve()
    if not (repo / 'components/esm4').is_dir():
        parser.error('repo is not the OpenMW Oblivion workspace')
    if output.exists():
        parser.error('output already exists; choose a fresh evidence directory')
    output.mkdir(parents=True)
    report = {'schema_version': 1, 'passed': False, 'commands': [], 'results': {}}
    env = os.environ.copy()
    # Explicit CLI filters cannot neutralize inherited sharding/repetition alone.
    for name in tuple(env):
        if name.startswith('GTEST_'):
            del env[name]
    def run(command, log, process_env=env):
        print(f'Running {log}', flush=True)
        start = time.monotonic()
        entry = {'argv': list(map(str, command)), 'log': log}
        report['commands'].append(entry)
        with (output / log).open('wb') as stream:
            process = subprocess.Popen(entry['argv'], cwd=repo, env=process_env, stdout=stream,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            try:
                returncode = process.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
                entry.update(returncode=process.returncode, timed_out=True,
                             seconds=round(time.monotonic() - start, 3))
                raise RuntimeError(f'{log}: command timed out after {args.timeout}s')
        entry.update(returncode=returncode, seconds=round(time.monotonic() - start, 3))
        if returncode:
            raise RuntimeError(f'{log}: command exited {returncode}')
    try:
        report['source_before'] = source_state(repo, output)
        for mode in args.mode:
            if mode == 'python':
                run([sys.executable, '-m', 'unittest', 'discover', '-s', 'scripts/tests'], 'python.log')
                log = (output / 'python.log').read_text()
                import re
                counts = re.findall(r'^Ran (\d+) tests? in ', log, re.MULTILINE)
                if not counts or int(counts[-1]) == 0 or not re.search(r'\nOK\s*\Z', log):
                    raise RuntimeError('Python suite empty, skipped, or lacks clean OK summary; inspect python.log')
                report['results'][mode] = {'executed': int(counts[-1]), 'passed': True, 'log_sha256': digest(output / 'python.log')}
                continue
            build = (repo / (args.sanitize_dir if mode == 'sanitize' else args.build_dir)).resolve()
            cache = build / 'CMakeCache.txt'
            if not cache.is_file():
                raise RuntimeError(f'configuration missing: {cache}; consult references/build-config.md')
            settings = {}
            for line in cache.read_text().splitlines():
                if '=' in line and ':' in line and not line.startswith(('//', '#')):
                    key, value = line.split('=', 1)
                    settings[key.split(':', 1)[0]] = value
            if Path(settings.get('CMAKE_HOME_DIRECTORY', '')).resolve() != repo:
                raise RuntimeError('CMake cache belongs to a different source checkout')
            flags = ' '.join(v for k, v in settings.items() if k.startswith('CMAKE_CXX_FLAGS'))
            if mode == 'sanitize' and not ('-fsanitize=address,undefined' in flags
                    or ('-fsanitize=address' in flags and '-fsanitize=undefined' in flags)):
                raise RuntimeError('sanitizer mode requires both ASan and UBSan in the CMake configuration')
            binary = build / ('openmw-tests' if mode == 'engine' else 'components-tests')
            targets = ['openmw', 'openmw-tests', 'esmtool'] if mode == 'engine' else ['components-tests']
            run(['cmake', '--build', build, '--target', *targets, '-j', str(args.jobs)], f'{mode}-build.log')
            selection = args.filter if args.filter is not None else ('ESM4*' if mode == 'sanitize' else '*')
            mode_env = env.copy()
            if mode == 'sanitize':
                mode_env.update(ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
            run([binary, '--gtest_list_tests'], f'{mode}-inventory.txt', mode_env)
            run([binary, f'--gtest_filter={selection}', '--gtest_repeat=1', '--gtest_shuffle=0',
                 f'--gtest_output=xml:{output / (mode + ".xml")}'], f'{mode}.log', mode_env)
            verified = verify(output / f'{mode}-inventory.txt', output / f'{mode}.xml', selection)
            verified.update(binary_sha256=digest(binary), cache_sha256=digest(cache),
                            xml_sha256=digest(output / f'{mode}.xml'),
                            inventory_sha256=digest(output / f'{mode}-inventory.txt'),
                            configuration={k: v for k, v in settings.items() if k in {
                                'CMAKE_BUILD_TYPE', 'CMAKE_CXX_COMPILER', 'CMAKE_CXX_FLAGS',
                                'CMAKE_CXX_FLAGS_DEBUG', 'OPENMW_USE_SYSTEM_BULLET'}})
            report['results'][mode] = verified
        report['source_after'] = source_state(repo, output)
        if report['source_before'] != report['source_after']:
            raise RuntimeError('source changed during checks; results do not validate a single revision')
        report['passed'] = True
    except (OSError, ValueError, RuntimeError, ET.ParseError, subprocess.SubprocessError) as error:
        report['error'] = str(error)
        print(f'FAILED: {error}', file=sys.stderr)
    finally:
        (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'passed': report['passed'], 'evidence': str(output), 'modes': list(report['results'])}), flush=True)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
