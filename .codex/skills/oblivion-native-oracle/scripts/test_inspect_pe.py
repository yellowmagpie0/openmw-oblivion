import hashlib
import struct
import tempfile
import unittest
from pathlib import Path

from inspect_pe import inspect


class InspectTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'synthetic.exe'
        self.data = bytearray(1024)
        self.data[:2] = b'MZ'
        struct.pack_into('<I', self.data, 0x3c, 0x80)
        self.data[0x80:0x84] = b'PE\0\0'
        struct.pack_into('<HH', self.data, 0x84, 0x14c, 1)
        struct.pack_into('<H', self.data, 0x94, 0xe0)
        struct.pack_into('<H', self.data, 0x98, 0x10b)
        struct.pack_into('<I', self.data, 0x98+28, 0x400000)
        struct.pack_into('<I', self.data, 0x98+56, 0x2000)
        struct.pack_into('<8sIIII', self.data, 0x178, b'.text', 0x100, 0x1000, 0x200, 0x200)

    def run_inspect(self):
        self.path.write_bytes(self.data)
        return inspect(self.path, hashlib.sha256(self.data).hexdigest())

    def test_section_mapping(self):
        result = self.run_inspect()
        self.assertEqual(result['sections'], [['.text', 0x401000, 0x200, 0x200]])
        self.assertEqual(result['image_size'], 0x2000)

    def test_wrong_hash(self):
        self.path.write_bytes(self.data)
        with self.assertRaises(ValueError): inspect(self.path, '0'*64)

    def test_truncated_table(self):
        self.data = self.data[:0x180]
        with self.assertRaises(ValueError): self.run_inspect()

    def test_wrong_architecture(self):
        struct.pack_into('<H', self.data, 0x84, 0x8664)
        with self.assertRaises(ValueError): self.run_inspect()

    def test_outside_file_and_image(self):
        for offset in (0x178+16, 0x178+12):
            with self.subTest(offset=offset):
                old = self.data[:]
                struct.pack_into('<I', self.data, offset, 0xffffffff)
                with self.assertRaises(ValueError): self.run_inspect()
                self.data = old


if __name__ == '__main__':
    unittest.main()
