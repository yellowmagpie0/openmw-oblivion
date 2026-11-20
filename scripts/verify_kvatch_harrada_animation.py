"""Audit real Harrada playback, active saved phase and own-save continuation."""
import argparse
import json
import re
from pathlib import Path

import tes4_runtime_state as runtime
from verify_kvatch_faction_reaction import inventory, reactions, QUEST
from verify_kvatch_topic_learning import known_topics, read, sha

PLANT = 'content:oblivion.esm:068733'
ILEND = 'content:oblivion.esm:098138'
PLAYER = 'dynamic:player:0000000000000001'
ATTACK = 'M7 ObScript: playgroup ref=' + PLANT + ' group=forward played=true'


def plant(state):
    return next(v for v in state['references'] if v['key'] == PLANT)


def locals_(state):
    return next(v['locals'] for v in state['script_instances'] if v['context'] == PLANT
                and v['unit'].startswith('content:oblivion.esm:066707@'))


def phase(state):
    return plant(state)['custom_state']['obscript.animation_progress']


def health(state):
    return sum(v or 0 for v in next(a for a in state['native_actor_values'] if a['actor'] == PLAYER)['values'][8])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=('attack', 'cold'))
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
    assert inventory(before) == inventory(after) and reactions(before) == reactions(after)
    assert known_topics(source) == known_topics(target) and before['quests'] == after['quests']
    assert next(q for q in after['quests'] if q['quest'] == QUEST)['stage'] == 25
    assert abs(health(before) - health(after)) < 1e-4
    for actor in (PLAYER, ILEND):
        assert next(v for v in after['native_actor_life'] if v['actor'] == actor)['phase'] == 0
    ref = plant(after)
    assert ref['base'] == 'content:oblivion.esm:07b7cd' and ref['enabled'] and not ref['deleted']
    state = ref['custom_state']
    assert state['obscript.animation_group'] == 'forward' and state['obscript.animation_playing']
    assert state['obscript.animation_scripted'] and state['obscript.animation_mode'] == 1
    assert 0 < phase(after) < 1
    local = locals_(after)
    assert local[0] == {'type': 'reference', 'value': PLANT}
    assert 0 < local[1]['value'] < 2 and local[2]['value'] == 1
    assert [v['value'] for v in local[3:]] == [5, 750, 0, 0, 0]
    log = (root / 'process.log').read_text()
    assert ATTACK in log and 'Unsupported ObScript command' not in log
    bindings = re.findall(r'NIF embedded morph tracks: model=meshes/oblivion/plants/harradauprightattack.nif controller=(\d+) channels=(\d+)', log)
    assert {int(a) for a, b in bindings if int(b) == 3} == {7, 18, 28, 38, 48, 58, 68, 78, 88, 98, 108, 118}
    prefix = 'cold-' if args.phase == 'cold' else ''
    start = root / (prefix + 'original-harrada-attack-start.png')
    progress = root / (prefix + 'original-harrada-attack-progress.png')
    assert sha(start) != sha(progress)  # Structural evidence; manual visual review is separate.
    epoch = read(root / 'process-epoch.json')
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in events if e['event'] == 'load-complete']
    saves = [e for e in events if e['event'] == 'save-complete']
    assert len(loads) == len(saves) == 1
    for event in loads + saves:
        assert event['pid'] == epoch['pid'] and sha(root / event['live']) == event['live_sha256']
    assert loads[0]['save_sha256'] == sha(source) and saves[0]['save_sha256'] == sha(target)
    assert read(root / saves[0]['live']) == after
    loaded_phase = None
    if args.phase == 'cold':
        loaded = read(root / loads[0]['live'])
        loaded_phase = phase(loaded)
        assert abs(loaded_phase - phase(before)) < 1e-4
        assert locals_(loaded) == locals_(before)
        assert phase(before) < phase(after) < 1
        assert 0 < local[1]['value'] < locals_(before)[1]['value']
        assert ATTACK not in log[:log.index('Writing saved game')]
        prior = read(source.parents[3] / 'process-epoch.json')
        assert (prior['pid'], prior['start_ticks'], prior['boot_id']) != (epoch['pid'], epoch['start_ticks'], epoch['boot_id'])
    if args.sanitized:
        env = read(root / 'observed-sanitizer-environment.json')['environment']
        assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
        libs = read(root / 'loaded-libraries.json')
        assert any('libasan' in p for p in libs) and any('libubsan' in p for p in libs)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  input_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch['exe_sha256'],
                  loaded_animation_phase=loaded_phase, saved_animation_phase=phase(after),
                  saved_script_timer=local[1]['value'], bound_morph_controllers=12,
                  independent_channels_per_controller=3, quest_stage=25, player_health=health(after),
                  earned_ownership_preserved=True, active_animation_continuity=True,
                  scope='Visible original Harrada animation and active-phase persistence; no phantom damage/pushback acceptance.')
    (root / 'harrada-animation-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
