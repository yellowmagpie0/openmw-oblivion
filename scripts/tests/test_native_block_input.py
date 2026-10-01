"""Behavioral checks of the shipped Lua held-block input scripts."""
from pathlib import Path
import subprocess
import unittest


class NativeBlockInputTest(unittest.TestCase):
    def check_lua(self, mode, expected):
        root = Path(__file__).resolve().parents[2]
        result = subprocess.run(
            ['luajit', str(root / 'scripts/tests/data/test_native_block_input.lua'), str(root), mode],
            text=True, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(expected, result.stdout)

    def test_controls_release_and_reload(self):
        self.check_lua('controls', 'PASS controls 64 combinations')

    def test_mouse_and_trigger_threshold(self):
        self.check_lua('bindings', 'PASS bindings 24 combinations')
