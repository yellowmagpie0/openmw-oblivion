"""Inspect genuine ordinary-input travel to the original Cairn Bolete plant."""
import hashlib, json, math, sys
from pathlib import Path
import tes4_runtime_state as runtime
from verify_blade_use import shared
from verify_skill_book_read import shared as book_progress
PLAYER = 'dynamic:player:0000000000000001'
CAP = 'content:oblivion.esm:06251e'
REF = 'content:oblivion.esm:06a060'

def read(p):
    return json.loads(p.read_text())

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p = Path(sys.argv[1])
    phase = sys.argv[2]
    selected_wellspring = sys.argv[3] if len(sys.argv) > 3 else '06a51a'
    assert selected_wellspring in ['06a51a', '06a535', '06cae9', '099b6f', '099b6e']
    assert phase in ['approach', 'harvest', 'harvest-second', 'sewer-approach', 'staging-prefix', 'door-staging-prefix', 'sewer-enter', 'rooms', 'room-staging-prefix', 'gate-staging-prefix', 'gate-open', 'wellspring-staging', 'wellspring-approach', 'wellspring-harvest', 'fourth-staging', 'wellspring-door-approach', 'wellspring-enter']
    d = read(p / 'driver-process.json')
    assert d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged']
    if phase in ['staging-prefix', 'door-staging-prefix', 'room-staging-prefix', 'gate-staging-prefix', 'wellspring-staging', 'fourth-staging']:
        assert d['exit_code'] != 0 and not read(p / 'scenario.json')['passed']
        manual = read(p / 'manual-staging-save.json')
        data = (p / 'sdl-input.txt').read_bytes().splitlines(keepends=True)
        appended = manual['appended_input'].encode()
        first = int(appended.split()[0]) - 1
        prefix = b''.join(data[:first])
        assert hashlib.sha256(prefix).hexdigest() == manual['input_prefix_sha256']
        assert b''.join(data[first:first + len(appended.splitlines())]) == appended
        assert hashlib.sha256(prefix + appended).hexdigest() == manual['input_after_sha256']
        expected_input = [['up', '26'], ['up', '225']]
        if phase == 'room-staging-prefix':
            assert manual['ordinary_portal_teleported']
            expected_input += [['down', '44'], ['up', '44']]
        expected_input += [['down', '62'], ['up', '62']]
        assert [line.split()[1:] for line in appended.decode().splitlines()] == expected_input
    else:
        assert d['exit_code'] == 0 and read(p / 'scenario.json')['passed']
    src = Path(d['input_save'])
    save = p / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(src) == d['input_sha256']
    b, a = (runtime.load_save(src), runtime.load_save(save))
    bs, ass = (shared(src), shared(save))
    assert a['schema_version'] == 101 and runtime.encode_payload(a) == runtime._find_runtime_record(save.read_bytes())[3]
    actor = lambda s: next((v for v in s['native_actor_values'] if v['actor'] == PLAYER))
    bp, pa = (actor(b), actor(a))
    assert all((bp['values'][i] == pa['values'][i] for i in range(len(bp['values'])) if i not in ([8, 10, 13, 26] if phase == 'wellspring-approach' else [10, 13, 26])))
    assert bp['passive_abilities'] == pa['passive_abilities']
    assert b['player']['character_generation'] == a['player']['character_generation']
    assert b['player']['awarded_skill_books'] == a['player']['awarded_skill_books']
    previous_book = book_progress(src)
    expected_book = dict(previous_book, attributes=list(previous_book['attributes']),
                         specializations=list(previous_book['specializations']))
    athletics_levels = int(ass[8]['base'] - bs[8]['base'])
    acrobatics_levels = int(ass[20]['base'] - bs[20]['base'])
    # This retained character has major Athletics and minor Acrobatics.
    # Both govern Speed; their specializations are Combat and Stealth.
    expected_book['level_progress'] += athletics_levels
    expected_book['attributes'][4] += athletics_levels + acrobatics_levels
    expected_book['specializations'][0] += athletics_levels
    expected_book['specializations'][2] += acrobatics_levels
    assert book_progress(save) == expected_book
    credited = [v for v in b['native_actor_life'] if v['phase'] == 1 and v['killer'] == PLAYER]
    lives = {v['actor']: v for v in a['native_actor_life']}
    assert all(lives[v['actor']]['phase'] == 1 and lives[v['actor']]['killer'] == PLAYER for v in credited)
    if phase in ['sewer-approach', 'staging-prefix', 'door-staging-prefix', 'sewer-enter', 'rooms', 'room-staging-prefix', 'gate-staging-prefix', 'gate-open', 'wellspring-staging', 'wellspring-approach', 'wellspring-harvest', 'fourth-staging', 'wellspring-door-approach', 'wellspring-enter']:
        for plant in b['references']:
            if plant.get('custom_state', {}).get('harvested'):
                assert next(v for v in a['references'] if v['key'] == plant['key']) == plant
    assert all((bs[i] == ass[i] for i in range(27) if i not in ([3, 8, 20] if phase == 'wellspring-approach' else [8, 20])))
    assert ass[8]['base'] >= bs[8]['base'] and ass[20]['base'] >= bs[20]['base']
    assert pa['values'][13][0] == ass[8]['base'] and pa['values'][26][0] == ass[20]['base']
    assert ass[8]['base'] > bs[8]['base'] or ass[8]['experience'] >= bs[8]['experience']
    assert ass[20]['base'] > bs[20]['base'] or ass[20]['experience'] >= bs[20]['experience']
    for s in ([b] if phase == 'wellspring-approach' else [b, a]):
        assert next((v for v in s['native_actor_life'] if v['actor'] == PLAYER))['phase'] == 0 and math.isclose(sum((v or 0 for v in actor(s)['values'][8])), (sum(bp['values'][8]) if phase in ['wellspring-harvest', 'fourth-staging', 'wellspring-door-approach', 'wellspring-enter'] else 83), abs_tol=0.01)
    count = lambda s: sum((i['count'] for i in s['player']['inventory'] if i['base'] == CAP))
    normalize = lambda s: sorted((json.dumps(i, sort_keys=True) for i in s['player']['inventory'] if i['count'] and i['base'] != CAP))
    if phase == 'wellspring-approach':
        log = (p / 'process.log').read_text()
        import re, copy
        hits = re.findall(r'M15 melee hit committed: actor=content:oblivion.esm:06e3f8 .*health_damage=(\S+) .*victim=dynamic:player:0000000000000001 .*armor_condition_writes=1', log)
        assert len(hits) == 2 and all(math.isclose(float(v), 7.76, abs_tol=.001) for v in hits)
        assert math.isclose(sum(pa['values'][8]), 67.48, abs_tol=.01)
        assert pa['values'][8][:3] == bp['values'][8][:3]
        assert ass[3] == dict(bs[3], experience=bs[3]['experience'] + 2.5)
        adjusted = copy.deepcopy(b)
        cuirass = next(i for i in adjusted['player']['inventory'] if i['base'] == 'content:oblivion.esm:0661c3')
        after_cuirass = next(i for i in a['player']['inventory'] if i['base'] == cuirass['base'])
        assert math.isclose(cuirass['condition'] - after_cuirass['condition'], 4.32, abs_tol=.001)
        cuirass['condition'] = after_cuirass['condition']
        assert normalize(adjusted) == normalize(a)
    else:
        assert normalize(b) == normalize(a)
    assert count(b) == 0
    if phase in ['harvest', 'harvest-second', 'wellspring-harvest']:
        assert count(a) in [0, 1]
        selected = ('content:oblivion.esm:' + selected_wellspring if phase == 'wellspring-harvest' else REF if phase == 'harvest' else 'content:oblivion.esm:06a05f')
        base = ('062517' if selected_wellspring == '099b6e' else '062514') if phase == 'wellspring-harvest' else '062513' if phase == 'harvest' else '062517'
        plant = next(v for v in a['references'] if v['key'] == selected)
        if phase == 'harvest-second':
            assert next(v for v in b['references'] if v['key'] == REF) == next(v for v in a['references'] if v['key'] == REF)
        assert plant['custom_state']['harvested'] and plant['custom_state']['harvest_chance'] == 33
        expected = 'harvested' if count(a) else 'harvest_failed'
        assert f'M5 interaction: kind=harvest result={expected} ref={selected} base=content:oblivion.esm:{base}' in (p / 'process.log').read_text()
        if count(a):
            cap = next(i for i in a['player']['inventory'] if i['base'] == CAP and i['count'])
            assert cap == dict(base=CAP, count=1, condition=-1, charge=-1,
                              equipped_slots=0, hotkey=-1, owner='null', remaining_usage_time=-1,
                              ownership_rank=None, ownership_global='null', extra_scale=None,
                              bound_source=None, script_owner=None)
    else:
        assert count(a) == 0
    pos = a['player']['position']
    if phase in ['sewer-enter', 'rooms', 'room-staging-prefix', 'gate-staging-prefix', 'gate-open', 'wellspring-enter']:
        expected_cell = ('content:oblivion.esm:0352a9' if phase == 'wellspring-enter' else 'content:oblivion.esm:0272f2' if phase == 'sewer-enter' else
                         'content:oblivion.esm:022282' if phase in ['room-staging-prefix', 'gate-staging-prefix', 'gate-open'] else
                         'content:oblivion.esm:0380bd')
        assert a['player']['cell'] == expected_cell
    destination = ([64485.90625, 60088.42578125] if phase == 'wellspring-door-approach' else [8.170623779296875, -20.691328048706055] if phase == 'wellspring-enter' else {'06a51a': [66060.2421875, 57621.80859375], '06a535': [65930.625, 56833.0390625], '06cae9': [68624.5390625, 47113.91015625], '099b6f': [-206.57440185546875, -1537.571533203125], '099b6e': [-571.6810302734375, -1962.499755859375]}[selected_wellspring] if phase in ['wellspring-harvest', 'fourth-staging', 'wellspring-door-approach', 'wellspring-enter'] else [65273.55078125, 59240.78515625] if phase == 'wellspring-approach' else
                   [59857, 83473] if phase == 'wellspring-staging' else
                   [3500, 9344] if phase == 'gate-open' else
                   [3798, 9335] if phase == 'gate-staging-prefix' else
                   [5120, 9344] if phase == 'room-staging-prefix' else
                   [3872, 2048] if phase == 'sewer-enter' else
                   [364.634765625, 714.0369873046875] if phase == 'rooms' else
                   [47770, 84394.8] if phase == 'staging-prefix' else
                   [47031.02734375, 82917.6328125] if phase in ['sewer-approach', 'door-staging-prefix'] else
                   [58828.2578125, 95994.859375])
    assert math.dist(pos[:2], destination) < (300 if phase in ['sewer-enter', 'room-staging-prefix'] else 180)
    log = (p / 'process.log').read_text()
    assert log.count('Writing saved game') == 1 and 'M16 potion attributes applied:' not in log
    if phase == 'gate-open':
        gate = next(v for v in a['references'] if v['key'] == 'content:oblivion.esm:03f1b3')
        assert not gate['custom_state']['locked'] and a['player']['position'][0] < 3600
        assert log.count('M5 door activation: result=animated ref=content:oblivion.esm:03f1b3') == 1
    portals = ['053fe0'] if phase == 'wellspring-enter' else ['00a395'] if phase == 'sewer-enter' else ['0666b7'] if phase == 'room-staging-prefix' else []
    if phase == 'rooms':
        portals = ['0666b7', '022812', '026c36']
        if b['player']['cell'] == 'content:oblivion.esm:022282': portals = portals[1:]
        elif b['player']['cell'] == 'content:oblivion.esm:022288': portals = portals[2:]
    for ref in portals:
        assert log.count('M5 door activation: result=teleport ref=content:oblivion.esm:' + ref + ' ') == 1
    es = [json.loads(l) for l in (p / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in es if e['event'] == 'load-complete']
    saves = [e for e in es if e['event'] == 'save-complete']
    epoch = read(p / 'process-epoch.json')
    assert len(loads) == len(saves) == 1 and loads[0]['pid'] == saves[0]['pid'] == epoch['pid']
    assert loads[0]['save_sha256'] == sha(src) and saves[0]['save_sha256'] == sha(save)
    assert read(p / saves[0]['live']) == a
    out = dict(passed=True, accepted_gameplay=phase not in ['staging-prefix', 'door-staging-prefix', 'room-staging-prefix', 'gate-staging-prefix', 'wellspring-staging', 'fourth-staging'], phase=phase, source_sha256=sha(src), save_sha256=sha(save), binary_sha256=epoch['exe_sha256'], health=sum(pa['values'][8]), position=pos, cairn_before=count(b), cairn_after=count(a), athletics_before=bs[8], athletics_after=ass[8], acrobatics_before=bs[20], acrobatics_after=ass[20], fatigue=pa['values'][10], other_full_item_metadata_retained=True, book_progress_retained=True, book_before=previous_book, book_after=book_progress(save), player_credited_deaths_retained=len(credited))
    (p / 'travel-audit.json').write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps(out))


if __name__ == '__main__':
    main()
