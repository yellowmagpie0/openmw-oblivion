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
    def source(self, folder, *, with_food=False, with_chameleon=False, with_potion=False, with_power=False, with_damage=False, with_health_drain=False, with_skill_books=False):
        state = runtime_tests.Tes4RuntimeStateTests.getup_pending_state()
        runtime._upgrade_actor_knockback(state); state['schema_version'] = runtime.CURRENT_VERSION
        if with_food:
            player = next(a for a in state['native_actor_values'] if a['owner'] == 0)
            player['ingredient_restoration'] = [{'ingredient': 'content:oblivion.esm:033681', 'elapsed': .25}]
        if with_damage:
            player = next(a for a in state['native_actor_values'] if a['owner'] == 0)
            player['ingredient_restoration'] = [dict(ingredient='content:oblivion.esm:01ebff', elapsed=2.,
                                                     magnitude=-1., duration=3.)]
        if with_health_drain:
            player = next(a for a in state['native_actor_values'] if a['owner'] == 0)
            player['values'][8][1] = -5.
            player['ingredient_restoration'] = [dict(ingredient='content:oblivion.esm:04e940', elapsed=.25,
                                                     magnitude=-5., duration=1., actor_value=8)]
        if with_skill_books:
            state['player']['awarded_skill_books'] = ['content:oblivion.esm:0243fc']
        if with_chameleon:
            player = next(a for a in state['native_actor_values'] if a['owner'] == 0)
            player['values'][46][1] = 25.
            player['scroll_chameleon'] = [dict(scroll='content:oblivion.esm:084aeb',
                enchantment='content:oblivion.esm:05b044', magnitude=25., elapsed=12.)]
        if with_potion:
            player = next(a for a in state['native_actor_values'] if a['owner'] == 0)
            player['values'][1][1] = player['values'][2][1] = -1.
            player['potion_attribute_effects'] = [dict(potion='content:oblivion.esm:0b1201',
                duration=60., elapsed=12., modifiers=[dict(attribute=1, delta=-1.), dict(attribute=2, delta=-1.)])]
        if with_power:
            player = next(a for a in state['native_actor_values'] if a['owner'] == 0)
            player['used_powers'] = [dict(spell='content:oblivion.esm:047ade', used_at=73.25)]
        graph = state['native_getup_playback'].pop()
        payload = runtime.encode_payload(state)
        body = struct.pack('<4sII', b'VERS', 4, runtime.CURRENT_VERSION) + struct.pack('<4sI', b'DATA', len(payload)) + payload
        opaque = struct.pack('<4sIII', b'TEST', 12, 0, 0) + struct.pack('<4sI', b'DATA', 4) + b'keep'
        source = folder / 'source.omwsave'
        source.write_bytes(struct.pack('<4sIII', b'T4ST', len(body), 0, 0) + body + opaque)
        recipe = dict(schema_version=1, purpose='synthetic codec test', source=str(source),
                      source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(), target_schema=runtime.CURRENT_VERSION,
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

    def test_schema92_staging_preserves_unrelated_pending_food(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory), with_food=True)
            before = runtime.load_save(source)
            target = source.with_name('pending-food-preserved.omwsave')
            stage(source, target, recipe)
            after = runtime.load_save(target)
            food = lambda s: next(a for a in s['native_actor_values'] if a['owner'] == 0)['ingredient_restoration']
            self.assertEqual(food(before), food(after))
            self.assertEqual(food(after), [{'ingredient': 'content:oblivion.esm:033681', 'elapsed': .25}])

    def test_schema98_staging_preserves_resolved_harmful_ingredient(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory), with_damage=True)
            target = Path(directory) / 'staged.omwsave'
            before = runtime.load_save(source)
            stage(source, target, recipe)
            after = runtime.load_save(target)
            food = lambda s: next(a for a in s['native_actor_values'] if a['owner'] == 0)['ingredient_restoration']
            self.assertEqual(food(before), food(after))

    def test_schema101_staging_preserves_awarded_skill_book_history(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory), with_skill_books=True)
            target = Path(directory) / 'staged.omwsave'
            before = runtime.load_save(source)
            stage(source, target, recipe)
            after = runtime.load_save(target)
            self.assertEqual(before['player']['awarded_skill_books'], after['player']['awarded_skill_books'])

    def test_schema100_staging_preserves_health_drain_owner_and_modifier(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory), with_health_drain=True)
            target = Path(directory) / 'staged.omwsave'
            before = runtime.load_save(source)
            stage(source, target, recipe)
            after = runtime.load_save(target)
            player = lambda state: next(a for a in state['native_actor_values'] if a['owner'] == 0)
            self.assertEqual(player(before)['ingredient_restoration'], player(after)['ingredient_restoration'])
            self.assertEqual(player(before)['values'][8], player(after)['values'][8])

    def test_schema93_staging_preserves_unrelated_chameleon_source_and_contribution(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory), with_chameleon=True)
            before = runtime.load_save(source)
            target = source.with_name('pending-chameleon-preserved.omwsave')
            stage(source, target, recipe)
            after = runtime.load_save(target)
            player = lambda s: next(a for a in s['native_actor_values'] if a['owner'] == 0)
            self.assertEqual(player(before)['scroll_chameleon'], player(after)['scroll_chameleon'])
            self.assertEqual(player(before)['values'][46], player(after)['values'][46])

    def test_schema94_staging_preserves_unrelated_potion_contributions_and_lifetime(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory), with_potion=True)
            before = runtime.load_save(source)
            target = source.with_name('pending-potion-preserved.omwsave')
            stage(source, target, recipe)
            after = runtime.load_save(target)
            player = lambda s: next(a for a in s['native_actor_values'] if a['owner'] == 0)
            self.assertEqual(player(before)['potion_attribute_effects'], player(after)['potion_attribute_effects'])
            self.assertEqual(player(before)['values'][1:3], player(after)['values'][1:3])

    def test_schema95_staging_preserves_unrelated_greater_power_daily_history(self):
        with tempfile.TemporaryDirectory() as directory:
            source, recipe, _ = self.source(Path(directory), with_power=True)
            before = runtime.load_save(source)
            target = source.with_name('pending-power-preserved.omwsave')
            stage(source, target, recipe)
            after = runtime.load_save(target)
            player = lambda s: next(a for a in s['native_actor_values'] if a['owner'] == 0)
            self.assertEqual(player(before)['used_powers'], player(after)['used_powers'])

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
