"""Cheap fixture and endpoint checks for the existing M15 joined melee course.

These validate fixture prerequisites, not runtime gameplay acceptance. Geometry
witnesses must come from the same fixture geometry and retain their provenance.
"""
import copy
import math
import re
import struct


def gamepad_controller_ready_actions(actions):
    """Do not press a virtual axis/button before SDL opens the controller.

    The existing detection acknowledgement follows controllerAdded/Open. This
    still permits a held input throughout world loading, without priming an
    axis before SDL has sampled its initial state. Existing ready courses keep
    their action sequence; applying the adapter twice is harmless.
    """
    result = copy.deepcopy(actions)
    ready = 'Detected game controller: Xbox 360 Controller'
    for index, action in enumerate(result):
        if action['type'] == 'wait_log' and action.get('pattern') == ready:
            break
        if action['type'] in ('gamepad_axis', 'gamepad_button'):
            result.insert(index, dict(type='wait_log', pattern=ready, timeout_seconds=600,
                                      abort_on_failure=True))
            break
    return result


def gamepad_view_ready_actions(actions):
    """Wait for a rendered world before the initial short POV press.

    A fixed post-load sleep can expire during the first long camera frame.
    Keep the normal input unchanged and abort if the required view is absent.
    Fresh continuations already restore their view and need no toggle.
    """
    result = copy.deepcopy(actions)
    for index, action in enumerate(result):
        if (action['type'] == 'wait_log'
                and action.get('pattern') == 'M15 course view: thirdPerson=true'):
            action['abort_on_failure'] = True
            if index and result[index - 1].get('type') == 'gamepad_button' \
                    and result[index - 1].get('value') == 'rightstick':
                ready = dict(type='wait_log', pattern='M15 course camera preflight: visible=true',
                             after_start=True, timeout_seconds=120, abort_on_failure=True)
                if index < 2 or result[index - 2] != ready:
                    result.insert(index - 1, ready)
            break
    return result


def creature_gamepad_reference_tail(manifest, reference):
    """Reuse the executed game's heading/geometry policy after the return hit.

    The older recovery template's fixed mouse yaw is unsuitable for a gamepad.
    Require the same complete authored files, keep the recovery prefix and save
    count, and copy the already translated/guarded reference suffix verbatim.
    """
    if not manifest.get('virtual_gamepad') or not reference.get('virtual_gamepad'):
        raise ValueError('Creature reference geometry requires gamepad provenance')
    files = lambda spec: {f['path']: f for f in spec['files']}
    if files(manifest) != files(reference):
        raise ValueError('Reference geometry requires identical observer/configuration files')
    def boundary(spec):
        matches = [i + 1 for i, a in enumerate(spec['actions'])
                   if a['type'] == 'screenshot' and a.get('name') == 'combined-target-hit.png']
        if len(matches) != 1:
            raise ValueError('Reference geometry requires one return-hit boundary')
        return matches[0]
    result = copy.deepcopy(manifest)
    tail = reference['actions'][boundary(reference):]
    patterns = [a.get('pattern', '') for a in tail if a['type'] == 'wait_log']
    if not all(any(value in p for p in patterns) for value in ('farFacing=true', 'nearWall=true')):
        raise ValueError('Reference geometry lacks required range/wall acknowledgements')
    if any(a['type'] == 'mouse_move' or (a['type'] in ('key', 'key_down', 'key_up', 'key_held')
            and a.get('value') != 'F5') for a in tail):
        raise ValueError('Reference geometry must already use gamepad gameplay controls')
    result['actions'] = result['actions'][:boundary(result)] + copy.deepcopy(tail)
    saves = lambda spec: sum(a.get('value') == 'F5' for a in spec['actions'])
    if saves(result) != saves(manifest):
        raise ValueError('Reference geometry changed the continuation save protocol')
    return result


def creature_gamepad_recovery_setup(manifest, *, baseline_pitch, recovery_look):
    """Adapt the existing authored Creature recovery before gamepad conversion.

    Only normal stick input moves the camera. Read-only rendering rays gate
    aiming; focus distance follows World::getFocusObject's camera-boom offset.
    Existing keyboard/mouse profiles and gameplay assertions stay untouched.
    """
    if (type(baseline_pitch) not in (int, float) or not math.isfinite(baseline_pitch)
            or not -1.5 < baseline_pitch < 1.5
            or type(recovery_look) is not int or recovery_look >= 0
            or not manifest.get('virtual_gamepad')):
        raise ValueError('Creature gamepad recovery requires explicit camera/input provenance')
    result = copy.deepcopy(manifest)
    observer = next(f for f in result['files'] if f['path'].endswith('m15_course_observer.lua'))
    replacements = {
        'local activationDistance=(t+util.vector3(0,0,40)-camera.getPosition()):length()':
            'local activationDistance=math.huge',
        'local viewEnd=camera.getViewTransform():inverse():apply(util.vector3(0,0,-150))':
            'local focusBoom=camera.getMode()==camera.MODE.ThirdPerson and camera.getThirdPersonDistance() or 0\n'
            '   local viewEnd=camera.getViewTransform():inverse():apply(util.vector3(0,0,-(150+focusBoom)))',
        'local faced=nearby.castRenderingRay(camera.getPosition(),viewEnd,{{ignore=self}})':
            'local faced=nearby.castRenderingRay(camera.getPosition(),viewEnd,{{ignore=self}})\n'
            '   if faced.hitObject==target then activationDistance=(faced.hitPos-camera.getPosition()):length()-focusBoom end\n'
            "   print('M15 Creature gamepad aim: target=' .. tostring(faced.hitObject==target and activationDistance>=0 and activationDistance<150))\n"
            "   print('M15 Creature gamepad pitch: baseline=' .. tostring(camera.getPitch()>=" + repr(baseline_pitch) + '))',
    }
    for before, after in replacements.items():
        if observer['content'].count(before) != 1:
            raise ValueError('Creature gamepad observer lacks a unique focus prerequisite')
        observer['content'] = observer['content'].replace(before, after)
    counts = {recovery_look: 0, -recovery_look: 0}
    actions = []
    for action in result['actions']:
        if action['type'] == 'mouse_move' and action.get('y') in counts:
            look = action['y']
            counts[look] += 1
            actions.extend([
                dict(type='gamepad_axis', axis='right_y', value=-.22 if look == recovery_look else .22),
                dict(type='wait_log', pattern=('M15 Creature gamepad aim: target=true' if look == recovery_look
                     else 'M15 Creature gamepad pitch: baseline=true'), after_start=True, timeout_seconds=60),
                dict(type='gamepad_axis', axis='right_y', value=0),
            ])
        else:
            actions.append(action)
    if list(counts.values()) != [1, 1]:
        raise ValueError('Creature gamepad recovery requires one aim/restore pair')
    result['actions'] = actions
    return result


