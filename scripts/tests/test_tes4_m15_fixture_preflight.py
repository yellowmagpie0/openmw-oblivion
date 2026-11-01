import copy
import json
from pathlib import Path
import subprocess
import unittest

from scripts.tes4_m15_fixture_preflight import approach_witness, forward_source_stage, pending_npc_power_source
from scripts.tes4_m15_fixture_preflight import applied_side_power_source, right_power_spacing_setup
from scripts.tes4_m15_fixture_preflight import pending_backward_overlap_source, applied_backward_interruption_source
from scripts.tes4_m15_fixture_preflight import pending_physical_backward_interruption_source
from scripts.tes4_m15_fixture_preflight import joined_power_capture_setup, validate_recoil_contact_waits


class FixturePreflightTests(unittest.TestCase):
    def test_recoil_wait_guard_accepts_recovery_and_preserves_manifest(self):
        manifest = dict(recoil_contact_waits=[dict(actor='player', cause=3)], actions=[
            dict(type='wait_log', pattern='M15 reaction recovered: actor=player cause=3 action=-1'),
            dict(type='wait_log', pattern='M15 melee finished: actor=npc id=3 action=-1')])
        original = copy.deepcopy(manifest)
        self.assertEqual(validate_recoil_contact_waits(manifest), 1)
        self.assertEqual(manifest, original)
        self.assertEqual(validate_recoil_contact_waits(dict(actions=[])), 0)

    def test_recoil_wait_guard_rejects_retained_wrong_completion_event(self):
        manifest = dict(recoil_contact_waits=[dict(actor='player', cause=3)], actions=[
            dict(type='wait_log', pattern='M15 melee finished: actor=player id=3 action=-1')])
        with self.assertRaisesRegex(ValueError, 'Recoil contact cannot await ordinary completion: 3'):
            validate_recoil_contact_waits(manifest)

    def test_recoil_wait_guard_rejects_malformed_actor_cause_metadata(self):
        for declarations in [None, {}, [None], [dict(actor='', cause=3)],
                             [dict(actor='player', cause=True)], [dict(actor='player', cause=0)],
                             [dict(actor='player', cause=2**64)], [dict(actor='player')]]:
            with self.subTest(declarations=declarations), self.assertRaises(ValueError):
                validate_recoil_contact_waits(dict(actions=[], recoil_contact_waits=declarations))

    def test_joined_framing_preserves_original_save_and_gameplay_requirements(self):
        data = Path(__file__).resolve().parents[1] / 'data/oblivion_compat'
        manifest = json.loads((data / 'oblivion_m15_combined_melee.json').read_text())
        original = copy.deepcopy(manifest)
        spaced = right_power_spacing_setup(manifest, json.loads((data / 'm15_right_power_setup.json').read_text()))
        recipe = json.loads((data / 'm15_forward_capture.json').read_text())['joined_course']
        framed = joined_power_capture_setup(spaced, recipe, (data / 'm15_forward_capture_guard.lua').read_text())
        self.assertEqual(manifest, original)
        saves = lambda value: [a for a in value['actions'] if a.get('value') == 'F5']
        self.assertEqual(saves(framed), saves(original))
        backsteps = [a for a in framed['actions'] if a['type'] == 'key_held' and a.get('value') == 's']
        self.assertEqual(backsteps, [dict(type='key_held', value='s', hold_seconds=recipe['left_backstep_seconds'])])
        self.assertEqual(framed['forbidden_log'], original['forbidden_log'])
        self.assertTrue(set(original['expected_log']).issubset(framed['expected_log']))
        broken = copy.deepcopy(spaced)
        broken['files'] = [f for f in broken['files'] if not f['path'].endswith('m15_course_observer.lua')]
        with self.assertRaises((ValueError, StopIteration)):
            joined_power_capture_setup(broken, recipe, '')

    def camera_guard(self, checks):
        module = Path(__file__).resolve().parents[1] / 'data/oblivion_compat/m15_forward_capture_guard.lua'
        setup = r'''
local now, pixelX, pixelY, depth, hitId, calls, blocked = 0, 20, 20, -1, 42, 0, false
local vectorMeta = {}
local function vector(x,y,z) return setmetatable({x=x,y=y,z=z},vectorMeta) end
vectorMeta.__add=function(a,b) return vector(a.x+b.x,a.y+b.y,a.z+b.z) end
local self = {}
local target = setmetatable({}, {
 __index=function(_,k)
  if k=='id' then return 42
  elseif k=='getBoundingBox' then return function() return {center=vector(4,5,6),halfSize=vector(20,30,70)} end
  elseif k=='position' then error('Visibility must follow rendered bounds, not the actor foot anchor') end
 end,
 __newindex=function() error('Camera probe must not mutate the actor') end})
package.loaded['openmw.self']=self
package.loaded['openmw.util']={vector3=vector}
package.loaded['openmw.core']={getSimulationTime=function() return now end}
package.loaded['openmw.camera']={
 worldToViewportVector=function() return {x=pixelX,y=pixelY} end,
 getViewTransform=function() return {apply=function() return {z=depth} end} end,
 getPosition=function() return vector(0,0,0) end}
package.loaded['openmw.nearby']={
 COLLISION_TYPE={World=1,HeightMap=2,Door=4},
 castRay=function(_,_,options) assert(options.collisionType==7);return {hit=blocked} end,
 castRenderingRay=function(_,_,options)
 assert(options.ignore==self);calls=calls+1
 return {hitObject=hitId and {id=hitId} or nil} end}
local module=assert(loadfile(arg[1]))()
local screen={x=1280,y=720}
'''
        result = subprocess.run(['luajit', '-', str(module)], input=setup + checks,
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_forward_camera_requires_projected_target_mesh_and_live_window(self):
        self.camera_guard(r'''
local guard=module.new(2)
local visible, active=guard(target,screen);assert(visible and active)
blocked=true;calls=0;visible,active=guard(target,screen)
assert(not visible and active and calls==0) -- A displaced mesh behind a wall is not visible.
blocked=false
for _,id in ipairs({43,false}) do
 hitId=id;visible,active=guard(target,screen);assert(not visible and active)
end
hitId=42
for _,x in ipairs({-1,1280}) do
 pixelX=x;calls=0;visible,active=guard(target,screen)
 assert(not visible and active and calls==0)
end
pixelX=20;depth=1;calls=0;assert(not guard(target,screen) and calls==0)
depth=-1;now=2;calls=0;visible,active=guard(target,screen)
assert(not visible and not active and calls==0)
assert(module.visible(target,screen)) -- Recovery visibility is independent of effect lifetime.
now=-1;visible,active=guard(target,screen);assert(not visible and not active)
''')

    def test_forward_camera_rejects_nonfinite_or_empty_source_windows(self):
        self.camera_guard(r'''
for _,window in ipairs({0,-1,math.huge,0/0,'2',true}) do
 assert(not pcall(module.new,window))
end
''')

    def test_measured_camera_motion_uses_view_geometry_and_actual_pitch(self):
        self.camera_guard(r'''
local api=package.loaded['openmw.camera']
api.getPitch=function() return .25 end
api.getViewTransform=function() return {apply=function() return vector(1,1,-1) end} end
local x,y=module.motion(target,'frame',1/5120)
assert(x==4021 and y==-3151)
x,y=module.motion(target,'level',1/5120);assert(x==0 and y==-1280)
local observed={position=vector(0,0,0),getBoundingBox=function()
 return {center=vector(0,0,0),halfSize=vector(30,40,20)} end}
x,y=module.motion(observed,'aim',1/5120);assert(x==4021 and y==0)
self.position=vector(0,0,0)
self.rotation={apply=function() return vector(0,1,0) end}
api.getPosition=function() return vector(0,0,120) end
observed.position=vector(0,100,0)
x,y=module.motion(observed,'upper',1/5120);assert(x==0 and y==-269)
assert(not pcall(module.motion,target,'unknown',1/5120))
''')

    def side_source(self):
        return dict(player=dict(reference='player'),
            physical_actions=dict(next=12, pending=[]), physical_action_owners=[], native_actor_ragdolls=[],
            native_melee_states=[dict(actor='player', strike=dict(id=11, kind=5,
                contact_committed=True, power_phase=1)),
                dict(actor='target', reaction=dict(contact_id=11, kind=2, inactive=False))])

    def test_applied_side_source_rejects_pending_or_mixed_ownership(self):
        state = self.side_source()
        self.assertEqual(applied_side_power_source(state, 'target')['stage'], 'applied-contact')
        changes = [lambda s: s['physical_actions'].update(next=13),
            lambda s: s['physical_actions']['pending'].append(11),
            lambda s: s['physical_action_owners'].append(dict(id=11)),
            lambda s: s['native_actor_ragdolls'].append(dict(actor='target')),
            lambda s: s['native_melee_states'][0]['strike'].update(contact_committed=False),
            lambda s: s['native_melee_states'][0]['strike'].update(power_phase=0),
            lambda s: s['native_melee_states'][1]['reaction'].update(contact_id=9),
            lambda s: s['native_melee_states'][1]['reaction'].update(inactive=True)]
        for change in changes:
            with self.subTest(change=change):
                state = self.side_source()
                change(state)
                with self.assertRaises(ValueError):
                    applied_side_power_source(state, 'target')

    def right_setup(self):
        root = Path(__file__).resolve().parents[1] / 'data/oblivion_compat'
        manifest = json.loads((root / 'oblivion_m15_combined_melee.json').read_text())
        recipe = json.loads((root / 'm15_right_power_setup.json').read_text())
        return manifest, recipe

    def test_right_setup_preserves_gameplay_assertions_and_prior_course(self):
        manifest, recipe = self.right_setup()
        before = copy.deepcopy(manifest)
        corrected = right_power_spacing_setup(manifest, recipe)
        self.assertEqual(manifest, before)
        for key in ['expected_log', 'forbidden_log']:
            self.assertEqual(corrected[key], manifest[key])
        left = next(i for i, a in enumerate(manifest['actions'])
                    if a.get('name') == 'combined-left-power.png')
        self.assertEqual(corrected['actions'][:left + 1], manifest['actions'][:left + 1])
        right = next(i for i, a in enumerate(manifest['actions'])
                     if a.get('pattern', '').endswith('group=onehandattackrightpower'))
        corrected_right = next(i for i, a in enumerate(corrected['actions'])
                     if a.get('pattern', '').endswith('group=onehandattackrightpower'))
        tail = copy.deepcopy(manifest['actions'][right:])
        self.assertEqual(tail[1], dict(type='mouse_move', x=-1608, y=0))
        tail[1] = dict(type='mouse_move', x=recipe['windup_mouse_x'], y=0)
        self.assertEqual(corrected['actions'][corrected_right:], tail)
        patterns = [a.get('pattern', '') for a in corrected['actions'][left:corrected_right]]
        self.assertIn('M15 course right spacing: retreated=true ready=true', patterns)

    def test_invalid_right_setup_does_not_mutate_source(self):
        manifest, recipe = self.right_setup()
        for change in [dict(retreat_distance=float('nan')), dict(minimum_distance=150),
                       dict(maximum_angle=10), dict(maximum_distance=105),
                       dict(windup_mouse_x=-1608), dict(windup_mouse_x=True)]:
            with self.subTest(change=change), self.assertRaises(ValueError):
                right_power_spacing_setup(manifest, {**recipe, **change})
        corrected = right_power_spacing_setup(manifest, recipe)
        with self.assertRaises(ValueError):
            right_power_spacing_setup(corrected, recipe)

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

    def backward_overlap(self):
        state = self.power_source()
        victim = state['native_melee_states'][0]
        victim['strike'].update(id=4, kind=0, animation_group='onehandattackleft',
                                power_phase=None, ordinary_phase=0)
        victim['input'] = dict(input_held=True, require_release=False)
        player = copy.deepcopy(victim)
        player.update(actor='player', ai_intent=None)
        player['strike'].update(id=3, kind=4, animation_group='onehandattackbackpower', power_phase=0)
        state['native_melee_states'].append(player)
        av = copy.deepcopy(state['native_actor_values'][0]); av['actor'] = 'player'
        state['native_actor_values'].append(av)
        state['physical_actions'] = dict(next=5, pending=[3, 4])
        state['physical_action_owners'] = [dict(id=3, actor='player'), dict(id=4, actor='npc')]
        return state

    def test_backward_overlap_requires_two_pending_owners_and_victim_input(self):
        proof = pending_backward_overlap_source(self.backward_overlap(), 'npc', 'style')
        self.assertEqual(proof, dict(stage='pending-backward-and-victim', cause=3,
                                     interrupted=4, victim_input_held=True))

    def test_backward_overlap_rejects_late_foreign_or_incapacitated_sources(self):
        changes = [
            lambda s:s['physical_actions'].update(pending=[3]),
            lambda s:s['physical_action_owners'][1].update(actor='other'),
            lambda s:s['native_melee_states'][0]['strike'].update(contact_committed=True),
            lambda s:s['native_melee_states'][0]['strike'].update(ordinary_phase=1),
            lambda s:s['native_melee_states'][0]['input'].update(input_held=False),
            lambda s:s['native_melee_states'][0]['ai_intent'].update(target='other'),
            lambda s:s['native_melee_states'][1]['strike'].update(power_phase=1),
            lambda s:s['native_actor_values'][0].update(process_knocked_state=2),
            lambda s:s['native_actor_ragdolls'].append(dict(actor='npc')),
        ]
        for change in changes:
            with self.subTest(change=change):
                state = self.backward_overlap(); change(state)
                with self.assertRaises(ValueError):
                    pending_backward_overlap_source(state, 'npc', 'style')

    def backward_interruption(self):
        state = self.backward_overlap()
        state['physical_actions'] = dict(next=5, pending=[])
        state['physical_action_owners'] = []
        state['native_melee_states'] = []
        state['native_actor_values'][0]['process_knocked_state'] = 1
        state['native_getup_playback'] = [dict(actor='npc', physical_hit=dict(cause=3, applied=True, force=85))]
        state['native_actor_ragdolls'] = [dict(actor='npc', bodies=[{} for _ in range(18)])]
        state['native_actor_life'] = [dict(actor='npc', phase=0)]
        state['actor_ai'] = [dict(actor='npc', restrained=True)]
        return state

    def test_applied_backward_interruption_requires_withdrawal_and_real_body(self):
        proof = applied_backward_interruption_source(self.backward_interruption(), 'npc')
        self.assertTrue(proof['victim_contact_withdrawn'])
        self.assertEqual(proof['body_count'], 18)

    def test_applied_backward_interruption_rejects_pending_foreign_and_unapplied_sources(self):
        changes = [
            lambda s:s['physical_actions'].update(pending=[4]),
            lambda s:s['native_getup_playback'][0]['physical_hit'].update(cause=4),
            lambda s:s['native_getup_playback'][0]['physical_hit'].update(applied=False),
            lambda s:s['native_actor_ragdolls'][0]['bodies'].pop(),
            lambda s:s['native_actor_values'][0].update(process_knocked_state=0),
            lambda s:s['native_actor_life'][0].update(phase=2),
            lambda s:s['actor_ai'][0].update(restrained=False),
            lambda s:s['native_melee_states'].append(self.backward_overlap()['native_melee_states'][0]),
        ]
        for change in changes:
            with self.subTest(change=change):
                state = self.backward_interruption(); change(state)
                with self.assertRaises(ValueError):
                    applied_backward_interruption_source(state, 'npc')

    def pending_physical_backward(self):
        state = self.backward_overlap()
        state['physical_actions'] = dict(next=5, pending=[4])
        state['physical_action_owners'] = [dict(id=4, actor='npc')]
        state['native_melee_states'][1]['strike'].update(contact_committed=True, power_phase=1)
        state['native_actor_values'][0]['process_knocked_state'] = 2
        state['native_actor_life'] = [dict(actor='npc', phase=0)]
        state['native_getup_playback'] = [dict(actor='npc', physical_hit=dict(cause=3, applied=False, force=85))]
        return state

    def test_pending_physical_backward_distinguishes_committed_damage_from_body_application(self):
        proof = pending_physical_backward_interruption_source(self.pending_physical_backward(), 'npc')
        self.assertTrue(proof['damage_committed'])
        self.assertFalse(proof['body_applied'])

    def test_pending_physical_backward_rejects_replay_mixed_stages_and_foreign_owners(self):
        changes = [
            lambda s:s['native_melee_states'][1]['strike'].update(contact_committed=False),
            lambda s:s['native_melee_states'][0]['strike'].update(contact_committed=True),
            lambda s:s['physical_action_owners'][0].update(actor='other'),
            lambda s:s['native_getup_playback'][0]['physical_hit'].update(cause=4),
            lambda s:s['native_getup_playback'][0]['physical_hit'].update(applied=True),
            lambda s:s['native_actor_ragdolls'].append(dict(actor='npc', bodies=[{}]*18)),
            lambda s:s['native_actor_life'][0].update(phase=2),
        ]
        for change in changes:
            with self.subTest(change=change):
                state = self.pending_physical_backward(); change(state)
                with self.assertRaises(ValueError):
                    pending_physical_backward_interruption_source(state, 'npc')

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
