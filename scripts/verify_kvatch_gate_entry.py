"""Audit original gate transfer, Harrada compatibility and cold interior ownership."""
import argparse
import json
import math
from pathlib import Path

import tes4_runtime_state as runtime
from verify_kvatch_faction_reaction import inventory, reactions, QUEST, GUARDS, PLAYER_FACTION
from verify_kvatch_topic_learning import known_topics, read, sha

PLAYER = 'dynamic:player:0000000000000001'
ENTRANCE = 'content:oblivion.esm:01e895'
HARRADA = 'content:oblivion.esm:066707@oblivion.esm/object/unit=0'
LANDING = (1372.57666015625, -1088.2403564453125, 661.1541748046875)


def harrada(state):
    return {v['context']: v['locals'] for v in state['script_instances'] if v['unit'] == HARRADA}


def in_oblivion(state):
    # Original MS48Script02802c declares inOblivion as local9.
    return next(v for v in state['script_instances']
                if v['context'] == QUEST and v['unit'].startswith('content:oblivion.esm:02802c@'))['locals'][9]['value']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=('entry', 'cold'))
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
    assert reactions(before) == reactions(after) and reactions(after)[(GUARDS, PLAYER_FACTION)] == 40
    quest = lambda s: next(q for q in s['quests'] if q['quest'] == QUEST)
    assert quest(before) == quest(after) and quest(after)['stage'] == 10
    assert next(v for v in after['native_actor_life'] if v['actor'] == PLAYER)['phase'] == 0
    assert after['player']['cell'] == ENTRANCE and in_oblivion(after) == 1
    pose = after['player']['position']
    assert all(math.isfinite(x) for x in pose)
    assert abs(pose[0] - LANDING[0]) < 1 and abs(pose[1] - LANDING[1]) < 1
    assert LANDING[2] - 1 <= pose[2] <= LANDING[2] + 32  # Capsule settles on original terrain.
    instances = harrada(after)
    assert len(instances) >= 5
    for locals_ in instances.values():
        assert locals_[2]['value'] == 1
        assert [v['value'] for v in locals_[3:]] == [5, 750, 0, 0, 0]
        assert locals_[1]['value'] == 0  # These real loaded plants remain outside attack range.
    log = (root / 'process.log').read_text()
    assert 'Unsupported ObScript command' not in log and 'M7 ObScript diagnostic' not in log
    assert 'TES4 trap impact:' not in log  # Inert command must not synthesize damage.
    if args.phase == 'entry':
        assert 'M5 door activation: result=teleport ref=content:oblivion.esm:01e8a3' in log
        assert before['player']['cell'] != ENTRANCE and in_oblivion(before) == 0
        assert 'M7 ObScript: releaseweatheroverride' in log
    else:
        assert before['player']['cell'] == ENTRANCE and harrada(before) == instances
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
        assert loaded['player']['cell'] == ENTRANCE and harrada(loaded) == harrada(before)
    if args.sanitized:
        env = read(root / 'observed-sanitizer-environment.json')['environment']
        assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
        libraries = read(root / 'loaded-libraries.json')
        assert any('libasan' in p for p in libraries) and any('libubsan' in p for p in libraries)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  input_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch['exe_sha256'],
                  cell=after['player']['cell'], settled_landing_pose=pose, harrada_instances=len(instances), authored_in_oblivion=1,
                  weather_override_release_observed=args.phase == 'entry',
                  earned_inventory_topics_reactions_stage_preserved=True,
                  scope='Actual first gate entry and cold interior compatibility; Harrada damage and further progression remain open.')
    (root / 'gate-entry-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
