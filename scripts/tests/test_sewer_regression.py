import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import sewer_regression as suite
from verify_oblivion_sewer_escape import compare_entry


class SewerRegressionTest(unittest.TestCase):
    def test_missing_checkpoint_never_constructs_state(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            catalog = {'checks': {'floor': {'path': str(root / 'missing'), 'sha256': '0' * 64}}}
            with self.assertRaises(FileNotFoundError):
                suite.checkpoint('floor', catalog, root)
            self.assertEqual(list(root.iterdir()), [])

    def test_checkpoint_copy_preserves_provenance_and_rejects_changed_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / 'floor.omwsave'
            path.write_bytes(b'genuine retained bytes')
            catalog = {'checks': {'floor': {'path': str(root / 'absent'), 'sha256': suite.sha(path)}}}
            resolved, meta = suite.checkpoint('floor', catalog, root)
            self.assertEqual(resolved, path)
            self.assertEqual(meta['kind'], 'retained canonical checkpoint')
            path.write_bytes(b'altered')
            with self.assertRaises(ValueError):
                suite.checkpoint('floor', catalog, root)

    def test_failure_classification_does_not_label_all_timeouts_engine_bugs(self):
        self.assertEqual(suite.classify({'passed': False}, {'error_count': 0}, 'native Player death')[0], 'automated_player')
        self.assertEqual(suite.classify({'passed': False}, {'error_count': 1}, None)[0], 'engine')
        self.assertEqual(suite.classify({'passed': False, 'exit_code': -11}, {}, None)[0], 'engine')
        self.assertEqual(suite.classify({'passed': False, 'timed_out': True}, {}, None)[0], 'unresolved_course_failure')

    def test_restore_tolerance_does_not_hide_health_changes(self):
        with self.assertRaises(ValueError):
            compare_entry({'player': {'health': 80}}, {'player': {'health': 79}})
        self.assertEqual(compare_entry({'a': 1}, {'a': 1}), [])

    def test_original_death_gate_allows_observed_shaman_friendly_fire(self):
        player = 'dynamic:player:0000000000000001'
        ref = lambda x: 'content:oblivion.esm:' + x
        state = {'player': {'reference': player, 'cell': ref('0380bd'),
                            'actor_values': {'health.current': 80, 'magicka.current': 80}},
                 'native_actor_life': [{'actor': ref(x), 'phase': 1, 'killer': player} for x in ['03b543', '03b541']] +
                                      [{'actor': ref('03b52f'), 'phase': 1, 'killer': ref('03b52e')},
                                       {'actor': ref('03b52e'), 'phase': 0, 'killer': 'null'}]}
        facts = suite.semantics('combat', state)
        self.assertEqual(facts['relevant_deaths'][-1]['killer'], ref('03b52e'))
        state['native_actor_life'][2]['phase'] = 0
        with self.assertRaises(ValueError):
            suite.semantics('combat', state)

    def test_preparation_keeps_full_new_game_and_checkpoint_load_distinct(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            args = argparse.Namespace(openmw=root / 'openmw', resources=root / 'resources', data=root / 'Data', library_path=None)
            full = json.loads(suite.prepare(args, root / 'full').read_text())
            self.assertFalse(any('--load-savegame' in x for x in full['command']))
            self.assertIn('new-game=1', full['files'][0]['content'])
            focused = json.loads(suite.prepare(args, root / 'focused', [65, 66, 67], root / 'input.omwsave').read_text())
            self.assertIn('--load-savegame=' + str(root / 'input.omwsave'), focused['command'])
            self.assertIn('new-game=0', focused['files'][0]['content'])
            self.assertEqual(focused['environment']['OPENMW_M15_EVENTS'], '{output}/combat-events.jsonl')

    def test_partial_results_cannot_look_like_a_completed_green_suite(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            args = argparse.Namespace(output=root, openmw=root / 'absent')
            suite.summarize(args, [{'name': 'floor', 'status': 'passed'}], 'focused', complete=False)
            report = json.loads((root / 'results.json').read_text())
            self.assertEqual(report['status'], 'running')
            self.assertFalse(report['complete'])

    def test_missing_native_semantics_produces_failed_result_instead_of_crashing(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'Data').mkdir()
            (root / 'Data/Oblivion.esm').write_bytes(b'test only')
            (root / 'openmw').write_bytes(b'test only')
            (root / 'resources').mkdir()
            out = root / 'result'
            argv = ['sewer_regression.py', 'focused', '--only', 'combat', '--output', str(out),
                    '--data', str(root / 'Data'), '--openmw', str(root / 'openmw'), '--resources', str(root / 'resources')]
            with mock.patch.object(sys, 'argv', argv), mock.patch.object(suite, 'checkpoint', return_value=(root / 'input', {})), \
                 mock.patch.object(suite, 'prepare', return_value=root / 'manifest'), \
                 mock.patch.object(suite, 'execute', return_value={'status': 'passed', 'state': {}}):
                self.assertEqual(suite.main(), 1)
            report = json.loads((out / 'results.json').read_text())
            self.assertTrue(report['complete'])
            self.assertEqual(report['checks'][0]['classification'], 'engine_or_persistence')
            self.assertTrue((out / 'combat/failure-traceback.txt').exists())

    def test_missing_runtime_reports_each_check_without_launching(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            out = root / 'result'
            result = subprocess.run([sys.executable, str(suite.ROOT / 'scripts/sewer_regression.py'), 'focused',
                                     '--output', str(out), '--openmw', str(root / 'missing'),
                                     '--resources', str(root / 'none'), '--data', str(root / 'Data')], capture_output=True)
            self.assertEqual(result.returncode, 2)
            report = json.loads((out / 'results.json').read_text())
            self.assertEqual(len(report['checks']), 6)
            self.assertEqual({x['name'] for x in report['checks'] if x['classification'] == 'upstream_failure'},
                             {'story-own-reload', 'exit-own-reload'})
            self.assertTrue(all(x['status'] == 'could_not_run' for x in report['checks']))
            self.assertFalse(report['full_escape_acceptance'])
            self.assertFalse(list(out.rglob('scenario.json')))


if __name__ == '__main__':
    unittest.main()
