"""Audit real original Shady Sam fixed-offer stock, purse and ownership courses."""
import argparse
import copy
import hashlib
import json
import math
import re
from pathlib import Path

import tes4_runtime_state as runtime
from verify_blade_use import shared

PLAYER = 'dynamic:player:0000000000000001'
SAM = 'content:oblivion.esm:0c45b8'
OWNER = 'content:oblivion.esm:0c45b5'
CHEST = 'content:oblivion.esm:0c45b7'
PICK = 'content:oblivion.esm:00000a'
GOLD = 'content:oblivion.esm:00000f'


def read(path):
    return json.loads(Path(path).read_text())


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def ordered(items):
    return sorted(items, key=lambda item: json.dumps(item, sort_keys=True))


def count(items, base):
    return sum(item['count'] for item in items if item['base'] == base)


def reference(state, key):
    return next(row for row in state['references'] if row['key'] == key)


def ordinary_gold(quantity):
    return dict(base=GOLD, count=quantity, condition=-1, charge=-1,
                equipped_slots=0, hotkey=-1, owner='null', remaining_usage_time=-1,
                ownership_rank=None, ownership_global='null', extra_scale=None,
                bound_source=None, script_owner=None, weapon_poison=None)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=['open', 'cancel', 'insufficient', 'sell', 'buy', 'reload'])
    parser.add_argument('--sales', type=int, default=1)
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    root = args.course
    driver = read(root / 'driver-process.json')
    assert driver['captured'] and driver['exit_code'] == 0
    assert driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert read(root / 'scenario.json')['passed']
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    source = Path(driver['input_save'])
    assert sha(source) == driver['input_sha256']
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [event for event in events if event['event'] == 'load-complete']
    saves = [event for event in events if event['event'] == 'save-complete']
    assert len(loads) == len(saves) == 1
    epoch = read(root / 'process-epoch.json')
    assert all(event['pid'] == epoch['pid'] and event['epoch'] == 1 for event in loads + saves)
    assert loads[0]['save_sha256'] == sha(source) and saves[0]['save_sha256'] == sha(target)
    before = read(root / loads[0]['live'])
    after = read(root / saves[0]['live'])
    assert before['schema_version'] == after['schema_version'] == 104
    saved_before = runtime.load_save(source)
    assert saved_before['schema_version'] == 104
    assert ordered(saved_before['player']['inventory']) == ordered(before['player']['inventory']), 'Inventory changed during load'
    saved_chest, loaded_chest = reference(saved_before, CHEST), reference(before, CHEST)
    assert ordered(saved_chest['inventory']) == ordered(loaded_chest['inventory']), 'Stock changed during load'
    assert {key: value for key, value in saved_chest.items() if key != 'inventory'} == {
        key: value for key, value in loaded_chest.items() if key != 'inventory'}, 'Chest state changed during load'
    assert saved_before['next_dynamic_serial'] == before['next_dynamic_serial']
    assert saved_before['quests'] == before['quests']
    assert saved_before['native_death_counts'] == before['native_death_counts']
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert before['next_dynamic_serial'] == after['next_dynamic_serial'] == 7
    assert before['player']['actor_values']['health.current'] == after['player']['actor_values']['health.current'] == 110
    assert before['player'].get('awarded_skill_books', []) == after['player'].get('awarded_skill_books', [])
    assert before['quests'] == after['quests']
    assert before['native_death_counts'] == after['native_death_counts']
    log = (root / 'process.log').read_text()
    trades = re.findall(r'TES4 merchant trade committed: merchant=' + re.escape(SAM)
                        + r' chest=' + re.escape(CHEST) + r' item=' + re.escape(PICK)
                        + r' count=1 gold=([0-9]+) buying=(true|false)', log)
    sold = [int(price) for price, buying in trades if buying == 'false']
    bought = [int(price) for price, buying in trades if buying == 'true']
    assert len(sold) == (args.sales if args.phase == 'sell' else 0)
    assert len(bought) == int(args.phase == 'buy')
    assert all(price > 0 for price in sold + bought)
    assert log.count('TES4 merchant trade committed:') == len(trades)
    player_before, player_after = before['player']['inventory'], after['player']['inventory']
    expected = copy.deepcopy(player_before)
    picks = next(item for item in expected if item['base'] == PICK)
    assert picks['owner'] == 'null' and picks['script_owner'] is None and picks['bound_source'] is None
    picks['count'] += len(bought) - len(sold)
    assert picks['count'] > 0
    gold_after = count(player_before, GOLD) + sum(sold) - sum(bought)
    assert gold_after >= 0
    expected = [item for item in expected if item['base'] != GOLD]
    if gold_after:
        expected.append(ordinary_gold(gold_after))
    assert ordered(expected) == ordered(player_after), 'Player item/metadata or purse discrepancy'
    chest_before, chest_after = reference(before, CHEST), reference(after, CHEST)
    assert {key: value for key, value in chest_before.items() if key != 'inventory'} == {
        key: value for key, value in chest_after.items() if key != 'inventory'}
    assert chest_after['owner'] == OWNER and chest_after['lock_level'] == 100
    expected_stock = copy.deepcopy(chest_before['inventory'])
    if args.phase == 'open':
        for item in expected_stock:
            assert item['owner'] in ('null', OWNER)
            item['owner'] = OWNER
    stock = next(item for item in expected_stock if item['base'] == PICK)
    stock['count'] += len(sold) - len(bought)
    assert ordered(expected_stock) == ordered(chest_after['inventory']), 'Merchant stock/metadata discrepancy'
    assert count(chest_after['inventory'], GOLD) == 0
    assert reference(before, SAM)['inventory'] == reference(after, SAM)['inventory']
    skills_before, skills_after = shared(source), shared(target)
    for index, (old, new) in enumerate(zip(skills_before, skills_after, strict=True)):
        assert old['base'] == new['base']
        expected_xp = old['experience'] + (len(sold) * .4 if index == 24 else 0)
        assert math.isclose(new['experience'], expected_xp, abs_tol=1e-4), (index, old, new, expected_xp)
    if args.sanitized:
        environment = read(root / 'observed-sanitizer-environment.json')['environment']
        assert environment['ASAN_OPTIONS'] == 'detect_leaks=0:halt_on_error=1'
        assert environment['UBSAN_OPTIONS'] == 'halt_on_error=1:print_stacktrace=1'
        libraries = read(root / 'loaded-libraries.json')
        assert any('libasan' in path for path in libraries) and any('libubsan' in path for path in libraries)
    result = dict(passed=True, source_inventory_loaded_exactly=True, phase=args.phase, sanitized=args.sanitized,
                  source_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch['exe_sha256'],
                  process=epoch, sales=sold, purchases=bought,
                  player_lockpicks=count(player_after, PICK), player_gold=gold_after,
                  merchant_lockpicks=count(chest_after['inventory'], PICK),
                  mercantile_before=skills_before[24], mercantile_after=skills_after[24])
    (root / 'trade-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
