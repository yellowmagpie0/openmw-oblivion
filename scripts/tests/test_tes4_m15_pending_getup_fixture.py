import copy
import hashlib
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tes4_m15_pending_getup_fixture import stage, runtime
from scripts.tests import test_tes4_runtime_state as runtime_tests


class PendingGetUpFixtureTests(unittest.TestCase):
    def source(self, folder):
        state = runtime_tests.Tes4RuntimeStateTests.getup_pending_state()
        graph = state['native_getup_playback'].pop()
        payload = runtime.encode_payload(state)
        body = struct.pack('<4sII', b'VERS', 4, 74) + struct.pack('<4sI', b'DATA', len(payload)) + payload
        opaque = struct.pack('<4sIII', b'TEST', 12, 0, 0) + struct.pack('<4sI', b'DATA', 4) + b'keep'
        source = folder / 'source.omwsave'
        source.write_bytes(struct.pack('<4sIII', b'T4ST', len(body), 0, 0) + body + opaque)
        recipe = dict(schema_version=1, purpose='synthetic codec test', source=str(source),
                      source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(), target_schema=74,
                      actor=graph['actor'], process_knocked_state=5, process_action=-1, playback=graph)
        return source, recipe, opaque

    def test_exact_mutations_preserve_opaque_records_seed_rng_and_other_state(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, opaque = self.source(Path(directory))
            before = source.read_bytes(); expected = runtime.load_save(source)
            values = next(v for v in expected['native_actor_values'] if v['actor'] == recipe['actor'])
            values.update(process_knocked_state=5, process_action=-1)
            expected['native_getup_playback'] = [copy.deepcopy(recipe['playback'])]
            target = source.with_name('pending.omwsave')
            report = stage(source, target, recipe)
            self.assertEqual(runtime.load_save(target), expected)
            self.assertEqual(source.read_bytes(), before)
            self.assertTrue(target.read_bytes().endswith(opaque))
            self.assertFalse(report['runtime_acceptance'])
            self.assertFalse(report['io_queued'])

    def test_invalid_graph_rejects_before_output_and_preserves_seed(self):
        for mutation in [lambda g: g['pending_requests'].pop(),
                         lambda g: g['requests'][0].update(resource=99),
                         lambda g: g['pending_requests'][0].update(load_pending=1)]:
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                source, recipe, _ = self.source(Path(directory)); before = source.read_bytes()
                mutation(recipe['playback']); target = source.with_name('bad.omwsave')
                with self.assertRaises(runtime.RuntimeStateError): stage(source, target, recipe)
                self.assertFalse(target.exists()); self.assertEqual(source.read_bytes(), before)

    def test_wrong_seed_or_recipe_cannot_write_or_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory)); target = source.with_name('new.omwsave')
            wrong = copy.deepcopy(recipe); wrong['source_sha256'] = '0' * 64
            with self.assertRaises(ValueError): stage(source, target, wrong)
            wrong = copy.deepcopy(recipe); wrong['process_action'] = True
            with self.assertRaises(ValueError): stage(source, target, wrong)
            self.assertFalse(target.exists()); target.write_bytes(b'keep')
            with self.assertRaises(FileExistsError): stage(source, target, recipe)
            self.assertEqual(target.read_bytes(), b'keep')
            before = source.read_bytes()
            with self.assertRaises(FileExistsError): stage(source, source, recipe)
            self.assertEqual(source.read_bytes(), before)

    def test_existing_pending_owner_cannot_be_replaced_by_fixture_staging(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory)); target = source.with_name('pending.omwsave')
            stage(source, target, recipe)
            recipe['source_sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
            second = source.with_name('second.omwsave')
            with self.assertRaises(ValueError): stage(target, second, recipe)
            self.assertFalse(second.exists())


if __name__ == '__main__':
    unittest.main()
