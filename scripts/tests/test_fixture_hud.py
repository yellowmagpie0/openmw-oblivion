import importlib.util
from pathlib import Path
import tempfile
import struct
import zlib
import unittest

path = Path(__file__).resolve().parents[2] / '.codex/skills/openmw-build-test/scripts/verify_fixture_hud.py'
spec = importlib.util.spec_from_file_location('fixture_hud', path)
hud = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hud)


class FixtureHudTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / 'capture.png'

    def image(self, size=(1280, 720), colors=()):
        width, height = size
        pixels = bytearray([20, 20, 20]) * (width * height)
        for y, start, stop, color in colors:
            for x in range(start, stop):
                offset = (y * width + x) * 3
                pixels[offset:offset + 3] = bytes(color)
        def chunk(tag, data):
            return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
        rows = b''.join(b'\0' + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
        self.path.write_bytes(b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))

    def test_distinguishes_quarter_health_and_full_fatigue(self):
        self.image(colors=[(674, 14, 30, (120, 50, 40)), (704, 14, 75, (40, 120, 40))])
        result = hud.verify(self.path, {'health': (.22, .30), 'fatigue': (.95, 1)})
        self.assertTrue(result['passed'])
        self.assertEqual(result['bars']['health']['pixels'], 16)
        self.assertEqual(result['bars']['fatigue']['pixels'], 61)
        self.assertFalse(hud.verify(self.path, {'health': (.95, 1)})['passed'])

    def test_rejects_empty_and_fragmented_fills(self):
        self.image()
        self.assertFalse(hud.verify(self.path, {'health': (.22, .30)})['passed'])
        self.image(colors=[(674, 14, 20, (120, 50, 40)), (674, 21, 31, (120, 50, 40))])
        self.assertFalse(hud.verify(self.path, {'health': (.22, .30)})['passed'])

    def test_rejects_mismatched_layout_and_missing_images(self):
        self.image(size=(640, 360))
        with self.assertRaises(ValueError):
            hud.verify(self.path, {'health': (.95, 1)})
        self.path.unlink()
        with self.assertRaises(FileNotFoundError):
            hud.verify(self.path, {'health': (.95, 1)})

    def test_rejects_invalid_expectations(self):
        self.image()
        for expected in [{}, {'unknown': (0, 1)}, {'health': (1, 0)},
                         {'health': (-.1, 1)}, {'health': (0, 1.1)}, {'health': (0, float('nan'))}]:
            with self.subTest(expected=expected), self.assertRaises(ValueError):
                hud.verify(self.path, expected)
