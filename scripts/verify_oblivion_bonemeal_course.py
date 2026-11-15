"""Audit earned Bonemeal use, remaining damage and completed reload; legacy food regression."""
import argparse
import copy
import hashlib
import json
import math
import re
from pathlib import Path
import tes4_runtime_state as runtime

PLAYER = 'dynamic:player:0000000000000001'
BONEMEAL = 'content:oblivion.esm:01ebff'
LETTUCE = 'content:oblivion.esm:033681'
SKELETON = 'content:oblivion.esm:04bb63'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def player(state):
    return next(a for a in state['native_actor_values'] if a['actor'] == PLAYER)

def effects(state):
    return player(state).get('ingredient_restoration', [])

def total(items, base):
    return sum(i['count'] for i in items if i['base'] == base)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('--phase', required=True, choices=('use', 'pending_restart', 'completed_restart', 'food_regression'))
    args = parser.parse_args()
    root = args.course
    driver = json.loads((root / 'driver-process.json').read_text())
    source = Path(driver['input_save'])
    output = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert driver['captured'] and driver['exit_code'] == 0 and driver['input_unchanged']
    assert digest(source) == driver['input_sha256']
    assert json.loads((root / 'scenario.json').read_text())['passed']
    before, after = runtime.load_save(source), runtime.load_save(output)
    assert before['schema_version'] in (92, 97, 98) and after['schema_version'] == 98
    assert runtime.encode_payload(after) == runtime._find_runtime_record(output.read_bytes())[3]
    events = [json.loads(x) for x in (root / 'events.jsonl').read_text().splitlines()]
    loaded = next(e for e in events if e['event'] == 'load-complete')
    saved = [e for e in events if e['event'] == 'save-complete'][-1]
    assert loaded['save_sha256'] == digest(source) and saved['save_sha256'] == digest(output)
    live = json.loads((root / loaded['live']).read_text())
    assert live['native_actor_values'] == before['native_actor_values']
    original_inventory = copy.deepcopy(before['player']['inventory'])
    if before['schema_version'] < 97:
        # Schema97 adds an optional inventory-script owner. Old omission has
        # the same meaning as current explicit null; do not invent an owner.
        for item in original_inventory:
            item.setdefault('script_owner', None)
    assert live['player']['inventory'] == original_inventory
    assert json.loads((root / saved['live']).read_text()) == after
    epoch = json.loads((root / 'process-epoch.json').read_text())
    assert loaded['pid'] == saved['pid'] == epoch['pid']
    assert after['next_dynamic_serial'] == before['next_dynamic_serial']
    assert after['player']['actor_values']['health.current'] == before['player']['actor_values']['health.current']
    # Native resource authority and the facade must agree even as ordinary
    # regeneration exceeds this weak ingredient's negative contribution.
    fatigue = sum(v or 0 for v in player(after)['values'][10])
    assert math.isclose(fatigue, after['player']['actor_values']['fatigue.current'], abs_tol=.002)
    assert after['native_death_counts'] == before['native_death_counts']
    assert after['disposition_modifiers'] == before['disposition_modifiers']
    old, new = original_inventory, after['player']['inventory']
    food = args.phase == 'food_regression'
    ingredient = LETTUCE if food else BONEMEAL
    rate, duration = (5, 1) if food else (-1, 3)
    log = (root / 'process.log').read_text()
    boundary = "Writing saved game 'Quicksave'"
    assert log.count(boundary) == 1
    log = log.split(boundary, 1)[0]
    amounts = re.findall(r'TES4 ingredient restoration: ingredient=' + ingredient + r' amount=([0-9.eE+-]+) elapsed=([0-9.eE+-]+)', log)
    if args.phase == 'use':
        assert not effects(before) and len(effects(after)) == 1
        assert total(old, ingredient) == 1 and total(new, ingredient) == 0
        assert [i for i in old if i['base'] != ingredient] == new
        assert log.count('TES4 ingredient consumed:') == 1
        assert 'magnitude=-1 duration=3' in log
        start = 0
        finish = effects(after)[0]['elapsed']
        assert 0 < finish < duration
    else:
        assert old == new and 'TES4 ingredient consumed:' not in log
        assert not effects(after)
        if args.phase == 'completed_restart':
            assert not effects(before) and not amounts
            start = finish = duration
        else:
            assert len(effects(before)) == 1
            start = effects(before)[0]['elapsed']
            finish = duration
    pending = effects(after) if args.phase == 'use' else effects(before)
    if pending:
        assert pending[0]['ingredient'] == ingredient
        assert pending[0].get('magnitude', 5) == rate and pending[0].get('duration', 1) == duration
    contribution = sum(float(x) for x, _ in amounts)
    assert math.isclose(contribution, rate * (finish - start), abs_tol=.002), (contribution, start, finish)
    if amounts:
        assert all(float(x) * rate > 0 for x, _ in amounts)
        assert math.isclose(float(amounts[-1][1]), finish, abs_tol=.0001)
    if not food:
        assert after['next_arrow_projectile'] == before['next_arrow_projectile']
        assert after['native_actor_life'] == before['native_actor_life']
        assert player(after).get('used_powers') == player(before).get('used_powers')
        assert {p['spell'] for p in player(after)['used_powers']} == {'content:oblivion.esm:047add', 'content:oblivion.esm:047ade'}
        for key in (SKELETON, 'dynamic:native-reference:0000000000000002'):
            assert next(r for r in after['references'] if r['key'] == key) == next(r for r in before['references'] if r['key'] == key)
        corpse = next(r for r in after['references'] if r['key'] == SKELETON)
        assert not corpse['inventory']
        life = next(r for r in after['native_actor_life'] if r['actor'] == SKELETON)
        assert life['phase'] == 1 and life['killer'] == PLAYER
        context = 'dynamic:inventory-script:0000000000000001'
        assert [s for s in after['script_instances'] if s['context'] == context] == [s for s in before['script_instances'] if s['context'] == context]
        for av in (7, 19, 41, 61, 64):
            assert player(after)['values'][av] == player(before)['values'][av]
        assert [sum(v or 0 for v in player(before)['values'][av]) for av in (7,19,64)] == [50,5,0]
    result = dict(passed=True,phase=args.phase,input_sha256=digest(source),output_sha256=digest(output),process=epoch,
                  source_schema=before['schema_version'],saved_schema=98,ingredient=ingredient,rate=rate,duration=duration,
                  elapsed_before=start,elapsed_after=finish,effect_contribution=contribution,
                  fatigue_after=fatigue,pending=effects(after),inventory_before=total(old,ingredient),inventory_after=total(new,ingredient),
                  wire_roundtrip=True,load_live_matches=True,save_live_matches=True,
                  scope='Actual normal-input earned item and independent original-game effect profile; ordinary regeneration remains active.')
    (root / 'semantic-verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(passed=True,phase=args.phase,contribution=contribution,pending=effects(after))))

if __name__ == '__main__':
    main()