def creature_gamepad_setup_only(manifest):
    """Run the same course's approach/held-block prefix, before activation."""
    result = copy.deepcopy(manifest)
    starts = [i for i, a in enumerate(result['actions'])
              if a['type'] == 'gamepad_button' and a.get('value') == 'a']
    if not starts or not any(a['type'] == 'wait_log'
            and a.get('pattern') == 'M15 course activation preflight: reachable=true'
            for a in result['actions'][:starts[0]]):
        raise ValueError('Creature setup prefix needs a guarded activation boundary')
    result['actions'] = result['actions'][:starts[0]] + [
        dict(type='screenshot', name='creature-third-gamepad-setup.png', minimum_entropy=.03)]
    result['expected_log'] = ['Applied TES4 runtime state:', 'M15 course view: thirdPerson=true',
        'Detected game controller: Xbox 360 Controller', 'M15 course camera preflight: visible=true',
        'M15 course activation preflight: reachable=true', 'M15 block begin:']
    result['forbidden_log'] += ['M15 melee hit committed:', 'M7 ObScript: startcombat actor=']
    result['name'] += '; setup prefix only, no contact acceptance'
    return result


def is_current_fixture_schema(target, current, minimum):
    """Admit reviewed current schemas without duplicating version lists.

    Version95 adds Player greater-power use history. Fixture copies retain its
    source and elapsed-game-hour timestamp without inventing uses for older seeds.
    Version94 adds owned timed potion attribute contributions. Current-schema
    copies retain these applications/modifiers; older seeds have no application.
    Version93 adds explicit Player scroll Chameleon sources. Older seeds have none;
    current-schema copies retain existing source/timer/modifier state unchanged.
    Version92 adds Player-owned ingredient restoration. Older seeds have no
    pending food; current-schema copies retain any existing timers unchanged.
    Version91 adds empty shared PACK waiting overrides; these fixtures issue no Wait commands.
    Version90 adds optional shared-base faction replacements, absent in these fixtures.
    Version89 adds empty daily-package completion history; these fixtures run no daily packages.
    Version88 adds empty magic-projectile ownership and its initial serial; these fixtures have no casts or projectiles.
    Version86 adds empty trap-contact ownership; these fixtures have no contacts.
    Version85 adds an empty animated-reference body collection; these fixture
    sources contain no released reference bodies.
    Version84 adds optional base essential flags, absent in these fixtures.
    Version83 adds empty bound ownership; older fixture items retain no source.
    Version81 adds optional directed faction reaction changes. These older
    fixtures keep an empty map, as they contain no such commands. Version80
    Player character-generation history remains absent. Future
    layouts require review here before fixtures can emit them.
    Each fixture retains its own minimum schema and complete state assertions.
    """
    return (type(target) is int and type(current) is int and type(minimum) is int
            and target == current and minimum <= target <= 95)


def creature_passive_course_state(state, actor, abilities, *, stage):
    """Validate a declared cold/pending/applied passive fixture without repair.

    This isolated course has no other modifiers on the declared effect values.
    Application belongs to the reference Maximum channel, never the shared base.
    Expected spell/effect identities and signed quantities come from the recipe.
    """
    if stage not in ('cold', 'pending', 'applied') or not isinstance(abilities, list) or not abilities:
        raise ValueError('Passive course needs explicit source stage and ability inputs')
    totals = {}
    for ability in abilities:
        if not isinstance(ability, dict) or ability.get('spell_type') != 4 or not ability.get('effects'):
            raise ValueError('Passive course requires complete Ability4 effect inputs')
        for effect in ability['effects']:
            if (not isinstance(effect, list) or len(effect) != 5 or effect[4] is None
                    or type(effect[2]) is not int or not 0 <= effect[2] < 72
                    or not all(type(effect[i]) in (int, float) and math.isfinite(effect[i]) for i in (3, 4))):
                raise ValueError('Passive course lacks initial effect quantity provenance')
            totals[effect[2]] = totals.get(effect[2], 0) + effect[3]
    rows = [row for row in state['native_actor_values'] if row['actor'] == actor]
    if stage == 'cold':
        if rows:
            raise ValueError('Cold passive fixture must not author actor/effect outcomes')
        return dict(passed=True, stage=stage)
    if (len(rows) != 1 or rows[0]['owner'] != 1
            or rows[0]['process'] != (0 if stage == 'pending' else 1)):
        raise ValueError('Passive course lacks its declared reference/process owner')
    row = rows[0]
    if row.get('passive_abilities') != ([] if stage == 'pending' else abilities):
        raise ValueError('Passive course has unknown or mismatched effect ownership')
    for av, total in totals.items():
        base, maximum, script, damage = row['values'][av]
        expected = total if stage == 'applied' else 0
        if base != 0 or (maximum or 0) != expected or (script or 0) != 0 or (damage or 0) != 0:
            raise ValueError('Passive course effect is absent, duplicated or in the wrong channel')
    return dict(passed=True, stage=stage, maximum_modifiers=totals if stage == 'applied' else {})


