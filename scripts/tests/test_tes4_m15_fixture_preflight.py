import copy
import unittest

from scripts.tes4_m15_fixture_preflight import approach_witness, forward_source_stage, pending_npc_power_source


class FixturePreflightTests(unittest.TestCase):
    def state(self):
        return dict(
            player=dict(reference='player'),
            native_actor_values=[dict(actor='target', values=[[0]] * 48 + [[0]],
                process_knocked_state=0, process_action=-1,
                melee_paralysis=[dict(cause=9, duration=10, elapsed=0,
                    applied=False, terminated=False, removed=False)])],
            native_melee_states=[dict(actor='player', strike=dict(id=9, contact_committed=True))],
            native_actor_ragdolls=[], physical_actions=dict(next=10, pending=[]))

    def test_pending_and_applied_source(self):
        state = self.state()
        self.assertEqual(forward_source_stage(state, 'target'), 'pending')
        row = state['native_actor_values'][0]
        row['values'][48] = [1]
        row['melee_paralysis'][0].update(applied=True, elapsed=2)
        row['process_knocked_state'] = 3
        state['native_actor_ragdolls'] = [dict(actor='target')]
        self.assertEqual(forward_source_stage(state, 'target'), 'applied')
        row['melee_paralysis'] = []
        row['values'][48] = [0]
        self.assertIsNone(forward_source_stage(state, 'target'))

    def test_invalid_pending_sources_rejected(self):
        changes = [
            lambda s: s['native_actor_values'][0]['melee_paralysis'][0].update(duration=float('nan')),
            lambda s: s['native_actor_values'][0]['melee_paralysis'][0].update(elapsed=1),
            lambda s: s['native_actor_values'][0]['melee_paralysis'][0].update(removed=True),
            lambda s: s['native_actor_values'][0]['values'].__setitem__(48, [1]),
            lambda s: s['physical_actions']['pending'].append(dict(id=9)),
            lambda s: s['native_melee_states'][0]['strike'].update(contact_committed=False),
            lambda s: s['native_actor_ragdolls'].append(dict(actor='target')),
            lambda s: s['native_actor_values'][0]['melee_paralysis'].append(
                copy.deepcopy(s['native_actor_values'][0]['melee_paralysis'][0])),
        ]
        for change in changes:
            with self.subTest(change=change):
                state = self.state()
                change(state)
                with self.assertRaises(ValueError):
                    forward_source_stage(state, 'target')

    def test_partial_applied_sources_rejected(self):
        for scalar, knocked, bodies in [(0, 3, 1), (1, 0, 1), (1, 3, 0), (1, 3, 2), (float('nan'), 3, 1)]:
            with self.subTest(scalar=scalar, knocked=knocked, bodies=bodies):
                state = self.state()
                row = state['native_actor_values'][0]
                row['melee_paralysis'][0]['applied'] = True
                row['values'][48] = [scalar]
                row['process_knocked_state'] = knocked
                state['native_actor_ragdolls'] = [dict(actor='target')] * bodies
                with self.assertRaises(ValueError):
                    forward_source_stage(state, 'target')

    def power_source(self):
        return dict(player=dict(reference='player'),
            native_actor_values=[dict(actor='npc', process_action=2, melee_paralysis=[])],
            native_melee_states=[dict(actor='npc', ai_intent=dict(target='player', style='style'),
                reaction=None, strike=dict(id=2, kind=2, animation_group='onehandattackpower',
                    contact_committed=False, power_phase=0, animation_time=.1,
                    sequence_timing=dict(output_time=.1)))],
            physical_actions=dict(next=3, pending=[2]), physical_action_owners=[dict(id=2, actor='npc')],
            native_combat_engagements=[dict(first='npc', second='player')], native_actor_ragdolls=[])

    def test_pending_npc_power_source_requires_real_windup_ownership(self):
        state = self.power_source()
        self.assertIs(pending_npc_power_source(state, 'npc', 'style'), state['native_melee_states'][0]['strike'])
        with self.assertRaises(ValueError):
            pending_npc_power_source(state, 'player', 'style')

    def test_applied_late_foreign_or_partial_npc_power_sources_rejected(self):
        changes = [
            lambda s:s['native_melee_states'][0]['strike'].update(contact_committed=True),
            lambda s:s['native_melee_states'][0]['strike'].update(power_phase=1),
            lambda s:s['native_melee_states'][0]['strike'].update(animation_time=float('nan')),
            lambda s:s['native_melee_states'][0]['strike'].update(kind=0),
            lambda s:s['native_melee_states'][0]['strike'].update(sequence_timing=None),
            lambda s:s['native_melee_states'][0]['ai_intent'].update(target='other'),
            lambda s:s['native_melee_states'][0]['ai_intent'].update(style='other'),
            lambda s:s['native_actor_values'][0].update(process_action=-1),
            lambda s:s['physical_actions'].update(pending=[]),
            lambda s:s['physical_action_owners'][0].update(actor='other'),
            lambda s:s.update(native_combat_engagements=[]),
            lambda s:s['native_actor_ragdolls'].append(dict(actor='npc')),
            lambda s:s['native_actor_values'][0]['melee_paralysis'].append(dict(applied=False)),
        ]
        for change in changes:
            with self.subTest(change=change):
                state = self.power_source(); change(state)
                with self.assertRaises(ValueError):
                    pending_npc_power_source(state, 'npc', 'style')

    def geometry(self, x, y, distance=120, facing=1, blocked='true', near='false'):
        return (f'M15 course geometry: x={x} y={y} distance={distance} facing={facing} '
                f'blocked100={blocked} blocked125={blocked} nearWall={near}')

    def trace(self):
        return '\n'.join([
            self.geometry(790, 50), self.geometry(790, 92),
            self.geometry(790, 92, near='true'),
            'M15 melee miss committed: actor=dynamic:player:0000000000000001 id=5 fatigue_cost=7.8',
            self.geometry(680, 92, distance=95, blocked='false')])

    def test_reachable_approach(self):
        result = approach_witness(self.trace(), wall_x=785, wall_y=92, close_range=110)
        self.assertEqual(list(result.values()), [1, 2, 3, 5])

    def test_unreachable_or_invalid_approaches(self):
        for thresholds in [dict(wall_x=800, wall_y=92, close_range=110),
                           dict(wall_x=785, wall_y=104, close_range=110),
                           dict(wall_x=785, wall_y=92, close_range=80),
                           dict(wall_x=float('nan'), wall_y=92, close_range=110)]:
            with self.subTest(thresholds=thresholds), self.assertRaises(ValueError):
                approach_witness(self.trace(), **thresholds)
        with self.assertRaises(ValueError):
            approach_witness(self.trace().replace('blocked125=false', 'blocked125=true'),
                             wall_x=785, wall_y=92, close_range=110)
        with self.assertRaises(ValueError):
            approach_witness('\n'.join(reversed(self.trace().splitlines())),
                             wall_x=785, wall_y=92, close_range=110)

    def test_late_target_movement_is_not_an_approach_witness(self):
        trace = self.trace().replace('distance=95', 'distance=120')
        trace += '\nM15 melee action: actor=dynamic:player:0000000000000001 id=6 action=2\n'
        trace += self.geometry(680, 92, distance=70, blocked='false')
        with self.assertRaises(ValueError):
            approach_witness(trace, wall_x=785, wall_y=92, close_range=110)
