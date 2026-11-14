"""Audit the original Wellspring sack through native Lua discovery and a real save."""
import hashlib
import json
from pathlib import Path
import sys

import tes4_runtime_state as runtime
from verify_blade_use import shared
from verify_skill_book_read import shared as progress


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def audit(root):
    read = lambda name: json.loads((root / name).read_text())
    driver = read('driver-process.json')
    assert driver['captured'] and driver['exit_code'] == 0
    assert driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert read('scenario.json')['passed']
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert before['schema_version'] == after['schema_version'] == 103
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    for key in ['inventory', 'actor_values', 'position']:
        assert before['player'][key] == after['player'][key], key
    for key in ['next_dynamic_serial', 'native_actor_life', 'native_death_counts',
                'native_loose_weapons', 'quests']:
        assert before[key] == after[key], key
    assert shared(source) == shared(target)
    assert progress(source) == progress(target)
    sack = lambda state: [r for r in state['references'] if r['key'] == 'content:oblivion.esm:06efcf']
    assert len(sack(after)) == 1 and sack(before) == sack(after)
    ref = sack(after)[0]
    assert ref['base'] == 'content:oblivion.esm:024945'
    assert ref['enabled'] and not ref['deleted'] and ref['custom_state']['count'] == 1
    assert ref['custom_state']['locked'] is False
    scripts = lambda state: [s for s in state['script_instances'] if
        s['unit'].startswith(('content:oblivion.esm:0661bf@', 'content:oblivion.esm:0297ad@'))]
    assert scripts(before) == scripts(after)
    epoch = read('process-epoch.json')
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    for event in ['load-complete', 'save-complete']:
        matching = [e for e in events if e['event'] == event]
        assert len(matching) == 1 and matching[0]['pid'] == epoch['pid']
    log = (root / 'process.log').read_text()
    assert 'NativeContainerDiscovery count=1 position=true bounds=true type=ESM4Container' in log
    assert 'M7 inventory add event' not in log and 'M7 inventory equip event' not in log
    assert not any(error in log for error in ['AddressSanitizer', 'runtime error:', 'UndefinedBehaviorSanitizer'])
    if (root / 'observed-sanitizer-environment.json').exists():
        env = read('observed-sanitizer-environment.json')['environment']
        assert env['ASAN_OPTIONS'] == 'detect_leaks=0:halt_on_error=1'
        assert env['UBSAN_OPTIONS'] == 'halt_on_error=1:print_stacktrace=1'
        libraries = read('loaded-libraries.json')
        assert any('libasan' in p for p in libraries) and any('libubsan' in p for p in libraries)
    result = dict(passed=True, process=epoch, source_sha256=sha(source),
                  save_sha256=sha(target), sack=ref, inventory_preserved=True,
                  progress_preserved=True, next_dynamic_serial=after['next_dynamic_serial'])
    (root / 'native-container-verification.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


if __name__ == '__main__':
    print(json.dumps(audit(Path(sys.argv[1]))))
