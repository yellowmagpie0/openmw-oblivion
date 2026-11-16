"""Audit the ordinary-input approach save used by the instrumented Renault course."""
import hashlib
import json
from pathlib import Path
import re
import sys

import tes4_runtime_state as io


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def audit(root):
    case = root / 'normal-preloot-checkpoint-01'
    driver = json.loads((case / 'driver-process.json').read_text())
    assert driver['exit_code'] == 0 and driver['captured']
    assert driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert json.loads((case / 'scenario.json').read_text())['passed']
    source = Path(driver['input_save'])
    assert sha(source) == 'e4f009a4536399cf294c953922e509577e9fe2fb69d8c709072a8d20d53832f9'
    save = case / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    before, after = io.load_save(source), io.load_save(save)
    key = 'content:oblivion.esm:032a15'
    corpse = lambda state: next(row for row in state['references'] if row['key'] == key)
    old, new = corpse(before), corpse(after)
    assert old['base'] == new['base'] == 'content:oblivion.esm:02349f'
    assert new['enabled'] and not new['deleted']
    assert len(old['inventory']) == 8
    assert sum(bool(item['equipped_slots']) for item in old['inventory']) == 5
    # v84 omits newer absent-owner fields; v106 explicitly records null defaults.
    def same_items(left, right):
        keys = {key for item in left + right for key in item}
        normalize = lambda items: [{key: item.get(key) for key in keys} for item in items]
        return normalize(left) == normalize(right)
    assert same_items(old['inventory'], new['inventory'])
    assert same_items(before['player']['inventory'], after['player']['inventory'])
    assert before.get('native_player_furniture') is after.get('native_player_furniture') is None
    deaths = lambda state: {row['base']: row['count'] for row in state['native_death_counts']}
    expected = deaths(before)
    expected['content:oblivion.esm:027338'] = 1
    assert deaths(after) == expected
    log = (case / 'process.log').read_text(errors='replace')
    callbacks = re.findall(r'M15 native death callback: id=(\d+) actor=(\S+) killer=(\S+)', log)
    assert callbacks == [('6', 'content:oblivion.esm:026be1', 'content:oblivion.esm:037e40')]
    for row in before['native_actor_life']:
        if row['actor'] != 'content:oblivion.esm:026be1':
            assert next(v for v in after['native_actor_life'] if v['actor'] == row['actor']) == row
    assert 'MergeRenault corpse loot facing' in log and 'Writing saved game' in log
    assert 'MergeRenault corpse loot transferred' not in log
    result = {
        'passed': True, 'case': str(case), 'source': str(source),
        'source_sha256': sha(source), 'save': str(save), 'save_sha256': sha(save),
        'original_corpse_stacks': 8, 'original_equipped_stacks': 5,
        'full_player_and_corpse_inventories_exact': True,
        'schema_normalization': 'Absent v84 inventory fields equal explicit null defaults in current schema; every non-null field and stack order remain exact.',
        'declared_natural_death': {'actor': callbacks[0][1], 'killer': callbacks[0][2], 'callback': 6},
        'process': json.loads((case / 'process-epoch.json').read_text()),
        'scope': 'Actual mouse/walking approach and F5 before looting; no grants, teleports or actor edits. Other native quest/time progression is allowed. This checkpoint supplies the composite SAN Take All source.'
    }
    (root / 'preloot-checkpoint-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


if __name__ == '__main__':
    print(json.dumps(audit(Path(sys.argv[1]))))
