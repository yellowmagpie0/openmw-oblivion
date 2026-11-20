"""Inspect automatic Kvatch NPC defense, real health loss and cold target ownership."""
import argparse
import json
import math
import re
from pathlib import Path

import tes4_runtime_state as runtime
from verify_kvatch_faction_reaction import inventory, reactions, QUEST, GUARDS as GUARD_FACTION, PLAYER_FACTION
from verify_kvatch_gate_spawn import spawned, MARKER, LEAF, TEMPLATE, SCRIPT
from verify_kvatch_topic_learning import known_topics, read, sha

PLAYER = 'dynamic:player:0000000000000001'
GUARDS = {'content:oblivion.esm:' + key for key in ('028a7a', '028a92', '028a91')}
HIT = re.compile(r'M15 melee hit committed: actor=(\S+) id=(\d+) .*?health_damage=([0-9.eE+-]+).*? victim=(\S+)')


def health(state, actor):
    values = next(v for v in state['native_actor_values'] if v['actor'] == actor)['values'][8]
    return sum(v for v in values if v is not None)


def defenders(state, creatures):
    melee = {v['actor']: v for v in state['native_melee_states']}
    life = {v['actor']: v for v in state['native_actor_life']}
    result = {}
    for actor in GUARDS:
        entries = state['combat_controllers'].get(actor, [])
        if not entries or entries[0]['target'] not in creatures:
            continue
        target = entries[0]['target']
        assert life[actor]['phase'] == life[target]['phase'] == 0
        assert melee[actor]['ai_intent']['target'] == target
        assert health(state, target) > 0
        result[actor] = target
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=('defense', 'cold'))
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    root = args.course
    driver, scenario = read(root / 'driver-process.json'), read(root / 'scenario.json')
    assert driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert driver['exit_code'] == scenario['exit_code'] == 0 and scenario['passed'] and scenario['actions_complete']
    assert all(a['passed'] for a in scenario['actions'])
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert after['schema_version'] >= 106
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert inventory(before) == inventory(after) and known_topics(source) == known_topics(target)
    assert reactions(before) == reactions(after) and reactions(after)[(GUARD_FACTION, PLAYER_FACTION)] == 40
    quest = lambda s: next(q for q in s['quests'] if q['quest'] == QUEST)
    assert quest(before) == quest(after) and quest(after)['stage'] == 10
    assert next(v for v in after['native_actor_life'] if v['actor'] == PLAYER)['phase'] == 0
    old, new = spawned(before), spawned(after)
    assert set(old) == set(new) and len(new) == 3
    for key, reference in new.items():
        assert reference['base'] == LEAF and reference['enabled'] and not reference['deleted']
        custom = reference['custom_state']
        assert custom['native.spawn.source'] == MARKER and custom['native.spawn.template'] == TEMPLATE
        assert custom['native.spawn.script'] == SCRIPT
        fields = lambda ref: {k: v for k, v in ref['custom_state'].items() if k.startswith('native.spawn.')}
        assert fields(old[key]) == fields(reference)
    active = defenders(after, new)
    assert active
    log = (root / 'process.log').read_text()
    hits = [dict(actor=m[1], action_id=int(m[2]), damage=float(m[3]), victim=m[4], offset=m.start())
            for m in HIT.finditer(log) if m[1] in GUARDS and m[4] in new and float(m[3]) > 0]
    assert hits and all(math.isfinite(h['damage']) for h in hits)
    assert len({(h['actor'], h['action_id']) for h in hits}) == len(hits)
    save_offset = log.index("Writing saved game")
    saved_hits = [h for h in hits if h['offset'] < save_offset]
    if args.phase == 'defense':
        assert not any(actor in before['combat_controllers'] for actor in GUARDS)
        assert 'M15 NPC Player defense: actor=content:oblivion.esm:' in log
        assert saved_hits
        assert any(health(after, h['victim']) < health(before, h['victim']) - 1e-4 for h in saved_hits)
    else:
        prior = read(source.parents[3] / 'guard-defense-audit.json')
        committed = {(h['actor'], h['action_id']) for h in prior['guard_hits_before_save']}
        assert not committed & {(h['actor'], h['action_id']) for h in hits}
    epoch = read(root / 'process-epoch.json')
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in events if e['event'] == 'load-complete']
    saves = [e for e in events if e['event'] == 'save-complete']
    assert len(loads) == len(saves) == 1
    for event in loads + saves:
        assert event['pid'] == epoch['pid'] and sha(root / event['live']) == event['live_sha256']
    assert loads[0]['save_sha256'] == sha(source) and saves[0]['save_sha256'] == sha(target)
    assert read(root / saves[0]['live']) == after
    if args.phase == 'cold':
        loaded = read(root / loads[0]['live'])
        assert defenders(loaded, new) == defenders(before, old)
        for victim in set(defenders(before, old).values()):
            assert abs(health(loaded, victim) - health(before, victim)) < 1e-4
    if args.sanitized:
        env = read(root / 'observed-sanitizer-environment.json')['environment']
        assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
        libraries = read(root / 'loaded-libraries.json')
        assert any('libasan' in p for p in libraries) and any('libubsan' in p for p in libraries)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  input_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch['exe_sha256'],
                  saved_defenders=active, saved_target_health={key: health(after, key) for key in set(active.values())},
                  guard_hits_before_save=saved_hits, guard_hits=hits, no_duplicate_guard_action=True,
                  earned_inventory_and_known_topics_preserved=True, quest_stage=10,
                  scope='NPC defense against actual gate Scamps, physical damage and live-target cold continuity; gate entry remains open.')
    (root / 'guard-defense-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('guard_hits', 'guard_hits_before_save')}))


if __name__ == '__main__':
    main()
