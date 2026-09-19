import math
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tes4_m15_audit as audit
from tes4_m15_fixture import sub, record


def style(size=124):
    data = bytearray(124)
    data[0:2] = bytes((75, 50))
    struct.pack_into('<ff', data, 4, 0.5, 1.5)
    struct.pack_into('<ff', data, 84, 1, 2)
    struct.pack_into('<fff', data, 92, 250, 1000, 325)
    struct.pack_into('<ff', data, 104, 500, 325)
    data[112] = 25
    struct.pack_into('<fI', data, 116, 1, 1)
    return bytes(data[:size])


def plugin(records, masters=()):
    header = sub('HEDR', struct.pack('<fII', 1, len(records), 0x900))
    for master in masters:
        header += sub('MAST', master.encode() + b'\0') + sub('DATA', bytes(8))
    return record('TES4', 0, header, 1) + b''.join(records)


class M15NativeAuditTests(unittest.TestCase):
    def test_independent_grouped_layout_preserves_every_optional_tail(self):
        for size in (84, 92, 104, 112, 120, 124):
            with self.subTest(size=size):
                result = audit.combat_style(style(size))['standard']
                self.assertEqual(result['dodge_chance'], 75)
                self.assertEqual(result['dodge_lr_max'], 1.5)
                self.assertEqual(result['optimal_range_multiplier'], 1 if size >= 92 else None)
                self.assertEqual(result['ranged_switch_distance'], 1000 if size >= 104 else None)
                self.assertEqual(result['group_standoff'], 325 if size >= 112 else None)
                self.assertEqual(result['rush_chance'], 25 if size >= 120 else None)
                self.assertEqual(result['do_not_acquire'], True if size == 124 else None)

    def test_invalid_lengths_numbers_percentages_flags_and_timers_fail(self):
        for size in range(140):
            if size in (84, 92, 104, 112, 120, 124):
                continue
            with self.assertRaises(audit.M15AuditError):
                audit.combat_style(bytes(size))
        for offset, format_, value in ((0, 'B', 101), (4, 'f', math.nan), (4, 'f', 2),
                                       (84, 'f', -1), (120, 'I', 2)):
            data = bytearray(style());struct.pack_into('<' + format_, data, offset, value)
            with self.subTest(offset=offset, value=value), self.assertRaises(audit.M15AuditError):
                audit.combat_style(data)
        advanced = bytearray(84);struct.pack_into('<f', advanced, 0, -20)
        self.assertEqual(audit.combat_style(style(), advanced)['advanced']['dodge_fatigue_multiplier'], -20)
        for data in (bytes(80), bytes(85), struct.pack('<f', math.inf) + bytes(80)):
            with self.assertRaises(audit.M15AuditError):
                audit.combat_style(style(), data)

    def test_winning_styles_resolve_multiple_masters_and_deleted_overrides(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first, second, patch = (root / name for name in ('one.esm', 'two.esm', 'patch.esp'))
            first.write_bytes(plugin([record('CSTY', 0x800, sub('CSTD', style()))]))
            second.write_bytes(plugin([record('CSTY', 0x800, sub('CSTD', style(92)))]))
            actor = sub('AIDT', bytes((5, 50, 60, 100)) + bytes(8)) + sub('ACBS', bytes(16))
            actor += sub('ZNAM', struct.pack('<I', 0x01000800))
            patch.write_bytes(plugin([record('NPC_', 0x02000801, actor)], ('one.esm', 'two.esm')))
            result = audit.inventory([first, second, patch])
            self.assertTrue(result['data_passed'], result['failures'])
            self.assertFalse(result['passed']) # A data read is not a reviewed rules oracle.
            self.assertEqual(result['actors']['content:patch.esp:000801']['style'], 'content:two.esm:000800')
            patch.write_bytes(plugin([record('NPC_', 0x02000801, actor), record('CSTY', 0x01000800, b'', 0x20)], ('one.esm', 'two.esm')))
            result = audit.inventory([first, second, patch])
            self.assertFalse(result['data_passed'])
            self.assertIn('missing/deleted/wrong-type', result['failures'][0])
            self.assertEqual(len(result['styles']), 1)
            with self.assertRaisesRegex(audit.M15AuditError, 'masters must precede'):
                audit.inventory([patch, first, second])

    def test_missing_style_remains_unresolved_and_duplicate_subrecords_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.esm'
            actor = sub('AIDT', bytes(12)) + sub('ACBS', bytes(16))
            path.write_bytes(plugin([record('NPC_', 0x800, actor)]))
            result = audit.inventory([path])
            self.assertTrue(result['data_passed'])
            self.assertEqual(result['unresolved_default_actors'], ['content:fixture.esm:000800'])
            self.assertFalse(result['runtime_rules_verified'])
            path.write_bytes(plugin([record('CSTY', 0x800, sub('CSTD', style()) * 2)]))
            result = audit.inventory([path])
            self.assertFalse(result['data_passed'])
            self.assertIn('duplicate CSTD', result['failures'][0])

    def test_gmst_types_and_faction_crime_values_are_validated(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.esm'
            gmst = record('GMST', 0x800, sub('EDID', b'fCrimeValue\0') + sub('DATA', struct.pack('<f', 10)))
            faction = record('FACT', 0x801, sub('DATA', b'\x04') + sub('CNAM', struct.pack('<f', 0.5)))
            path.write_bytes(plugin([gmst, faction]))
            result = audit.inventory([path])
            self.assertTrue(result['data_passed'])
            self.assertEqual(result['settings']['fCrimeValue']['value'], 10)
            self.assertEqual(result['factions']['content:fixture.esm:000801']['crime_multiplier'], 0.5)
            faction = record('FACT', 0x801, sub('DATA', b'\x08'))
            path.write_bytes(plugin([gmst, faction]))
            self.assertFalse(audit.inventory([path])['data_passed'])

    def test_equipment_decodes_padding_and_rejects_invalid_domains(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.esm'
            weapon = struct.pack('<B3sffIIIfH', 5, b'xyz', 0.8, 1.25, 1, 75, 100, 8, 12)
            ammo = struct.pack('<fB3sIfH', 1500, 1, b'xyz', 3, 0.1, 8)
            armor = struct.pack('<HIIf', 1250, 30, 200, 15)
            path.write_bytes(plugin([record('WEAP', 0x800, sub('DATA', weapon)),
                record('AMMO', 0x801, sub('DATA', ammo)),
                record('ARMO', 0x802, sub('DATA', armor) + sub('BMDT', struct.pack('<I', 0x00802000)))]))
            result = audit.inventory([path])
            self.assertTrue(result['data_passed'], result['failures'])
            self.assertEqual(result['equipment']['content:fixture.esm:000800']['damage'], 12)
            self.assertEqual(result['equipment']['content:fixture.esm:000801']['damage'], 8)
            self.assertEqual(result['equipment']['content:fixture.esm:000802']['armor_hundredths'], 1250)
            for invalid in (weapon[:-1], weapon + b'x', bytes((6,)) + weapon[1:],
                            weapon[:4] + struct.pack('<f', math.inf) + weapon[8:]):
                path.write_bytes(plugin([record('WEAP', 0x800, sub('DATA', invalid))]))
                self.assertFalse(audit.inventory([path])['data_passed'])

    def test_creature_attack_reach_and_multiple_sound_slots_survive_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.esm'
            creature = sub('AIDT', bytes(12)) + sub('ACBS', bytes(16))
            creature += sub('DATA', struct.pack('<5BxH2xH8B', 2, 45, 20, 10, 3, 80, 15, *([50] * 8)))
            creature += sub('RNAM', bytes((48,)))
            for sound_type, sound in ((6, 0x801), (8, 0x802)):
                creature += sub('CSDT', struct.pack('<I', sound_type))
                creature += sub('CSDI', struct.pack('<I', sound)) + sub('CSDC', bytes((100,)))
            path.write_bytes(plugin([record('CREA', 0x800, creature), record('SOUN', 0x801, b''), record('SOUN', 0x802, b'')]))
            result = audit.inventory([path])
            self.assertTrue(result['data_passed'], result['failures'])
            actor = result['actors']['content:fixture.esm:000800']
            self.assertEqual(actor['creature']['attack_damage'], 15)
            self.assertEqual(actor['creature']['reach'], 48)
            self.assertEqual([s['type'] for s in actor['creature']['sounds']], [6, 8])
            path.write_bytes(plugin([record('CREA', 0x800, creature)]))
            self.assertFalse(audit.inventory([path])['data_passed'])

    def test_nonmaster_indices_follow_native_source_file_resolution(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.esm'
            path.write_bytes(plugin([record('CSTY', 0x03000800, sub('CSTD', style()))]))
            result = audit.inventory([path])
            self.assertTrue(result['data_passed'])
            self.assertIn('content:fixture.esm:000800', result['styles'])

    def test_relationships_and_membership_resolve_after_deletion(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base, patch = root / 'base.esm', root / 'patch.esp'
            faction = sub('DATA', b'\x04') + sub('XNAM', struct.pack('<Ii', 0x801, -50))
            actor = sub('AIDT', bytes(12)) + sub('ACBS', bytes(16))
            actor += sub('SNAM', struct.pack('<Ib3x', 0x800, 2))
            base.write_bytes(plugin([record('FACT', 0x800, faction), record('FACT', 0x801, sub('DATA', b'\0')),
                                     record('NPC_', 0x802, actor)]))
            result = audit.inventory([base])
            self.assertTrue(result['data_passed'], result['failures'])
            self.assertEqual(result['factions']['content:base.esm:000800']['relationships'],
                             [{'faction': 'content:base.esm:000801', 'modifier': -50}])
            self.assertEqual(result['actors']['content:base.esm:000802']['factions'],
                             [{'faction': 'content:base.esm:000800', 'rank': 2}])
            patch.write_bytes(plugin([record('FACT', 0x801, b'', 0x20)], ('base.esm',)))
            self.assertFalse(audit.inventory([base, patch])['data_passed'])

    def test_truncated_headers_and_wrong_game_version_fail_before_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.esm'
            for data in (b'', b'TES4', plugin([])[:-1], record('TES4', 0, sub('HEDR', struct.pack('<fII', 1.7, 0, 0)), 1)):
                path.write_bytes(data)
                with self.assertRaises((ValueError, RuntimeError)):
                    audit.inventory([path])


if __name__ == '__main__':
    unittest.main()
