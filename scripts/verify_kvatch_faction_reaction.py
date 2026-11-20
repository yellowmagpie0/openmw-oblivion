"""Audit MS48's real ModFactionReaction result and cold native getter continuity."""
import argparse
import collections
import json
from pathlib import Path

import tes4_runtime_state as runtime
from verify_kvatch_topic_learning import known_topics, read, sha

QUEST = 'content:oblivion.esm:0224d8'
GUARDS = 'content:oblivion.esm:0224d7'
PLAYER_FACTION = 'content:oblivion.esm:01dbcd'
PLAYER = 'dynamic:player:0000000000000001'


def reactions(state):
    rows = state['faction_reactions']
    result = {(v['from'], v['to']): v['value'] for v in rows}
    assert len(result) == len(rows)
    return result


def inventory(state):
    result = collections.Counter()
    for item in state['player']['inventory']:
        result[(item['base'], item['owner'], item['ownership_rank'], item['ownership_global'])] += item['count']
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=('agreement', 'cold'))
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    root = args.course
    driver, scenario = read(root / 'driver-process.json'), read(root / 'scenario.json')
    assert driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert driver['exit_code'] == scenario['exit_code'] == 0
    assert scenario['passed'] and scenario['actions_complete'] and all(a['passed'] for a in scenario['actions'])
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert after['schema_version'] >= 106
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert inventory(before) == inventory(after)
    assert known_topics(source) == known_topics(target)
    old, new = reactions(before), reactions(after)
    expected = dict(old)
    expected[(GUARDS, PLAYER_FACTION)] = 40
    assert new == expected and (PLAYER_FACTION, GUARDS) not in new
    quest = lambda state: next(q for q in state['quests'] if q['quest'] == QUEST)
    assert quest(after)['stage'] == 10 and quest(after)['running']
    assert {5, 8, 10} <= set(quest(after)['completed_stages'])
    life = next(v for v in after['native_actor_life'] if v['actor'] == PLAYER)
    assert life['phase'] == 0
    log = (root / 'process.log').read_text()
    if args.phase == 'agreement':
        assert quest(before)['stage'] == 5 and (GUARDS, PLAYER_FACTION) not in old
        assert log.count('modfactionreaction from=' + GUARDS + ' to=' + PLAYER_FACTION
                         + ' delta=20 previous=20 value=40') == 1
        assert 'Native dialogue menu: actor=content:oblivion.esm:028a7a info=content:oblivion.esm:028030 choices=0' in log
    else:
        assert old == new and quest(before) == quest(after)
        assert 'modfactionreaction from=' not in log and 'setfactionreaction from=' not in log
        assert 'name=GetFactionReaction result=40' in log and 'name=GetFactionReaction result=0' in log
    epoch = read(root / 'process-epoch.json')
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in events if e['event'] == 'load-complete']
    saves = [e for e in events if e['event'] == 'save-complete']
    assert len(loads) == len(saves) == 1
    for event in loads + saves:
        assert event['pid'] == epoch['pid'] and sha(root / event['live']) == event['live_sha256']
    assert loads[0]['save_sha256'] == sha(source) and saves[0]['save_sha256'] == sha(target)
    assert read(root / saves[0]['live']) == after
    if args.sanitized:
        env = read(root / 'observed-sanitizer-environment.json')['environment']
        assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
        libraries = read(root / 'loaded-libraries.json')
        assert any('libasan' in p for p in libraries) and any('libubsan' in p for p in libraries)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  input_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch['exe_sha256'],
                  saved_reaction=40, reverse_override_absent=True, unrelated_reactions_preserved=True,
                  earned_inventory_counts_and_ownership_preserved=True, quest_stage=10,
                  scope='Original MS48 stage10 directed +20 and save/getter continuity; gate entry remains open.')
    (root / 'faction-reaction-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
