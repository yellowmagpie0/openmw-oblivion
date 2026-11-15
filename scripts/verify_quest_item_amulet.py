"""Independently inspect actual earned-Amulet saves; never alter them."""
import argparse
import copy
import hashlib
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path.cwd() / 'scripts'))
import tes4_runtime_state as runtime

AMULET = 'content:oblivion.esm:0250a0'
ROOT_SOURCE = '90baa6fdfc96832b8a9e6d66883632decbbc9a3d174d8822afff93267598d1ee'

def read(p):
    return json.loads(Path(p).read_text())

def sha(p):
    with Path(p).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def inventory(state):
    return sorted(state['player']['inventory'], key=lambda row: json.dumps(row, sort_keys=True))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('--cold', action='store_true')
    parser.add_argument('--equip', action='store_true', help='Check the original scripted avatar-equip regression')
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    root = args.course.resolve()
    driver = read(root / 'driver-process.json')
    assert driver['exit_code'] == 0 and driver['captured']
    assert driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert read(root / 'scenario.json')['passed']
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    source = Path(driver['input_save'])
    assert sha(source) == driver['input_sha256']
    if not args.cold:
        assert sha(source) == ROOT_SOURCE
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in events if e['event'] == 'load-complete']
    saves = [e for e in events if e['event'] == 'save-complete']
    assert len(loads) == len(saves) == 1
    epoch = read(root / 'process-epoch.json')
    assert all(e['pid'] == epoch['pid'] and e['epoch'] == 1 for e in loads + saves)
    assert loads[0]['save_sha256'] == sha(source)
    assert saves[0]['save_sha256'] == sha(target)
    original = runtime.load_save(source)
    before = read(root / loads[0]['live'])
    after = read(root / saves[0]['live'])
    saved = runtime.load_save(target)
    assert before['schema_version'] == after['schema_version'] == 105
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert inventory(original) == inventory(before)
    expected = copy.deepcopy(original)
    expected_amulet = next(row for row in expected['player']['inventory'] if row['base'] == AMULET)
    expected_serial = original['next_dynamic_serial']
    if args.equip:
        assert args.cold
        if expected_amulet['script_owner'] is None:
            expected_amulet['script_owner'] = f'dynamic:inventory-script:{expected_serial:016x}'
            expected_serial += 1
    assert inventory(expected) == inventory(after) == inventory(saved)
    amulet = [row for row in after['player']['inventory'] if row['base'] == AMULET]
    assert len(amulet) == 1 and amulet[0]['count'] == 1
    assert amulet[0]['equipped_slots'] == 0
    original_amulet = next(row for row in original['player']['inventory'] if row['base'] == AMULET)
    assert amulet[0]['script_owner'] == expected_amulet['script_owner']
    if not args.cold:
        assert original_amulet['script_owner'] is None and original['next_dynamic_serial'] == 7
    assert original['next_dynamic_serial'] == before['next_dynamic_serial']
    assert after['next_dynamic_serial'] == expected_serial
    if amulet[0]['script_owner'] is not None:
        owner = amulet[0]['script_owner']
        instances = [[row for row in state['script_instances'] if row['context'] == owner]
                     for state in (original, before, after)]
        if args.equip:
            assert instances[2] == [{
                'unit': 'content:oblivion.esm:027589@oblivion.esm/object/unit=0',
                'context': owner, 'on_load_fired': False,
                'locals': [{'type': 'reference', 'value': 'dynamic:player:0000000000000001'}],
            }]
        else:
            assert len(instances[0]) == 1 and instances[0] == instances[1] == instances[2]
    assert original['quests'] == before['quests'] == after['quests']
    values = [state['player']['actor_values'] for state in (original, before, after)]
    # Encumbrance is the behavior being corrected: the source was written by
    # the old runtime, while a cold continuation already contains the fix.
    assert abs(values[0]['encumbrance'] - (101.3 if args.cold else 102.3)) <= .001
    assert all(abs(row['encumbrance'] - 101.3) <= .001 for row in values[1:])
    stable_values = [{key: value for key, value in row.items() if key != 'encumbrance'} for row in values]
    assert stable_values[0] == stable_values[1] == stable_values[2]
    source_amulet_refs = {row['key'] for row in original['references'] if row['base'] == AMULET}
    final_amulet_refs = {row['key'] for row in after['references'] if row['base'] == AMULET}
    assert source_amulet_refs == final_amulet_refs, 'Refused drop created a native Amulet reference'
    text = (root / 'process.log').read_text(errors='replace')
    samples = re.findall(r'VilverinContinue amulet sample count=(\d+) weight=([\d.]+) stance=\d+ health=([\d.]+)', text)
    assert samples and all(int(count) == 1 and abs(float(weight) - 101.3) <= .001
                           and float(health) == 110 for count, weight, health in samples)
    messages = text.count('messagebox text=The Amulet of Kings slips off')
    assert messages == (1 if args.equip else 0), 'Unexpected original OnEquip message count'
    if args.sanitized:
        observed = read(root / 'observed-sanitizer-environment.json')['environment']
        assert observed['ASAN_OPTIONS'] == 'detect_leaks=0:halt_on_error=1'
        assert observed['UBSAN_OPTIONS'] == 'halt_on_error=1:print_stacktrace=1'
        assert not re.search(r'ERROR: AddressSanitizer|runtime error:|LeakSanitizer', text)
    result = dict(status='passed actual native-save audit; inspect linked UI captures for refusal text',
                  source_sha256=sha(source), save_sha256=sha(target), manifest_sha256=driver['manifest_input_sha256'],
                  binary_sha256=epoch['exe_sha256'], auditor_sha256=sha(__file__),
                  codec_sha256=sha(Path(runtime.__file__)), cold=args.cold, equip=args.equip, sanitized=args.sanitized,
                  weight=101.3, samples=len(samples), amulet=amulet[0], allocator=after['next_dynamic_serial'],
                  inventory_matches_declared_event=True, exact_quests=True,
                  exact_player_values_except_corrected_encumbrance=True, no_new_amulet_reference=True)
    (root / 'quest-item-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))

if __name__ == '__main__':
    main()
