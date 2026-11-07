"""Fresh read-only observations may choose only ordinary diagnostic inputs."""
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from test_oblivion_compat import MODULE


class ScenarioLogBranchTests(unittest.TestCase):
    def action(self):
        return dict(type='branch_on_log', pattern=r'decision=(?P<choice>\w+)',
                    timeout_seconds=1, cases={'heal': [dict(type='key', value='c')],
                                             'attack': [dict(type='mouse_click', button=1)],
                                             'done': []})

    def manifest(self, action):
        return dict(schema_version=1, name='branch', command=['openmw'],
                    sdl_offscreen_input=True, actions=[action])

    def test_ignores_old_observation_and_uses_latest_complete_fresh_choice(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            log = output / 'process.log'
            log.write_text('decision=heal\n')
            replay = mock.Mock()
            replay.action.return_value = dict(passed=True, first_input_sequence=1)
            chunks = iter(['decision=heal', '\ndecision=attack\n'])

            def append(_seconds):
                with log.open('a') as stream:
                    stream.write(next(chunks))

            with mock.patch.object(MODULE.time, 'sleep', side_effect=append):
                result = MODULE._run_action(self.action(), environment={}, output=output,
                                            offscreen_input=replay)
            self.assertTrue(result['passed'])
            self.assertEqual(result['choice'], 'attack')
            self.assertEqual(result['start_offset'], len('decision=heal\n'))
            self.assertEqual(result['actions'][0]['first_input_sequence'], 1)
            replay.action.assert_called_once_with(dict(type='mouse_click', button=1))

    def test_missing_fresh_observation_times_out_without_input(self):
        with tempfile.TemporaryDirectory() as directory:
            replay = mock.Mock()
            action = self.action()
            action['timeout_seconds'] = .001
            result = MODULE._run_action(action, environment={}, output=Path(directory),
                                        offscreen_input=replay)
            self.assertFalse(result['passed'])
            replay.action.assert_not_called()

    def test_unknown_choice_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            replay = mock.Mock()
            with mock.patch.object(MODULE.time, 'sleep', side_effect=lambda _: (
                    output / 'process.log').write_text('decision=unreviewed\n')):
                with self.assertRaisesRegex(ValueError, 'Unrecognized observed input choice'):
                    MODULE._run_action(self.action(), environment={}, output=output,
                                       offscreen_input=replay)
            replay.action.assert_not_called()

    def test_failed_selected_input_stops_remaining_actions(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            action = self.action()
            action['cases']['heal'].append(dict(type='key', value='w'))
            replay = mock.Mock()
            replay.action.return_value = dict(passed=False)
            with mock.patch.object(MODULE.time, 'sleep', side_effect=lambda _: (
                    output / 'process.log').write_text('decision=heal\n')):
                result = MODULE._run_action(action, environment={}, output=output,
                                            offscreen_input=replay)
            self.assertFalse(result['passed'])
            replay.action.assert_called_once_with(dict(type='key', value='c'))

    def test_parent_deadline_prevents_observation_and_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            replay = mock.Mock()
            result = MODULE._run_action(self.action(), environment={}, output=Path(directory),
                                        offscreen_input=replay, _repeat_deadline=0)
            self.assertFalse(result['passed'])
            replay.action.assert_not_called()

    def test_validation_rejects_mutation_hidden_in_any_case(self):
        action = self.action()
        action['cases']['unused'] = [dict(type='command', command=['true'])]
        with self.assertRaises(ValueError):
            MODULE.validate_scenario_manifest(self.manifest(action))

    def test_validation_checks_nested_case_inputs_and_pattern(self):
        MODULE.validate_scenario_manifest(self.manifest(self.action()))
        for changes in [dict(pattern='no capture'), dict(path='../process.log'),
                        dict(timeout_seconds=0), dict(cases={'heal': 'not actions'})]:
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                MODULE.validate_scenario_manifest(self.manifest({**self.action(), **changes}))


if __name__ == '__main__':
    unittest.main()
