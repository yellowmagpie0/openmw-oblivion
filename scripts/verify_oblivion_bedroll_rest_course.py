"""Audit original bedroll menu, hourly recovery and restart without changing a save."""

import argparse
import copy
import hashlib
import json
from pathlib import Path

import tes4_runtime_state as runtime


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('--phase', choices=('cancel', 'hour', 'restart'), required=True)
    args = parser.parse_args()
    root = args.course
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    driver = json.loads((root / 'driver-process.json').read_text())
    source = Path(driver['input_save'])
    output = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert driver['exit_code'] == 0 and driver['captured'] and driver['input_unchanged']
    assert sha(source) == driver['input_sha256']
    assert json.loads((root / 'scenario.json').read_text())['passed']
    initial, final = runtime.load_save(source), runtime.load_save(output)
    assert initial['schema_version'] == final['schema_version'] == 97
    assert runtime.encode_payload(final) == runtime._find_runtime_record(output.read_bytes())[3]
    events = [json.loads(x) for x in (root / 'events.jsonl').read_text().splitlines()]
    loaded = next(x for x in events if x['event'] == 'load-complete')
    saved = [x for x in events if x['event'] == 'save-complete'][-1]
    assert loaded['save_sha256'] == sha(source) and saved['save_sha256'] == sha(output)
    live_before = json.loads((root / loaded['live']).read_text())
    assert live_before['player']['inventory'] == initial['player']['inventory']
    assert live_before['player']['actor_values']['health.current'] == initial['player']['actor_values']['health.current']
    assert live_before['clock'] == initial['clock']
    assert json.loads((root / saved['live']).read_text()) == final
    epoch = json.loads((root / 'process-epoch.json').read_text())
    assert loaded['pid'] == saved['pid'] == epoch['pid']
    before, after = copy.deepcopy(initial['player']['inventory']), copy.deepcopy(final['player']['inventory'])
    assert len(before) == len(after)
    torch_times = []
    for left, right in zip(before, after):
        if left['remaining_usage_time'] != right['remaining_usage_time']:
            assert left['base'] == right['base'] == 'content:oblivion.esm:02cf9f'
            assert left['equipped_slots'] == right['equipped_slots'] == 262144
            assert 0 < right['remaining_usage_time'] <= left['remaining_usage_time']
            torch_times.append([left['remaining_usage_time'], right['remaining_usage_time']])
            right['remaining_usage_time'] = left['remaining_usage_time']
    assert before == after, 'Unrelated inventory or equipment metadata changed'
    amulet = next(x for x in final['player']['inventory'] if x['base'] == 'content:oblivion.esm:0250a0')
    assert amulet['script_owner'] == 'dynamic:inventory-script:0000000000000001'
    assert amulet['count'] == 1 and amulet['equipped_slots'] == 0
    assert final['next_dynamic_serial'] == initial['next_dynamic_serial']
    context = amulet['script_owner']
    assert [x for x in initial['script_instances'] if x['context'] == context] == [
        x for x in final['script_instances'] if x['context'] == context]
    log = (root / 'process.log').read_text()
    assert log.count('M13 native bed rest: ref=content:oblivion.esm:06bf97 opened=true') == 1
    hours = int(args.phase == 'hour')
    assert log.count('M13 native rest hour:') == hours
    assert log.count('M13 native rest hour: sleeping=true') == hours
    assert 'M7 inventory equip event:' not in log
    health_before = initial['player']['actor_values']['health.current']
    health_after = final['player']['actor_values']['health.current']
    maximum = final['player']['actor_values']['health.base'] + final['player']['actor_values']['health.modifier']
    assert final['player']['cell'] == initial['player']['cell'] == 'content:oblivion.esm:0463f5'
    first_clock, last_clock = initial['clock'], final['clock']
    assert (first_clock['year'], first_clock['month']) == (last_clock['year'], last_clock['month'])
    elapsed_hours = (last_clock['day'] - first_clock['day']) * 24 + last_clock['hour'] - first_clock['hour']
    # One explicit hour plus ordinary scene/input time, at the source time scale30.
    assert hours <= elapsed_hours < hours + .3
    if hours:
        assert health_before < maximum and abs(health_after - maximum) < 1e-4
        autosaves = [x for x in events if x['event'] == 'save-complete' and Path(x['save']).name == 'Autosave.omwsave']
        assert len(autosaves) == 1
        autosave = autosaves[0]
        assert sha(Path(autosave['save'])) == autosave['save_sha256']
        pre_hour = json.loads((root / autosave['live']).read_text())
        assert abs(pre_hour['player']['actor_values']['health.current'] - health_before) < 1e-5
    else:
        assert abs(health_after - health_before) < 1e-5
    corpse = 'content:oblivion.esm:06bfc2'
    life = next(x for x in final['native_actor_life'] if x['actor'] == corpse)
    assert life['phase'] == 1 and life['killer'] == 'dynamic:player:0000000000000001'
    assert final['native_death_counts'] == initial['native_death_counts']
    report = dict(passed=True, phase=args.phase, hours=hours, input_sha256=sha(source),
                  output_sha256=sha(output), player_health_before=health_before,
                  player_health_after=health_after, health_maximum=maximum, elapsed_game_hours=elapsed_hours,
                  torch_times=torch_times, inventory_preserved=True, amulet_identity_preserved=True,
                  prior_death_credit_preserved=True, binary_roundtrip=True, observed_load_save_boundaries=True)
    (root / 'bedroll-verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
