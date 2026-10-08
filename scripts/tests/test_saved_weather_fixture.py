import copy
import hashlib
import struct
import unittest

from scripts.saved_weather_fixture import build
from scripts.saved_weather_state import decode_weather_state
from scripts.tests.test_saved_weather_state import fields, record
from scripts.tes4_runtime_state import RuntimeStateError, _save_records


class RegionalWeatherFixtureTest(unittest.TestCase):
    def inputs(self):
        source = record(b'TES3', [(b'MAST', b'a.esm\0')])
        source += record(b'WTHR', fields()) + record(b'T4ST', [(b'DATA', b'opaque-native-state')])
        recipe = dict(version=1, source_sha256=hashlib.sha256(source).hexdigest(), update_time=10000,
                      regions=[dict(region='string:climate', weather='content:b.esp:000002',
                                    fallback='content:b.esp:000002', buckets=[
                                        dict(weather='content:a.esm:000001', chance=25),
                                        dict(weather='content:b.esp:000002', chance=25)])])
        return source, recipe

    def test_preserves_all_other_records_and_checks_exact_regional_readback(self):
        source, recipe = self.inputs()
        output, report = build(source, recipe)
        self.assertEqual((output, report), build(source, recipe))
        self.assertEqual([source[a:b] for a,b,tag,_ in _save_records(source) if tag != b'WTHR'],
                         [output[a:b] for a,b,tag,_ in _save_records(output) if tag != b'WTHR'])
        state = decode_weather_state(output)
        self.assertEqual(state['update_time'], 10000)
        self.assertEqual(state['regions']['string:climate'], dict(weather='content:b.esp:000002',
                         fallback='content:b.esp:000002', chances=[25,25],
                         selection_order=['content:a.esm:000001', 'content:b.esp:000002']))
        self.assertEqual(report['non_weather_records_exact'], 2)
        self.assertEqual(report['output_sha256'], hashlib.sha256(output).hexdigest())

    def test_rejects_invalid_recipe_and_unknown_duplicate_or_overfull_buckets(self):
        source, recipe = self.inputs()
        mutations = [lambda x:x.update(extra=1), lambda x:x.update(version=True),
                     lambda x:x.update(source_sha256='0'*64), lambda x:x.update(update_time=float('nan')),
                     lambda x:x.update(update_time=-1), lambda x:x.update(update_time=True),
                     lambda x:x.update(regions=[]), lambda x:x['regions'].append(x['regions'][0]),
                     lambda x:x['regions'][0].update(region='missing'),
                     lambda x:x['regions'][0].update(weather=[]),
                     lambda x:x['regions'][0].update(fallback='missing'),
                     lambda x:x['regions'][0]['buckets'][0].update(chance=True),
                     lambda x:x['regions'][0]['buckets'][0].update(chance=0),
                     lambda x:x['regions'][0]['buckets'][0].update(chance=100),
                     lambda x:x['regions'][0]['buckets'].append(x['regions'][0]['buckets'][0])]
        for mutation in mutations:
            bad = copy.deepcopy(recipe); mutation(bad)
            with self.subTest(recipe=bad), self.assertRaises(ValueError):
                build(source, bad)

    def test_rejects_legacy_catalog_absence_and_malformed_save(self):
        _, recipe = self.inputs()
        for source in [record(b'WTHR', fields(native=False)), b'truncated']:
            recipe['source_sha256'] = hashlib.sha256(source).hexdigest()
            with self.subTest(source=source), self.assertRaises((ValueError, RuntimeStateError)):
                build(source, recipe)


if __name__ == '__main__':
    unittest.main()
