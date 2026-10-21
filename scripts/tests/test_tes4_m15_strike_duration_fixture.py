import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tes4_m15_reaction_duration_fixture import HEADER
from tes4_m15_strike_duration_fixture import build_fixture


class StrikeDurationFixtureTests(unittest.TestCase):
    def prepare(self, root):
        recipe = dict(schema_version=1, purpose='Synthetic test without game assets',
                      source_archive_sha256='a' * 64, extended_end=4., blend_frames=30,
                      assets={'body': [], 'first': []})
        sources = {}
        for view, directory in [('body', '_male'), ('first', '_1stperson')]:
            for suffix in ('left', 'right', 'power'):
                name = ('Attack' + suffix.title()).encode()
                data = bytearray(b'Z' * 400)
                data[:len(HEADER)] = HEADER
                offset, blocks = 64, 1
                data[offset - 4:offset + len(name)] = struct.pack('<I', len(name)) + name
                struct.pack_into('<II', data, offset + len(name), blocks, 1)
                stop = offset + 28 + len(name) + 33 * blocks
                bits = 0x3f000000
                struct.pack_into('<fIIffI', data, stop - 20, 1., 5, 2, 1., 0., bits)
                count_offset = len(data)
                data += struct.pack('<I', 3)
                end_offset = None
                for time, key in [(0., b'start'), (.2, b'Hit'), (.5, b'end')]:
                    if key == b'end': end_offset = len(data)
                    data += struct.pack('<fI', time, len(key)) + key
                data += bytes.fromhex('0100000000000000')
                path = f'meshes/characters/{directory}/handtohandattack{suffix}.kf'
                asset = dict(path=path, sha256=hashlib.sha256(data).hexdigest(), stop_offset=stop,
                             end_key_offset=end_offset, original_end_bits=bits,
                             sequence_name_offset=offset, controlled_blocks=blocks,
                             sequence_name=name.decode(), text_key_count_offset=count_offset)
                recipe['assets'][view].append(asset)
                destination = root / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(data)
                sources[path] = bytes(data)
        return recipe, sources

    def test_both_views_preserve_every_byte_outside_declared_times_and_inserted_key(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recipe, sources = self.prepare(root / 'source')
            for view in ('body', 'first'):
                output = root / view
                report = build_fixture(root / 'source', view, recipe, output)
                self.assertEqual(len(report['files']), 3)
                self.assertFalse(report['runtime_acceptance'])
                self.assertFalse(report['nif_reader_validation'])
                self.assertFalse(report['archive_itself_rehashed'])
                for asset in recipe['assets'][view]:
                    actual = (output / asset['path']).read_bytes()
                    expected = bytearray(sources[asset['path']])
                    for key in ('stop_offset', 'end_key_offset'):
                        struct.pack_into('<f', expected, asset[key], 4.)
                    pos = asset['text_key_count_offset']
                    struct.pack_into('<I', expected, pos, 4)
                    expected[pos + 4:pos + 4] = struct.pack('<fI', 0., 8) + b'blend:30'
                    self.assertEqual(actual, expected)
                    self.assertEqual((root / 'source' / asset['path']).read_bytes(), sources[asset['path']])
                self.assertEqual(len(list(output.rglob('*.kf'))), 3)
                with self.assertRaises(ValueError):
                    build_fixture(root / 'source', view, recipe, output)

    def test_bad_recipes_and_late_asset_failure_leave_no_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recipe, _ = self.prepare(root / 'source')
            mutations = [
                lambda r: r.update(schema_version=True),
                lambda r: r.update(blend_frames=True),
                lambda r: r.update(blend_frames=0),
                lambda r: r.update(blend_frames=256),
                lambda r: r.update(source_archive_sha256='no'),
                lambda r: r.update(extended_end=float('nan')),
                lambda r: r.update(unexpected=1),
                lambda r: r['assets']['body'].pop(),
                lambda r: r['assets']['body'].__setitem__(2, r['assets']['body'][0]),
                lambda r: r['assets']['body'][2].update(path='../foreign.kf'),
                lambda r: r['assets']['body'][2].update(sequence_name='AttackLeft'),
                lambda r: r['assets']['body'][2].update(sha256='0' * 64),
                lambda r: r['assets']['body'][2].update(text_key_count_offset=True),
            ]
            for index, mutate in enumerate(mutations):
                with self.subTest(index=index):
                    bad = copy.deepcopy(recipe)
                    mutate(bad)
                    output = root / f'bad-{index}'
                    with self.assertRaises(ValueError):
                        build_fixture(root / 'source', 'body', bad, output)
                    self.assertFalse(output.exists())

    def test_rehashed_wrong_key_array_and_footer_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recipe, sources = self.prepare(root / 'source')
            last = recipe['assets']['body'][2]
            pristine = sources[last['path']]
            positions = [last['text_key_count_offset'], last['text_key_count_offset'] + 12,
                         last['end_key_offset'] + 8, len(pristine) - 1]
            for index, position in enumerate(positions):
                with self.subTest(position=position):
                    damaged = bytearray(pristine)
                    damaged[position] ^= 1
                    (root / 'source' / last['path']).write_bytes(damaged)
                    bad = copy.deepcopy(recipe)
                    bad['assets']['body'][2]['sha256'] = hashlib.sha256(damaged).hexdigest()
                    output = root / f'bad-layout-{index}'
                    with self.assertRaises(ValueError):
                        build_fixture(root / 'source', 'body', bad, output)
                    self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
