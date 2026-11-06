"""Check combined-course gates against the actual engine/Lua event formats."""
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


class SewerCourseAssemblyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo = Path(__file__).resolve().parents[2]
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        output = Path(cls.directory.name) / 'course.json'
        subprocess.run(
            [sys.executable, str(cls.repo / 'scripts/assemble_oblivion_sewer_course.py'),
             '--output', str(output)], cwd=cls.repo, check=True, capture_output=True, text=True)
        cls.course = json.loads(output.read_text())
        cls.actions = cls.course['actions']

    def gate(self, *texts):
        matches = [a['pattern'] for a in self.actions if a['type'] in ('wait_log', 'repeat_until_log')
                   and all(text in a['pattern'] for text in texts)]
        self.assertEqual(len(matches), 1, texts)
        return matches[0]

    def test_speech_and_door_events_keep_engine_producer(self):
        speech = self.gate('sayto topic=')
        self.assertRegex('M7 ObScript: sayto topic=content:oblivion.esm:032adf '
                         'info=content:oblivion.esm:03334e responses=1', speech)
        door = self.gate('M5 door activation:')
        self.assertRegex('M5 door activation: result=animated ref=content:oblivion.esm:017753', door)
        self.assertNotIn('Escape segment', speech)
        self.assertNotIn('Escape segment', door)

    def test_movement_accepts_its_lua_or_original_native_callback(self):
        for text in ('Fresh standback east reached', 'Fresh standback south reached'):
            with self.subTest(gate=text):
                pattern = self.gate(text)
                self.assertRegex('Escape segment 1:\t' + text + ' (731,-200,-108)', pattern)
                self.assertRegex('M7 ObScript: setstage quest=content:oblivion.esm:02466e stage=14', pattern)
                self.assertNotRegex('Escape segment 2:\t' + text, pattern)
        pattern = self.gate('Adaptive route complete', 'stage=70')
        self.assertRegex('Escape segment 60:\tAdaptive route complete', pattern)
        self.assertRegex('M7 ObScript: setstage quest=content:oblivion.esm:02466e stage=70', pattern)
        self.assertNotRegex('Escape segment 61:\tAdaptive route complete', pattern)

    def test_nested_info_alternatives_and_all_gate_patterns_compile(self):
        pattern = self.gate('(?:053261|02ad12)')
        for info in ('053261', '02ad12'):
            self.assertRegex('Native dialogue menu: actor=content:oblivion.esm:032a18 '
                             'info=content:oblivion.esm:' + info, pattern)
        count = 0
        def check(action):
            nonlocal count
            if 'pattern' in action:
                re.compile(action['pattern'])
                count += 1
            if 'from_log' in action:
                re.compile(action['from_log']['pattern'])
            for field in ('actions', 'cleanup_actions'):
                for child in action.get(field, []):
                    check(child)
        for action in self.actions:
            check(action)
        self.assertGreater(count, 0)

    def test_recipe_uses_original_new_game_and_only_readonly_scheduled_queries(self):
        self.assertFalse(any('--load-savegame' in arg for arg in self.course['command']))
        files = {entry['path']: entry['content'] for entry in self.course['files']}
        self.assertRegex(files['config/openmw.cfg'], r'(?m)^new-game=1$')
        queries = files['test-data/events.txt'].splitlines()
        self.assertTrue(queries)
        for query in queries:
            self.assertRegex(query, r'^at \d+ command \S+ (GetStage|GetDead)$')

    def test_observed_camera_motion_waits_for_frames_before_continuing(self):
        motions = 0
        def check(actions):
            nonlocal motions
            for index, action in enumerate(actions):
                if action['type'] == 'mouse_move' and 'from_log' in action:
                    motions += 1
                    following = actions[index + 1:index + 3]
                    self.assertEqual(following[0], {'type': 'key_down', 'value': 'F7'})
                    self.assertEqual(following[1]['type'], 'wait_log')
                    self.assertIn('camera settled', following[1]['pattern'])
                    self.assertTrue(following[1]['after_start'])
                for field in ('actions', 'cleanup_actions'):
                    check(action.get(field, []))
        check(self.actions)
        self.assertGreater(motions, 0)

    def test_focused_selection_retains_a_real_save_load_and_scoped_completion(self):
        selection = json.loads((self.repo / 'scripts/data/oblivion_compat/'
                                'oblivion_sewer_continuous_selection.json').read_text())
        selection['segments'] = [row for row in selection['segments'] if row['segment'] in (4, 5, 6)]
        root = Path(self.directory.name) / 'focused'
        root.mkdir()
        source = root / 'selection.json'
        source.write_text(json.dumps(selection))
        output = root / 'course.json'
        subprocess.run([sys.executable, str(self.repo / 'scripts/assemble_oblivion_sewer_course.py'),
                        '--selection', str(source), '--output', str(output)],
                       cwd=self.repo, check=True, capture_output=True, text=True)
        course = json.loads(output.read_text())
        self.assertTrue(any('--load-savegame=' in arg for arg in course['command']))
        self.assertIn('Escape outer segment 3 ready', course['expected_log'])
        self.assertNotIn('Escape outer segment 67 ready', course['expected_log'])
        files = {entry['path']: entry['content'] for entry in course['files']}
        self.assertRegex(files['config/openmw.cfg'], r'(?m)^new-game=0$')


if __name__ == '__main__':
    unittest.main()