def creature_disease_course_state(state, actor, spell, *, cold=False, applied=False, selected=None):
    """Check the isolated Dog course's pending/applied ownership before replay.

    Geometry/visibility use the existing joined-course guards. This course has
    no other temporary Intelligence modifier: its saved Maximum channel must
    equal the one applied DRAT amount. No source data or outcome is modified.
    """
    player = state['player']['reference']
    players = [row for row in state['native_actor_values'] if row['actor'] == player]
    actors = [row for row in state['native_actor_values'] if row['actor'] == actor]
    if (len(players) != 1 or players[0]['owner'] != 0
            or players[0].get('passive_abilities') is None
            or type(cold) is not bool or type(applied) is not bool
            or (selected is not None and type(selected) is not bool)):
        raise ValueError('Disease course requires known Player effect ownership and explicit state flags')
    owned = [entry for entry in players[0]['passive_abilities'] if entry['spell'] == spell]
    if cold:
        if actors or owned or applied or selected is not None:
            raise ValueError('Cold disease fixture must not inject carrier selection or applied effects')
        return dict(passed=True, source='cold-pending', applied=False)
    if (len(actors) != 1 or actors[0]['owner'] != 1 or actors[0]['process'] != 1
            or actors[0].get('leveled_contact_spells') not in ([], [spell])):
        raise ValueError('Disease course requires a known active carrier selection')
    actual_selected = actors[0]['leveled_contact_spells'] == [spell]
    if selected is not None and selected != actual_selected:
        raise ValueError('Disease course selection differs from declared input seed')
    if bool(owned) != applied or len(owned) > 1 or (applied and not actual_selected):
        raise ValueError('Disease course pending/applied effect ownership differs')
    if applied:
        effects = owned[0]['effects']
        if (owned[0].get('spell_type') != 1 or len(effects) != 1 or len(effects[0]) != 5
                or effects[0][:3] != [0, int.from_bytes(b'DRAT', 'little'), 1]
                or effects[0][4] != 5 or type(effects[0][3]) not in (int, float)
                or not math.isfinite(effects[0][3])
                or players[0]['values'][1][1] != effects[0][3]):
            raise ValueError('Disease course requires an applied, owned Intelligence drain')
    return dict(passed=True, source='selected' if actual_selected else 'none', applied=applied)


def regenerated_fatigue_endpoint(state, actor):
    """Compare fully regenerated NPC Fatigue without changing retained state.

    Only an idle endpoint's explicit Damage residue may be cleared in the
    returned comparison copy. Both native low/active float32 queries must equal
    the full pool, and the residue must fit half the float32 step below it.
    This is never a source-restore or per-save equality policy.
    """
    values = copy.deepcopy(state['native_actor_values'])
    matches = [row for row in values if row['actor'] == actor]
    if (len(matches) != 1 or matches[0]['owner'] != 1 or matches[0]['process'] != 1
            or matches[0]['process_action'] != -1 or matches[0]['process_knocked_state'] != 0
            or state['physical_actions']['pending'] or state['physical_action_owners']):
        raise ValueError('Regenerated Fatigue requires an idle nonplayer endpoint')
    fatigue = matches[0]['values'][10]
    if (len(fatigue) != 4 or any(type(v) not in (int, float) or not math.isfinite(v) for v in fatigue)):
        raise ValueError('Regenerated Fatigue requires explicit finite channels')
    def f32(value):
        try:
            result = struct.unpack('<f', struct.pack('<f', value))[0]
        except (OverflowError, struct.error) as error:
            raise ValueError('Regenerated Fatigue query overflow') from error
        if not math.isfinite(result):
            raise ValueError('Regenerated Fatigue query is nonfinite')
        return result
    base, maximum, script, damage = fatigue
    if base != math.trunc(base):
        raise ValueError('Regenerated NPC Fatigue requires an integral native base')
    full_low = f32(base + script)
    full_active = f32(full_low + maximum)
    if full_low <= 0 or full_active <= 0:
        raise ValueError('Regenerated Fatigue requires a positive full pool')
    bits = struct.unpack('<I', struct.pack('<f', full_low))[0]
    previous = struct.unpack('<f', struct.pack('<I', bits - 1))[0]
    half_step = (full_low - previous) / 2
    low = f32(base + script + damage)
    active = f32(low + maximum)
    if damage > 0 or abs(damage) > half_step or low != full_low or active != full_active:
        raise ValueError('Fatigue residue changes a native query or exceeds half a float32 step')
    fatigue[3] = 0
    return values, dict(actor=actor,damage=damage,maximum_absolute_residue=half_step,
                        low_query=low,active_query=active,full_pool=full_active,
                        comparison_only=True)


def validate_gamepad_stick_inputs(manifest, dead_zone):
    """Reject movement/look commands that cannot clear the declared dead zone.

    Zero releases remain valid. Trigger inputs have a separate binding policy;
    this check concerns sticks used for approach and camera acknowledgements.
    """
    if (type(dead_zone) not in (int, float) or not math.isfinite(dead_zone)
            or not 0 <= dead_zone < 1):
        raise ValueError('Invalid gamepad stick dead zone')
    count = 0
    for action in manifest['actions']:
        if (action.get('type') != 'gamepad_axis'
                or action.get('axis') not in ('left_x', 'left_y', 'right_x', 'right_y')):
            continue
        value = action.get('value')
        if (type(value) not in (int, float) or not math.isfinite(value)
                or not -1 <= value <= 1):
            raise ValueError('Invalid gamepad stick value')
        if value != 0:
            if abs(value) <= dead_zone:
                raise ValueError('Gamepad stick command lies inside its dead zone: ' + action['axis'])
            count += 1
    return count


def validate_recoil_contact_waits(manifest):
    """Reject the canonical ordinary-finish wait for a declared Recoil contact.

    This is a cheap fixture prerequisite, not a gameplay outcome assertion.
    The actor/cause pair matters: another actor's ordinary strike may finish.
    """
    declarations = manifest.get('recoil_contact_waits', [])
    if not isinstance(declarations, list):
        raise ValueError('Recoil wait declarations require a list')
    for expected in declarations:
        if (not isinstance(expected, dict) or set(expected) != {'actor', 'cause'}
                or not isinstance(expected['actor'], str) or not expected['actor']
                or type(expected['cause']) is not int or not 0 < expected['cause'] < 2**64):
            raise ValueError('Invalid Recoil wait actor/cause declaration')
        actor, cause = expected['actor'], expected['cause']
        wrong = f'M15 melee finished: actor={actor} id={cause} action=-1'
        if any(a.get('type') == 'wait_log' and a.get('pattern') == wrong
               for a in manifest['actions']):
            raise ValueError('Recoil contact cannot await ordinary completion: ' + str(cause))
    return len(declarations)


