import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tes4_m15_npc_passive_fixture import stage, runtime
from scripts.tests import test_tes4_runtime_state as runtime_tests


class NpcPassiveFixtureTests(unittest.TestCase):
    def source(self, directory, idle=False):
        recipe = json.loads((Path(__file__).resolve().parents[1] /
            'data/oblivion_compat/m15_npc_passive_strike_fixture.json').read_text())
        state = runtime_tests.Tes4RuntimeStateTests().passive_sequence_state()
        actor = state['native_actor_values'][0]['actor']
        recipe['actor'] = recipe['melee_state']['actor'] = actor
        state['native_melee_states'] = []
        if idle:
            entry = copy.deepcopy(recipe['melee_state']); entry['passive_sequence'] = None
            state['native_melee_states'] = [entry]
        state['physical_actions'] = {'next': 2, 'pending': []}; state['physical_action_owners'] = []
        for value in state['native_actor_values']: value['process_action'] = -1
        state['references'][0]['actor_draw_state'] = 0
        state['native_animation_clocks'].append({'actor': actor, 'clock': recipe['actor_clock']})
        state['content'].append({'plugin': recipe['content_override']['plugin'],
                                 'fingerprint': recipe['content_override']['source_fingerprint']})
        plugin = directory / 'private.esp'; plugin.write_bytes(b'inspected private fixture bytes')
        recipe['content_override']['replacement_fingerprint'] = 'sha256:' + hashlib.sha256(plugin.read_bytes()).hexdigest()
        payload = runtime.encode_payload(state)
        body = struct.pack('<4sII', b'VERS', 4, 59) + struct.pack('<4sI', b'DATA', len(payload)) + payload
        opaque = struct.pack('<4sIII', b'TEST', 12, 0, 0) + struct.pack('<4sI', b'DATA', 4) + b'keep'
        source = directory / 'source.omwsave'
        source.write_bytes(struct.pack('<4sIII', b'T4ST', len(body), 0, 0) + body + opaque)
        recipe['source_sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
        return source, recipe, plugin, opaque

    def test_full_wire_preserves_every_field_except_declared_history_draw_and_content(self):
        for idle in (False, True):
            with tempfile.TemporaryDirectory() as folder:
                source, recipe, plugin, opaque = self.source(Path(folder), idle)
                original = source.read_bytes(); before = runtime.load_save(source)
                output = Path(folder) / 'new/staged.omwsave'
                report = stage(source, output, recipe, plugin); after = runtime.load_save(output)
                self.assertEqual(after['native_melee_states'], [recipe['melee_state']])
                after['native_melee_states'] = before['native_melee_states']
                after['references'][0]['actor_draw_state'] = 0
                after['content'] = before['content']
                self.assertEqual(after, before)
                self.assertEqual(output.read_bytes()[-len(opaque):], opaque)
                self.assertEqual(source.read_bytes(), original)
                self.assertTrue(report['semantic_reinspection_passed'])
                self.assertFalse(report['runtime_acceptance'])
                self.assertFalse(report['compiler_world_validation'])

    def test_foreign_hash_clock_schema_and_view_reject_without_output(self):
        with tempfile.TemporaryDirectory() as folder:
            source, recipe, plugin, _ = self.source(Path(folder))
            for key, value in [('source_sha256', '0' * 64), ('target_schema', 58),
                               ('schema_version', True), ('actor_draw_state', True),
                               ('actor_clock', recipe['actor_clock'] + 1), ('actor', 'null')]:
                with self.subTest(key=key):
                    bad = copy.deepcopy(recipe); bad[key] = value
                    output = Path(folder) / 'rejected.omwsave'
                    with self.assertRaises(ValueError): stage(source, output, bad, plugin)
                    self.assertFalse(output.exists())
            bad = copy.deepcopy(recipe); bad['melee_state']['first_person'] = {'clock': 0}
            with self.assertRaises(ValueError): stage(source, Path(folder) / 'view.omwsave', bad, plugin)
            bad = copy.deepcopy(recipe); bad['melee_state']['input']['queued'] = 1
            with self.assertRaises(ValueError): stage(source, Path(folder) / 'queued.omwsave', bad, plugin)
            self.assertFalse((Path(folder) / 'queued.omwsave').exists())

    def test_plugin_bytes_and_old_content_fingerprint_are_both_pinned(self):
        with tempfile.TemporaryDirectory() as folder:
            source, recipe, plugin, _ = self.source(Path(folder))
            bad = copy.deepcopy(recipe); bad['content_override']['source_fingerprint'] = 'sha256:foreign'
            with self.assertRaises(ValueError): stage(source, Path(folder) / 'old.omwsave', bad, plugin)
            plugin.write_bytes(b'changed after inspection')
            with self.assertRaises(ValueError): stage(source, Path(folder) / 'new.omwsave', recipe, plugin)
            self.assertFalse((Path(folder) / 'old.omwsave').exists())
            self.assertFalse((Path(folder) / 'new.omwsave').exists())

    def test_replaying_passive_cause_rejects_and_existing_destination_is_untouched(self):
        with tempfile.TemporaryDirectory() as folder:
            source, recipe, plugin, _ = self.source(Path(folder))
            output = Path(folder) / 'output.omwsave'; output.write_bytes(b'keep')
            with self.assertRaises(FileExistsError): stage(source, output, recipe, plugin)
            self.assertEqual(output.read_bytes(), b'keep')
            bad = copy.deepcopy(recipe); bad['melee_state']['passive_sequence']['strike']['id'] = 2
            output = Path(folder) / 'replay.omwsave'
            with self.assertRaises(runtime.RuntimeStateError): stage(source, output, bad, plugin)
            self.assertFalse(output.exists())
