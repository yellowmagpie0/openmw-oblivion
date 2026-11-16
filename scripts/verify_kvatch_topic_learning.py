"""Audit original Savlian INFO topic learning and DIAS cold persistence."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import tes4_runtime_state as runtime

QUEST = 'content:oblivion.esm:0224d8'
TOPICS = {0x02d756, 0x098112}


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def known_topics(path):
    data = path.read_bytes()
    records = list(runtime._save_records(data))
    masters = [data[start + 8:end].rstrip(b'\0').decode()
               for _, _, name, subs in records if name == b'TES3'
               for tag, start, end in subs if tag == b'MAST']
    topics = [data[start + 8:end] for _, _, name, subs in records if name == b'DIAS'
              for tag, start, end in subs if tag == b'TOPI']
    assert len(topics) == len(set(topics))
    native = set()
    for topic in topics:
        if topic[0] == 3:
            assert len(topic) == 9
            index, content_file = struct.unpack('<Ii', topic[1:])
            assert 0 <= content_file < len(masters)
            native.add((masters[content_file].lower(), index))
    return set(topics), native


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=('learned', 'cold'))
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    root = args.course
    driver, scenario = read(root / 'driver-process.json'), read(root / 'scenario.json')
    assert driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert driver['exit_code'] == scenario['exit_code'] == 0
    assert scenario['passed'] and scenario['actions_complete']
    assert all(a.get('passed') for a in scenario['actions'])
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert after['schema_version'] == 106
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    old_topics, old_native = known_topics(source)
    new_topics, new_native = known_topics(target)
    expected = {('oblivion.esm', topic) for topic in TOPICS}
    assert expected <= new_native and not old_topics - new_topics
    quest = lambda state: next(q for q in state['quests'] if q['quest'] == QUEST)
    assert quest(after)['stage'] == 5 and quest(after)['running']
    script = next(s for s in after['script_instances'] if s['context'] == QUEST
                  and s['unit'].startswith('content:oblivion.esm:02802c@'))
    assert script['locals'][7] == {'type': 'number', 'value': 1}
    log = (root / 'process.log').read_text()
    if args.phase == 'learned':
        assert quest(before)['stage'] == 0
        assert not expected & old_native
        assert new_native - old_native == expected and len(new_topics - old_topics) == 2
        for topic in TOPICS:
            assert f'Native dialogue added topic: info=content:oblivion.esm:098143 topic=content:oblivion.esm:{topic:06x} new=1' in log
    else:
        assert old_topics == new_topics and quest(before) == quest(after)
        assert ' new=1' not in log
        # Save before reopening: retention cannot be supplied by a second INFO.
        assert log.index('Writing saved game') < log.index('Native dialogue menu:')
        assert (root / 'cold-savlian-retained-topic-selected.png').is_file()
        assert 'Native dialogue menu: actor=content:oblivion.esm:028a7a info=content:oblivion.esm:098144 choices=2' in log
        assert 'M7 ObScript: setstage quest=content:oblivion.esm:0224d8 stage=8' in log
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
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  input_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch['exe_sha256'],
                  before_known=len(old_topics), after_known=len(new_topics), quest_stage=quest(after)['stage'],
                  scope='Player INFO-added topics and DIAS persistence; not full Kvatch progression.')
    (root / 'topic-learning-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
