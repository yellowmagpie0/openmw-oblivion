import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tes4_m15_cached_block_input import stage, runtime
from scripts.tests.test_tes4_runtime_state import make_state


class CachedBlockInputTests(unittest.TestCase):
    def source(self, directory, *, local=0):
        recipe = json.loads((Path(__file__).resolve().parents[1] /
                             'data/oblivion_compat/m15_cached_block_input.json').read_text())
        state = make_state()
        state.update(schema_version=46, ai_rng_state=17, combat_rng_state=123,
                     physical_actions={'next': 2, 'pending': []})
        state['content'][0]['plugin'] = 'oblivion.esm'
        state['script_instances'] = [dict(unit=recipe['old_unit'], context=recipe['context'],
                                          on_load_fired=False,
                                          locals=[{'type': 'number', 'value': local}])]
        payload = runtime.encode_payload(state)
        body = struct.pack('<4sII', b'VERS', 4, 46) + struct.pack('<4sI', b'DATA', len(payload)) + payload
        opaque = struct.pack('<4sIII', b'TEST', 12, 0, 0) + struct.pack('<4sI', b'DATA', 4) + b'keep'
        source = directory / 'source.omwsave'
        source.write_bytes(struct.pack('<4sIII', b'T4ST', len(body), 0, 0) + body + opaque)
        recipe['source_sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
        return source, recipe, opaque

    def test_real_wire_migration_changes_only_origin_and_empty_schema_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            source, recipe, opaque = self.source(folder)
            before_bytes = source.read_bytes()
            before = runtime.load_save(source)
            output = folder / 'new' / 'staged.omwsave'
            report = stage(source, output, recipe)
            actual = runtime.load_save(output)
            self.assertEqual(actual['schema_version'], 59)
            self.assertEqual(actual['script_instances'][0]['unit'], recipe['new_unit'])
            actual['schema_version'] = before['schema_version']
            actual['script_instances'][0]['unit'] = recipe['old_unit']
            for item in before['player']['inventory']:
                item['extra_scale'] = None
            for key, value in before.items():
                self.assertEqual(actual[key], value, key)
            self.assertEqual(actual.get('native_loose_weapons'), [])
            self.assertEqual(output.read_bytes()[-len(opaque):], opaque)
            self.assertEqual(source.read_bytes(), before_bytes)
            self.assertTrue(report['structural_write_success'])
            self.assertTrue(report['semantic_reinspection_passed'])
            self.assertFalse(report['runtime_acceptance'])
            self.assertEqual(report['target_schema'], 59)
            self.assertEqual(json.loads(output.with_suffix('.staging.json').read_text()), report)

    def test_stale_hash_schema_and_foreign_script_recipe_reject_before_output(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            source, recipe, _ = self.source(folder)
            before = source.read_bytes()
            for key, value in [('source_sha256', '0' * 64), ('target_schema', 57),
                               ('target_schema', 58), ('target_schema', True),
                               ('source_schema', 45), ('new_unit', recipe['old_unit']),
                               ('context', 'content:oblivion.esm:000007')]:
                with self.subTest(key=key, value=value):
                    bad = copy.deepcopy(recipe)
                    bad[key] = value
                    output = folder / 'rejected.omwsave'
                    with self.assertRaises(ValueError):
                        stage(source, output, bad)
                    self.assertFalse(output.exists())
                    self.assertEqual(source.read_bytes(), before)

    def test_existing_output_and_nonidle_script_are_preserved_and_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            source, recipe, _ = self.source(folder)
            output = folder / 'existing.omwsave'
            output.write_bytes(b'user-owned')
            with self.assertRaises(FileExistsError):
                stage(source, output, recipe)
            self.assertEqual(output.read_bytes(), b'user-owned')
            source, recipe, _ = self.source(folder, local=1)
            rejected = folder / 'rejected.omwsave'
            with self.assertRaisesRegex(ValueError, 'declared idle script instance'):
                stage(source, rejected, recipe)
            self.assertFalse(rejected.exists())


if __name__ == '__main__':
    unittest.main()
