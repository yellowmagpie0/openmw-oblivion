"""Audit original Mead use after Cheap Wine support, including old-save null defaults."""
import json, sys, hashlib, math
from pathlib import Path
import tes4_runtime_state as t
from verify_blade_use import shared
def main():
    p = Path(sys.argv[1])
    read = lambda p: json.loads(p.read_text())
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    d = read(p / 'driver-process.json')
    assert d['exit_code'] == 0 and d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and read(p / 'scenario.json')['passed']
    src = Path(d['input_save'])
    save = p / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert sha(src) == d['input_sha256']
    b, a = (t.load_save(src), t.load_save(save))
    player = 'dynamic:player:0000000000000001'
    mead = 'content:oblivion.esm:0b1201'
    actor = lambda s: next((v for v in s['native_actor_values'] if v['actor'] == player))
    bp, ap = (actor(b), actor(a))
    assert a['schema_version'] == 101 and t.encode_payload(a) == t._find_runtime_record(save.read_bytes())[3]
    assert shared(src) == shared(save)
    normalize = lambda s: sorted((json.dumps(dict(i, script_owner=i.get('script_owner')), sort_keys=True) for i in s['player']['inventory'] if i['base'] != mead and i['count']))
    assert normalize(b) == normalize(a)
    count = lambda s: sum((i['count'] for i in s['player']['inventory'] if i['base'] == mead))
    assert count(b) == 2 and count(a) == 1
    mb = dict(next((i for i in b['player']['inventory'] if i['base'] == mead)), count=1, script_owner=None)
    ma = next((i for i in a['player']['inventory'] if i['base'] == mead))
    assert mb == ma
    for i in range(len(bp['values'])):
        if i in [1, 2, 9, 10]:
            continue
        assert bp['values'][i] == ap['values'][i], i
    for i in [1, 2]:
        v = list(bp['values'][i])
        v[1] = (v[1] or 0) - 1
        assert ap['values'][i] == v
    assert ap['values'][9][0] == bp['values'][9][0] - 2 and ap['values'][9][1:] == bp['values'][9][1:]
    assert ap['values'][10][0] == bp['values'][10][0] - 1
    es = ap['potion_attribute_effects']
    assert len(es) == 1 and es[0]['potion'] == mead and (es[0]['duration'] == 60) and (0 < es[0]['elapsed'] < 60) and (es[0]['modifiers'] == [dict(attribute=1, delta=-1), dict(attribute=2, delta=-1)])
    log = (p / 'process.log').read_text()
    assert log.count('M16 potion attributes applied: potion=' + mead) == 1 and 'fatigue_amount=2 fatigue_restored=2 count=1' in log
    assert log.count('Writing saved game') == 1
    if len(sys.argv) > 2:
        env = read(p / 'observed-sanitizer-environment.json')['environment']
        assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
        libs = read(p / 'loaded-libraries.json')
        assert any(('libasan' in v for v in libs)) and any(('libubsan' in v for v in libs))
    events = [json.loads(v) for v in (p / 'events.jsonl').read_text().splitlines()]
    loads = [v for v in events if v['event'] == 'load-complete']
    saves = [v for v in events if v['event'] == 'save-complete']
    epoch = read(p / 'process-epoch.json')
    assert len(loads) == len(saves) == 1 and loads[0]['pid'] == saves[0]['pid'] == epoch['pid'] and (loads[0]['save_sha256'] == sha(src)) and (saves[0]['save_sha256'] == sha(save)) and (read(p / saves[0]['live']) == a)
    out = dict(passed=True, source_sha256=sha(src), save_sha256=sha(save), binary_sha256=epoch['exe_sha256'], mead_before=2, mead_after=1, effects=es, health=sum((v or 0 for v in ap['values'][8])), all_skills_retained=True, other_full_item_metadata_retained=True, source_schema=b['schema_version'], null_default_normalization='Old schema93 lacks script_owner; absent is compared with explicit null only, all other complete metadata remains strict.')
    (p / 'mead-regression-audit.json').write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps(out))


if __name__ == '__main__':
    main()
