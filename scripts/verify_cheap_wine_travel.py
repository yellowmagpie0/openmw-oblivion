"""Inspect genuine ordinary-input travel to the original Cheap Wine bottle."""
import hashlib, json, math, sys
from pathlib import Path
import tes4_runtime_state as runtime
from verify_blade_use import shared
PLAYER = 'dynamic:player:0000000000000001'
WINE = 'content:oblivion.esm:037f7f'
REF = 'content:oblivion.esm:06752a'

def read(p):
    return json.loads(p.read_text())

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p = Path(sys.argv[1])
    phase = sys.argv[2]
    assert phase in ['prefix', 'approach', 'acquire']
    d = read(p / 'driver-process.json')
    assert d['exit_code'] == 0 and d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged']
    assert read(p / 'scenario.json')['passed']
    src = Path(d['input_save'])
    save = p / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(src) == d['input_sha256']
    b, a = (runtime.load_save(src), runtime.load_save(save))
    bs, ass = (shared(src), shared(save))
    assert a['schema_version'] == 101 and runtime.encode_payload(a) == runtime._find_runtime_record(save.read_bytes())[3]
    actor = lambda s: next((v for v in s['native_actor_values'] if v['actor'] == PLAYER))
    bp, pa = (actor(b), actor(a))
    assert all((bp['values'][i] == pa['values'][i] for i in range(len(bp['values'])) if i not in [10, 13, 26]))
    assert bp['passive_abilities'] == pa['passive_abilities']
    assert all((bs[i] == ass[i] for i in range(27) if i not in [8, 20]))
    assert ass[8]['base'] >= bs[8]['base'] and ass[20]['base'] >= bs[20]['base']
    assert pa['values'][13][0] == ass[8]['base'] and pa['values'][26][0] == ass[20]['base']
    assert ass[8]['base'] > bs[8]['base'] or ass[8]['experience'] >= bs[8]['experience']
    assert ass[20]['base'] > bs[20]['base'] or ass[20]['experience'] >= bs[20]['experience']
    for s in [b, a]:
        assert next((v for v in s['native_actor_life'] if v['actor'] == PLAYER))['phase'] == 0 and math.isclose(sum((v or 0 for v in actor(s)['values'][8])), 110, abs_tol=0.01)
    count = lambda s: sum((i['count'] for i in s['player']['inventory'] if i['base'] == WINE))
    normalize = lambda s: sorted((json.dumps(i, sort_keys=True) for i in s['player']['inventory'] if i['count'] and i['base'] != WINE))
    assert normalize(b) == normalize(a)
    if phase == 'acquire':
        assert count(b) == 0 and count(a) == 1
        wine = next(i for i in a['player']['inventory'] if i['base'] == WINE and i['count'])
        assert wine == dict(base=WINE, count=1, condition=-1, charge=-1,
                            equipped_slots=0, hotkey=-1, owner='null', remaining_usage_time=-1,
                            ownership_rank=None, ownership_global='null', extra_scale=None,
                            bound_source=None, script_owner=None)
        ref = next((v for v in a['references'] if v['key'] == REF))
        assert ref['deleted'] and ref['custom_state']['taken']
        assert 'M5 interaction: kind=take result=taken ref=' + REF + ' base=' + WINE in (p / 'process.log').read_text()
    else:
        assert count(b) == count(a) == 0
    pos = a['player']['position']
    destination = [40832, 88064] if phase == 'prefix' else [29238.23046875, 81712.9921875]
    assert math.dist(pos[:2], destination) < 180
    log = (p / 'process.log').read_text()
    assert log.count('Writing saved game') == 1 and 'M16 potion attributes applied:' not in log
    es = [json.loads(l) for l in (p / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in es if e['event'] == 'load-complete']
    saves = [e for e in es if e['event'] == 'save-complete']
    epoch = read(p / 'process-epoch.json')
    assert len(loads) == len(saves) == 1 and loads[0]['pid'] == saves[0]['pid'] == epoch['pid']
    assert loads[0]['save_sha256'] == sha(src) and saves[0]['save_sha256'] == sha(save)
    assert read(p / saves[0]['live']) == a
    out = dict(passed=True, phase=phase, source_sha256=sha(src), save_sha256=sha(save), binary_sha256=epoch['exe_sha256'], health=110, position=pos, wine_before=count(b), wine_after=count(a), athletics_before=bs[8], athletics_after=ass[8], acrobatics_before=bs[20], acrobatics_after=ass[20], fatigue=pa['values'][10], other_full_item_metadata_retained=True)
    (p / 'travel-audit.json').write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps(out))


if __name__ == '__main__':
    main()
