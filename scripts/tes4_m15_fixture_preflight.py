"""Cheap fixture checks for the existing M15 joined melee course.

These validate fixture prerequisites, not runtime gameplay acceptance. Geometry
witnesses must come from the same fixture geometry and retain their provenance.
"""
import math
import re


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
