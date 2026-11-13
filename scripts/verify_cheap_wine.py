"""Audit original Cheap Wine consumption, active reload, expiry and completed reload."""
import argparse
import copy
import hashlib
import json
import math
import re
from pathlib import Path

import tes4_runtime_state as runtime
from verify_blade_use import shared
import verify_nirnroot_pickup as nirnroot

PLAYER = 'dynamic:player:0000000000000001'
WINE = 'content:oblivion.esm:037f7f'
REFERENCE = 'content:oblivion.esm:06752a'
MODIFIERS = [{'attribute': 3, 'delta': -5}, {'attribute': 1, 'delta': -5}, {'attribute': 6, 'delta': -10}]


def read(path):
    return json.loads(path.read_text())


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def actor(state):
    return next(v for v in state['native_actor_values'] if v['actor'] == PLAYER)


def count(state):
    return sum(i['count'] for i in state['player']['inventory'] if i['base'] == WINE)


def inventory(state):
    return sorted(json.dumps(i, sort_keys=True) for i in state['player']['inventory']
                  if i['count'] and i['base'] != WINE)


def samples(text):
    pattern = (r'WineUse sample stock=(\d+) health=([\d.eE+-]+) magicka=([\d.eE+-]+) '
               r'fatigue=([\d.eE+-]+) fatigue_max=([\d.eE+-]+) intelligence=([\d.eE+-]+) '
               r'agility=([\d.eE+-]+) personality=([\d.eE+-]+) paused=(true|false)')
    return [dict(stock=int(v[0]), health=float(v[1]), magicka=float(v[2]), fatigue=float(v[3]),
                 maximum=float(v[4]), intelligence=float(v[5]), agility=float(v[6]),
                 personality=float(v[7]), paused=v[8] == 'true') for v in re.findall(pattern, text)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('course', type=Path)
    parser.add_argument('phase', choices=['baseline', 'use', 'active', 'expiry', 'complete'])
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    p = args.course
    driver = read(p / 'driver-process.json')
    assert driver['exit_code'] == 0 and driver['captured']
    assert driver['input_unchanged'] and driver['manifest_input_unchanged']
    assert read(p / 'scenario.json')['passed']
    source = Path(driver['input_save'])
    save = p / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(source) == driver['input_sha256']
    before, after = runtime.load_save(source), runtime.load_save(save)
    assert after['schema_version'] == 101
    assert runtime.encode_payload(after) == runtime._find_runtime_record(save.read_bytes())[3]
    b, a = actor(before), actor(after)
    assert shared(source) == shared(save), 'Potion use and an air miss must not train any skill'
    assert inventory(before) == inventory(after), 'Unrelated complete item metadata changed'
    assert b['passive_abilities'] == a['passive_abilities']
    assert b['values'][64] == a['values'][64] == [0, 0, 0, 0]
    # The earned expedition still owns one scripted Nirnroot and its quest progress.
    assert nirnroot.items(before) == nirnroot.items(after)
    assert len(nirnroot.items(after)) == 1
    for state in [before, after]:
        item = nirnroot.items(state)[0]
        assert item['count'] == 1
        script = nirnroot.instance(state, item['script_owner'], 'content:oblivion.esm:0eed76')
        assert [v['value'] for v in script['locals']] == [1]
        assert next(q for q in state['quests'] if q['quest'] == nirnroot.QUEST)['stage'] == 10
        quest = nirnroot.instance(state, nirnroot.QUEST, 'content:oblivion.esm:04e93b')
        assert [v['value'] for v in quest['locals'][1:3]] == [2, 2]
        assert state['player']['actor_values']['pc_misc_stat.27'] == 2
    assert before['player']['character_generation'] == after['player']['character_generation']
    credited = [v for v in before['native_actor_life'] if v['phase'] == 1 and v['killer'] == PLAYER]
    lives = {v['actor']: v for v in after['native_actor_life']}
    assert all(lives[v['actor']]['phase'] == 1 and lives[v['actor']]['killer'] == PLAYER for v in credited)
    assert before['player']['cell'] == after['player']['cell']
    assert math.dist(before['player']['position'][:3], after['player']['position'][:3]) < 200
    assert all(b['values'][i] == a['values'][i] for i in range(len(b['values']))
               if i not in [1, 3, 6, 9, 10])
    for state in [before, after]:
        assert next(v for v in state['native_actor_life'] if v['actor'] == PLAYER)['phase'] == 0
        assert math.isclose(sum(v or 0 for v in actor(state)['values'][8]), 110, abs_tol=.01)
    reference = next(v for v in after['references'] if v['key'] == REFERENCE)
    assert reference['deleted'] and reference['custom_state']['taken']
    log = (p / 'process.log').read_text()
    assert log.count('Writing saved game') == 1
    pre = log.split('Writing saved game', 1)[0]
    applied = 'M16 potion attributes applied: potion=' + WINE
    expired = 'M16 potion attributes expired: potion=' + WINE
    owned_before = b.get('potion_attribute_effects', [])
    owned_after = a.get('potion_attribute_effects', [])
    active = args.phase in ['use', 'active']
    assert len(owned_after) == int(active)
    if active:
        effect = owned_after[0]
        assert effect['potion'] == WINE and effect['duration'] == 120
        assert 0 < effect['elapsed'] < 120 and effect['modifiers'] == MODIFIERS
    if args.phase in ['baseline', 'use']:
        assert count(before) == 1 and not owned_before
        assert count(after) == (1 if args.phase == 'baseline' else 0)
        assert len(re.findall(r'M15 melee miss committed: actor=' + PLAYER + r' .*fatigue_cost=[1-9]', pre)) == 1
    else:
        assert count(before) == count(after) == 0
        assert applied not in pre and 'M15 melee miss committed: actor=' + PLAYER not in pre
    if args.phase == 'use':
        assert pre.count(applied) == 1 and expired not in pre
        match = re.search(re.escape(applied) + r' duration=120 modifiers=3 fatigue_amount=10 fatigue_restored=([\d.eE+-]+) count=1', pre)
        assert match and float(match[1]) > 2
        earlier, later = pre.split(applied, 1)
        observed_before = [s for s in samples(earlier) if s['paused'] and s['stock'] == 1][-1]
        observed_after = [s for s in samples(later) if s['paused'] and s['stock'] == 0][0]
        expected = copy.deepcopy(observed_before)
        expected.update(stock=0, intelligence=35, agility=25, personality=40, magicka=70,
                        maximum=165, fatigue=min(observed_before['fatigue'] + 10, observed_before['maximum']) - 5)
        for key in ['health', 'magicka', 'fatigue', 'maximum', 'intelligence', 'agility', 'personality']:
            assert math.isclose(observed_after[key], expected[key], abs_tol=.02), (key, observed_after, expected)
        # After menus close, only ordinary regeneration may reduce the deficit.
        assert sum(v or 0 for v in a['values'][10]) >= observed_after['fatigue'] - .02
    elif args.phase == 'baseline':
        assert applied not in pre and expired not in pre
    elif args.phase == 'active':
        assert len(owned_before) == 1 and expired not in pre
        assert owned_before[0]['potion'] == WINE and owned_before[0]['modifiers'] == MODIFIERS
        assert owned_after[0]['elapsed'] >= owned_before[0]['elapsed']
    elif args.phase == 'expiry':
        assert len(owned_before) == 1 and owned_before[0]['potion'] == WINE
        assert pre.count(expired) == 1 and 'elapsed=120 modifiers=3 dead=0' in pre
    else:
        assert not owned_before and expired not in pre
    for av, base, drain in [(1, 40, -5), (3, 30, -5), (6, 50, -10)]:
        assert a['values'][av] == [base, drain if active else 0, 0, 0]
    assert a['values'][9] == [70 if active else 80, 0, 0, 0]
    assert a['values'][10][:3] == [165 if active else 170, 0, 0]
    assert a['values'][10][3] <= 0
    assert not a.get('ingredient_restoration') and not a.get('scroll_chameleon')
    events = [json.loads(line) for line in (p / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in events if e['event'] == 'load-complete']
    saves = [e for e in events if e['event'] == 'save-complete']
    epoch = read(p / 'process-epoch.json')
    assert len(loads) == len(saves) == 1
    assert loads[0]['pid'] == saves[0]['pid'] == epoch['pid']
    assert loads[0]['save_sha256'] == sha(source) and saves[0]['save_sha256'] == sha(save)
    assert read(p / saves[0]['live']) == after
    if args.sanitized:
        env = read(p / 'observed-sanitizer-environment.json')['environment']
        assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
        libraries = read(p / 'loaded-libraries.json')
        assert any('libasan' in name for name in libraries) and any('libubsan' in name for name in libraries)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized, source_sha256=sha(source),
                  save_sha256=sha(save), binary_sha256=epoch['exe_sha256'], wine_before=count(before),
                  wine_after=count(after), effects_before=owned_before, effects_after=owned_after,
                  health=110, intelligence=a['values'][1], agility=a['values'][3], personality=a['values'][6],
                  magicka=a['values'][9], fatigue=a['values'][10], all_skills_retained=True,
                  other_full_item_metadata_retained=True, nirnroot_quest_retained=True,
                  player_credited_deaths_retained=len(credited))
    (p / 'wine-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
