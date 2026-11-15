"""Audit original Skeleton owned draw, independent flight and Player impact."""
import argparse
import copy
import hashlib
import json
import math
import re
from pathlib import Path

import tes4_runtime_state as runtime

PLAYER = 'dynamic:player:0000000000000001'
SKELETON = 'content:oblivion.esm:04bb63'
BOW = 'content:oblivion.esm:025231'
PLAYER_BOW = 'content:oblivion.esm:047aca'
AMMO = 'content:oblivion.esm:017829'
ARMOR = {'content:oblivion.esm:0661c1', 'content:oblivion.esm:0661c3'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def reference(state):
    return next(x for x in state['references'] if x['key'] == SKELETON)


def item(items, base):
    return next(x for x in items if x['base'] == base)


def clock(state):
    return next(x['clock'] for x in state['native_animation_clocks'] if x['actor'] == SKELETON)


def health(state):
    return state['player']['actor_values']['health.current']


def skeleton_health(state):
    values = next(x for x in state['native_actor_values'] if x['actor'] == SKELETON)['values'][8]
    return sum(x or 0 for x in values)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('--phase', choices=('draw', 'release', 'impact'), required=True)
    args = parser.parse_args()
    root = args.course
    driver = json.loads((root / 'driver-process.json').read_text())
    source = Path(driver['input_save'])
    output = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert driver['captured'] and driver['exit_code'] == 0 and driver['input_unchanged']
    assert digest(source) == driver['input_sha256']
    assert json.loads((root / 'scenario.json').read_text())['passed']
    before, after = runtime.load_save(source), runtime.load_save(output)
    assert before['schema_version'] == after['schema_version'] == 97
    assert runtime.encode_payload(after) == runtime._find_runtime_record(output.read_bytes())[3]
    events = [json.loads(x) for x in (root / 'events.jsonl').read_text().splitlines()]
    loaded = next(x for x in events if x['event'] == 'load-complete')
    saved = [x for x in events if x['event'] == 'save-complete'][-1]
    assert loaded['save_sha256'] == digest(source) and saved['save_sha256'] == digest(output)
    live = json.loads((root / loaded['live']).read_text())
    for field in ['native_bow_states', 'native_arrow_projectiles', 'native_animation_clocks', 'clock']:
        assert live[field] == before[field]
    assert live['player']['inventory'] == before['player']['inventory']
    assert reference(live)['inventory'] == reference(before)['inventory']
    assert json.loads((root / saved['live']).read_text()) == after
    epoch = json.loads((root / 'process-epoch.json').read_text())
    assert loaded['pid'] == saved['pid'] == epoch['pid']
    assert after['next_dynamic_serial'] == before['next_dynamic_serial']
    ca, cb = before['clock'], after['clock']
    assert (ca['year'], ca['month'], ca['time_scale']) == (cb['year'], cb['month'], 30)
    seconds = ((cb['day'] - ca['day']) * 24 + cb['hour'] - ca['hour']) * 120
    clock_delta = clock(after) - clock(before)
    assert abs(clock_delta - seconds) <= .25, (clock_delta, seconds)
    assert 0 < health(after) <= 110
    assert after['player']['actor_values']['magicka.current'] == 80
    assert after['native_death_counts'] == before['native_death_counts']
    assert after['disposition_modifiers'] == before['disposition_modifiers']
    old_player = next(x for x in before['native_actor_values'] if x['actor'] == PLAYER)
    new_player = next(x for x in after['native_actor_values'] if x['actor'] == PLAYER)
    assert old_player.get('used_powers', []) == new_player.get('used_powers', [])
    assert {x['spell'] for x in new_player['used_powers']} == {
        'content:oblivion.esm:047add', 'content:oblivion.esm:047ade'}
    recovered = 'dynamic:native-reference:0000000000000002'
    assert next(x for x in before['references'] if x['key'] == recovered) == next(
        x for x in after['references'] if x['key'] == recovered)
    for state in (before, after):
        corpse = next(x for x in state['native_actor_life'] if x['actor'] == 'content:oblivion.esm:06bfc2')
        assert corpse['phase'] == 1 and corpse['killer'] == PLAYER
        life = next(x for x in state['native_actor_life'] if x['actor'] == SKELETON)
        assert life['phase'] == 0
        assert item(reference(state)['inventory'], AMMO)['count'] == 6
    context = item(after['player']['inventory'], 'content:oblivion.esm:0250a0')['script_owner']
    assert context == 'dynamic:inventory-script:0000000000000001'
    assert [x for x in before['script_instances'] if x['context'] == context] == [
        x for x in after['script_instances'] if x['context'] == context]
    complete_log = (root / 'process.log').read_text()
    # Saving is synchronous after animation-state persistence. Simulation may
    # resume between save completion and the ordinary Escape pause. Audit
    # activity through the retained snapshot, allowing later branch activity.
    boundary = "Writing saved game 'Quicksave'"
    assert complete_log.count(boundary) == 1
    log = complete_log.split(boundary, 1)[0]
    releases = re.findall(r'M15 actor arrow released: id=(\d+) draw=(\d+) caster=content:oblivion.esm:04bb63', log)
    impacts = re.findall(r'M15 actor arrow impact: id=(\d+) target=dynamic:player:0000000000000001 damage=([0-9.eE+-]+)', log)
    assert 'M15 native bow rejected:' not in log and 'M15 melee action: actor=' + SKELETON not in log
    old_bow, new_bow = before['native_bow_states'], after['native_bow_states']
    old_flight, new_flight = before['native_arrow_projectiles'], after['native_arrow_projectiles']
    left, right = copy.deepcopy(before['player']['inventory']), copy.deepcopy(after['player']['inventory'])
    npc_left, npc_right = copy.deepcopy(reference(before)['inventory']), copy.deepcopy(reference(after)['inventory'])
    if args.phase == 'draw':
        assert not old_bow and not old_flight and not new_flight and len(new_bow) == 1
        assert health(before) == health(after) == 110
        assert skeleton_health(before) == 20 and 17 < skeleton_health(after) < 20
        assert not releases and not impacts and log.count('M15 Player arrow released:') == 1
        assert sum(x['count'] for x in left if x['base'] == AMMO) == 19
        assert sum(x['count'] for x in right if x['base'] == AMMO) == 18
        assert 0 < item(right, PLAYER_BOW)['condition'] < item(left, PLAYER_BOW)['condition']
        item(right, PLAYER_BOW)['condition'] = item(left, PLAYER_BOW)['condition']
        # The genuinely recovered arrow remains a separate unequipped stack.
        # Only the equipped original stack funds the provocation shot.
        old_ammo = next(x for x in left if x['base'] == AMMO and x['equipped_slots'] == 131072)
        new_ammo = next(x for x in right if x['base'] == AMMO and x['equipped_slots'] == 131072)
        assert new_ammo['count'] == old_ammo['count'] - 1
        new_ammo['count'] = old_ammo['count']
        assert after['next_arrow_projectile'] == before['next_arrow_projectile'] + 1
        assert not new_bow[0]['release_committed'] and new_bow[0]['phase'] == 1
        assert new_bow[0]['actor'] == SKELETON and new_bow[0]['bow_base'] == BOW
        assert new_bow[0]['ammo_base'] == AMMO and new_bow[0]['animation_group'] == 'bowattack'
        assert not new_bow[0]['player_hold_latched']
        assert item(npc_right, BOW)['equipped_slots'] == 65536
        assert item(npc_right, AMMO)['equipped_slots'] == 131072
        for row in npc_right:
            row['equipped_slots'] = item(npc_left, row['base'])['equipped_slots']
        delta = [x - y for x, y in zip(after['player']['position'][:3], reference(after)['position'][:3])]
        assert 700 <= math.sqrt(sum(x * x for x in delta)) <= 800
    elif args.phase == 'release':
        assert len(old_bow) == len(new_bow) == 1 and not old_bow[0]['release_committed']
        assert not old_flight and len(new_flight) == 1 and len(releases) == 1 and not impacts
        assert health(before) == health(after) == 110
        assert skeleton_health(after) == skeleton_health(before)
        assert new_bow[0]['id'] == old_bow[0]['id'] == int(releases[0][1])
        assert new_bow[0]['release_committed']
        arrow = new_flight[0]
        assert arrow['id'] == int(releases[0][0]) == before['next_arrow_projectile']
        assert arrow['caster'] == SKELETON and arrow['bow'] == BOW and arrow['ammunition'] == AMMO
        assert after['next_arrow_projectile'] == before['next_arrow_projectile'] + 1
        assert abs(item(npc_left, BOW)['condition'] - item(npc_right, BOW)['condition'] - .48) <= .01
        item(npc_right, BOW)['condition'] = item(npc_left, BOW)['condition']
    else:
        assert len(old_flight) == 1 and not new_flight and not releases and len(impacts) == 1
        assert int(impacts[0][0]) == old_flight[0]['id']
        assert after['next_arrow_projectile'] == before['next_arrow_projectile']
        assert 0 < health(before) - health(after) <= 15
        assert abs(health(before) - health(after) - float(impacts[0][1])) <= .05
        assert skeleton_health(after) == skeleton_health(before)
        for row in right:
            if row['base'] in ARMOR:
                old = item(left, row['base'])
                assert 0 < row['condition'] <= old['condition']
                row['condition'] = old['condition']
    assert left == right and npc_left == npc_right
    report = dict(passed=True, phase=args.phase, auditor_sha256=digest(Path(__file__)), process=epoch,
                  input_sha256=digest(source), output_sha256=digest(output),
                  clock=dict(calendar_simulation_delta=seconds, actor_delta=clock_delta, tolerance=.25),
                  player_health_before=health(before), player_health_after=health(after),
                  npc_ammunition=6, releases=releases, impacts=impacts,
                  pending_bow=new_bow, flight=new_flight,
                  scope='Original live level1 Skeleton, healthy Iron Bow, ordinary Iron Arrows and visible Player; full tactics remain open.')
    (root / 'course-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
