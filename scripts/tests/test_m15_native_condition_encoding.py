import struct
import unittest

from tes4_m14_audit import _parse_pack


class NativeConditionEncodingTests(unittest.TestCase):
    def test_native_function_word_and_flag_subject_ignore_padding(self):
        # Original56A970/56AC50 padding oracle. PACK and IDLE use the same
        # native condition object; these values also cover the installed
        # nonzero IDLE tail that originally redirected the fork's context.
        for tail in (0, 1, 2, 3, 4, 0xff, 0xfeedbeef):
            for padding in (0, 1, 0xffff):
                for target in (False, True):
                    with self.subTest(tail=tail, padding=padding, target=target):
                        condition = struct.pack('<IfIIII', 2 if target else 0, 1.,
                                                107 | padding << 16, 0, 0, tail)
                        subrecords = [
                            {'name': 'PKDT', 'offset': 0, 'payload': struct.pack('<IB3x', 0, 1)},
                            {'name': 'PSDT', 'offset': 0, 'payload': struct.pack('<bbBbi', -1, -1, 0, -1, 0)},
                            {'name': 'CTDA', 'offset': 0, 'payload': condition},
                        ]
                        result = _parse_pack({'plugin': 'base.esm', 'key': 'content:base.esm:001000'},
                                             subrecords, [])['conditions'][0]
                        self.assertEqual(result['function'], 107)
                        self.assertEqual(result['unknown_tail'], tail)
                        self.assertEqual(result['run_on_raw'], int(target))
                        self.assertEqual(result['run_on'], 'Target' if target else 'Subject')
