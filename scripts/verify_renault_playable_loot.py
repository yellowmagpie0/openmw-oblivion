"""Audit actual Renault Take All and fresh-process continuation saves."""
import collections
import hashlib
import json
from pathlib import Path
import re
import sys

import tes4_runtime_state as io

CORPSE = 'content:oblivion.esm:032a15'
RAT_VICTIM = 'content:oblivion.esm:026be1'
VICTIM_BASE = 'content:oblivion.esm:027338'
NATURAL_CALLBACK = ('6', RAT_VICTIM, 'content:oblivion.esm:037e40')
HIDDEN = {'content:oblivion.esm:' + value for value in ['022f71', '022f70', '022f6f', '022f65', '022f20']}
FIELDS = ['base', 'condition', 'charge', 'owner', 'remaining_usage_time', 'ownership_rank',
          'ownership_global', 'extra_scale', 'bound_source', 'equipped_slots', 'script_owner', 'weapon_poison']


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def counts(items):
    result = collections.Counter()
    for item in items:
        result[json.dumps({key: item.get(key) for key in FIELDS}, sort_keys=True)] += item['count']
    return result


def deaths(state):
    return {row['base']: row['count'] for row in state['native_death_counts']}


def audit_case(case, phase):
    driver = json.loads((case / 'driver-process.json').read_text())
    assert driver['exit_code'] == 0 and driver['captured']
    assert driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert json.loads((case / 'scenario.json').read_text())['passed']
    source = Path(driver['input_save'])
    save = case / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    before, after = io.load_save(source), io.load_save(save)
    old = next(row for row in before['references'] if row['key'] == CORPSE)
    new = next(row for row in after['references'] if row['key'] == CORPSE)
    assert old['base'] == new['base'] and new['enabled'] and not new['deleted']
    assert not new['inventory'] and after['native_player_furniture'] is None
    old_deaths, new_deaths = deaths(before), deaths(after)
    log = (case / 'process.log').read_text(errors='replace')
    new_natural_death = phase == 'loot' and old_deaths.get(VICTIM_BASE, 0) == 0
    if phase == 'loot':
        expected_deaths = dict(old_deaths)
        expected_deaths[VICTIM_BASE] = 1
        assert new_deaths == expected_deaths
        callbacks = re.findall(r'M15 native death callback: id=(\d+) actor=(\S+) killer=(\S+)', log)
        assert callbacks == ([NATURAL_CALLBACK] if new_natural_death else [])
    else:
        assert old_deaths == new_deaths and 'M15 native death callback:' not in log
    for row in before['native_actor_life']:
        if new_natural_death and row['actor'] == RAT_VICTIM:
            continue
        assert next(value for value in after['native_actor_life'] if value['actor'] == row['actor']) == row
    player_life = next(row for row in after['native_actor_life']
                       if row['actor'] == 'dynamic:player:0000000000000001')
    assert player_life['phase'] == 0
    expected_items = counts(before['player']['inventory'])
    if phase == 'loot':
        expected_items += counts([item for item in old['inventory'] if item['base'] not in HIDDEN])
        assert len(old['inventory']) == 8
        assert sum(bool(item['equipped_slots']) for item in old['inventory']) == 5
        assert not any(item['base'] in HIDDEN for item in after['player']['inventory'])
        original_bases = {item['base'] for item in old['inventory']}
        assert all(item['equipped_slots'] == 0 for item in after['player']['inventory']
                   if item['base'] in original_bases)
    else:
        assert before['player']['inventory'] == after['player']['inventory']
    assert counts(after['player']['inventory']) == expected_items
    result = {
        'case': str(case), 'phase': phase, 'passed': True,
        'input_sha256': sha(source), 'save_sha256': sha(save),
        'process': json.loads((case / 'process-epoch.json').read_text()),
        'transferred_original_stacks': 3 if phase == 'loot' else 0,
        'exact_fields': FIELDS, 'previous_death_counts_unchanged': True,
        'declared_new_natural_death': {
            'actor': RAT_VICTIM, 'killer': NATURAL_CALLBACK[2], 'callback_id': 6
        } if new_natural_death else None
    }
    (case / 'audit.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def audit(root, mode, attempt):
    rows = [audit_case(root / f'{mode}-nonplayable-{phase}-{attempt}', phase) for phase in ['loot', 'cold']]
    assert rows[0]['save_sha256'] == rows[1]['input_sha256']
    assert rows[0]['process']['pid'] != rows[1]['process']['pid']
    assert rows[0]['process']['exe_sha256'] == rows[1]['process']['exe_sha256']
    result = {
        'passed': True, 'phases': rows,
        'scope': 'Ordinary-input original Renault loot: three playable stacks transferred with exact fields, '
                 'five hidden armor stacks discarded, cold continuation without replay. UI/appearance separately '
                 'reviewed. SAN uses the independently audited ordinary-input preloot checkpoint.'
    }
    (root / (mode + '-nonplayable-chain.json')).write_text(json.dumps(result, indent=2) + '\n')
    return rows[-1]['save_sha256']


if __name__ == '__main__':
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('build/oblivion-compat/nonplayable-armor')
    mode = sys.argv[2] if len(sys.argv) > 2 else 'normal'
    attempt = sys.argv[3] if len(sys.argv) > 3 else '01'
    print(json.dumps({'passed': True, 'save_sha256': audit(root, mode, attempt)}))
