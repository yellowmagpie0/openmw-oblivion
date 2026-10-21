import copy
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tes4_m15_cached_block_hold_fixture import build_fixture
from scripts.tests import test_tes4_m15_reaction_duration_fixture as duration_fixture


class CachedBlockHoldFixtureTests(unittest.TestCase):
    def source(self, directory):
        original, asset = duration_fixture.ReactionDurationFixtureTests().source()
        data = bytearray(original[:400])
        keys = struct.pack('<I', 3)
        for time, text in [(0., b'start'),
                           (struct.unpack('<f', struct.pack('<I', 0x3d088880))[0], b'Blend: 0'),
                           (struct.unpack('<f', struct.pack('<I', 0x3e999998))[0], b'end')]:
            if text == b'end':
                asset['end_key_offset'] = len(data) + len(keys)
            if text == b'Blend: 0':
                asset['blend_text_offset'] = len(data) + len(keys) + 8
            keys += struct.pack('<fI', time, len(text)) + text
        data += keys + bytes.fromhex('0100000000000000')
        source = bytes(data)
        asset.update(sha256=hashlib.sha256(source).hexdigest(), text_key_count_offset=400)
        assets = {}
        for view, name in [('body', '_male'), ('first', '_1stperson')]:
            row = dict(asset, path='meshes/characters/' + name + '/onehandblockhit.kf')
            path = directory / row['path']
            path.parent.mkdir(parents=True)
            path.write_bytes(source)
            assets[view] = row
        return source, dict(schema_version=1, purpose='synthetic test', source_archive_sha256='a' * 64,
                            extended_end=4., blend_frames=30, assets=assets)

    def test_only_declared_time_stores_and_existing_blend_change_in_both_views(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, recipe = self.source(root / 'source')
            output = root / 'fixture'
            report = build_fixture(root / 'source', recipe, output)
            self.assertEqual(len(report['files']), 2)
            for asset in recipe['assets'].values():
                result = (output / asset['path']).read_bytes()
                offset = asset['text_key_count_offset']
                self.assertEqual(struct.unpack_from('<I', result, offset)[0], 3)
                self.assertEqual(len(result), len(source))
                blend = asset['blend_text_offset']
                self.assertEqual(result[blend:blend + 8], b'Blend:30')
                self.assertEqual(struct.unpack_from('<I', result, blend - 8)[0], 0x3d088880)
                restored = bytearray(result)
                self.assertEqual(struct.unpack_from('<f', restored, asset['stop_offset'])[0], 4.)
                self.assertEqual(struct.unpack_from('<f', restored, asset['end_key_offset'])[0], 4.)
                restored[blend:blend + 8] = b'Blend: 0'
                for field in ('stop_offset', 'end_key_offset'):
                    struct.pack_into('<I', restored, asset[field], 0x3e999998)
                self.assertEqual(bytes(restored), source)
                self.assertEqual((root / 'source' / asset['path']).read_bytes(), source)
            self.assertTrue(report['structural_write_success'])
            self.assertFalse(report['nif_reader_validation'])
            self.assertFalse(report['runtime_acceptance'])

    def test_foreign_recipe_bad_second_asset_and_existing_output_reject_before_write(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, recipe = self.source(root / 'source')
            changes = [('blend_frames', 24), ('blend_frames', True), ('extended_end', 3.),
                       ('source_archive_sha256', 'foreign')]
            for field, value in changes:
                bad = copy.deepcopy(recipe)
                bad[field] = value
                with self.subTest(field=field), self.assertRaises(ValueError):
                    build_fixture(root / 'source', bad, root / 'rejected')
                self.assertFalse((root / 'rejected').exists())
            for field, value in [('path', '../foreign.kf'), ('sha256', '0' * 64),
                                 ('text_key_count_offset', 401), ('blend_text_offset', 426)]:
                bad = copy.deepcopy(recipe)
                bad['assets']['first'][field] = value
                with self.subTest(field=field), self.assertRaises(ValueError):
                    build_fixture(root / 'source', bad, root / 'rejected')
                self.assertFalse((root / 'rejected').exists())
            bad = copy.deepcopy(recipe)
            first = root / 'source' / bad['assets']['first']['path']
            original = first.read_bytes()
            changed = bytearray(original)
            changed[bad['assets']['first']['blend_text_offset'] + 7] = ord('1')
            first.write_bytes(changed)
            bad['assets']['first']['sha256'] = hashlib.sha256(changed).hexdigest()
            with self.assertRaisesRegex(ValueError, 'reviewed key-array'):
                build_fixture(root / 'source', bad, root / 'rejected')
            self.assertFalse((root / 'rejected').exists())
            first.write_bytes(original)
            output = root / 'existing'
            output.mkdir()
            (output / 'user-file').write_text('keep')
            with self.assertRaises(FileExistsError):
                build_fixture(root / 'source', recipe, output)
            self.assertEqual((output / 'user-file').read_text(), 'keep')


if __name__ == '__main__':
    unittest.main()
