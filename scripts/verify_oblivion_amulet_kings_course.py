"""Inspect real Amulet of Kings equip/restart evidence without changing a save."""

import argparse
import copy
import hashlib
import json
from pathlib import Path

import tes4_runtime_state as runtime


AMULET = 'content:oblivion.esm:0250a0'
PLAYER = 'dynamic:player:0000000000000001'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('--restart', action='store_true')
    args = parser.parse_args()
    root = args.course
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    driver = json.loads((root / 'driver-process.json').read_text())
    source = Path(driver['input_save'])
    output = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert driver['exit_code'] == 0 and driver['captured'] and driver['input_unchanged']
    assert digest(source) == driver['input_sha256']
    assert json.loads((root / 'scenario.json').read_text())['passed']
    initial, final = runtime.load_save(source), runtime.load_save(output)
    assert final['schema_version'] == 97
    assert runtime.encode_payload(final) == runtime._find_runtime_record(output.read_bytes())[3]
    before = copy.deepcopy(initial['player']['inventory'])
    after = copy.deepcopy(final['player']['inventory'])
    runtime._upgrade_inventory_scripts({'schema_version': 97, 'player': {'inventory': before}})
    old = next(item for item in before if item['base'] == AMULET)
    new = next(item for item in after if item['base'] == AMULET)
    assert old['count'] == new['count'] == 1
    assert old['equipped_slots'] == new['equipped_slots'] == 0
    owner = new['script_owner']
    assert owner.startswith('dynamic:inventory-script:')
    if old['script_owner'] is not None:
        assert owner == old['script_owner']
        assert final['next_dynamic_serial'] == initial['next_dynamic_serial']
    else:
        assert not args.restart
        assert final['next_dynamic_serial'] == initial['next_dynamic_serial'] + 1
    new['script_owner'] = old['script_owner']
    torch_times = []
    for left, right in zip(before, after):
        if left['remaining_usage_time'] != right['remaining_usage_time']:
            assert left['equipped_slots'] == right['equipped_slots'] == 262144
            assert left['base'] == right['base'] == 'content:oblivion.esm:02cf9f'
            assert 0 < right['remaining_usage_time'] <= left['remaining_usage_time']
            torch_times.append([left['remaining_usage_time'], right['remaining_usage_time']])
            right['remaining_usage_time'] = left['remaining_usage_time']
    assert before == after, 'Unrelated inventory or metadata changed'
    instances = [row for row in final['script_instances'] if row['context'] == owner]
    assert len(instances) == 1
    instance = instances[0]
    assert '027589' in instance['unit']
    assert any(value['value'] == PLAYER for value in instance['locals']), instance
    log = (root / 'process.log').read_text()
    expected = int(not args.restart)
    assert log.count('M7 inventory equip event:') == expected
    assert log.count('messagebox text=The Amulet of Kings slips off') == expected
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loaded = next(row for row in events if row['event'] == 'load-complete')
    saved = [row for row in events if row['event'] == 'save-complete'][-1]
    assert loaded['save_sha256'] == digest(source)
    assert saved['save_sha256'] == digest(output)
    assert json.loads((root / saved['live']).read_text()) == final
    epoch = json.loads((root / 'process-epoch.json').read_text())
    assert loaded['pid'] == saved['pid'] == epoch['pid']
    loaded_state = json.loads((root / loaded['live']).read_text())
    assert loaded_state['player']['inventory'] == before, 'Load changed inventory before ordinary input'
    report = dict(passed=True, restart=args.restart, item=AMULET, script_instance=instance,
                  owner=owner, input_sha256=digest(source), output_sha256=digest(output),
                  events=expected, item_retained_unworn=True, unrelated_inventory_preserved=True,
                  torch_times=torch_times, binary_roundtrip=True, observed_load_save_boundaries=True)
    (root / 'amulet-verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
