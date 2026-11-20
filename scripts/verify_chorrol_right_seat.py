"""Audit original Chorrol right-entry chair input, cold ownership and preserved reward state."""
import argparse
import hashlib
import json
import math
from pathlib import Path

import tes4_runtime_state as runtime
from verify_blade_use import shared as skills
from verify_skill_book_read import shared as advancement

PLAYER = 'dynamic:player:0000000000000001'
REFERENCE = 'content:oblivion.esm:03fdce'
BASE = 'content:oblivion.esm:02dd43'


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=('seat', 'exit', 'standing'))
    parser.add_argument('--sanitized', action='store_true')
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
    assert after['schema_version'] >= 106
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert before['player']['inventory'] == after['player']['inventory']
    assert skills(source) == skills(target) and advancement(source) == advancement(target)
    for field in ('quests', 'native_death_counts', 'next_dynamic_serial'):
        assert before[field] == after[field], field
    quest = next(q for q in after['quests'] if q['quest'] == 'content:oblivion.esm:027815')
    assert quest['stage'] == 100
    actor = lambda state: next(v for v in state['native_actor_values'] if v['actor'] == PLAYER)
    assert actor(before)['reputation'] == actor(after)['reputation']
    assert actor(after)['reputation']['fame'] == 1
    assert actor(before)['values'] == actor(after)['values']
    reference = lambda state: next(v for v in state['references'] if v['key'] == REFERENCE)
    assert reference(before) == reference(after)
    assert after['player']['cell'] == before['player']['cell'] == 'content:oblivion.esm:02898f'
    old, new = before.get('native_player_furniture'), after['native_player_furniture']
    pose = after['player']['position']
    if args.phase == 'seat':
        assert old is None
        assert new['reference'] == REFERENCE and new['base'] == BASE
        assert new['marker'] == 1 and new['phase'] == 3
        assert math.dist(pose[:3], [-1693.12, 703.42, -317.92]) < 2
        heading = math.radians(359.98)
    elif args.phase == 'exit':
        assert old['reference'] == REFERENCE and old['phase'] == 3 and new is None
        assert math.dist(pose[:3], [-1643.61, 704.00, -318.96]) < 12
        heading = math.radians(89.98)
    else:
        assert old is None and new is None
        assert math.dist(pose[:2], before['player']['position'][:2]) > 1
        heading = None
    if heading is not None:
        assert abs(math.remainder(pose[5] - heading, 2 * math.pi)) < .02
    epoch = read(root / 'process-epoch.json')
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in events if e['event'] == 'load-complete']
    saves = [e for e in events if e['event'] == 'save-complete']
    assert len(loads) == len(saves) == 1
    for event in loads + saves:
        assert event['pid'] == epoch['pid'] and sha(root / event['live']) == event['live_sha256']
    assert loads[0]['save_sha256'] == sha(source) and saves[0]['save_sha256'] == sha(target)
    assert read(root / loads[0]['live']).get('native_player_furniture') == old
    assert read(root / saves[0]['live']) == after
    if args.sanitized:
        env = read(root / 'observed-sanitizer-environment.json')['environment']
        assert 'halt_on_error=1' in env['ASAN_OPTIONS']
        assert 'halt_on_error=1' in env['UBSAN_OPTIONS']
        libraries = read(root / 'loaded-libraries.json')
        assert any('libasan' in name for name in libraries)
        assert any('libubsan' in name for name in libraries)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  input_sha256=sha(source), save_sha256=sha(target),
                  binary_sha256=epoch['exe_sha256'], process=epoch, pose=pose,
                  furniture=new, reward=quest,
                  tolerances='Original seated <2 units, standing <12 units, heading .02rad; exact ownership and cold identity.')
    (root / 'right-seat-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
