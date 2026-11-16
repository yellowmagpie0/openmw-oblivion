"""Inspect the original Kvatch gate's weather slice without accepting a failed gate course."""
import argparse
import hashlib
import json
import math
import re
import struct
from pathlib import Path

import tes4_runtime_state as runtime
from verify_blade_use import shared as skills
from verify_skill_book_read import shared as advancement

STORM = 'content:oblivion.esm:0836d5'
GATE = 'content:oblivion.esm:01e8a3'
PLAYER = 'dynamic:player:0000000000000001'


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def weather(path):
    data = path.read_bytes()
    records = [subs for _, _, name, subs in runtime._save_records(data) if name == b'WTHR']
    assert len(records) == 1
    result = {'override': False}
    identities = []
    for tag, start, end in records[0]:
        value = data[start + 8:end]
        if tag == b'WXID':
            identities.append(value.decode().rstrip('\0'))
        elif tag == b'OWTH':
            result['override'] = bool(struct.unpack('<B', value)[0])
        elif tag in (b'CWTH', b'NWTH', b'QWTH', b'TRFC'):
            result[tag.decode()] = struct.unpack('<f' if tag == b'TRFC' else '<i', value)[0]
    assert identities and len(set(identities)) == len(identities)
    assert math.isfinite(result['TRFC']) and 0 <= result['TRFC'] <= 1
    for tag in ('CWTH', 'NWTH', 'QWTH'):
        index = result[tag]
        assert index == -1 or 0 <= index < len(identities)
        result[tag + '_key'] = identities[index] if index >= 0 else None
    result['identities'] = identities
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=('transition', 'cold'))
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    root = args.course
    driver = read(root / 'driver-process.json')
    scenario = read(root / 'scenario.json')
    assert driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert scenario['actions_complete'] and scenario['exit_code'] == 0
    assert all(action.get('passed') for action in scenario['actions'])
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    if not scenario['passed']:
        assert args.phase == 'transition'
        # This explicitly remains failed-route evidence. Only the separately
        # observed unsupported spawn dependency is allowed in a weather audit.
        findings = scenario['unreviewed_error_log_findings']
        assert findings and all('Unsupported ObScript command placeatme' in f['text'] for f in findings)
        assert all('Unsupported ObScript command placeatme' in f['text'] for f in scenario['forbidden_log_findings'])
        assert not scenario['missing_expected_log']
        assert driver['exit_code'] == 1
    else:
        assert driver['exit_code'] == 0
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert after['schema_version'] == 106
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert before['player']['inventory'] == after['player']['inventory']
    assert skills(source) == skills(target) and advancement(source) == advancement(target)
    assert before['quests'] == after['quests']
    actor = lambda state: next(v for v in state['native_actor_values'] if v['actor'] == PLAYER)
    assert actor(before)['values'] == actor(after)['values']
    assert actor(before)['reputation'] == actor(after)['reputation']
    reference = lambda state: next(v for v in state['references'] if v['key'] == GATE)
    old_reference, new_reference = dict(reference(before)), dict(reference(after))
    for item in (old_reference, new_reference):
        custom = dict(item['custom_state'])
        # The original gate runs PlayGroup; its live animation clock can wrap
        # while weather changes. Preserve every other reference field.
        progress = custom.pop('obscript.animation_progress')
        assert math.isfinite(progress) and progress >= 0
        item['custom_state'] = custom
    assert old_reference == new_reference
    assert before.get('native_player_furniture') is None and after.get('native_player_furniture') is None
    assert math.dist(after['player']['position'][:2], before['player']['position'][:2]) < 4
    old, new = weather(source), weather(target)
    assert new['override'] and new['QWTH'] == -1
    assert new['CWTH_key'] == STORM or new['NWTH_key'] == STORM
    log = (root / 'process.log').read_text()
    assert 'name=GetIsCurrentWeather result=1' in log
    if args.phase == 'transition':
        assert old['CWTH_key'] != STORM and old['NWTH_key'] != STORM and not old['override']
        assert 'setweather weather=' + STORM + ' override=true applied=true' in log
        assert new['NWTH_key'] == STORM and 0 < new['TRFC'] < 1
    else:
        assert old['override'] and old['NWTH_key'] == STORM and 0 < old['TRFC'] < 1
        assert 'setweather weather=' not in log
        samples = re.findall(r'KvatchWeather first current=(\S+) next=(\S+) factor=([0-9.eE+-]+) paused=(true|false)', log)
        assert len(samples) == 1
        current, next_weather, factor, paused = samples[0]
        assert next_weather == 'OblivionStormTamriel' and current != 'OblivionStormTamriel'
        first_factor = float(factor)
        assert 0 < first_factor <= old['TRFC'] + 1e-6
        if paused == 'true':
            assert abs(first_factor - old['TRFC']) < 1e-6
        assert new['TRFC'] <= old['TRFC'] + 1e-6
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
    result = dict(weather_slice_semantics_passed=True, runtime_scenario_passed=scenario['passed'],
                  gate_entry_accepted=False, phase=args.phase, sanitized=args.sanitized,
                  input_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch['exe_sha256'],
                  before_weather=old, saved_weather=new,
                  scope='Actual gate weather slice only. Unsupported creature spawning remains failed-route evidence; no full approach/entry or main-quest acceptance.')
    (root / 'gate-weather-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
