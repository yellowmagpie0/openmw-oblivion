"""Audit original native Fundaments stack reader/take/cold-inventory gameplay."""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path
import tes4_runtime_state as runtime
from verify_blade_use import shared
from verify_skill_book_read import shared as progression

BASE = 'content:oblivion.esm:024567'
REFERENCE = 'content:oblivion.esm:06efb4'
PLAYER = 'dynamic:player:0000000000000001'
COUNTER = 'pc_misc_stat.17'

def read(path):
    return json.loads(path.read_text())

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=['take'])
    args = parser.parse_args()
    root, phase = args.course, args.phase
    driver = read(root / 'driver-process.json')
    assert driver['captured'] and driver['exit_code'] == 0
    assert driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert read(root / 'scenario.json')['passed']
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert before['schema_version'] == after['schema_version'] == 103
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert shared(source) == shared(target) and progression(source) == progression(target)
    own = lambda state: [v for v in state['player']['inventory'] if v['base'] == BASE and v['count']]
    other = lambda state: [v for v in state['player']['inventory'] if v['base'] != BASE and v['count']]
    assert other(before) == other(after)
    assert sum(v['count'] for v in own(before)) == (1 if phase == 'take' else 2)
    assert len(own(after)) == 1 and own(after)[0]['count'] == 2
    item = own(after)[0]
    assert item['condition'] == item['charge'] == item['remaining_usage_time'] == -1
    assert item['equipped_slots'] == 0 and item['script_owner'] is None
    assert item['owner'] == 'null' and item['ownership_rank'] is None and item['ownership_global'] == 'null'
    assert item['extra_scale'] is None and item['bound_source'] is None and item['hotkey'] == -1
    if phase != 'take':
        assert own(before) == own(after)
    old, new = before['player']['actor_values'], after['player']['actor_values']
    reads = 0 if phase == 'cold' else 1
    assert old.get(COUNTER, 0) == {'take': 13, 'inventory': 14, 'cold': 14}[phase]
    assert new.get(COUNTER, 0) == old.get(COUNTER, 0) + reads
    assert {k: v for k, v in old.items() if k not in (COUNTER, 'encumbrance')} == {k: v for k, v in new.items() if k not in (COUNTER, 'encumbrance')}
    assert math.isclose(new['encumbrance'], old['encumbrance'] + int(phase == 'take'), abs_tol=1e-4)
    assert before['player']['awarded_skill_books'] == after['player']['awarded_skill_books']
    assert before['player']['character_generation'] == after['player']['character_generation']
    player = lambda state: next(v for v in state['native_actor_values'] if v['actor'] == PLAYER)
    assert player(before) == player(after)
    for field in ('native_actor_life', 'native_death_counts', 'quests', 'next_dynamic_serial'):
        assert before[field] == after[field], field
    assert after['next_dynamic_serial'] == 15
    assert before['native_combat_engagements'] == after['native_combat_engagements']
    assert sum(PLAYER in (v['first'], v['second']) for v in after['native_combat_engagements']) == 3
    hour = 'content:oblivion.esm:000038'
    assert {k: v for k, v in before['globals'].items() if k != hour} == {k: v for k, v in after['globals'].items() if k != hour}
    refs = lambda state: {v['key']: v for v in state['references'] if v['base'] == BASE}
    old_refs, new_refs = refs(before), refs(after)
    original = new_refs[REFERENCE]
    assert original['deleted'] and original['custom_state']['count'] == 0
    assert original['custom_state']['read'] and original['custom_state']['taken']
    assert {k: v for k, v in old_refs.items() if k != REFERENCE} == {k: v for k, v in new_refs.items() if k != REFERENCE}
    if phase != 'take':
        assert old_refs == new_refs
    for ref in before['references']:
        if ref['key'].startswith('dynamic:native-reference:'):
            assert next(v for v in after['references'] if v['key'] == ref['key']) == ref
    assert before['native_loose_weapons'] == after['native_loose_weapons'] == []
    cg = lambda state: [v for v in state['script_instances'] if v['unit'].startswith('content:oblivion.esm:0661bf@')]
    assert cg(before) == cg(after) and len(cg(after)) == 1
    assert cg(after)[0]['context'] == 'dynamic:inventory-script:0000000000000009'
    for state in (before, after):
        mq = next(v for v in state['script_instances'] if v['unit'] == 'content:oblivion.esm:0297ad@oblivion.esm/quest/unit=0')
        assert [v['value'] for v in mq['locals'][11:13]] == [1, 1]
        assert not any(v['key'] == 'dynamic:native-reference:0000000000000009' for v in state['references'])
    log = (root / 'process.log').read_text()
    assert log.count('M16 ordinary book read: book=' + BASE) == reads
    assert 'M16 skill book read:' not in log
    assert log.count('M5 interaction: kind=take result=taken ref=' + REFERENCE) == int(phase == 'take')
    assert log.count('M5 interaction: kind=read result=read ref=' + REFERENCE) == int(phase == 'take')
    pages = [(int(page), int(total)) for page, total in re.findall(r'M13 native book page: base=FormId:0x1024567 first=(\d+) pages=(\d+)', log)]
    assert pages == [(1, 6)], pages
    assert not re.search(r'Failed to open image:.*tx_menubook_', log)
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads, saves = ([v for v in events if v['event'] == name] for name in ('load-complete', 'save-complete'))
    epoch = read(root / 'process-epoch.json')
    assert len(loads) == len(saves) == 1 and loads[0]['pid'] == saves[0]['pid'] == epoch['pid']
    assert loads[0]['save_sha256'] == sha(source) and saves[0]['save_sha256'] == sha(target)
    loaded = read(root / loads[0]['live'])
    assert loaded['player']['actor_values'].get(COUNTER, 0) == old.get(COUNTER, 0)
    assert read(root / saves[0]['live']) == after
    if (root / 'observed-sanitizer-environment.json').exists():
        env = read(root / 'observed-sanitizer-environment.json')['environment']
        assert env['ASAN_OPTIONS'] == 'detect_leaks=0:halt_on_error=1'
        assert env['UBSAN_OPTIONS'] == 'halt_on_error=1:print_stacktrace=1'
        libs = read(root / 'loaded-libraries.json')
        assert any('libasan' in v for v in libs) and any('libubsan' in v for v in libs)
        assert not re.search(r'AddressSanitizer|runtime error:|UndefinedBehaviorSanitizer', log)
    result = dict(passed=True, phase=phase, source_sha256=sha(source), save_sha256=sha(target),
                  process=epoch, inventory=own(after), original=original, pages=pages,
                  books_read_before=old.get(COUNTER, 0), books_read_after=new.get(COUNTER, 0),
                  allocator=after['next_dynamic_serial'], other_inventory_skills_scripts_life_retained=True)
    (root / 'fundaments-stack-book-verification.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))

if __name__ == '__main__':
    main()
