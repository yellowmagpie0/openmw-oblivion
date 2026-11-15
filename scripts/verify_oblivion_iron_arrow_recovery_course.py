"""Audit real Iron Arrow stone impact, pickup, restart and settled expiry."""

import argparse
import copy
import hashlib
import json
from pathlib import Path

import tes4_runtime_state as runtime

PLAYER = 'dynamic:player:0000000000000001'
AMMO = 'content:oblivion.esm:017829'
BOW = 'content:oblivion.esm:047aca'
SURFACE = 'content:oblivion.esm:049be8'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def arrow_references(state):
    return [r for r in state['references'] if 'native.arrow.projectile' in r['custom_state']]


def ammunition(state):
    return sum(x['count'] for x in state['player']['inventory'] if x['base'] == AMMO)


def without_ammo(state):
    items = copy.deepcopy(state['player']['inventory'])
    return [x for x in items if x['base'] != AMMO]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('--phase', choices=('create', 'take', 'restart', 'expire'), required=True)
    args = parser.parse_args()
    root = args.course
    driver = json.loads((root / 'driver-process.json').read_text())
    source = Path(driver['input_save'])
    output = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert driver['captured'] and driver['exit_code'] == 0 and driver['input_unchanged']
    assert digest(source) == driver['input_sha256']
    assert json.loads((root / 'scenario.json').read_text())['passed']
    initial, final = runtime.load_save(source), runtime.load_save(output)
    assert initial['schema_version'] == final['schema_version'] == 97
    assert runtime.encode_payload(final) == runtime._find_runtime_record(output.read_bytes())[3]
    events = [json.loads(x) for x in (root / 'events.jsonl').read_text().splitlines()]
    loaded = next(x for x in events if x['event'] == 'load-complete')
    saved = [x for x in events if x['event'] == 'save-complete'][-1]
    assert loaded['save_sha256'] == digest(source) and saved['save_sha256'] == digest(output)
    live = json.loads((root / loaded['live']).read_text())
    assert live['player']['inventory'] == initial['player']['inventory']
    assert live['clock'] == initial['clock']
    assert arrow_references(live) == arrow_references(initial)
    assert json.loads((root / saved['live']).read_text()) == final
    epoch = json.loads((root / 'process-epoch.json').read_text())
    assert loaded['pid'] == saved['pid'] == epoch['pid']
    old, new = arrow_references(initial), arrow_references(final)
    assert len(new) == 1
    arrow = new[0]
    data = arrow['custom_state']
    assert arrow['key'].startswith('dynamic:native-reference:')
    assert arrow['base'] == AMMO and arrow['cell'] == 'content:oblivion.esm:0463f5'
    assert data['native.arrow.bow'] == BOW and data['native.arrow.caster'] == PLAYER
    assert data['native.arrow.surface'] == SURFACE
    assert data['native.arrow.projectile'] == '6'
    assert not final['native_arrow_projectiles']
    log = (root / 'process.log').read_text()
    clock0, clock1 = initial['clock'], final['clock']
    assert clock0['time_scale'] == clock1['time_scale'] == 30
    assert (clock0['year'], clock0['month']) == (clock1['year'], clock1['month'])
    seconds = ((clock1['day'] - clock0['day']) * 24 + clock1['hour'] - clock0['hour']) * 120
    bow0 = next(x for x in initial['player']['inventory'] if x['base'] == BOW)
    bow1 = next(x for x in final['player']['inventory'] if x['base'] == BOW)
    left, right = without_ammo(initial), without_ammo(final)
    if args.phase == 'create':
        assert not old and ammunition(initial) == 19 and ammunition(final) == 18
        assert final['next_dynamic_serial'] == initial['next_dynamic_serial'] + 1
        assert final['next_arrow_projectile'] == initial['next_arrow_projectile'] + 1
        assert log.count('M15 Player arrow recovered reference:') == 1
        assert log.count('M15 Player arrow released:') == 1
        assert not arrow['deleted'] and data['count'] == 1
        assert 0 <= data['native.arrow.age'] < 10
        assert data['native.arrow.opacity'] == 1 and not data['native.arrow.fading']
        # Only the actual bow's per-shot wear may change unrelated inventory.
        assert 0 < bow1['condition'] <= bow0['condition']
        for item in right:
            if item['base'] == BOW:
                item['condition'] = bow0['condition']
    else:
        assert len(old) == 1 and old[0]['key'] == arrow['key']
        assert old[0]['position'] == arrow['position']
        assert final['next_dynamic_serial'] == initial['next_dynamic_serial']
        assert final['next_arrow_projectile'] == initial['next_arrow_projectile']
        assert 'M15 Player arrow released:' not in log
        assert 'M15 Player arrow recovered reference:' not in log
        assert arrow['deleted'] and data['count'] == 0
        if args.phase == 'take':
            assert ammunition(initial) == 18 and ammunition(final) == 19
            assert not old[0]['deleted']
            assert not data['native.arrow.fading'] and data['native.arrow.opacity'] == 1
            assert 'M15 recovered arrow expired:' not in log
        elif args.phase == 'restart':
            assert ammunition(initial) == ammunition(final) == 19 and old[0]['deleted']
            assert data == old[0]['custom_state']
        else:
            assert ammunition(initial) == ammunition(final) == 18
            assert not old[0]['deleted'] and data['native.arrow.fading']
            assert data['native.arrow.opacity'] == 0 and data['native.arrow.age'] > 90
            assert log.count('M15 recovered arrow expired:') == 1
            remaining = 93 - old[0]['custom_state']['native.arrow.age']
            assert remaining - 1 <= seconds <= remaining + 2
    assert left == right
    amulet = next(x for x in final['player']['inventory'] if x['base'] == 'content:oblivion.esm:0250a0')
    context = amulet['script_owner']
    assert context == 'dynamic:inventory-script:0000000000000001'
    assert [x for x in initial['script_instances'] if x['context'] == context] == [
        x for x in final['script_instances'] if x['context'] == context]
    assert final['native_death_counts'] == initial['native_death_counts']
    assert final['disposition_modifiers'] == initial['disposition_modifiers']
    for state in (initial, final):
        assert state['player']['actor_values']['health.current'] == 110
        corpse = next(x for x in state['native_actor_life'] if x['actor'] == 'content:oblivion.esm:06bfc2')
        assert corpse['phase'] == 1 and corpse['killer'] == PLAYER
    report = dict(passed=True, phase=args.phase, auditor_sha256=digest(Path(__file__)),
                  process=epoch, input_sha256=digest(source), output_sha256=digest(output),
                  ammunition_before=ammunition(initial), ammunition_after=ammunition(final),
                  arrow=arrow, elapsed_simulation_seconds=seconds,
                  scope='Ordinary unenchanted Iron Arrow on original STAT stone; settled age starts at impact.')
    (root / 'course-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
