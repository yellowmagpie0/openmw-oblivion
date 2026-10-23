import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tes4_m15_passive_slot3_fixture import stage, runtime
from scripts.tests import test_tes4_runtime_state as runtime_tests


class PassiveSlot3FixtureTests(unittest.TestCase):
    def source(self, folder, *, idle_player=False, drawn=1):
        recipe = json.loads((Path(__file__).resolve().parents[1] /
            'data/oblivion_compat/m15_passive_slot3_controller_fixture.json').read_text())
        state = runtime_tests.Tes4RuntimeStateTests().passive_sequence_state()
        state['native_melee_states'] = []
        if idle_player:
            idle = copy.deepcopy(recipe['melee_state'])
            idle.pop('held_blocks', None)
            idle.update(reaction=None, first_person=None, passive_sequence=None)
            state['native_melee_states'] = [idle]
        state['physical_actions'] = copy.deepcopy(recipe['physical_actions'])
        state['physical_action_owners'] = []
        for value in state['native_actor_values']:
            value['process_action'] = -1
        for clock in state['native_animation_clocks']:
            if clock['actor'] == recipe['actor']:
                clock['clock'] = recipe['actor_clock']
        payload = runtime.encode_payload(state)
        body = struct.pack('<4sII', b'VERS', 4, 59) + struct.pack('<4sI', b'DATA', len(payload)) + payload
        draw = struct.pack('<4sIi', b'DRAW', 4, drawn)
        opaque = (struct.pack('<4sIII', b'PLAY', len(draw), 0, 0) + draw
                  + struct.pack('<4sIII', b'CAM_', 9, 0, 0) + struct.pack('<4sIB', b'FIRS', 1, 1)
                  + struct.pack('<4sIII', b'TEST', 12, 0, 0) + struct.pack('<4sI', b'DATA', 4) + b'keep')
        source = folder / 'source.omwsave'
        source.write_bytes(struct.pack('<4sIII', b'T4ST', len(body), 0, 0) + body + opaque)
        recipe['source_sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
        return source, recipe, opaque

    def test_complete_wire_and_opaque_records_preserve_every_undeclared_field(self):
        for idle_player in (False, True):
            with tempfile.TemporaryDirectory() as directory:
                source, recipe, opaque = self.source(Path(directory), idle_player=idle_player)
                before_bytes = source.read_bytes()
                before = runtime.load_save(source)
                before['schema_version'] = runtime.CURRENT_VERSION; before['native_movement'] = None
                output = Path(directory) / 'new' / 'staged.omwsave'
                report = stage(source, output, recipe)
                after = runtime.load_save(output)
                self.assertEqual(after['native_melee_states'], [recipe['melee_state']])
                after['native_melee_states'] = before['native_melee_states']
                next(v for v in after['native_actor_values'] if v['actor'] == recipe['actor'])['process_action'] = -1
                self.assertEqual(after, before)
                self.assertEqual(output.read_bytes()[-len(opaque):], opaque)
                self.assertEqual(source.read_bytes(), before_bytes)
                self.assertTrue(report['semantic_reinspection_passed'])
                self.assertFalse(report['runtime_acceptance'])
                self.assertFalse(report['normal_input_contact'])

    def test_foreign_recipe_hash_clock_and_schema_reject_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory))
            before = source.read_bytes()
            for key, value in [('source_sha256', '0' * 64), ('target_schema', 58),
                               ('schema_version', True), ('process_action', True),
                               ('shared_player_draw_state', True),
                               ('actor_clock', recipe['actor_clock'] + 1), ('actor', 'null')]:
                with self.subTest(key=key):
                    bad = copy.deepcopy(recipe); bad[key] = value
                    output = Path(directory) / 'rejected.omwsave'
                    with self.assertRaises(ValueError): stage(source, output, bad)
                    self.assertFalse(output.exists())
                    self.assertEqual(source.read_bytes(), before)

    def test_existing_output_and_replaying_passive_cause_are_preserved_or_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory))
            output = Path(directory) / 'existing.omwsave'; output.write_bytes(b'keep output')
            with self.assertRaises(FileExistsError): stage(source, output, recipe)
            self.assertEqual(output.read_bytes(), b'keep output')
            bad = copy.deepcopy(recipe)
            bad['melee_state']['passive_sequence']['strike']['id'] = 2
            rejected = Path(directory) / 'replaying.omwsave'
            with self.assertRaises(runtime.RuntimeStateError): stage(source, rejected, bad)
            self.assertFalse(rejected.exists())

    def test_holstered_source_rejects_before_injecting_resident_attack_history(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory), drawn=0)
            output = Path(directory) / 'holstered.omwsave'
            with self.assertRaises(ValueError): stage(source, output, recipe)
            self.assertFalse(output.exists())

    def test_third_person_variant_changes_only_the_declared_opaque_camera_byte(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, opaque = self.source(Path(directory))
            recipe['camera_first_person'] = False
            output = Path(directory) / 'third.omwsave'
            report = stage(source, output, recipe)
            actual = output.read_bytes()[-len(opaque):]
            differences = [(a, b) for a, b in zip(opaque, actual) if a != b]
            self.assertEqual(differences, [(1, 0)])
            self.assertEqual(runtime.load_save(output)['native_melee_states'], [recipe['melee_state']])
            self.assertEqual(report['shared_camera'], {'source_first_person': True, 'output_first_person': False})