def applied_side_power_source(state, actor, cause=11):
    """Admit an applied side contact, without inventing an idle/pending source."""
    player = state['player']['reference']
    melee = next(v for v in state['native_melee_states'] if v['actor'] == player)
    strike = melee.get('strike')
    target = next(v for v in state['native_melee_states'] if v['actor'] == actor)
    reaction = target.get('reaction')
    if (state['physical_actions'] != dict(next=cause + 1, pending=[])
            or state['physical_action_owners'] or state['native_actor_ragdolls']
            or not strike or strike['id'] != cause or strike['kind'] not in (5, 6)
            or not strike['contact_committed'] or strike['power_phase'] != 1
            or not reaction or reaction['contact_id'] != cause
            or reaction['kind'] != 2 or reaction['inactive']):
        raise ValueError('Side-power source lacks the applied contact/reaction ownership')
    return dict(cause=cause, next=cause + 1, stage='applied-contact')


def right_power_spacing_setup(manifest, recipe):
    """Correct the existing Right approach; leave gameplay assertions intact."""
    retreat, minimum, maximum, angle = (recipe[k] for k in
        ('retreat_distance', 'minimum_distance', 'maximum_distance', 'maximum_angle'))
    steering = recipe['windup_mouse_x']
    if (not all(type(v) in (int, float) and math.isfinite(v)
                for v in (retreat, minimum, maximum, angle))
            or not 0 < minimum <= retreat < maximum or not 0 < angle <= 5
            or type(steering) is not int or not 0 <= steering <= 8192):
        raise ValueError('Invalid Right-power setup band')
    result = copy.deepcopy(manifest)
    actions = result['actions']
    try:
        at = next(i for i, a in enumerate(actions) if a.get('name') == 'combined-left-power.png') + 1
        approach = next(i for i in range(at, len(actions))
                        if actions[i] == dict(type='key_down', value='w'))
    except StopIteration as error:
        raise ValueError('Missing shared Right-power approach') from error
    if (actions[approach + 1]['type'] != 'wait_log'
            or 'close110=true' not in actions[approach + 1]['pattern']
            or actions[approach + 2] != dict(type='key_up', value='w')):
        raise ValueError('Right-power approach no longer matches the shared course')
    wait = lambda pattern, seconds: dict(type='wait_log', pattern=pattern,
        after_start=True, timeout_seconds=seconds, abort_on_failure=True)
    actions[approach:approach + 3] = [
        dict(type='key_down', value='s'),
        wait('M15 course right spacing: retreated=true', 20),
        dict(type='key_up', value='s'), dict(type='key_down', value='o'),
        wait('M15 course aim:.*target=true', 60), dict(type='key_up', value='o'),
        wait('M15 course right spacing: retreated=true ready=true', 10)]
    right = next(i for i in range(approach, len(actions))
                 if actions[i].get('pattern', '').endswith('group=onehandattackrightpower'))
    if actions[right + 1] != dict(type='mouse_move', x=-1608, y=0):
        raise ValueError('Unexpected Right-power windup steering')
    # Preserve the setup aim by default: a fixed turn during root displacement
    # can move the target outside the contact ray. This remains normal input.
    actions[right + 1] = dict(type='mouse_move', x=steering, y=0)
    observer = next(f for f in result['files'] if f['path'].endswith('m15_course_observer.lua'))
    anchor = "   print('M15 course return: lane='"
    if observer['content'].count(anchor) != 1:
        raise ValueError('Missing unique shared geometry observer anchor')
    predicate = (f'distance>={minimum} and distance<={maximum} and angle<{angle}'
                 ' and not low.hit and not high.hit and visible')
    observer['content'] = observer['content'].replace(anchor,
        "   print('M15 course right spacing: retreated=' .. tostring(distance>="
        + str(retreat) + ") .. ' ready=' .. tostring(" + predicate + "))\n" + anchor)
    return result


def forward_source_stage(state, actor, cause=9):
    """Reject partial/mixed Forward sources before choosing a restart fixture."""
    row = next(v for v in state["native_actor_values"] if v["actor"] == actor)
    effects = row.get("melee_paralysis", [])
    scalar = sum(v or 0 for v in row["values"][48])
    if not effects and scalar == 0:
        return None
    if len(effects) != 1:
        raise ValueError("Forward source needs exactly one owned paralysis effect")
    effect = effects[0]
    duration, elapsed = effect["duration"], effect["elapsed"]
    if (effect["cause"] != cause or not math.isfinite(duration) or duration <= 0
            or not math.isfinite(elapsed) or not 0 <= elapsed < duration
            or effect["terminated"] or effect["removed"]):
        raise ValueError("Invalid active Forward effect")
    bodies = [v for v in state["native_actor_ragdolls"] if v["actor"] == actor]
    if state["physical_actions"] != dict(next=cause + 1, pending=[]):
        raise ValueError("Forward source has unexpected action ownership")
    if effect["applied"]:
        if not math.isfinite(scalar) or scalar <= 0 or row["process_knocked_state"] not in (3, 4) or len(bodies) != 1:
            raise ValueError("Applied Forward effect lacks scalar/fall/body state")
        return "applied"
    player = next(v for v in state["native_melee_states"] if v["actor"] == state["player"]["reference"])
    strike = player.get("strike")
    if (elapsed != 0 or scalar != 0 or row["process_knocked_state"] != 0
            or row["process_action"] != -1 or bodies or not strike
            or strike["id"] != cause or not strike["contact_committed"]):
        raise ValueError("Pending Forward effect is not a committed, unapplied source")
    return "pending"



def pending_npc_power_source(state, actor, style, cause=2):
    """Require a real owned NPC power windup before a fresh-process course."""
    player = state['player']['reference']
    rows = [v for v in state['native_melee_states'] if v['actor'] == actor]
    values = [v for v in state['native_actor_values'] if v['actor'] == actor]
    if actor == player or len(rows) != 1 or len(values) != 1:
        raise ValueError('NPC power source requires distinct actor authority')
    row, av = rows[0], values[0]
    strike = row.get('strike')
    if (row.get('ai_intent') != dict(target=player, style=style)
            or not strike or strike['id'] != cause or strike['kind'] != 2
            or strike['animation_group'] != 'onehandattackpower'
            or strike['contact_committed'] or strike.get('power_phase') != 0
            or not math.isfinite(strike['animation_time']) or strike['animation_time'] < 0
            or row.get('reaction') or av['process_action'] != 2):
        raise ValueError('NPC power source is not an uncommitted owned windup')
    timing = strike.get('sequence_timing')
    if not timing or not math.isfinite(timing['output_time']) or timing['output_time'] < 0:
        raise ValueError('NPC power source lacks native sequence timing')
    if (state['physical_actions'] != dict(next=cause + 1, pending=[cause])
            or state['physical_action_owners'] != [dict(id=cause, actor=actor)]
            or not any({v['first'], v['second']} == {actor, player}
                       for v in state['native_combat_engagements'])):
        raise ValueError('NPC power source lacks pending action/engagement ownership')
    if av.get('melee_paralysis') or any(v['actor'] == actor for v in state['native_actor_ragdolls']):
        raise ValueError('NPC power windup cannot substitute an incapacitated effect state')
    return strike


