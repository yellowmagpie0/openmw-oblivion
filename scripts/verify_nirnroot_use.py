"""Audit genuine Nirnroot ingestion, owned Health drain and cold-load recovery."""
import argparse
import json
from pathlib import Path

import tes4_runtime_state as runtime
import verify_nirnroot_pickup as pickup


def main():
    p = argparse.ArgumentParser()
    p.add_argument('course', type=Path)
    p.add_argument('phase', choices=['use', 'expired', 'restart'])
    p.add_argument('--count', type=int, choices=[1, 2], default=1)
    args = p.parse_args()
    remaining = 2 - args.count
    r = args.course
    read = lambda n: json.loads((r / n).read_text())
    d = read('driver-process.json')
    assert d['exit_code'] == 0 and d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged']
    assert read('scenario.json')['passed']
    source = Path(d['input_save'])
    target = r / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    before = runtime.load_save(source)
    after = runtime.load_save(target)
    assert pickup.sha(source) == d['input_sha256']
    assert after['schema_version'] == 100
    assert after['next_dynamic_serial'] == before['next_dynamic_serial']
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    player = lambda s: next((a for a in s['native_actor_values'] if a['owner'] == 0))
    a, b = (player(before), player(after))
    owned = pickup.items(after)
    assert len(owned) == remaining and all(i['count'] == 1 for i in owned)
    owners = [i['script_owner'] for i in owned]
    for owner in owners:
        assert [v['value'] for v in pickup.instance(after, owner, 'content:oblivion.esm:0eed76')['locals']] == [1]
    assert len([i for i in after['script_instances'] if i['unit'].startswith('content:oblivion.esm:0eed76@')]) == remaining
    assert next((q for q in after['quests'] if q['quest'] == pickup.QUEST))['stage'] == 10
    assert [v['value'] for v in pickup.instance(after, pickup.QUEST, 'content:oblivion.esm:04e93b')['locals'][1:3]] == [2, 2]
    assert after['player']['actor_values']['pc_misc_stat.27'] == 2
    assert after['player']['character_generation'] == before['player']['character_generation']
    assert pickup.blunt(source) == pickup.blunt(target) and pickup.restoration(source) == pickup.restoration(target)
    other = lambda s: [i for i in s['player']['inventory'] if i['base'] != pickup.BASE]
    assert other(before) == other(after)
    assert after['native_death_counts'] == before['native_death_counts']
    assert pickup.actor(after, 'native_actor_life') == pickup.actor(before, 'native_actor_life')
    assert len(pickup.actor(after, 'native_actor_ragdolls')['bodies']) == 17
    assert pickup.actor(after, 'native_actor_values')['passive_abilities'] == pickup.actor(before, 'native_actor_values')['passive_abilities']
    assert next((x for x in after['references'] if x['key'] == pickup.SKELETON))['inventory'] == []
    assert after['player']['actor_values']['fatigue.current'] == 170
    if args.phase == 'use':
        assert len(pickup.items(before)) == 2
        effects = b['ingredient_restoration']
        assert len(effects) == args.count
        for e in effects:
            assert e['ingredient'] == pickup.BASE and e['magnitude'] == -5 and (e['duration'] == 1) and (e['actor_value'] == 8) and (0 <= e['elapsed'] < 1)
        assert b['values'][8][1] == -5 * args.count
        assert after['player']['actor_values']['health.current'] == 110 - 5 * args.count
    else:
        assert len(pickup.items(before)) == remaining
        assert not b.get('ingredient_restoration') and b['values'][8][1] == 0
        assert after['player']['actor_values']['health.current'] == 110
        assert after['player']['inventory'] == before['player']['inventory']
    events = [json.loads(l) for l in (r / 'events.jsonl').read_text().splitlines()]
    epoch = read('process-epoch.json')
    loads = [x for x in events if x['event'] == 'load-complete']
    saves = [x for x in events if x['event'] == 'save-complete']
    assert len(loads) == len(saves) == 1
    assert all((x['pid'] == epoch['pid'] and x['epoch'] == 1 for x in loads + saves))
    assert read(loads[0]['live'])['player']['inventory'] == before['player']['inventory']
    assert player(read(loads[0]['live'])).get('ingredient_restoration', []) == a.get('ingredient_restoration', [])
    assert read(saves[0]['live']) == after
    log = (r / 'process.log').read_text()
    assert log.count('TES4 ingredient consumed: ingredient=' + pickup.BASE) == (args.count if args.phase == 'use' else 0)
    assert 'M7 inventory add event: item=' + pickup.BASE not in log
    out = dict(passed=True, phase=args.phase, input_sha256=pickup.sha(source), output_sha256=pickup.sha(target), health=after['player']['actor_values']['health.current'], retained_owners=owners, count=args.count, effects=b.get('ingredient_restoration', []), game_sha256=epoch['exe_sha256'])
    (r / 'use-audit.json').write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps(out))
if __name__ == '__main__':
    main()
