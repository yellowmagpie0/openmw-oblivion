"""Audit the actual novice crafted-poison course and its saved ownership."""
import argparse
import copy
import hashlib
import json
import math
import re
from pathlib import Path

import tes4_runtime_state as runtime
from verify_tutorial_alchemy import generated_potions, skills, progress

PLAYER = 'dynamic:player:0000000000000001'
POISON = 'dynamic:shared-item:0000000000000001'
AXE = 'content:oblivion.esm:090613'
GOBLIN = 'content:oblivion.esm:03b513'
MORTAR = 'content:oblivion.esm:0c7968'


def read(path):
    return json.loads(Path(path).read_text())


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inventory(state):
    rows = copy.deepcopy(state['player']['inventory'])
    for row in rows:
        row.setdefault('weapon_poison', None)  # Explicit empty v103 -> v105 migration.
    return sorted(rows, key=lambda row: json.dumps(row, sort_keys=True))


def item(rows, base):
    matches = [row for row in rows if row['base'] == base]
    assert len(matches) == 1, (base, matches)
    return matches[0]


def actor(state):
    return next(row for row in state['native_actor_values'] if row['actor'] == GOBLIN)


def health(state):
    return sum(value or 0 for value in actor(state)['values'][8])


def effects(state):
    return actor(state).get('weapon_poison_effects', [])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=['decline', 'apply', 'miss', 'approach', 'contact', 'paused', 'expiry', 'spent'])
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
    if args.phase in ('decline', 'apply'):
        original = ('62a04d89b60303c34eabd168a8c3ccddad932b54a888be6c91c7322f94d845ce'
                    if args.sanitized else
                    '3daa7c67027e16b2209b831a85c8de82c38b3ae8039e02ad02c1f915fa8a1e86')
        assert sha(source) == original, 'Course must start from accepted genuine alchemy branch'

    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [event for event in events if event['event'] == 'load-complete']
    saves = [event for event in events if event['event'] == 'save-complete']
    assert len(loads) == 1 and len(saves) == 1
    epoch = read(root / 'process-epoch.json')
    assert all(event['pid'] == epoch['pid'] and event['epoch'] == 1 for event in loads + saves)
    assert loads[0]['save_sha256'] == sha(source) and saves[0]['save_sha256'] == sha(target)
    saved_source = runtime.load_save(source)
    before = read(root / loads[0]['live'])
    after = read(root / saves[0]['live'])
    assert before['schema_version'] == after['schema_version'] == 105
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert inventory(saved_source) == inventory(before), 'Inventory changed during load'
    assert saved_source['quests'] == before['quests'] == after['quests']
    assert saved_source['next_dynamic_serial'] == before['next_dynamic_serial'] == after['next_dynamic_serial'] == 2
    assert saved_source['native_death_counts'] == before['native_death_counts'] == after['native_death_counts']
    assert generated_potions(source) == generated_potions(target)
    assert len(generated_potions(target)) == 1
    definition = generated_potions(target)[0]
    assert definition['key'] == POISON
    assert definition['raw_sha256'] == '66fd2b7263ef4eda0b07f932943c62cedba1453baaa2bdc86174f06879b60182'
    assert definition['effect'] == [0, 0, 5, 1, 1] and definition['flags'] == 0x100
    assert definition['name'] == 'Damage Health' and definition['value'] == 3
    assert math.isclose(definition['weight'], .1, abs_tol=1e-6)

    assert item(inventory(before), MORTAR) == item(inventory(after), MORTAR)
    assert item(inventory(after), MORTAR)['script_owner'] == 'dynamic:inventory-script:0000000000000001'
    mortar_scripts = lambda state: [row for row in state['script_instances'] if '0c7966' in row['unit']]
    assert len(mortar_scripts(before)) == 1 and mortar_scripts(before) == mortar_scripts(after)
    if args.phase in ('paused', 'expiry', 'spent'):
        assert effects(saved_source) == effects(before), 'Timer changed during load'
        assert actor(saved_source)['values'][8] == actor(before)['values'][8], 'Health changed during load'
    log = (root / 'process.log').read_text()
    applied = log.count('TES4 weapon poison applied: poison=' + POISON)
    consumed = log.count('TES4 weapon poison contact committed: poison=' + POISON)
    assert applied == int(args.phase == 'apply') and consumed == int(args.phase == 'contact')
    hits = re.findall(r'M15 melee hit committed: actor=' + re.escape(PLAYER)
                      + r' .*?health_damage=([0-9.e+-]+).*?victim=' + re.escape(GOBLIN), log)
    assert len(hits) == int(args.phase == 'contact')
    incoming = re.findall(r'M15 melee hit committed: actor=' + re.escape(GOBLIN)
                          + r' .*?health_damage=([0-9.e+-]+).*?victim=' + re.escape(PLAYER)
                          + r' .*?armor_condition_writes=([0-9]+)', log)
    incoming_damage = sum(float(hit[0]) for hit in incoming)
    armor_writes = sum(int(hit[1]) for hit in incoming)
    assert math.isclose(after['player']['actor_values']['health.current'],
                        before['player']['actor_values']['health.current'] - incoming_damage, abs_tol=.02)
    expected = inventory(before)
    expected_axe = item(expected, AXE)
    if args.phase == 'apply':
        bottle = item(expected, POISON)
        assert bottle['count'] == 1
        expected.remove(bottle)
        assert expected_axe['weapon_poison'] is None
        expected_axe['weapon_poison'] = POISON
    elif args.phase == 'contact':
        assert expected_axe['weapon_poison'] == POISON
        expected_axe['weapon_poison'] = None
        current = item(inventory(after), AXE)
        assert 0 <= current['condition'] < expected_axe['condition']
        expected_axe['condition'] = current['condition']
    remaining = inventory(after)
    changed_armor = 0
    for old in expected:
        if old in remaining:
            remaining.remove(old)
            continue
        matches = [new for new in remaining if {k: v for k, v in new.items() if k != 'condition'}
                   == {k: v for k, v in old.items() if k != 'condition'}]
        assert len(matches) == 1 and incoming_damage > 0
        new = matches[0]
        assert old['count'] == 1 and old['equipped_slots'] & 0xffff
        assert 0 <= new['condition'] < old['condition']
        changed_armor += 1
        remaining.remove(new)
    assert not remaining and changed_armor <= armor_writes
    if len(incoming) == 1:
        assert changed_armor == armor_writes
    if args.phase == 'decline':
        assert item(inventory(after), POISON)['count'] == 1
        assert item(inventory(after), AXE)['weapon_poison'] is None
    elif args.phase != 'apply':
        assert not any(row['base'] == POISON for row in inventory(after))
        expected_coating = POISON if args.phase in ('miss', 'approach') else None
        assert item(inventory(after), AXE)['weapon_poison'] == expected_coating
    if args.phase == 'miss':
        assert log.count('M15 melee miss committed: actor=' + PLAYER) == 1
    if args.phase == 'contact':
        assert len(effects(after)) == 1
        effect = effects(after)[0]
        assert effect['poison'] == POISON and effect['attacker'] == PLAYER
        assert effect['magnitude'] == 1 and effect['duration'] == 5 and 0 <= effect['elapsed'] < 5
        assert math.isclose(health(after), health(before) - float(hits[0]) - effect['elapsed'], abs_tol=.02)
    elif args.phase in ('paused', 'expiry'):
        assert len(effects(before)) == 1
        old = effects(before)[0]
        if args.phase == 'paused':
            assert len(effects(after)) == 1
            new = effects(after)[0]
            assert {k: v for k, v in old.items() if k != 'elapsed'} == {k: v for k, v in new.items() if k != 'elapsed'}
            assert old['elapsed'] <= new['elapsed'] < 5
            damage = new['elapsed'] - old['elapsed']
            paused = [float(value) for value in re.findall(r'CraftedTarget health=([0-9.e+-]+) dead=false paused=true', log)]
            assert len(paused) >= 2 and max(paused) - min(paused) <= .02
        else:
            assert not effects(after)
            damage = 5 - old['elapsed']
        assert math.isclose(health(after), health(before) - damage, abs_tol=.02)
    elif args.phase == 'spent':
        assert not effects(before) and not effects(after)
        assert math.isclose(health(before), health(after), abs_tol=.02)
    old_skills, new_skills = skills(source), skills(target)
    for index, (old, new) in enumerate(zip(old_skills, new_skills, strict=True)):
        assert old['base'] == new['base']
        # Existing native weapon and worn-heavy-armour uses, witnessed by actual
        # physical contacts. Poison ticks themselves award no training.
        award = .5 * len(hits) if index == 4 else 1.25 * armor_writes if index == 3 else 0
        assert math.isclose(new['experience'], old['experience'] + award, abs_tol=1e-4), (index, old, new, award)
    assert progress(source) == progress(target)
    binary = Path(epoch['argv'][0])
    metadata = read(binary.parent / 'metadata.json')
    assert epoch['exe_sha256'] == sha(binary) == metadata['binary_sha256']
    for name, digest in metadata['sources_sha256'].items():
        if Path(name).suffix in ('.cpp', '.hpp', '.h'):
            assert sha(name) == digest, 'C++ source changed since runtime candidate: ' + name
    for name, digest in metadata['frozen_resource_sha256'].items():
        assert sha(binary.parent / 'resources' / name) == digest
    if args.sanitized:
        environment = read(root / 'observed-sanitizer-environment.json')['environment']
        assert environment['ASAN_OPTIONS'] == 'detect_leaks=0:halt_on_error=1'
        assert environment['UBSAN_OPTIONS'] == 'halt_on_error=1:print_stacktrace=1'
        libraries = read(root / 'loaded-libraries.json')
        assert any('libasan' in name for name in libraries) and any('libubsan' in name for name in libraries)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  auditor_sha256=sha(Path(__file__)), runtime_codec_sha256=sha(Path(runtime.__file__)),
                  canonical_payload_matches_actual_save=True,
                  source_sha256=sha(source), save_sha256=sha(target), process=epoch,
                  source_inventory_loaded_exactly=True, generated_definition_retained=True,
                  incoming_contacts=len(incoming), incoming_damage=incoming_damage,
                  changed_armor=changed_armor, player_weapon_hits=len(hits), apply_events=applied,
                  poison_contacts=consumed)
    if args.phase in ('contact', 'paused', 'expiry', 'spent'):
        result.update(target_health_before=health(before), target_health_after=health(after),
                      effects_before=effects(before), effects_after=effects(after))
    (root / 'crafted-poison-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