def pending_backward_overlap_source(state, actor, style, cause=3, interrupted=4):
    """Require two real pending attacks, including the victim's held AI input."""
    player = state['player']['reference']
    if actor == player or interrupted != cause + 1:
        raise ValueError('Backward overlap requires distinct ordered action owners')
    owners = [dict(id=cause, actor=player), dict(id=interrupted, actor=actor)]
    if (state['physical_actions'] != dict(next=interrupted + 1, pending=[cause, interrupted])
            or state['physical_action_owners'] != owners
            or not any({v['first'], v['second']} == {actor, player}
                       for v in state['native_combat_engagements'])):
        raise ValueError('Backward overlap lacks both pending action owners')
    for key, action, kinds in [(player, cause, (4,)), (actor, interrupted, (0, 1))]:
        rows = [v for v in state['native_melee_states'] if v['actor'] == key]
        values = [v for v in state['native_actor_values'] if v['actor'] == key]
        if len(rows) != 1 or len(values) != 1:
            raise ValueError('Backward overlap lacks distinct actor authority')
        row, av = rows[0], values[0]
        strike = row.get('strike')
        if (not strike or strike['id'] != action or strike['kind'] not in kinds
                or strike['contact_committed'] or row.get('reaction')
                or av['process_action'] != 2 or av.get('process_knocked_state', 0)
                or av.get('melee_paralysis')
                or any(v['actor'] == key for v in state['native_actor_ragdolls'])):
            raise ValueError('Backward overlap cannot substitute an applied or incapacitated actor')
        time = strike['animation_time']
        timing = strike.get('sequence_timing')
        if (not math.isfinite(time) or time < 0 or not timing
                or not math.isfinite(timing['output_time']) or timing['output_time'] < 0):
            raise ValueError('Backward overlap lacks native sequence timing')
        if key == player:
            if strike.get('power_phase') != 0 or not strike['animation_group'].endswith('attackbackpower'):
                raise ValueError('Backward overlap requires the pre-contact Backward power')
        elif (row.get('ai_intent') != dict(target=player, style=style)
                or strike.get('ordinary_phase') != 0
                or not row['input']['input_held'] or row['input']['require_release']):
            raise ValueError('Backward overlap requires the victim held-input owner')
    return dict(stage='pending-backward-and-victim', cause=cause, interrupted=interrupted,
                victim_input_held=True)


def pending_physical_backward_interruption_source(state, actor, cause=3, interrupted=4):
    """Require committed damage with the physical KO and victim withdrawal pending."""
    player = state['player']['reference']
    if (actor == player or interrupted != cause + 1
            or state['physical_actions'] != dict(next=interrupted + 1, pending=[interrupted])
            or state['physical_action_owners'] != [dict(id=interrupted, actor=actor)]):
        raise ValueError('Pending physical interruption lacks the victim contact owner')
    rows = {key: [v for v in state['native_melee_states'] if v['actor'] == key]
            for key in (player, actor)}
    if any(len(v) != 1 for v in rows.values()):
        raise ValueError('Pending physical interruption lacks distinct combat states')
    source, victim = rows[player][0].get('strike'), rows[actor][0].get('strike')
    if (not source or source['id'] != cause or source['kind'] != 4
            or not source['contact_committed'] or source.get('power_phase') != 1
            or not victim or victim['id'] != interrupted or victim['kind'] not in (0, 1)
            or victim['contact_committed'] or victim.get('ordinary_phase') != 0):
        raise ValueError('Pending physical interruption mixes damage and contact stages')
    values = [v for v in state['native_actor_values'] if v['actor'] == actor]
    life = [v for v in state['native_actor_life'] if v['actor'] == actor]
    graphs = [v for v in state['native_getup_playback'] if v['actor'] == actor]
    if (len(values) != 1 or values[0]['process_knocked_state'] != 2
            or values[0].get('melee_paralysis') or len(life) != 1 or life[0]['phase'] != 0
            or len(graphs) != 1 or any(v['actor'] == actor for v in state['native_actor_ragdolls'])):
        raise ValueError('Pending physical interruption cannot substitute an applied body')
    hit = graphs[0].get('physical_hit')
    if (not hit or hit['cause'] != cause or hit['applied']
            or not math.isfinite(hit['force']) or hit['force'] <= 0):
        raise ValueError('Pending physical interruption lacks its unapplied physical cause')
    return dict(stage='pending-physical-backward-interruption', cause=cause,
                interrupted=interrupted, damage_committed=True, body_applied=False)


def applied_backward_interruption_source(state, actor, cause=3, interrupted=4):
    """Require the real applied KO after withdrawal of the interrupted attack."""
    if (state['physical_actions'] != dict(next=interrupted + 1, pending=[])
            or state['physical_action_owners']):
        raise ValueError('Backward interruption still owns a pending contact')
    values = [v for v in state['native_actor_values'] if v['actor'] == actor]
    graphs = [v for v in state['native_getup_playback'] if v['actor'] == actor]
    bodies = [v for v in state['native_actor_ragdolls'] if v['actor'] == actor]
    life = [v for v in state['native_actor_life'] if v['actor'] == actor]
    if (len(values) != 1 or values[0]['process_knocked_state'] not in (1, 2, 3, 4)
            or values[0].get('melee_paralysis') or len(life) != 1 or life[0]['phase'] != 0
            or len(graphs) != 1 or len(bodies) != 1 or len(bodies[0]['bodies']) != 18):
        raise ValueError('Backward interruption lacks its alive knocked actor and physical body')
    hit = graphs[0].get('physical_hit')
    if (not hit or hit['cause'] != cause or not hit['applied']
            or not math.isfinite(hit['force']) or hit['force'] <= 0):
        raise ValueError('Backward interruption lacks its applied physical cause')
    rows = [v for v in state['native_melee_states'] if v['actor'] == actor]
    for row in rows:
        if (row.get('strike') or row.get('reaction') or row.get('passive_sequence')
                or row['input']['input_held'] or row['input']['queued']
                or row.get('retiring_sequences')
                or any(not h['inactive'] for h in row.get('held_blocks', []))):
            raise ValueError('Backward interruption retains victim combat ownership')
    ai = [v for v in state['actor_ai'] if v['actor'] == actor]
    if len(ai) != 1 or not ai[0]['restrained']:
        raise ValueError('Backward interruption course lacks its post-hit AI pause')
    return dict(stage='applied-backward-interruption', cause=cause, interrupted=interrupted,
                body_count=18, victim_contact_withdrawn=True)


