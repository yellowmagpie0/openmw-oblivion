import math
import json
import copy
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
    def test_shipped_magic_link_review_requires_exact_record_plugin_and_issue(self):
        issue = dict(source='content:base.esm:000800', target='content:base.esm:000810',
                     expected_types=['SCPT'], actual_type='REFR', deleted=False, message='wrong type')
        result = dict(plugins=[dict(name='base.esm', sha256='a'*64)],
                      spells={issue['source']: dict(plugin='base.esm', record_sha256='b'*64)},
                      link_issues=[issue], failures=['wrong type'], data_passed=False,
                      runtime_rules_verified=False, passed=False)
        exception = dict(source=issue['source'], target=issue['target'], expected_types=['SCPT'],
                         actual_type='REFR', plugin='base.esm', plugin_sha256='a'*64,
                         record_sha256='b'*64, owner='M16', reason='Reviewed shipped defect')
        catalog = dict(schema_version=1, exceptions=[exception])
        reviewed = copy.deepcopy(result)
        self.assertTrue(audit.classify_shipped_magic_links(reviewed, catalog)['passed'])
        self.assertTrue(reviewed['data_passed'])
        self.assertFalse(reviewed['passed'])
        self.assertFalse(reviewed['runtime_rules_verified'])
        self.assertFalse(reviewed['shipped_magic_link_defects'][0]['activation_supported'])
        self.assertEqual(reviewed['link_issues'], result['link_issues'])
        unknown = copy.deepcopy(result)
        unknown['failures'].append('another unsupported link')
        audit.classify_shipped_magic_links(unknown, catalog)
        self.assertEqual(unknown['failures'], ['another unsupported link'])
        self.assertFalse(unknown['data_passed'])
        for field, value in [('plugin_sha256','c'*64), ('record_sha256','c'*64),
                             ('plugin','patch.esp'), ('target','content:base.esm:000811'),
                             ('actual_type',None)]:
            changed = copy.deepcopy(catalog)
            changed['exceptions'][0][field] = value
            reviewed = copy.deepcopy(result)
            self.assertFalse(audit.classify_shipped_magic_links(reviewed, changed)['passed'])
            self.assertIn('wrong type', reviewed['failures'])
            self.assertFalse(reviewed['data_passed'])
        for change in ('missing_source', 'missing_issue', 'duplicate_issue', 'deleted'):
            reviewed = copy.deepcopy(result)
            if change == 'missing_source': reviewed['spells'].clear()
            elif change == 'missing_issue': reviewed['link_issues'].clear()
            elif change == 'duplicate_issue': reviewed['link_issues'].append(copy.deepcopy(issue))
            else: reviewed['link_issues'][0]['deleted'] = True
            self.assertFalse(audit.classify_shipped_magic_links(reviewed, catalog)['passed'])
            self.assertFalse(reviewed['data_passed'])
        for field, value in [('owner','M15'), ('expected_types',['NPC_']), ('reason','')]:
            changed = copy.deepcopy(catalog)
            changed['exceptions'][0][field] = value
            with self.assertRaises(audit.M15AuditError):
                audit.classify_shipped_magic_links(copy.deepcopy(result), changed)
        with self.assertRaises(audit.M15AuditError):
            audit.classify_shipped_magic_links(copy.deepcopy(result),
                dict(schema_version=1, exceptions=[exception, exception]))
        for malformed in (None, {}, dict(exception, reason=' '), dict(exception, plugin_sha256='invalid'),
                          dict(exception, record_sha256=None), dict(exception, actual_type=[])):
            with self.assertRaises(audit.M15AuditError):
                audit.classify_shipped_magic_links(copy.deepcopy(result),
                    dict(schema_version=1, exceptions=[malformed]))

    def test_effect_prefix_preserves_partial_layouts_padding_and_reference_identity(self):
        def decode(data,code=b'FOSP\0'):
            return audit.effect_definition(dict(plugin='patch.esp',masters=['other.esm','base.esm'],
                subrecords=[dict(name='EDID',payload=code),dict(name='DATA',payload=data)]))
        prefix=struct.pack('<4IiHH',0x1000072,0x7fc12345,9,5,-1,7,0x80ff)
        for length in range(24,69,4):
            result=decode(prefix+bytes(length-24))
            self.assertEqual(result['data_length'],length);self.assertEqual(result['base_cost_bits'],0x7fc12345)
            self.assertEqual(result['associated_data'],9);self.assertIsNone(result['associated_form'])
            self.assertEqual(result['resistance_actor_value'],-1);self.assertEqual(result['counter_padding'],0x80ff)
        for flag in (1<<16,1<<17,1<<18):
            data=struct.pack('<4IiHH',flag,0,0x01000812,5,-1,0,0)
            self.assertEqual(decode(data,b'FOSP')['associated_form'],'content:base.esm:000812')
        for length in range(73):
            if 24<=length<=68 and length%4==0:continue
            with self.assertRaises(audit.M15AuditError):decode(bytes(length))
        for code in (b'',b'FO',b'F\0SP',b'FOSPX',b'FOSP\0\0'):
            with self.assertRaises(audit.M15AuditError):decode(prefix,code)

    def test_effect_inventory_uses_stable_keys_overrides_deletes_and_reports_ambiguous_codes(self):
        def effect(form,code,data):
            return record('MGEF',form,sub('EDID',code)+sub('DATA',struct.pack('<4IiHH',0x1000072,0,data,5,-1,0,0)))
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);base=root/'base.esm';patch=root/'patch.esp'
            base.write_bytes(plugin([effect(0x800,b'FOSP\0',9)]))
            patch.write_bytes(plugin([effect(0x800,b'FOSP\0',40)],['base.esm']))
            result=audit.inventory([base,patch]);self.assertTrue(result['data_passed'],result['failures'])
            self.assertEqual(result['effect_definitions']['content:base.esm:000800']['associated_data'],40)
            patch.write_bytes(plugin([record('MGEF',0x800,b'',0x20)],['base.esm']))
            self.assertEqual(audit.inventory([base,patch])['summary']['effect_definitions'],0)
            patch.write_bytes(plugin([effect(0x01000800,b'FOSP\0',40)],['base.esm']))
            result=audit.inventory([base,patch]);self.assertFalse(result['data_passed'])
            self.assertIn('ambiguous winning MGEF effect codes',result['failures'])

    def test_spell_layout_order_padding_partial_script_fields_and_name(self):
        def decode(parts):
            return audit.spell_definition(dict(plugin='base.esm', masters=['other.esm'],
                subrecords=[dict(name=tag,payload=data) for tag,data in parts]))
        info = struct.pack('<IIIB3s',4,123,2,1,b'\xa5\xff\x80')
        effect = struct.pack('<4s5I',b'SEFF',50,3,7,0,9)
        basic = [('SPIT',info),('FULL',b'spell\0'),('EFID',b'SEFF'),('ZZZZ',b'poison'),('EFIT',effect)]
        for size in (4,12,16):
            script = struct.pack('<IIIB3s',0x01000812,3,0x47444946,1,b'\xa5\xff\x80')[:size]
            result = decode(basic+[('SCIT',script),('FULL',b'effect\0')])
            self.assertEqual(result['flags'],1)
            self.assertEqual(result['padding'],[0xa5,0xff,0x80])
            self.assertEqual(result['full_name'],'spell')
            parsed = result['effects'][0]['script']
            self.assertEqual(parsed['key'],'content:base.esm:000812')
            self.assertEqual(parsed['name'],'effect')
            self.assertEqual(parsed['school'],3 if size>=12 else None)
            self.assertEqual(parsed['visual'],0x47444946 if size>=12 else None)
            self.assertEqual(parsed['flags'],1 if size==16 else None)
        malformed = [[],[('SPIT',info[:-1])],basic+[('SPIT',info)],basic+[('EFIT',effect)],
                     basic+[('EFID',b'FOAT')],basic+[('SCIT',bytes(8))],basic+[('SCIT',bytes(4))]*2,
                     [('SPIT',info),('EFID',b'FOAT'),('EFIT',effect)],
                     [('SPIT',info),('SCIT',bytes(4))]]
        for parts in malformed:
            with self.assertRaises(audit.M15AuditError):decode(parts)

    def test_spell_inventory_overrides_remaps_script_links_and_deletes(self):
        def spell(form,magnitude,script):
            return record('SPEL',form,sub('SPIT',struct.pack('<IIIB3s',4,0,0,1,bytes(3)))
                +sub('EFID',b'SEFF')+sub('EFIT',struct.pack('<4s5I',b'SEFF',magnitude,0,0,0,9))
                +sub('SCIT',struct.pack('<I',script)))
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);base=root/'base.esm';other=root/'other.esm';patch=root/'patch.esp'
            base.write_bytes(plugin([spell(0x800,50,0x810),record('SCPT',0x810,b'')]))
            other.write_bytes(plugin([spell(0x800,60,0x811),record('SCPT',0x811,b'')]))
            patch.write_bytes(plugin([spell(0x01000800,100,0x00000811)],['other.esm','base.esm']))
            result=audit.inventory([base,other,patch]);self.assertTrue(result['data_passed'],result['failures'])
            effects=result['spells']['content:base.esm:000800']['effects']
            self.assertEqual(effects[0]['magnitude'],100)
            self.assertEqual(effects[0]['script']['key'],'content:other.esm:000811')
            self.assertEqual(result['spells']['content:other.esm:000800']['effects'][0]['magnitude'],60)
            patch.write_bytes(plugin([record('SPEL',0x01000800,b'',0x20)],['other.esm','base.esm']))
            result=audit.inventory([base,other,patch]);self.assertEqual(result['summary']['spells'],1)
            patch.write_bytes(plugin([spell(0x01000800,100,0x00000999)],['other.esm','base.esm']))
            self.assertFalse(audit.inventory([base,other,patch])['data_passed'])

    def test_native_skill_layout_domains_and_mismatched_index(self):
        index = struct.pack('<I', 19)
        data = struct.pack('<III2f', 19, 1, 1, 5, .5)
        self.assertEqual(audit.skill_definition(index, data),
                         dict(actor_value=19, governing_attribute=1, specialization=1, use_values=[5, .5]))
        for bad in (data[:-1], data + b'\0', struct.pack('<III2f', 18, 1, 1, 5, .5),
                    struct.pack('<III2f', 19, 8, 1, 5, .5), struct.pack('<III2f', 19, 1, 3, 5, .5),
                    struct.pack('<III2f', 19, 1, 1, math.nan, .5)):
            with self.assertRaises(audit.M15AuditError):
                audit.skill_definition(index, bad)

    def test_skill_winners_overrides_deletions_and_duplicate_actor_values(self):
        def skill(ident, av, attribute):
            return record('SKIL', ident, sub('INDX', struct.pack('<I', av))
                          + sub('DATA', struct.pack('<III2f', av, attribute, 0, 1, 2)))
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory) / 'base.esm'
            patch = Path(directory) / 'patch.esp'
            base.write_bytes(plugin([skill(i + 100, i + 12, 0) for i in range(21)]))
            patch.write_bytes(plugin([skill(100, 12, 5)], ['base.esm']))
            result = audit.inventory([base, patch])
            self.assertTrue(result['data_passed'])
            self.assertTrue(result['skill_inventory_complete'])
            self.assertEqual(result['skills']['content:base.esm:000064']['governing_attribute'], 5)
            patch.write_bytes(plugin([record('SKIL', 100, b'', 0x20)], ['base.esm']))
            self.assertFalse(audit.inventory([base, patch])['skill_inventory_complete'])
            patch.write_bytes(plugin([skill(0x01000100, 12, 5)], ['base.esm']))
            result = audit.inventory([base, patch])
            self.assertFalse(result['data_passed'])
            self.assertFalse(result['skill_inventory_complete'])

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

    def test_actor_equipment_spells_and_race_resolve_winning_master_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base, other, patch = (root / name for name in ('base.esm', 'other.esm', 'patch.esp'))
            base.write_bytes(plugin([record('MISC', 0x800, b''), record('SPEL', 0x801, sub('SPIT', struct.pack('<IIIB3s', 0, 0, 0, 0, bytes(3)))),
                                    record('RACE', 0x802, b''), record('CLAS', 0x803, b''), record('LVSP', 0x805, b'')]))
            other.write_bytes(plugin([record('SGST', 0x800, b'')]))
            actor = sub('AIDT', bytes(12)) + sub('ACBS', bytes(16))
            actor += sub('CNTO', struct.pack('<Ii', 0x01000800, 3))
            actor += sub('SPLO', struct.pack('<I', 0x801)) + sub('SPLO', struct.pack('<I', 0x805))
            actor += sub('RNAM', struct.pack('<I', 0x802)) + sub('CNAM', struct.pack('<I', 0x803))
            patch.write_bytes(plugin([record('NPC_', 0x02000804, actor)], ('base.esm', 'other.esm')))
            result = audit.inventory([base, other, patch])
            self.assertTrue(result['data_passed'], result['failures'])
            npc = result['actors']['content:patch.esp:000804']
            self.assertEqual(npc['inventory'], [{'item': 'content:other.esm:000800', 'count': 3}])
            self.assertEqual(npc['spells'], ['content:base.esm:000801', 'content:base.esm:000805'])
            self.assertEqual(npc['race'], 'content:base.esm:000802')
            self.assertEqual(npc['class'], 'content:base.esm:000803')
            patch.write_bytes(plugin([record('NPC_', 0x02000804, actor),
                record('SPEL', 0x801, b'', 0x20)], ('base.esm', 'other.esm')))
            self.assertFalse(audit.inventory([base, other, patch])['data_passed'])

    def test_actor_dependencies_reject_bad_layouts_and_wrong_types(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.esm'
            core = sub('AIDT', bytes(12)) + sub('ACBS', bytes(16))
            for tag, size in [('CNTO', 8), ('SPLO', 4), ('RNAM', 4), ('CNAM', 4)]:
                for bad_size in [0, size - 1, size + 1]:
                    with self.subTest(tag=tag, size=bad_size):
                        path.write_bytes(plugin([record('NPC_', 0x800, core + sub(tag, bytes(bad_size)))]))
                        self.assertFalse(audit.inventory([path])['data_passed'])
            for tag, data in [('CNTO', struct.pack('<Ii', 0x801, 1)), ('SPLO', struct.pack('<I', 0x801)),
                              ('RNAM', struct.pack('<I', 0x801)), ('CNAM', struct.pack('<I', 0x801))]:
                with self.subTest(tag=tag):
                    path.write_bytes(plugin([record('NPC_', 0x800, core + sub(tag, data)), record('STAT', 0x801, b'')]))
                    self.assertFalse(audit.inventory([path])['data_passed'])

    def test_policy_inventory_resolves_native_defaults_and_authored_zero_rules(self):
        catalog = json.loads((Path(__file__).resolve().parents[2] /
            'docs/oblivion/M15-COMBAT-STYLE-DEFAULTS.json').read_text())
        authored = audit.combat_style(style(84))
        report = {'settings': {'iAIDefaultDodgeChance': {'type': 'i', 'value': 42},
            'fAIDefaultOptimalRangeMult': {'type': 'f', 'value': 3.0}},
            'styles': {'style': authored}, 'actors': {'default': {'style': 'null'}, 'custom': {'style': 'style'}}}
        result = audit.policy_inventory(report, catalog)
        self.assertTrue(result['passed'], result['failures'])
        default = result['policies']['null']
        self.assertEqual(default['standard']['dodge_chance'], 42)
        self.assertEqual(default['standard']['optimal_range_multiplier'], 3)
        self.assertEqual(default['advanced']['dodge_fatigue_multiplier'], -20)
        custom = result['policies']['style']
        self.assertEqual(custom['standard']['dodge_chance'], 75)
        self.assertEqual(custom['standard']['optimal_range_multiplier'], 1)
        self.assertEqual(custom['standard']['melee_switch_distance'], 250)
        self.assertEqual(custom['standard']['rush_chance'], 25)
        self.assertFalse(custom['standard']['do_not_acquire'])
        self.assertEqual(result['actor_policy_keys'], {'default': 'null', 'custom': 'style'})
        self.assertIsNone(authored['standard']['optimal_range_multiplier'])
        full = audit.combat_style(style())
        full['standard'].update(melee_switch_distance=0, rush_chance=0, rush_distance_multiplier=0,
                                buff_standoff=0, ranged_standoff=0)
        report['styles']['style'] = full
        resolved = audit.policy_inventory(report, catalog)['policies']['style']['standard']
        self.assertEqual(resolved['melee_switch_distance'], 250)
        self.assertEqual(resolved['rush_chance'], 25)
        self.assertEqual(resolved['rush_distance_multiplier'], 1)
        self.assertEqual(resolved['buff_standoff'], 0)
        self.assertEqual(resolved['ranged_standoff'], 0)

    def test_policy_inventory_rejects_bad_defaults_and_missing_advanced_data(self):
        catalog = json.loads((Path(__file__).resolve().parents[2] /
            'docs/oblivion/M15-COMBAT-STYLE-DEFAULTS.json').read_text())
        report = {'settings': {}, 'styles': {}, 'actors': {}}
        for value in [-1, 101, 42.0, True]:
            report['settings'] = {'iAIDefaultDodgeChance': {'type': 'i', 'value': value}}
            with self.subTest(value=value), self.assertRaises(audit.M15AuditError):
                audit.policy_inventory(report, catalog)
        report['settings'] = {'fAIDefaultIdleMaxTime': {'type': 'f', 'value': math.nan}}
        with self.assertRaises(audit.M15AuditError):
            audit.policy_inventory(report, catalog)
        report['settings'] = {}
        incomplete = copy.deepcopy(catalog);incomplete['settings'].pop()
        with self.assertRaises(audit.M15AuditError):
            audit.policy_inventory(report, incomplete)
        authored = audit.combat_style(style());authored['standard']['flags'] = 1
        report['styles'] = {'style': authored};report['actors'] = {'actor': {'style': 'style'}}
        result = audit.policy_inventory(report, catalog)
        self.assertFalse(result['passed'])
        self.assertNotIn('actor', result['actor_policy_keys'])
        report['styles'] = {}
        self.assertFalse(audit.policy_inventory(report, catalog)['passed'])

    def test_policy_inventory_keeps_advanced_signed_values_and_rejects_ambiguous_inputs(self):
        catalog = json.loads((Path(__file__).resolve().parents[2] /
            'docs/oblivion/M15-COMBAT-STYLE-DEFAULTS.json').read_text())
        extra = struct.pack('<21f', -13, *([2] * 20))
        authored = audit.combat_style(style(), extra);authored['standard']['flags'] = 1
        report = {'settings': {}, 'styles': {'style': authored}, 'actors': {}}
        resolved = audit.policy_inventory(report, catalog)['policies']['style']
        self.assertEqual(resolved['advanced']['dodge_fatigue_multiplier'], -13)
        self.assertEqual(resolved['advanced']['power_attack_fatigue_multiplier'], 2)
        authored['standard']['flags'] = 0
        self.assertEqual(audit.policy_inventory(report, catalog)['policies']['style']
                         ['advanced']['power_attack_fatigue_multiplier'], -10)
        report['settings'] = {'fAIDefaultIdleMinTime': {'type': 'f', 'value': 5}}
        with self.assertRaises(audit.M15AuditError):
            audit.policy_inventory(report, catalog)
        report['settings'] = {'iAIDefaultYieldEnabled': {'type': 'i', 'value': -1}}
        self.assertEqual(audit.policy_inventory(report, catalog)['policies']['null']['standard']['flags'], 8)
        report['settings']['iaidefaultyieldenabled'] = {'type': 'i', 'value': 1}
        with self.assertRaises(audit.M15AuditError):
            audit.policy_inventory(report, catalog)
        report['settings'] = {}
        catalog['settings'].append(dict(catalog['settings'][0]))
        with self.assertRaises(audit.M15AuditError):
            audit.policy_inventory(report, catalog)

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

    def test_ownership_keeps_reference_and_cell_policy_separate(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.esm'
            owner = record('FACT', 0x800, sub('DATA', b'\0'))
            cell = record('CELL', 0x801, sub('XOWN', struct.pack('<I', 0x800)) + sub('XRNK', struct.pack('<i', 2)))
            item = record('MISC', 0x802, b'')
            ref = record('REFR', 0x803, sub('NAME', struct.pack('<I', 0x802)))
            group = struct.pack('<4sIIII', b'GRUP', 20 + len(ref), 0x801, 6, 0) + ref
            path.write_bytes(plugin([owner, cell, item]) + group)
            result = audit.inventory([path])
            self.assertTrue(result['data_passed'], result['failures'])
            self.assertEqual(result['ownership']['content:fixture.esm:000801']['owner'], 'content:fixture.esm:000800')
            self.assertEqual(result['ownership']['content:fixture.esm:000801']['rank'], 2)
            reference = result['references']['content:fixture.esm:000803']
            self.assertEqual(reference['cell'], 'content:fixture.esm:000801')
            self.assertEqual(reference['owner'], 'null')
            self.assertEqual(reference['base'], 'content:fixture.esm:000802')
            path.write_bytes(plugin([cell, item]) + group)
            self.assertFalse(audit.inventory([path])['data_passed'])

    def test_prison_markers_are_reciprocal_doors_and_roles_require_real_bases(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.esm'
            records = [record('CELL', 0x800, b''), record('DOOR', 0x801, b''),
                       record('CONT', 0x802, b''), record('FURN', 0x803, b'')]
            refs = []
            for ident, base, destination in ((0x810, 0x801, 0x811), (0x811, 0x801, 0x810),
                                              (0x812, 0x801, None), (0x813, 0x802, None), (0x814, 0x803, None)):
                payload = sub('NAME', struct.pack('<I', base))
                if destination:
                    payload += sub('XTEL', struct.pack('<I6f', destination, *([0] * 6)))
                refs.append(record('REFR', ident, payload))
            group = struct.pack('<4sIIII', b'GRUP', 20 + sum(map(len, refs)), 0x800, 6, 0) + b''.join(refs)
            path.write_bytes(plugin(records) + group)
            key = lambda ident: f'content:fixture.esm:{ident:06x}'
            prison = dict(name='Fixture', cell=key(0x800), prison_marker=key(0x810), release_marker=key(0x811),
                          cell_door=key(0x812), evidence=key(0x813), bed_candidate=key(0x814), guards=[])
            result = audit.inventory([path], [prison])
            self.assertTrue(result['data_passed'], result['failures'])
            self.assertEqual(result['summary']['prisons'], 1)
            prison['release_marker'] = key(0x812)
            result = audit.inventory([path], [prison])
            self.assertFalse(result['data_passed'])
            self.assertTrue(any('reciprocal prison teleport' in failure for failure in result['failures']))
            prison['evidence'] = key(0x814)
            self.assertFalse(audit.inventory([path], [prison])['data_passed'])

    def test_count_lock_requires_exact_order_hashes_and_reviewed_counts(self):
        import copy
        report = {'plugins': [{'name': 'base.esm', 'sha256': 'a' * 64},
                              {'name': 'patch.esp', 'sha256': 'b' * 64}],
                  'summary': {'styles': 1, 'actors': 2}}
        lock = {'schema_version': 1, 'plugins': copy.deepcopy(report['plugins']),
                'counts': {'styles': 1, 'actors': 2}}
        self.assertTrue(audit.check_count_lock(report, lock)['passed'])
        for candidate in ({}, dict(lock, plugins=[]), dict(lock, counts={'styles': 2}),
                          dict(lock, counts={'unknown': 0}), dict(lock, schema_version=2),
                          dict(lock, plugins=list(reversed(lock['plugins']))),
                          dict(lock, counts={'styles': True, 'actors': 2})):
            self.assertFalse(audit.check_count_lock(report, candidate)['passed'])
        changed = copy.deepcopy(report)
        changed['plugins'][0]['sha256'] = 'b' * 64
        self.assertFalse(audit.check_count_lock(changed, lock)['passed'])
        changed = copy.deepcopy(report)
        changed['summary']['actors'] = 3
        self.assertFalse(audit.check_count_lock(changed, lock)['passed'])

    def test_truncated_headers_and_wrong_game_version_fail_before_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.esm'
            for data in (b'', b'TES4', plugin([])[:-1], record('TES4', 0, sub('HEDR', struct.pack('<fII', 1.7, 0, 0)), 1)):
                path.write_bytes(data)
                with self.assertRaises((ValueError, RuntimeError)):
                    audit.inventory([path])


if __name__ == '__main__':
    unittest.main()
