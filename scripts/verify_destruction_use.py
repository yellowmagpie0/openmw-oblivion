"""Independently audit genuine Flare skill use and saved projectile ownership."""
import argparse
import hashlib
import json
import math
import re
import struct
from pathlib import Path

import tes4_runtime_state as runtime

PLAYER = 'dynamic:player:0000000000000001'
FLARE = 'content:oblivion.esm:0a97df'
SKELETON = 'content:oblivion.esm:04bb63'


def read(path):
    return json.loads(path.read_text())


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def shared(path):
    data = path.read_bytes()
    subs = next(s for _, _, tag, s in runtime._save_records(data) if tag == b'PLAY')
    starts = [i for i, (tag, _, _) in enumerate(subs) if tag == b'STBA']
    fields = {tag: data[a + 8:b] for tag, a, b in subs[starts[10]:starts[11]]}
    all_fields = {tag: data[a + 8:b] for tag, a, b in subs}
    return dict(base=struct.unpack('<f', fields[b'STBA'])[0],
                experience=struct.unpack('<f', fields.get(b'STPR', bytes(4)))[0],
                major=struct.unpack('<i', all_fields.get(b'LPRO', bytes(4)))[0],
                attributes=list(struct.unpack('<8i', all_fields.get(b'INCR', bytes(32)))),
                specializations=list(struct.unpack('<3i', all_fields.get(b'SPEC', bytes(12)))))


def player_values(state):
    return next(v['values'] for v in state['native_actor_values'] if v['owner'] == 0)


def flights(state):
    return [v for v in state['native_magic_projectiles'] if v['caster'] == PLAYER and v['source'] == FLARE]


def inventory(state):
    # Actual incoming arrows may wear equipped armor during the encounter.
    rows = []
    for item in state['player']['inventory']:
        row = dict(item)
        # Schema93 introduced inventory script owners; older absent owners
        # migrate to null, without creating an owned script instance.
        row.setdefault('script_owner', None)
        if row['equipped_slots']:
            row.pop('condition', None)
        rows.append(row)
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=['four_hits', 'pending', 'resume', 'completed', 'miss', 'scroll'])
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    root, phase = args.course, args.phase
    driver = read(root / 'driver-process.json')
    assert driver['exit_code'] == 0 and driver['captured']
    assert driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert read(root / 'scenario.json')['passed']
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    old, new = shared(source), shared(target)
    assert after['schema_version'] == 101
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    expected_base = 6 if phase == 'four_hits' else old['base']
    expected_xp = 1.9062095 if phase == 'four_hits' else 1.2 if phase == 'resume' else old['experience']
    assert new['base'] == expected_base and math.isclose(new['experience'], expected_xp, abs_tol=.001)
    levels = int(phase == 'four_hits')
    assert new['major'] == old['major']
    assert new['attributes'] == [v + (levels if i == 2 else 0) for i, v in enumerate(old['attributes'])]
    assert new['specializations'] == [v + (levels if i == 1 else 0) for i, v in enumerate(old['specializations'])]
    assert player_values(after)[22][0] == expected_base
    assert player_values(after)[22][1:] == player_values(before)[22][1:]
    for key in ('skill_advances.22', 'pc_misc_stat.2'):
        assert after['player']['actor_values'].get(key, 0) == before['player']['actor_values'].get(key, 0) + levels
    assert before['player']['character_generation'] == after['player']['character_generation']
    assert before['player'].get('awarded_skill_books', []) == after['player'].get('awarded_skill_books', [])
    assert inventory(before) == inventory(after)
    for left, right in zip(before['player']['inventory'], after['player']['inventory']):
        if left['equipped_slots'] and left['condition'] >= 0:
            assert 0 <= right['condition'] <= left['condition'] + .001
        else:
            assert right['condition'] == left['condition']
    assert after['player']['actor_values']['health.current'] > 0
    if phase == 'pending':
        assert not flights(before) and len(flights(after)) == 1
        assert after['next_magic_projectile'] == before['next_magic_projectile'] + 1
    elif phase == 'resume':
        assert len(flights(before)) == 1 and not flights(after)
        assert after['next_magic_projectile'] == before['next_magic_projectile']
    else:
        assert not flights(after)
    target_key = 'content:oblivion.esm:06c359' if phase == 'scroll' else SKELETON
    life = next(v for v in after['native_actor_life'] if v['actor'] == target_key)
    target_values = next(v['values'] for v in after['native_actor_values'] if v['actor'] == target_key)
    if phase in ('four_hits', 'scroll'):
        assert life['phase'] == 1 and life['killer'] == PLAYER
    elif phase in ('pending', 'resume', 'miss'):
        assert life['phase'] == 0
    old_counts = {v['base']: v['count'] for v in before['native_death_counts']}
    new_counts = {v['base']: v['count'] for v in after['native_death_counts']}
    if phase in ('four_hits', 'scroll'):
        old_counts[life['base']] = old_counts.get(life['base'], 0) + 1
    assert old_counts == new_counts
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [v for v in events if v['event'] == 'load-complete']
    saves = [v for v in events if v['event'] == 'save-complete']
    assert len(loads) == len(saves) == 1
    epoch = read(root / 'process-epoch.json')
    assert all(v['pid'] == epoch['pid'] and v['epoch'] == 1 for v in loads + saves)
    assert read(root / saves[0]['live']) == after
    loaded = read(root / loads[0]['live'])
    assert loaded['native_magic_projectiles'] == before['native_magic_projectiles']
    log = (root / 'process.log').read_text().split('Writing saved game', 1)[0]
    impacts = re.findall(r'M16 magic projectile impact: id=(\d+) actor=(\d+) damage=([0-9.e+-]+) destruction_use=(\d+)', log)
    assert sum(int(v[3]) for v in impacts) == {'four_hits': 4, 'resume': 1}.get(phase, 0)
    releases = log.count('M16 native cast release: spell=' + FLARE)
    assert releases == {'four_hits': 4, 'pending': 1, 'miss': 1}.get(phase, 0)
    if phase == 'miss':
        assert len(impacts) == 1 and impacts[0][1:] == ('0', '0', '0')
    if phase == 'scroll':
        assert len(impacts) == 1 and impacts[0][1] == '1' and float(impacts[0][2]) > 0
    if args.sanitized:
        env = read(root / 'observed-sanitizer-environment.json')['environment']
        assert env['ASAN_OPTIONS'] == 'detect_leaks=0:halt_on_error=1'
        assert env['UBSAN_OPTIONS'] == 'halt_on_error=1:print_stacktrace=1'
        libs = read(root / 'loaded-libraries.json')
        assert any('libasan' in p for p in libs) and any('libubsan' in p for p in libs)
    out = dict(passed=True, phase=phase, input_sha256=sha(source), output_sha256=sha(target),
               before=old, after=new, impacts=impacts, releases=releases,
               health=after['player']['actor_values']['health.current'], target_life=life,
               target_health_values=target_values[8], pending_flights=flights(after),
               game_sha256=epoch['exe_sha256'])
    (root / 'destruction-use-audit.json').write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps(out))


if __name__ == '__main__':
    main()