def approach_witness(log, *, wall_x, wall_y, close_range):
    """Require measured path witnesses for planned gates; never infer from timeout.

    The wall waypoints must occur in order before the actual wall miss. A later
    power approach must meet the original two-ray, facing and distance predicate.
    Caller must bind the trace and target coordinates to unchanged fixture inputs.
    """
    if not all(math.isfinite(v) for v in (wall_x, wall_y, close_range)) or close_range <= 0:
        raise ValueError("Invalid approach thresholds")
    reached = []
    wall_done = False
    for index, line in enumerate(log.splitlines(), 1):
        if "M15 melee action: actor=dynamic:player:0000000000000001 id=6 " in line:
            break
        if "M15 melee miss committed: actor=dynamic:player:0000000000000001 id=5 " in line:
            wall_done = True
        if "M15 course geometry:" not in line:
            continue
        fields = dict(re.findall(r"(\w+)=([^ ]+)", line))
        x, y = float(fields["x"]), float(fields["y"])
        if not all(math.isfinite(v) for v in (x, y, float(fields["distance"]), float(fields["facing"]))):
            raise ValueError("Nonfinite geometry witness")
        if not wall_done:
            if len(reached) == 0 and x >= wall_x:
                reached.append(index)
            if len(reached) == 1 and y >= wall_y:
                reached.append(index)
            if len(reached) == 2 and fields["nearWall"] == "true":
                reached.append(index)
        elif (len(reached) == 3 and float(fields["distance"]) < close_range
              and float(fields["facing"]) < 10 and fields["blocked100"] == "false"
              and fields["blocked125"] == "false"):
            reached.append(index)
    if len(reached) != 4:
        raise ValueError("Planned approach lacks ordered wall/clear-power reachability witnesses")
    return dict(wall_lane_line=reached[0], wall_along_line=reached[1],
                blocked_wall_line=reached[2], clear_power_line=reached[3])


