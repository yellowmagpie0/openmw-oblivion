#!/usr/bin/env python3
"""Compare an M15 mouse windup's uninterrupted and fresh-process continuations.

Inputs are immutable launcher captures, live exports and process observations.
This verifies continuation equivalence, not original-game numerical parity.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

import tes4_runtime_state as runtime

PLAYER = 'dynamic:player:0000000000000001'


def read_json(path):
    return json.loads(path.read_text())


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def player_melee(state):
    return next(value for value in state['native_melee_states'] if value['actor'] == PLAYER)


def captured_state(output, capture):
    save = Path(capture['copy'])
    event = capture['event']
    assert sha256(save) == event['save_sha256']
    live = output / event['live']
    assert sha256(live) == event['live_sha256']
    state = runtime.load_save(save)
    assert state == read_json(live)
    assert state['schema_version'] == 59
    return state


def verify(base):
    spec = read_json(base / 'preregistered.json')
    assert sha256(Path(spec['source'])) == spec['source_sha256']
    source = runtime.load_save(Path(spec['source']))
    states = []
    epochs = []
    saves = []
    for phase, count in [('first', 2), ('second', 1)]:
        output = base / phase
        launcher = read_json(output / 'launcher.json')
        assert launcher['scenario_returncode'] == launcher['monitor_returncode'] == 0
        assert not launcher['observer_errors'] and launcher['environment_observation_error'] is None
        assert launcher['binary_unchanged'] and launcher['binary_sha256'] == spec['binary_sha256']
        assert len(launcher['captured']) == count
        captures = launcher['captured']
        current = [captured_state(output, capture) for capture in captures]
        states.append(current)
        saves.extend(dict(phase=phase, path=capture['copy'], sha256=capture['event']['save_sha256'])
                     for capture in captures)
        pristine = runtime.load_save(output / 'input-pristine.omwsave')
        events = [json.loads(line) for line in (output / 'events.jsonl').read_text().splitlines()]
        loads = [event for event in events if event['event'] == 'load-complete']
        assert len(loads) == 1
        loaded = read_json(output / loads[0]['live'])
        for key in ('native_melee_states', 'native_animation_clocks', 'physical_actions'):
            assert loaded[key] == pristine[key]
        log = (output / 'process.log').read_text()
        selections = re.findall(r'M15 melee selection: actor=' + PLAYER + r' group=(\w+) available=1', log)
        assert selections == (['handtohandattackleft'] if phase == 'first' else [])
        contacts = re.findall(r'M15 melee (hit|miss) committed: actor=' + PLAYER + r' id=(\d+)\b', log)
        assert contacts == [('hit', '2')]
        assert len(re.findall(r'M15 melee finished: actor=' + PLAYER + r' id=2 action=-1', log)) == 1
        assert log.rfind('Viewer::startThreading() - starting threading') < log.rfind(
            'Viewer::stopThreading() - stopped threading.') < log.rfind('Quitting peacefully.')
        assert 'Viewer::startThreading() - starting threading' in log
        epoch = read_json(output / 'process-epoch.json')
        assert not Path('/proc', str(epoch['pid'])).exists()
        epochs.append((epoch['boot_id'], epoch['pid'], epoch['start_ticks']))
    assert epochs[0] != epochs[1]
    windup, reference = states[0]
    restarted = states[1][0]
    assert (base / 'first/input-pristine.omwsave').read_bytes() == Path(spec['source']).read_bytes()
    assert sha256(base / 'second/input-pristine.omwsave') == saves[0]['sha256']
    assert windup['physical_actions'] == dict(next=3, pending=[2])
    for view in (player_melee(windup), player_melee(windup)['first_person']):
        assert view['passive_sequence'] is None and view['reaction'] is None
        strike = view['strike']
        assert strike['id'] == 2 and strike['kind'] == 0
        assert strike['animation_group'] == 'handtohandattackleft'
        assert strike['ordinary_phase'] == 0 and not strike['contact_committed']
    assert windup['combat_rng_state'] == source['combat_rng_state']
    expected = json.loads(json.dumps(source['native_actor_values']))
    next(value for value in expected if value['actor'] == PLAYER)['process_action'] = 2
    assert windup['native_actor_values'] == expected
    assert windup['native_actor_bases'] == source['native_actor_bases']
    assert windup['player']['inventory'] == source['player']['inventory']
    refs = {value['key']: value for value in windup['references']}
    assert all(refs[value['key']]['inventory'] == value['inventory'] for value in source['references'])
    for complete in (reference, restarted):
        assert complete['physical_actions'] == dict(next=3, pending=[])
        assert next(value for value in complete['native_actor_values'] if value['actor'] == PLAYER)['process_action'] == -1
        for view in (player_melee(complete), player_melee(complete)['first_person']):
            assert all(view[key] is None for key in ('strike', 'reaction', 'passive_sequence'))
            assert view['retiring_sequences'] == []
    for key in ('native_actor_values', 'native_actor_bases', 'combat_rng_state', 'physical_actions'):
        assert restarted[key] == reference[key]
    assert restarted['player']['inventory'] == reference['player']['inventory']
    refmap = {value['key']: value for value in reference['references']}
    restartmap = {value['key']: value for value in restarted['references']}
    assert all(restartmap[value['key']]['inventory'] == refmap[value['key']]['inventory']
               for value in source['references'])
    return dict(passed=True, scope=spec['scope'], saves=saves, whole_save_live_pairs=3,
                fresh_load_owned_state_and_clock_comparisons=2, fresh_process_epochs=2,
                both_views_windup_and_recovery=True, old_cause_consumed=True,
                exactly_one_action2_hit_per_process=True, resources_rng_ledger_and_inventories_match=True,
                original_reference_inventories_compared=len(source['references']),
                original_game_numeric_parity=False, audio_acceptance=False,
                measured_fps=False, full_M15S4_acceptance=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    args = parser.parse_args()
    report = verify(args.course)
    (args.course / 'mouse-restart-verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
