"""Audit real ordinary-book reads, native counters and retained save state."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import tes4_runtime_state as runtime
from verify_blade_use import shared
from verify_skill_book_read import shared as progression

PLAYER = 'dynamic:player:0000000000000001'
BOOK = 'content:oblivion.esm:024567'
REFERENCE = 'content:oblivion.esm:06efb3'
STAT = 'pc_misc_stat.17'

def read(path):
    return json.loads(path.read_text())

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=['baseline', 'reads', 'cold'])
    parser.add_argument('--reads', type=int, default=1)
    args = parser.parse_args()
    root = args.course
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
    assert before['player']['inventory'] == after['player']['inventory']
    assert before['player']['awarded_skill_books'] == after['player']['awarded_skill_books']
    old, new = before['player']['actor_values'], after['player']['actor_values']
    assert {k: v for k, v in old.items() if k != STAT} == {k: v for k, v in new.items() if k != STAT}
    delta = 0 if args.phase == 'baseline' else args.reads
    assert new.get(STAT, 0) == old.get(STAT, 0) + delta
    player = lambda state: next(v for v in state['native_actor_values'] if v['actor'] == PLAYER)
    assert player(before) == player(after)
    for field in ('native_actor_life', 'native_death_counts', 'quests', 'next_dynamic_serial'):
        assert before[field] == after[field], field
    assert after['next_dynamic_serial'] == 14
    hour = 'content:oblivion.esm:000038'
    assert {k: v for k, v in before['globals'].items() if k != hour} == {k: v for k, v in after['globals'].items() if k != hour}
    for field in ('character_generation',):
        assert before['player'][field] == after['player'][field]
    original = next(v for v in after['references'] if v['key'] == REFERENCE)
    assert original['base'] == BOOK and original['enabled'] and not original['deleted']
    assert original['custom_state']['read']
    assert not any(i['base'] == BOOK for i in after['player']['inventory'])
    for old_ref in before['references']:
        if old_ref['key'].startswith('dynamic:native-reference:'):
            assert next(v for v in after['references'] if v['key'] == old_ref['key']) == old_ref
    assert before['native_loose_weapons'] == after['native_loose_weapons'] == []
    log = (root / 'process.log').read_text()
    assert log.count('M5 interaction: kind=read result=read ref=' + REFERENCE) == args.reads
    assert log.count('M16 ordinary book read: book=' + BOOK) == delta
    assert 'M16 skill book read:' not in log
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [v for v in events if v['event'] == 'load-complete']
    saves = [v for v in events if v['event'] == 'save-complete']
    epoch = read(root / 'process-epoch.json')
    assert len(loads) == len(saves) == 1
    assert loads[0]['pid'] == saves[0]['pid'] == epoch['pid']
    assert loads[0]['save_sha256'] == sha(source) and saves[0]['save_sha256'] == sha(target)
    loaded = read(root / loads[0]['live'])
    assert loaded['player']['actor_values'].get(STAT, 0) == old.get(STAT, 0)
    assert loaded['player']['awarded_skill_books'] == before['player']['awarded_skill_books']
    assert read(root / saves[0]['live']) == after
    if (root / 'observed-sanitizer-environment.json').exists():
        env = read(root / 'observed-sanitizer-environment.json')['environment']
        assert env['ASAN_OPTIONS'] == 'detect_leaks=0:halt_on_error=1'
        assert env['UBSAN_OPTIONS'] == 'halt_on_error=1:print_stacktrace=1'
        assert 'libasan' in env['LD_PRELOAD'].split(':')[0]
        libs = read(root / 'loaded-libraries.json')
        assert any('libasan' in p for p in libs) and any('libubsan' in p for p in libs)
        assert not re.search(r'AddressSanitizer|runtime error:|UndefinedBehaviorSanitizer', log)
    result = dict(passed=True, phase=args.phase, reads=args.reads, books_read_before=old.get(STAT, 0),
                  books_read_after=new.get(STAT, 0), source_sha256=sha(source), save_sha256=sha(target),
                  process=epoch, allocator=after['next_dynamic_serial'], skills_and_history_preserved=True)
    (root / 'ordinary-book-verification.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))

if __name__ == '__main__':
    main()
