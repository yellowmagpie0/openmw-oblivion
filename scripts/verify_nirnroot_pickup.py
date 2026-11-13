"""Audit original Nirnroot acquisitions and cold loads from genuine expedition saves."""
import argparse
import hashlib
import json
import math
from pathlib import Path

import tes4_runtime_state as runtime
from verify_blunt_use import shared as blunt
from verify_restoration_use import shared as restoration

BASE = 'content:oblivion.esm:04e940'
QUEST = 'content:oblivion.esm:04e936'
SKELETON = 'content:oblivion.esm:06c2ed'
PLANTS = ['content:oblivion.esm:06f184', 'content:oblivion.esm:06f182']


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def items(state):
    return [i for i in state['player']['inventory'] if i['base'] == BASE]


def instance(state, owner, unit):
    return next(i for i in state['script_instances'] if i['context'] == owner and i['unit'].startswith(unit + '@'))


def actor(state, field):
    return next(i for i in state[field] if i['actor'] == SKELETON)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=['pickup', 'restart'])
    parser.add_argument('--count', type=int, choices=[1, 2], required=True)
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    root, count = args.course, args.count
    driver = read(root / 'driver-process.json')
    assert driver['exit_code'] == 0 and driver['captured']
    assert driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert read(root / 'scenario.json')['passed']
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert after['schema_version'] == 99
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert after['player']['character_generation'] == before['player']['character_generation']
    assert after['player']['actor_values']['health.current'] == before['player']['actor_values']['health.current'] == 110
    assert blunt(source) == blunt(target) and restoration(source) == restoration(target)
    acquired = items(after)
    assert len(acquired) == count and all(i['count'] == 1 for i in acquired)
    owners = {i['script_owner'] for i in acquired}
    assert len(owners) == count and all(o and o.startswith('dynamic:inventory-script:') for o in owners)
    for owner in owners:
        script = instance(after, owner, 'content:oblivion.esm:0eed76')
        assert [v['value'] for v in script['locals']] == [1]
    assert not any(i['context'] == BASE for i in after['script_instances'])
    assert next(q for q in after['quests'] if q['quest'] == QUEST)['stage'] == 10
    quest_script = instance(after, QUEST, 'content:oblivion.esm:04e93b')
    assert [v['value'] for v in quest_script['locals'][1:3]] == [count, count]
    assert after['player']['actor_values']['pc_misc_stat.27'] == count
    for plant in PLANTS[:count]:
        reference = next(r for r in after['references'] if r['key'] == plant)
        assert not reference['enabled'] and not reference['deleted']
    assert before['native_death_counts'] == after['native_death_counts']
    assert before['next_death_event'] == after['next_death_event']
    assert actor(before, 'native_actor_life') == actor(after, 'native_actor_life')
    assert actor(after, 'native_actor_life')['phase'] == 1
    corpse = actor(after, 'native_actor_ragdolls')
    assert len(corpse['bodies']) == 17
    assert corpse['internal_filters'] == actor(before, 'native_actor_ragdolls')['internal_filters']
    assert next(r for r in after['references'] if r['key'] == SKELETON)['inventory'] == []
    assert actor(after, 'native_actor_values')['passive_abilities'] == actor(before, 'native_actor_values')['passive_abilities']
    epoch = read(root / 'process-epoch.json')
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in events if e['event'] == 'load-complete']
    saves = [e for e in events if e['event'] == 'save-complete']
    assert len(loads) == 1 and len(saves) == (2 if args.phase == 'pickup' else 1)
    assert all(e['pid'] == epoch['pid'] and e['epoch'] == 1 for e in loads + saves)
    assert loads[0]['save_sha256'] == sha(source) and saves[-1]['save_sha256'] == sha(target)
    assert read(root / saves[-1]['live']) == after
    assert read(root / loads[0]['live'])['player']['inventory'] == before['player']['inventory']
    log = (root / 'process.log').read_text()
    event = 'M7 inventory add event: item=' + BASE
    if args.phase == 'pickup':
        assert len(items(before)) == count - 1
        old_owners = {i['script_owner'] for i in items(before)}
        assert old_owners < owners and len(owners - old_owners) == 1
        added_owner = next(iter(owners - old_owners))
        assert [i for i in after['player']['inventory'] if i['script_owner'] != added_owner] == before['player']['inventory']
        for owner in old_owners:
            assert instance(before, owner, 'content:oblivion.esm:0eed76') == instance(after, owner, 'content:oblivion.esm:0eed76')
        assert log.count(event) == 1
    else:
        assert before['player']['inventory'] == after['player']['inventory']
        assert math.dist(before['player']['position'][:3], after['player']['position'][:3]) < 1
        selected = owners | {QUEST, *PLANTS[:count]}
        scripts = lambda state: [i for i in state['script_instances'] if i['context'] in selected]
        assert scripts(before) == scripts(after)
        assert event not in log
        previous = read(source.parents[3] / 'process-epoch.json')
        assert (previous['pid'], previous['start_ticks']) != (epoch['pid'], epoch['start_ticks'])
    if args.sanitized:
        environment = read(root / 'observed-sanitizer-environment.json')['environment']
        assert 'halt_on_error=1' in environment['ASAN_OPTIONS'] and 'halt_on_error=1' in environment['UBSAN_OPTIONS']
        libraries = read(root / 'loaded-libraries.json')
        assert any('libasan' in name for name in libraries) and any('libubsan' in name for name in libraries)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized, count=count,
                  source_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch['exe_sha256'],
                  ingredient_owners=sorted(owners), quest_stage=10, found=1, NirnCountTotal=count,
                  NirnCountCurrent=count, misc_stat_27=count, health=110, inventory_and_progression_preserved=True)
    (root / 'nirnroot-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