def joined_power_capture_setup(manifest, recipe, probe_source):
    """Use observed geometry to frame the existing saves with normal mouse input."""
    distance, backstep, step = (recipe[k] for k in
        ('right_capture_distance', 'left_backstep_seconds', 'mouse_radians_per_unit'))
    if (type(distance) not in (int, float) or not math.isfinite(distance) or not 140 <= distance <= 180
            or type(backstep) not in (int, float) or not math.isfinite(backstep) or not 0 < backstep <= .5
            or step != 1 / 5120):
        raise ValueError('Invalid joined-course camera input profile')
    if recipe['captures'] != [['combined-forward-power.png', 9], ['combined-left-power.png', 11],
                               ['combined-right-power.png', 13]]:
        raise ValueError('Invalid joined-course capture identities')
    active_capture = recipe.get('active_forward_capture', False)
    if type(active_capture) is not bool:
        raise ValueError('Invalid active Forward capture prerequisite')
    settling = recipe.get('settled_capture')
    if active_capture and settling is not None:
        raise ValueError('Choose active or settled Forward capture prerequisites')
    if settling is not None:
        keys = ('window_seconds', 'minimum_seconds', 'quiet_seconds', 'maximum_bounds_speed',
                'retreat_distance', 'body_clearance')
        if (not isinstance(settling, dict) or any(type(settling.get(k)) not in (int, float)
                or not math.isfinite(settling[k]) or settling[k] <= 0 for k in keys)
                or settling['minimum_seconds'] + settling['quiet_seconds'] >= settling['window_seconds']):
            raise ValueError('Invalid settled-body capture prerequisites')
    result = copy.deepcopy(manifest)
    actions = result['actions']
    settings = next(f['content'] for f in result['files'] if f['path'] == 'config/settings.cfg')
    if 'camera sensitivity = 0.05' not in settings:
        raise ValueError('Measured mouse profile requires the declared sensitivity')
    result['files'].append(dict(path='test-data/scripts/m15_forward_capture_guard.lua',
        content=probe_source.replace('{', '{{').replace('}', '}}')))
    def wait(pattern, seconds=30, fresh=True):
        return dict(type='wait_log', pattern=pattern, after_start=fresh,
            timeout_seconds=seconds, abort_on_failure=True)
    def key(kind, value):
        return dict(type=kind, value=value)
    def mouse(y=0):
        return dict(type='mouse_move', x=0, y=y)
    def measured(mode):
        pattern = 'M15 joined motion ' + mode + r': x=(?P<x>-?\d+) y=(?P<y>-?\d+)$'
        return [mouse(), mouse(), wait('M15 joined motion ' + mode + ':'),
            dict(type='mouse_move', x=0, y=0,
                from_log=dict(path='process.log', pattern=pattern))]
    # Restore reach before the existing minimum-spacing correction.
    left = next(i for i, a in enumerate(actions) if a.get('name') == 'combined-left-power.png')
    retreat = next(i for i in range(left, len(actions)) if actions[i] == key('key_down', 's'))
    actions[retreat:retreat] = [key('key_down', 'w'),
        wait('M15 course power range:.*close110=true', 180), key('key_up', 'w')]
    # Correct the actual stopped heading, including slow-frame key-up latency.
    ready = next(i for i in range(retreat, len(actions))
        if actions[i].get('pattern') == 'M15 course right spacing: retreated=true ready=true')
    actions[ready:ready] = measured('aim')
    observer = next(f for f in result['files'] if f['path'].endswith('m15_course_observer.lua'))
    anchor = 'local previousFatigue = nil\n'
    if observer['content'].count(anchor) != 1:
        raise ValueError('Missing unique shared observer import anchor')
    observer['content'] = observer['content'].replace(anchor,
        anchor + "local joinedCameraProbe=require('scripts.m15_forward_capture_guard')\n"
        + ('local joinedSettledProbe\n' if settling else ''))
    anchor = "   print('M15 course return: lane='"
    if observer['content'].count(anchor) != 1:
        raise ValueError('Missing unique shared camera observer anchor')
    witness = (f"   print('M15 joined camera spacing: ready=' .. tostring(distance>{distance}"
        " and angle<10 and not low.hit and not high.hit))\n"
        "   local joinedFirst=camera.getMode()==camera.MODE.FirstPerson\n"
        "   print('M15 joined camera: first=' .. tostring(joinedFirst) .. ' visible=' .. "
        "tostring(joinedFirst and joinedCameraProbe.visible(target,screen)))\n"
        "   print('M15 joined forward: applied=' .. tostring(Actor.isParalyzed(target)))\n"
        "   for _,mode in ipairs({{'frame','upper','aim','level'}}) do\n"
        f"    local mx,my=joinedCameraProbe.motion(target,mode,{step})\n"
        "    print(string.format('M15 joined motion %s: x=%d y=%d',mode,mx,my))\n"
        "   end\n")
    if active_capture:
        witness += (
            "   print('M15 joined forward frame: applied=' .. tostring(Actor.isParalyzed(target))"
            " .. ' visible=' .. tostring(joinedCameraProbe.framedVisible(target,screen)))\n")
    if settling:
        witness += (
            "   local jr,jw,jv,jclear=false,false,false,false\n"
            "   if Actor.isParalyzed(target) then\n"
            "    joinedSettledProbe=joinedSettledProbe or joinedCameraProbe.newSettled("
            + ','.join(str(settling[k]) for k in ('window_seconds', 'minimum_seconds',
                       'quiet_seconds', 'maximum_bounds_speed')) + ")\n"
            "    jr,jw=joinedSettledProbe(target)\n"
            "    local jb=target:getBoundingBox()\n"
            "    local jx=math.max(0,math.abs(p.x-jb.center.x)-jb.halfSize.x)\n"
            "    local jy=math.max(0,math.abs(p.y-jb.center.y)-jb.halfSize.y)\n"
            "    local jf=nearby.castRay(p+util.vector3(0,0,40),p-util.vector3(0,0,120),{{collisionType=mask}}).hit\n"
            "    jclear=distance>"
            + str(settling['retreat_distance']) + " and math.sqrt(jx*jx+jy*jy)>"
            + str(settling['body_clearance'])
            + " and not low.hit and not high.hit and jf and Actor.isOnGround(self)\n"
            "    jv=joinedCameraProbe.framedVisible(target,screen)\n"
            "   else joinedSettledProbe=nil end\n"
            "   print('M15 joined settled retreat: ready=' .. tostring(jclear))\n"
            "   print('M15 joined settled: ready=' .. tostring(jr) .. ' window=' .. tostring(jw)"
            " .. ' visible=' .. tostring(jv))\n")
    observer['content'] = observer['content'].replace(anchor, witness + anchor)
    for capture, cause in recipe['captures']:
        matches = [i for i, a in enumerate(actions) if a.get('name') == capture]
        if not matches:
            continue
        i = matches[0]
        if len(matches) != 1 or actions[i-2].get('value') != 'F5':
            raise ValueError('Capture no longer follows its original save')
        before = []
        if cause == 11:
            before.append(dict(type='key_held', value='s', hold_seconds=backstep))
        if cause == 13:
            before += [key('key_down', 's'), wait('M15 joined camera spacing: ready=true'),
                key('key_up', 's')]
        if cause != 11:
            before.append(wait('M15 melee finished: actor=dynamic:player:0000000000000001 id='
                + str(cause) + ' action=-1', 60, False))
        if cause == 9:
            before.append(wait('M15 joined forward: applied=true', 60))
            if active_capture:
                before += [key('key_down', 's'), wait('M15 joined camera spacing: ready=true'),
                    key('key_up', 's')]
            if settling:
                before += [key('key_down', 's'), wait('M15 joined settled retreat: ready=true'),
                    key('key_up', 's'), wait('M15 joined settled: ready=true window=true')]
        before += measured('upper' if cause == 11 else 'frame') + [wait('M15 joined camera: first=true visible=true')]
        if cause == 9 and (settling or active_capture):
            before.append(wait('M15 joined forward frame: applied=true visible=true' if active_capture
                               else 'M15 joined settled: ready=true window=true visible=true'))
            # Photograph the acknowledged phase before save I/O; inspect the
            # saved bodies/effect as well. Do not extend the gameplay timer.
            save, saved, photograph = actions[i-2:i+1]
            actions[i-2:i+1] = before + [photograph, save, saved] + measured('level')
            continue
        # Observe a rendered frame after saving, before taking the photograph.
        actions[i:i] = [wait('M15 joined camera: first=true visible=true')]
        actions[i+2:i+2] = measured('level')
        actions[i-2:i-2] = before
    result['expected_log'].append('M15 joined camera: first=true visible=true')
    if settling:
        result['expected_log'].append('M15 joined settled: ready=true window=true visible=true')
    if active_capture:
        result['expected_log'].append('M15 joined forward frame: applied=true visible=true')
    return result


