"""Audit the real Kvatch gate's spawned actors and fresh-process continuity."""
import argparse
import hashlib
import json
from pathlib import Path

import tes4_runtime_state as runtime

GATE = 'content:oblivion.esm:01e8a3'
MARKER = 'content:oblivion.esm:091229'
LIST = 'content:oblivion.esm:008bfd'
LEAF = 'content:oblivion.esm:03e9cb'
TEMPLATE = 'content:oblivion.esm:09122a'
SCRIPT = 'content:oblivion.esm:09122b'
FACTION = 'content:oblivion.esm:090ac0'
PLAYER = 'dynamic:player:0000000000000001'


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def gate(state):
    return next(v for v in state['script_instances'] if v['context'] == GATE)


def spawned(state):
    return {v['key']: v for v in state['references']
            if v['custom_state'].get('native.spawn.source') == MARKER}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=('first', 'cold', 'three', 'death', 'death-cold'))
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    root = args.course
    driver = read(root / 'driver-process.json')
    scenario = read(root / 'scenario.json')
    assert driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert driver['exit_code'] == 0 and scenario['passed'] and scenario['actions_complete']
    assert scenario['exit_code'] == 0 and all(a.get('passed') for a in scenario['actions'])
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert after['schema_version'] == 106
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    epoch = read(root / 'process-epoch.json')
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in events if e['event'] == 'load-complete']
    saves = [e for e in events if e['event'] == 'save-complete']
    assert len(loads) == len(saves) == 1
    for event in loads + saves:
        assert event['pid'] == epoch['pid'] and sha(root / event['live']) == event['live_sha256']
    assert loads[0]['save_sha256'] == sha(source) and saves[0]['save_sha256'] == sha(target)
    assert read(root / saves[0]['live']) == after
    old, new = spawned(before), spawned(after)
    if args.phase in ('death', 'death-cold'):
        assert 1 <= len(new) <= 3
    else:
        assert len(new) == (3 if args.phase == 'three' else 1)
    assert gate(after)['locals'][1]['value'] == len(new)
    assert 0 <= gate(after)['locals'][2]['value'] <= 10
    assert 0 <= gate(after)['locals'][3]['value'] <= len(new)
    values = {v['actor']: v for v in after['native_actor_values']}
    life = {v['actor']: v for v in after['native_actor_life']}
    ai = {v['actor']: v for v in after['actor_ai']}
    scripts = {v['context']: v for v in after['script_instances']}
    serials = []
    for key, reference in new.items():
        assert key.startswith('dynamic:native-reference:')
        serials.append(int(key.rsplit(':', 1)[1], 16))
        assert reference['base'] == LEAF and reference['enabled'] and not reference['deleted']
        custom = reference['custom_state']
        assert custom['native.spawn.requested_base'] == LIST
        assert custom['native.spawn.player_level'] == 1
        assert custom['native.spawn.template'] == TEMPLATE and custom['native.spawn.script'] == SCRIPT
        assert custom['native.spawn.factions'] == 2
        assert custom['native.spawn.faction.0.key'] == FACTION
        assert custom['native.spawn.faction.0.rank'] == 0
        assert custom['native.spawn.faction.1.key'] == 'content:oblivion.esm:053fed'
        assert custom['native.spawn.faction.1.rank'] == 0
        assert 'native.spawn.inventory_pending' not in custom
        assert values[key]['base'] == life[key]['base'] == ai[key]['base'] == LEAF
        # Observed original selected-leaf AVs, rather than the template's 35/0.
        assert values[key]['values'][8][0] == 40 and values[key]['values'][9][0] == 50
        script = scripts[key]
        assert script['unit'].startswith(SCRIPT + '@')
        assert not script['on_load_fired']  # Original template script has GameMode only.
        assert len(script['locals']) == 1
        assert script['locals'][0]['value'] == int(life[key]['phase'] == 1)
    dead = [key for key in new if life[key]['phase'] == 1]
    assert gate(after)['locals'][3]['value'] == len(dead)
    assert life[PLAYER]['phase'] == 0
    assert [v for v in before['native_actor_bases'] if v['base'] == LEAF] == [
        v for v in after['native_actor_bases'] if v['base'] == LEAF]
    assert len(set(serials)) == len(serials) and max(serials) < after['next_dynamic_serial']
    old_inventory, new_inventory = before['player']['inventory'], after['player']['inventory']
    assert len(old_inventory) == len(new_inventory)
    for old_item, new_item in zip(old_inventory, new_inventory):
        old_item, new_item = dict(old_item), dict(new_item)
        if args.phase in ('three', 'death', 'death-cold'):
            old_condition, new_condition = old_item.pop('condition'), new_item.pop('condition')
            assert old_condition == new_condition or 0 <= new_condition <= old_condition
        assert old_item == new_item
    assert values[PLAYER]['base'] == next(v for v in before['native_actor_values'] if v['actor'] == PLAYER)['base']
    if args.phase == 'first':
        assert not old and gate(before)['locals'][1]['value'] == 0
        assert gate(after)['locals'][3]['value'] == 0
    if args.phase == 'cold':
        assert old.keys() == new.keys()
        loaded = read(root / loads[0]['live'])
        assert spawned(loaded) == old
        assert gate(loaded) == gate(before)
        assert after['next_dynamic_serial'] == before['next_dynamic_serial']
        assert 'Native PlaceAtMe actor=' not in (root / 'process.log').read_text()
        for key in old:
            assert old[key]['custom_state'] == new[key]['custom_state']
            assert old[key]['inventory'] == new[key]['inventory']
    if args.phase in ('death', 'death-cold'):
        assert dead and all(life[key]['killer'] == PLAYER for key in dead)
        if args.phase == 'death-cold':
            assert old.keys() <= new.keys()
            loaded = read(root / loads[0]['live'])
            assert spawned(loaded) == old and gate(loaded) == gate(before)
            old_life = {v['actor']: v for v in before['native_actor_life']}
            old_dead = [key for key in old if old_life[key]['phase'] == 1]
            assert dead == old_dead
            assert gate(after)['locals'][3] == gate(before)['locals'][3]
            old_scripts = {v['context']: v for v in before['script_instances']}
            for key in old_dead:
                assert scripts[key] == old_scripts[key] and life[key] == old_life[key]
    if args.sanitized:
        env = read(root / 'observed-sanitizer-environment.json')['environment']
        assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
        libraries = read(root / 'loaded-libraries.json')
        assert any('libasan' in p for p in libraries) and any('libubsan' in p for p in libraries)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  input_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch['exe_sha256'],
                  actors=list(new), gate_locals=gate(after)['locals'],
                  scope='Original gate actor creation/template ownership and declared continuation only; gate entry/main quest remain open.')
    (root / 'gate-spawn-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
