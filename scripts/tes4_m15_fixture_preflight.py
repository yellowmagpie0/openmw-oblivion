"""Cheap fixture checks for the existing M15 joined melee course.

These validate fixture prerequisites, not runtime gameplay acceptance. Geometry
witnesses must come from the same fixture geometry and retain their provenance.
"""
import copy
import math
import re


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
        anchor + "local joinedCameraProbe=require('scripts.m15_forward_capture_guard')\n")
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
        before += measured('upper' if cause == 11 else 'frame') + [wait('M15 joined camera: first=true visible=true')]
        # Observe a rendered frame after saving, before taking the photograph.
        actions[i:i] = [wait('M15 joined camera: first=true visible=true')]
        actions[i+2:i+2] = measured('level')
        actions[i-2:i-2] = before
    result['expected_log'].append('M15 joined camera: first=true visible=true')
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
