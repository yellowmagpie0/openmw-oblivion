"""Audit ordinary original Vilverin mace passage and fresh-process ownership."""
import argparse
import hashlib
import json
import math
from pathlib import Path

import tes4_runtime_state as runtime
from verify_blunt_use import shared as blunt
from verify_restoration_use import shared as restoration

PLAYER = 'dynamic:player:0000000000000001'
SKELETON = 'content:oblivion.esm:06c2ed'
MACES = ('content:oblivion.esm:0c4986', 'content:oblivion.esm:06c605')
SOURCE = 'fec79d98e5222b121d406e9d2bc8e8ad3908914f381f9be638d7794c95945aab'
BODY_IDS = [13, 18, 24, 30, 36, 42, 48, 54]
JOINT_IDS = [19, 25, 31, 37, 43, 49, 55]


def read(path):
    return json.loads(path.read_text())


def sha(path):
    return hashlib.file_digest(path.open('rb'), 'sha256').hexdigest()


def actor(state, field, key):
    return next(x for x in state[field] if x['actor'] == key)


def rig(state, key):
    return next(x for x in state['native_animated_references'] if x['reference'] == key)


def reference(state, key):
    return next(x for x in state['references'] if x['key'] == key)


def scripts(state):
    owners = {*MACES, 'content:oblivion.esm:06bf50'}
    return [x for x in state['script_instances'] if x['context'] in owners]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=['passage', 'restart'])
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    root = args.course
    driver = read(root / 'driver-process.json')
    assert driver['exit_code'] == 0 and driver['captured']
    assert driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert read(root / 'scenario.json')['passed']
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert after['schema_version'] == 99
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert after['player']['cell'] == before['player']['cell'] == 'content:oblivion.esm:01663a'
    assert after['player']['position'][1] > -1900 and after['player']['position'][2] > -1090
    assert after['player']['actor_values']['health.current'] == before['player']['actor_values']['health.current'] == 110
    assert after['player']['inventory'] == before['player']['inventory']
    assert after['player']['character_generation'] == before['player']['character_generation']
    assert blunt(source) == blunt(target) and restoration(source) == restoration(target)
    assert before['native_death_counts'] == after['native_death_counts']
    assert before['next_death_event'] == after['next_death_event']
    assert scripts(before) == scripts(after) and len(scripts(after)) == 3
    for key in MACES:
        initial, current = rig(before, key), rig(after, key)
        assert current['asset_hash'] == initial['asset_hash'] == '455cadf55d26ccf31d16e44553b4a6ef'
        assert current['base'] == initial['base'] == 'content:oblivion.esm:053257'
        assert current['model'] == initial['model'] == 'meshes/dungeons/caves/traps/ctrapswingmaceshort01.nif'
        assert [x['record'] for x in current['bodies']] == BODY_IDS
        assert [(x['record'], x['node_record'], x['native_motion']) for x in current['bodies']] == [
            (x['record'], x['node_record'], x['native_motion']) for x in initial['bodies']]
        assert current['bodies'][0]['native_motion'] == 7
        assert all(x['native_motion'] == 1 for x in current['bodies'][1:])
        assert current['world_registration'] == initial['world_registration'] == dict(bodies=BODY_IDS, constraints=JOINT_IDS)
        assert current['native_blends'] == initial['native_blends'] == []
        assert current['native_controllers'] == initial['native_controllers'] == dict(blends=[], velocities=[])
        assert math.dist(current['bodies'][0]['position'], initial['bodies'][0]['position']) < .01
        assert reference(before, key) == reference(after, key)
        assert reference(after, key)['enabled'] and not reference(after, key)['deleted']
    assert reference(before, SKELETON)['inventory'] == reference(after, SKELETON)['inventory'] == []
    life = actor(after, 'native_actor_life', SKELETON)
    assert actor(before, 'native_actor_life', SKELETON) == life and life['phase'] == 1 and life['killer'] == PLAYER
    corpse_before = actor(before, 'native_actor_ragdolls', SKELETON)
    corpse = actor(after, 'native_actor_ragdolls', SKELETON)
    assert len(corpse['bodies']) == 17 and corpse['asset_hash'] == corpse_before['asset_hash']
    assert corpse['internal_filters'] == corpse_before['internal_filters']
    assert [(b['record'], b['node_record']) for b in corpse['bodies']] == [
        (b['record'], b['node_record']) for b in corpse_before['bodies']]
    abilities = actor(after, 'native_actor_values', SKELETON)['passive_abilities']
    assert abilities == actor(before, 'native_actor_values', SKELETON)['passive_abilities'] and len(abilities) == 1
    assert not any(x['actor'] == SKELETON for x in after['native_getup_playback'])
    events = [json.loads(x) for x in (root / 'events.jsonl').read_text().splitlines()]
    loads = [x for x in events if x['event'] == 'load-complete']
    saves = [x for x in events if x['event'] == 'save-complete']
    epoch = read(root / 'process-epoch.json')
    assert len(loads) == 1 and len(saves) == (2 if args.phase == 'passage' else 1)
    assert all(e['pid'] == epoch['pid'] and e['epoch'] == 1 for e in loads + saves)
    assert loads[0]['save_sha256'] == sha(source) and saves[-1]['save_sha256'] == sha(target)
    loaded = read(root / loads[0]['live'])
    assert read(root / saves[-1]['live']) == after
    for key in MACES:
        for prior, restored in zip(rig(before, key)['bodies'], rig(loaded, key)['bodies']):
            assert math.dist(prior['position'], restored['position']) < .05
            assert math.dist(prior['linear_velocity'], restored['linear_velocity']) < .05
    log = (root / 'process.log').read_text()
    assert 'VilverinContinue mace passage clear' in log
    displacement = None
    if args.phase == 'passage':
        assert sha(source) == SOURCE
        assert 'Native character push: actor=' + PLAYER + ' reference=' + MACES[0] in log
        mid = read(root / saves[0]['live'])
        displacement = math.dist(rig(before, MACES[0])['bodies'][-1]['position'], rig(mid, MACES[0])['bodies'][-1]['position'])
        assert displacement > 2, 'Actual head motion must exceed settling noise'
    else:
        assert math.dist(before['player']['position'][:3], after['player']['position'][:3]) < 1
        previous = read(source.parents[3] / 'process-epoch.json')
        assert (previous['pid'], previous['start_ticks']) != (epoch['pid'], epoch['start_ticks'])
        assert 'Native character push:' not in log
    if args.sanitized:
        env = read(root / 'observed-sanitizer-environment.json')['environment']
        assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
        libraries = read(root / 'loaded-libraries.json')
        assert any('libasan' in name for name in libraries) and any('libubsan' in name for name in libraries)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  source_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch['exe_sha256'],
                  player_position=after['player']['position'], player_health=110,
                  observed_live_head_displacement=displacement, mace_bodies=8, mace_constraints=7,
                  corpse_bodies=17, blunt=blunt(target), restoration=restoration(target))
    (root / 'mace-push-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
