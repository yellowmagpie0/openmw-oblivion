import copy
import hashlib
import struct
import unittest
import zlib

from scripts.saved_fog_fixture import build
from scripts.tes4_runtime_state import _save_records, RuntimeStateError


def sub(tag, payload):
    return tag + struct.pack('<I', len(payload)) + payload


def record(tag, payload):
    return tag + struct.pack('<III', len(payload), 0, 0) + payload


def png(width=32):
    def chunk(tag, payload):
        return struct.pack('>I', len(payload)) + tag + payload + struct.pack('>I', zlib.crc32(tag + payload) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, 32, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress((b'\0' + bytes([0, 0, 0, 255]) * width) * 32)) + chunk(b'IEND', b''))


def source(image=None):
    return (record(b'SAVE', sub(b'NAME', b'private diagnostic'))
            + record(b'T4ST', sub(b'VERS', struct.pack('<I', 46)) + sub(b'DATA', b'opaque native bytes'))
            + record(b'CSTA', sub(b'CELL', b'cell identity') + sub(b'FTEX', struct.pack('<ii', -2, 7) + (png() if image is None else image)))
            + record(b'OTHR', sub(b'TEST', b'unchanged')))


def recipe(data):
    return dict(version=1, source_sha256=hashlib.sha256(data).hexdigest(), native_schema=46, pixel=[12, 34, 56, 78])


class SavedFogFixtureTest(unittest.TestCase):
    def test_deterministic_output_preserves_native_and_other_fields_and_has_valid_pixel(self):
        data = source()
        result, report = build(data, recipe(data))
        self.assertEqual((result, report), build(data, recipe(data)))
        old = list(_save_records(data)); new = list(_save_records(result))
        self.assertEqual([data[a:b] for a, b, tag, _ in old if tag != b'CSTA'],
                         [result[a:b] for a, b, tag, _ in new if tag != b'CSTA'])
        fields = next(fields for _, _, tag, fields in new if tag == b'CSTA')
        _, a, b = fields[1]
        self.assertEqual(result[a + 8:a + 16], struct.pack('<ii', -2, 7))
        image = result[a + 16:b]
        self.assertEqual(image[:8], b'\x89PNG\r\n\x1a\n')
        offset = 8; chunks = []
        while offset < len(image):
            size = struct.unpack_from('>I', image, offset)[0]
            tag = image[offset + 4:offset + 8]; payload = image[offset + 8:offset + 8 + size]
            self.assertEqual(struct.unpack_from('>I', image, offset + 8 + size)[0], zlib.crc32(tag + payload) & 0xffffffff)
            chunks.append((tag, payload)); offset += size + 12
        self.assertEqual([tag for tag, _ in chunks], [b'IHDR', b'IDAT', b'IEND'])
        self.assertEqual(chunks[0][1], struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0))
        self.assertEqual(zlib.decompress(chunks[1][1]), b'\0\x0c\x22\x38\x4e')
        self.assertEqual(report['output_sha256'], hashlib.sha256(result).hexdigest())

    def test_recipe_identity_and_types_reject_before_mutation(self):
        data = source(); valid = recipe(data)
        for field, value in [('source_sha256', 'wrong'), ('version', True), ('version', 2),
                             ('native_schema', 45), ('native_schema', True), ('pixel', [0, 0, 0, True]),
                             ('pixel', [0, 0, 0, 256]), ('pixel', [0, 0, 0]), ('pixel', 'rgba')]:
            with self.subTest(field=field, value=value):
                changed = copy.deepcopy(valid); changed[field] = value
                with self.assertRaises(ValueError): build(data, changed)
        with self.assertRaises(ValueError): build(data, dict(valid, extra=True))

    def test_original_image_shape_and_format_are_pinned(self):
        for image in [png(31), b'bad', png()[:25] + b'\x02' + png()[26:]]:
            with self.subTest(image=image[:26]):
                data = source(image)
                with self.assertRaises(ValueError): build(data, recipe(data))

    def test_native_schema_and_duplicate_native_record_reject(self):
        valid = source()
        for data in [valid.replace(sub(b'VERS', struct.pack('<I', 46)), sub(b'VERS', struct.pack('<I', 45))),
                     valid + record(b'T4ST', sub(b'VERS', struct.pack('<I', 46)))]:
            with self.assertRaises(ValueError): build(data, recipe(data))

    def test_missing_or_multiple_texture_reject(self):
        for data in [source().replace(b'FTEX', b'NONE'), source() + record(b'CSTA', sub(b'FTEX', struct.pack('<ii', 0, 0) + png()))]:
            with self.assertRaises(ValueError): build(data, recipe(data))

    def test_truncated_save_rejects_structural_read(self):
        data = source()[:-1]
        with self.assertRaises(RuntimeStateError): build(data, recipe(data))
