"""Audit real Dark Salvation corpse flames and own-save cold continuation."""
import argparse
import json
import math
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path.cwd() / 'scripts'))
import tes4_runtime_state as runtime
from verify_kvatch_faction_reaction import inventory, reactions
from verify_kvatch_harrada_animation import health
from verify_kvatch_topic_learning import known_topics, read, sha

CORPSE = 'content:oblivion.esm:06bc4e'
SHADER = 'content:oblivion.esm:0852fe'
ILEND = 'content:oblivion.esm:098138'
PLAYER = 'dynamic:player:0000000000000001'
AGE = 'obscript.shader.age.' + SHADER
DURATION = 'obscript.shader.duration.' + SHADER
HEALTHY = '4088d33c8172b40a798fc877b1aca3e5b3d2fead7c8db00a1aed98b56a84a429'


def reference(state, key):
    return next(v for v in state['references'] if v['key'] == key)


def locals_(state):
    return next(v['locals'] for v in state['script_instances'] if v['context'] == CORPSE
                and v['unit'].startswith('content:oblivion.esm:0c47ae@'))


def age(state):
    return reference(state, CORPSE)['custom_state'][AGE]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=('entry', 'cold'))
    parser.add_argument('--producer', type=Path, required=True)
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    root = args.course
    driver, scenario = read(root / 'driver-process.json'), read(root / 'scenario.json')
    assert driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert driver['exit_code'] == scenario['exit_code'] == 0 and scenario['passed'] and scenario['actions_complete']
    assert all(v['passed'] for v in scenario['actions'])
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert after['schema_version'] == 106
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert inventory(before) == inventory(after) and reactions(before) == reactions(after)
    assert known_topics(source) == known_topics(target) and before['quests'] == after['quests']
    assert abs(health(before) - health(after)) < .1
    assert before['path_points'] == after['path_points']
    life = lambda s, k: next(v for v in s['native_actor_life'] if v['actor'] == k)
    for actor in (PLAYER, ILEND):
        assert life(after, actor)['phase'] == 0
    for actor in ('content:oblivion.esm:093ede', 'content:oblivion.esm:093ee1'):
        assert life(before, actor) == life(after, actor)
        assert life(after, actor)['phase'] == 1 and life(after, actor)['killer'] == ILEND
    assert reference(before, ILEND)['inventory'] == reference(after, ILEND)['inventory']
    assert after['player']['cell'] == reference(after, ILEND)['cell'] == 'content:oblivion.esm:085e4f'
    follow = next(v for v in after['actor_ai'] if v['actor'] == ILEND)
    assert follow['package'] == 'content:oblivion.esm:1778d1' and follow['target'] == PLAYER
    assert follow['last_transition_door'] == 'content:oblivion.esm:01d580'
    ref = reference(after, CORPSE)
    assert ref['base'] == 'content:oblivion.esm:0bf1b9' and ref['enabled'] and not ref['deleted']
    assert ref['custom_state'][DURATION] == -1 and age(after) > 0
    local = locals_(after)
    assert local[0] is None and local[1] == {'type': 'number', 'value': 1}
    assert local[2]['type'] == 'number' and 90 <= local[2]['value'] < 100
    stats = next(v for v in read(root / 'effect-shaders.json')['effect_shaders']
                 if v['owner'] == CORPSE and v['shader'] == SHADER)
    assert stats['instances'] == 1 and stats['particles'] > 0 and stats['surface_triangles'] > 0
    log = (root / 'process.log').read_text()
    command = 'M7 ObScript: pms reference=' + CORPSE + ' shader=' + SHADER
    events = [json.loads(v) for v in (root / 'events.jsonl').read_text().splitlines()]
    loads = [v for v in events if v['event'] == 'load-complete']
    saves = [v for v in events if v['event'] == 'save-complete']
    assert len(loads) == len(saves) == 1
    epoch = read(root / 'process-epoch.json')
    assert epoch['exe_sha256'] == read(args.producer)['sha256']
    for event in loads + saves:
        assert event['pid'] == epoch['pid'] and sha(root / event['live']) == event['live_sha256']
    assert loads[0]['save_sha256'] == sha(source) and saves[0]['save_sha256'] == sha(target)
    assert read(root / saves[0]['live']) == after
    loaded = read(root / loads[0]['live'])
    assert math.dist(loaded['player']['position'][:3], before['player']['position'][:3]) < .1
    previous = read(source.parents[3] / 'process-epoch.json')
    assert (epoch['pid'], epoch['start_ticks'], epoch['boot_id']) != (previous['pid'], previous['start_ticks'], previous['boot_id'])
    if args.phase == 'entry':
        assert sha(source) == HEALTHY
        assert before['player']['cell'] == 'content:oblivion.esm:01d0c8'
        assert log.count(command) == 1
        assert 'DarkSalvation follower arrival verified' in log and 'RendingApproach checkpoint=door' in log
        assert not any(v['context'] == CORPSE for v in before['script_instances'])
    else:
        assert before['player']['cell'] == 'content:oblivion.esm:085e4f'
        assert locals_(loaded) == locals_(before) == local
        assert abs(age(loaded) - age(before)) < 1e-4 and age(after) > age(before)
        assert reference(before, CORPSE)['custom_state'][DURATION] == -1
        assert not re.search(r'M7 ObScript: (?:pms|sms|playmagicshadervisuals|stopmagicshadervisuals) ', log, re.I)
        assert 'FlamingCorpse static-camera diagnostic' in log
        assert math.dist(after['player']['position'][:3], before['player']['position'][:3]) < .1
        assert (root / 'original-restored-corpse-flames.png').is_file()
        assert (root / 'original-restored-corpse-flames-progress.png').is_file()
        assert sha(root / 'original-restored-corpse-flames.png') != sha(root / 'original-restored-corpse-flames-progress.png')
        # Frame difference is structural evidence. Visible fire/animation review
        # remains a separately recorded manual observation.
    if args.sanitized:
        env = read(root / 'observed-sanitizer-environment.json')['environment']
        assert env['ASAN_OPTIONS'] == 'detect_leaks=0:halt_on_error=1'
        assert env['UBSAN_OPTIONS'] == 'halt_on_error=1:print_stacktrace=1'
        assert env['LD_PRELOAD'].split(':')[0] == '/usr/lib64/libasan.so.8.0.0'
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  source_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch['exe_sha256'],
                  shader_owner=CORPSE, shader=SHADER, saved_duration=-1, elapsed=age(after),
                  corpse_locals=local, live_renderer=stats, health=health(after),
                  player_pose=after['player']['position'], ilend_pose=reference(after, ILEND)['position'],
                  shader_report_sha256=sha(root / 'effect-shaders.json'),
                  scope='Original natural corpse PMS at earned Dark Salvation entry and/or genuine own-save cold continuation. Cold camera is a declared visual diagnostic; Player stays at earned entrance. No upper-corridor travel, individual particle/RNG parity or complete main-quest acceptance.')
    (root / 'flame-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
