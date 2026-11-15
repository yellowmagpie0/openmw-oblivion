"""Audit original scriptless Rice across repeated world saves, pickup and inventory reload."""
import hashlib
import json
import sys
from pathlib import Path

import tes4_runtime_state as runtime
from verify_blade_use import shared
from verify_skill_book_read import shared as book_progress

BASE = 'content:oblivion.esm:033690'
NIRNROOT = 'content:oblivion.esm:04e940'
SCRIPT = 'content:oblivion.esm:0eed76@oblivion.esm/object/unit=0'
QUEST = 'content:oblivion.esm:04e936'
PLAYER = 'dynamic:player:0000000000000001'
# Exact metadata audited in the retained, hash-bound Wine continuation.
ITEM_METADATA = dict(base=BASE, count=1, condition=-1.0, charge=-1.0,
                     equipped_slots=0, hotkey=-1, owner='null', remaining_usage_time=-1.0,
                     ownership_rank=None, ownership_global='null', extra_scale=None, bound_source=None)


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    root, phase = Path(sys.argv[1]), sys.argv[2]
    assert phase in ['drop', 'world', 'pickup', 'inventory']
    driver = read(root / 'driver-process.json')
    assert driver['captured'] and driver['exit_code'] == 0
    assert driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert read(root / 'scenario.json')['passed']
    assert sha(root / 'captured-input-manifest.json') == driver['manifest_input_sha256']
    source = Path(driver['input_save'])
    target = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert before['schema_version'] == after['schema_version'] == 101
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert shared(source) == shared(target) and book_progress(source) == book_progress(target)
    assert before['player']['character_generation'] == after['player']['character_generation']
    assert before['player'].get('awarded_skill_books', []) == after['player'].get('awarded_skill_books', [])
    for state in [before, after]:
        assert state['player']['actor_values']['health.current'] == 110
        assert state['player']['actor_values']['pc_misc_stat.27'] == 2
        assert next(q for q in state['quests'] if q['quest'] == QUEST)['stage'] == 10
        quest = next(i for i in state['script_instances'] if i['context'] == QUEST and i['unit'].startswith('content:oblivion.esm:04e93b@'))
        assert [v['value'] for v in quest['locals'][1:3]] == [2, 2]
    other_items = lambda s: [i for i in s['player']['inventory'] if i['base'] != BASE and i['count']]
    assert other_items(before) == other_items(after)
    before_items = [i for i in before['player']['inventory'] if i['base'] == BASE and i['count']]
    after_items = [i for i in after['player']['inventory'] if i['base'] == BASE and i['count']]
    references = [r for r in after['references'] if r['base'] == BASE and r['key'].startswith('dynamic:')]
    live = [r for r in references if not r['deleted'] and r['custom_state'].get('count', 1) > 0]
    scripts = [i for i in after['script_instances'] if i['unit'] == SCRIPT]
    assert len(scripts) == 1 and scripts[0]['locals'] == [{'type': 'number', 'value': 1}]
    assert scripts[0]['context'] == 'dynamic:inventory-script:0000000000000004'
    assert scripts == [i for i in before['script_instances'] if i['unit'] == SCRIPT]
    assert not scripts[0]['on_load_fired']
    for item in before_items + after_items:
        assert item == dict(ITEM_METADATA, script_owner=None)
    if phase in ['drop', 'world']:
        assert not after_items and len(live) == 1
        world = live[0]
        assert world['enabled'] and not world['inventory']
        assert world['owner'] is None and world['ownership_global'] == 'null'
        assert world['ownership_rank'] is None and world['item_condition'] is None and world['item_charge'] is None
        assert world['custom_state']['count'] == 1 and world['custom_state']['scale'] == 1
        assert not any(i['context'] == world['key'] for i in after['script_instances'])
        if phase == 'drop':
            assert len(before_items) == 1 and before_items[0]['count'] == 1
            assert world['key'] == f"dynamic:native-reference:{before['next_dynamic_serial']:016x}"
        else:
            assert references == [r for r in before['references'] if r['base'] == BASE and r['key'].startswith('dynamic:')]
    else:
        assert len(after_items) == 1 and after_items[0]['count'] == 1 and not live
        assert after_items[0]['script_owner'] is None
        if phase == 'pickup':
            assert not before_items
            assert len([r for r in before['references'] if r['base'] == BASE and r['key'].startswith('dynamic:') and r['custom_state'].get('count', 1) > 0]) == 1
        else:
            assert before_items == after_items
    assert after['next_dynamic_serial'] == before['next_dynamic_serial'] + (phase == 'drop')
    bp = next(a for a in before['native_actor_values'] if a['actor'] == PLAYER)
    ap = next(a for a in after['native_actor_values'] if a['actor'] == PLAYER)
    assert bp['passive_abilities'] == ap['passive_abilities'] and bp['used_powers'] == ap['used_powers']
    assert bp['values'] == ap['values']
    lives = {a['actor']: a for a in after['native_actor_life']}
    credited = [a for a in before['native_actor_life'] if a['phase'] == 1 and a['killer'] == PLAYER]
    assert len(credited) == 33 and all(lives[a['actor']]['phase'] == 1 and lives[a['actor']]['killer'] == PLAYER for a in credited)
    assert lives[PLAYER]['phase'] == 0
    epoch = read(root / 'process-epoch.json')
    events = [json.loads(line) for line in (root / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in events if e['event'] == 'load-complete']
    saves = [e for e in events if e['event'] == 'save-complete']
    assert len(loads) == len(saves) == 1
    assert all(e['pid'] == epoch['pid'] and e['epoch'] == 1 for e in loads + saves)
    loaded = read(root / loads[0]['live'])
    assert loaded['player']['inventory'] == before['player']['inventory']
    dynamic_rice = lambda s: [r for r in s['references'] if r['base'] == BASE and r['key'].startswith('dynamic:')]
    assert dynamic_rice(loaded) == dynamic_rice(before)
    assert [i for i in loaded['script_instances'] if i['unit'] == SCRIPT] == scripts
    assert [i for i in after['player']['inventory'] if i['base'] == NIRNROOT] == [i for i in before['player']['inventory'] if i['base'] == NIRNROOT]
    assert read(root / saves[0]['live'])['player']['inventory'] == after['player']['inventory']
    result = dict(phase=phase, source_sha256=sha(source), save_sha256=sha(target),
                  process=epoch, next_dynamic_serial=after['next_dynamic_serial'],
                  inventory=after_items, world=references, nirnroot_script=scripts[0], credited_deaths=33)
    (root / 'plain-ingredient-drop-verification.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(phase=phase, passed=True, save_sha256=sha(target))))


if __name__ == '__main__':
    main()
