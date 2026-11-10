"""Read-only audit of original Ayleid Cask interaction and saved continuation.

Run from the checkout root with a captured course directory and its phase.
For attempts, reviewed-loot.json records the actual loot summary independently
reinspected in that course's screenshot, keyed by native content identity.
"""
import argparse
import collections
import hashlib
import json
import math
import re
import sys
from pathlib import Path
sys.path.insert(0, str(Path.cwd() / 'scripts'))
import tes4_runtime_state as runtime

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=('cancel', 'attempt', 'restart'))
    args = parser.parse_args()
    root, phase = args.course, args.phase
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    driver = json.loads((root / 'driver-process.json').read_text())
    source = Path(driver['input_save'])
    output = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert driver['captured'] and driver['input_unchanged'] and driver['exit_code'] == 0
    assert digest(source) == driver['input_sha256']
    scenario = json.loads((root / 'scenario.json').read_text())
    assert scenario['passed']
    before, after = runtime.load_save(source), runtime.load_save(output)
    assert before['schema_version'] == after['schema_version'] == 98
    assert runtime.encode_payload(after) == runtime._find_runtime_record(output.read_bytes())[3]
    events = [json.loads(x) for x in (root / 'events.jsonl').read_text().splitlines()]
    loaded = next(e for e in events if e['event'] == 'load-complete')
    saved = next(e for e in events if e['event'] == 'save-complete')
    assert loaded['save_sha256'] == digest(source) and saved['save_sha256'] == digest(output)
    assert json.loads((root / loaded['live']).read_text())['player']['inventory'] == before['player']['inventory']
    assert json.loads((root / saved['live']).read_text()) == after
    epoch = json.loads((root / 'process-epoch.json').read_text())
    assert loaded['pid'] == saved['pid'] == epoch['pid']
    assert after['next_dynamic_serial'] == before['next_dynamic_serial']
    assert after['native_death_counts'] == before['native_death_counts']
    assert after['disposition_modifiers'] == before['disposition_modifiers']
    player = 'dynamic:player:0000000000000001'
    values = next(a for a in after['native_actor_values'] if a['actor'] == player)['values']
    health = after['player']['actor_values']['health.current']
    assert health == before['player']['actor_values']['health.current'] and health > 0
    assert math.isclose(sum(v or 0 for v in values[8]), health, abs_tol=.002)
    assert after['player']['position'][:3] == before['player']['position'][:3]
    cask = lambda state: next(x for x in state['references'] if x['key'] == 'content:oblivion.esm:066e79')
    old, new = cask(before), cask(after)
    assert old['base'] == new['base'] == 'content:oblivion.esm:03767e'
    inventory = lambda state: state['player']['inventory']
    def totals(state):
        result = collections.Counter()
        for i in inventory(state): result[i['base']] += i['count']
        return result
    picks = 'content:oblivion.esm:00000a'
    log = (root / 'process.log').read_text().split("Writing saved game 'Quicksave'", 1)[0]
    failures = log.count('result=lockpick_failed')
    attempts = new['custom_state'].get('lockpick_auto_attempts', 0) - old['custom_state'].get('lockpick_auto_attempts', 0)
    pause = None
    if phase == 'cancel':
        assert inventory(after) == inventory(before)
        assert old['inventory'] == new['inventory'] and new['custom_state']['locked']
        assert attempts == 0 and failures == 0
        assert log.count('result=lockpick_prompt') == log.count('result=lockpick_cancelled') == 1
        assert 'result=looted' not in log
    elif phase == 'attempt':
        assert attempts == failures + 1 and attempts >= 1
        assert new['custom_state']['unlocked_with_lockpick'] and not new['custom_state']['locked']
        assert new['custom_state']['opened'] and not new['inventory']
        assert log.count('result=looted') == 1 and 'result=lockpick_cancelled' not in log
        loot = json.loads((root / 'reviewed-loot.json').read_text())
        assert isinstance(loot, dict) and loot
        assert all(isinstance(k, str) and type(v) is int and v > 0 for k,v in loot.items())
        expected = totals(before) + collections.Counter(loot)
        expected[picks] -= failures
        assert totals(after) == +expected, 'Saved inventory must match reviewed actual loot minus broken picks'
        assert log.count('result=lockpick_prompt') == attempts
        # Existing equipped items, durability and independent inventory-script owners survive.
        for item in inventory(before):
            if item['base'] == picks: continue
            shape = {k:v for k,v in item.items() if k != 'count'}
            matches = [x for x in inventory(after) if {k:v for k,v in x.items() if k != 'count'} == shape]
            assert sum(x['count'] for x in matches) >= item['count']
        assert totals(after) - totals(before), 'The genuine cask must yield actual loot'
    elif phase == 'restart':
        assert inventory(after) == inventory(before) and new == old
        assert attempts == failures == 0 and 'result=lockpick_prompt' not in log
    else:
        raise ValueError(phase)
    if phase in ('cancel', 'attempt'):
        lines = log.splitlines()
        start = next(i for i,x in enumerate(lines) if 'result=lockpick_prompt' in x)
        finish = next(i for i in range(start+1,len(lines)) if any(k in lines[i] for k in ('result=lockpick_cancelled','result=lockpick_failed','result=looted')))
        sample = re.compile(r'VilverinContinue sample sim=([\d.e+-]+) health=([\d.e+-]+) game=([\d.e+-]+)')
        earlier = [sample.search(x) for x in lines[:start] if sample.search(x)][-1]
        later = next(sample.search(x) for x in lines[finish:] if sample.search(x))
        sim = float(later[1]) - float(earlier[1])
        assert 0 <= sim <= .6, ('World advanced during paused menu', sim)
        assert float(later[2]) == float(earlier[2])
        # The driver held the actual menu for eight wall seconds before any choice.
        held = next(x for x in scenario['actions'] if x['type'] == 'sleep' and x.get('duration_seconds',0) >= 8)
        pause = dict(wall_seconds=held['duration_seconds'],simulated_seconds_including_boundary_frames=sim)
    report = dict(passed=True,phase=phase,source_sha256=digest(source),save_sha256=digest(output),
                  schema=98,health=health,picks_before=totals(before)[picks],picks_after=totals(after)[picks],
                  attempts=attempts,failures=failures,pause=pause,
                  loot_delta=dict(totals(after)-totals(before)),process=epoch,
                  cask_locked=new['custom_state'].get('locked'),source_unchanged=True)
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))

if __name__ == '__main__':
    main()
