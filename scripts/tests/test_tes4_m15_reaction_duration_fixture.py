import hashlib
import math
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tes4_m15_reaction_duration_fixture import build, HEADER


class ReactionDurationFixtureTests(unittest.TestCase):
    def source(self):
        data = bytearray(b'Z' * 512)
        data[:len(HEADER)] = HEADER
        name, count = 64, 1
        data[name - 4:name + 8] = struct.pack('<I', 8) + b'BlockHit'
        struct.pack_into('<II', data, name + 8, count, 1)
        stop, key = name + 36 + count * 33, 480
        struct.pack_into('<fIIffI', data, stop - 20, 1., 5, 2, 1., 0., 0x3e999998)
        data[key:key + 11] = struct.pack('<II', 0x3e999998, 3) + b'end'
        source = bytes(data)
        asset = dict(path='meshes/characters/_male/onehandblockhit.kf',
                     sha256=hashlib.sha256(source).hexdigest(), stop_offset=stop, end_key_offset=key,
                     original_end_bits=0x3e999998, sequence_name_offset=name, controlled_blocks=count)
        return source, asset

    def test_only_two_declared_times_change_and_opaque_bytes_are_preserved(self):
        source, asset = self.source()
        output, report = build(source, asset, 1.5)
        self.assertEqual(build(source, asset, 1.5)[0], output)
        self.assertEqual(len(output), len(source))
        excluded = {i for key in ('stop_offset', 'end_key_offset') for i in range(asset[key], asset[key] + 4)}
        self.assertEqual(bytes(v for i, v in enumerate(output) if i not in excluded),
                         bytes(v for i, v in enumerate(source) if i not in excluded))
        for key in ('stop_offset', 'end_key_offset'):
            self.assertEqual(struct.unpack_from('<f', output, asset[key])[0], 1.5)
        self.assertEqual(hashlib.sha256(source).hexdigest(), asset['sha256'])
        self.assertTrue(report['transform_tracks_and_other_bytes_preserved'])
        self.assertFalse(report['nif_reader_validation_performed'])
        self.assertFalse(report['runtime_acceptance'])

    def test_wrong_hash_layout_identity_and_float_locations_are_rejected(self):
        source, asset = self.source()
        for position in (0, asset['sequence_name_offset'], asset['sequence_name_offset'] + 8,
                         asset['stop_offset'] - 20, asset['stop_offset'] - 12,
                         asset['stop_offset'] - 8, asset['stop_offset'] - 4,
                         asset['stop_offset'], asset['end_key_offset'], asset['end_key_offset'] + 8):
            with self.subTest(position=position):
                damaged = bytearray(source); damaged[position] ^= 1
                with self.assertRaises(ValueError): build(bytes(damaged), asset, 1.5)
                declared = dict(asset, sha256=hashlib.sha256(damaged).hexdigest())
                with self.assertRaises(ValueError): build(bytes(damaged), declared, 1.5)
        for key, value in [('stop_offset', -1), ('end_key_offset', 510), ('sequence_name_offset', True),
                           ('controlled_blocks', 0), ('original_end_bits', True)]:
            bad = dict(asset); bad[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): build(source, bad, 1.5)

    def test_nonfinite_shortened_and_unrepresentable_extensions_are_rejected(self):
        source, asset = self.source()
        original = struct.unpack('<f', struct.pack('<I', asset['original_end_bits']))[0]
        for end in (False, -1., 0., original, .2, math.nextafter(original, 1.), 4.01,
                    float('inf'), float('-inf'), float('nan')):
            with self.subTest(end=end), self.assertRaises(ValueError): build(source, asset, end)


if __name__ == '__main__':
    unittest.main()