def standing_capture_setup(manifest, recipe, probe_source):
    """Frame existing standing photos with measured normal input and restore aim.

    Preserve all saves/contact requirements. This is a fixture correction;
    visible mesh checks and final image review remain required at runtime.
    """
    step, pitch = recipe['mouse_radians_per_unit'], recipe['restore_pitch']
    if step != 1 / 5120 or not isinstance(pitch, (int, float)) or not math.isfinite(pitch) or abs(pitch) > 1.5:
        raise ValueError('Invalid standing camera profile')
    captures = recipe['captures']
    if not captures or len(set(captures)) != len(captures):
        raise ValueError('Invalid standing capture identities')
    result = copy.deepcopy(manifest)
    observer = next(f for f in result['files'] if f['path'].endswith('m15_course_observer.lua'))
    anchor = 'local previousFatigue = nil\n'
    if observer['content'].count(anchor) != 1:
        raise ValueError('Missing unique shared observer import anchor')
    observer['content'] = observer['content'].replace(anchor,
        anchor + "local standingCamera=require('scripts.m15_forward_capture_guard')\nlocal standingFrames=0\n")
    anchor = "   print('M15 course return: lane='"
    if observer['content'].count(anchor) != 1:
        raise ValueError('Missing unique shared camera observer anchor')
    observer['content'] = observer['content'].replace(anchor,
        "   if not Actor.getKnockedDown(target) then\n"
        f"    local mx,my=standingCamera.motion(target,'upper',{step})\n"
        "    print(string.format('M15 standing motion: x=%d y=%d',mx,my))\n"
        f"    local restore=math.floor(({pitch}-camera.getPitch())/{step}+0.5)\n"
        "    print(string.format('M15 standing restore: x=0 y=%d',restore))\n"
        "    local framed=camera.getMode()==camera.MODE.FirstPerson and standingCamera.upperVisible(target,screen) and math.abs(mx)<=50 and math.abs(my)<=50\n"
        "    standingFrames=framed and standingFrames+1 or 0\n"
        "    print('M15 standing camera: visible=' .. tostring(standingFrames>=2))\n"
        "   else standingFrames=0 end\n" + anchor)
    path = 'test-data/scripts/m15_forward_capture_guard.lua'
    existing = [f for f in result['files'] if f['path'] == path]
    content = probe_source.replace('{', '{{').replace('}', '}}')
    if existing:
        if len(existing) != 1 or existing[0]['content'] != content:
            raise ValueError('Conflicting shared camera module')
    else:
        result['files'].append(dict(path=path, content=content))
    def measured(kind):
        pattern = 'M15 standing ' + kind + r': x=(?P<x>-?\d+) y=(?P<y>-?\d+)$'
        return [dict(type='mouse_move', x=0, y=0), dict(type='mouse_move', x=0, y=0),
            dict(type='wait_log', pattern='M15 standing ' + kind + ':', after_start=True,
                 timeout_seconds=30, abort_on_failure=True),
            dict(type='mouse_move', x=0, y=0, from_log=dict(path='process.log', pattern=pattern))]
    for name in captures:
        matches = [i for i, a in enumerate(result['actions']) if a.get('name') == name]
        if len(matches) != 1:
            raise ValueError('Missing unique standing capture: ' + name)
        i = matches[0]
        result['actions'][i:i+1] = measured('motion') + [
            dict(type='wait_log', pattern='M15 standing camera: visible=true', after_start=True,
                 timeout_seconds=30, abort_on_failure=True), result['actions'][i]] + measured('restore')
    return result


def recovery_query_setup(manifest, recipe):
    """Add read-only native query observations around an actual recovery.

    These observe existing state. They cannot schedule mutations or supply
    contact/reaction outcomes, and leave all gameplay actions/assertions intact.
    """
    times = recipe['times']
    if (len(times) != 2 or any(type(t) not in (int, float) or not math.isfinite(t) for t in times)
            or not 0 < times[0] < 1 or not 30 <= times[1] <= 120):
        raise ValueError('Invalid recovery query times')
    queries = recipe['queries']
    allowed = {'GetKnockedState', 'GetDead', 'IsWeaponOut', 'GetWeaponAnimType'}
    actors = {'player': 'dynamic:player:0000000000000001',
              'content:oblivion.esm:01fc43': 'content:oblivion.esm:01fc43'}
    if len(queries) != 4:
        raise ValueError('Recovery observation requires four read-only queries')
    signatures = set()
    for query in queries:
        signature = (query['actor'], query['command'])
        if (query['actor'] not in actors or query['command'] not in allowed
                or signature in signatures or len(query['expected']) != 2
                or any(type(value) is not int or not 0 <= value <= 3 for value in query['expected'])):
            raise ValueError('Invalid read-only recovery query')
        signatures.add(signature)
    if queries[0] != dict(actor='content:oblivion.esm:01fc43', command='GetKnockedState', expected=[1, 0]):
        raise ValueError('Recovery query must observe actual Down then recovery')
    result = copy.deepcopy(manifest)
    events = next(f for f in result['files'] if f['path'] == 'test-data/events.txt')
    patterns = []
    for phase, time in enumerate(times):
        for query in queries:
            events['content'] += f"at {time} command {query['actor']} {query['command']}\n"
            patterns.append(re.escape('acceptance-command actor=' + actors[query['actor']]
                + ' name=' + query['command'] + ' result=' + str(query['expected'][phase])) + r'(?=\s|$)')
    # The ordered sequence cannot be satisfied by a standing-only query trace.
    pattern = '(?s)' + '.*'.join(patterns)
    actions = result['actions']
    returns = [i for i, a in enumerate(actions[:-1]) if a == dict(type='key_down', value='u')
               and 'id=3 action=2' in actions[i+1].get('pattern', '')]
    if len(returns) != 1:
        raise ValueError('Missing unique ordinary return after recovery')
    actions.insert(returns[0], dict(type='wait_log', pattern=pattern,
        timeout_seconds=600, abort_on_failure=True))
    result['expected_log'].append(pattern)
    return result


def blocking_dodge_source(state):
    """Require a consumed Player dodge in flight, never an injected pending hit."""
    player = state['player']['reference']
    rows = [row for row in state['native_melee_states'] if row['actor'] == player]
    values = next(row for row in state['native_actor_values'] if row['actor'] == player)
    if len(rows) != 1 or not rows[0].get('dodge'):
        raise ValueError('Dodge continuation requires an actual active Player source')
    dodge = rows[0]['dodge']
    if (values['process_action'] != 9 or values['process_knocked_state'] != 0
            or state['physical_actions']['pending'] or state['physical_action_owners']
            or not 0 < dodge['action'] < state['physical_actions']['next']
            or not dodge['body']['begin'] < dodge['body']['time'] < dodge['body']['end']
            or not dodge['first_person'] or rows[0]['strike'] or rows[0]['reaction']):
        raise ValueError('Dodge source lacks consumed action9 and in-flight paired playback')
    return dict(action=dodge['action'], group=dodge['group'],
                body_time=dodge['body']['time'], first_person_time=dodge['first_person']['time'])
