"""Audit one original Repair Hammer action, unchanged menu, and cold ownership."""
import argparse, hashlib, json, math, re
from pathlib import Path
import tes4_runtime_state as runtime
from verify_blade_use import shared
PLAYER = 'dynamic:player:0000000000000001'
ITEM = 'content:oblivion.esm:0661c1'
TOOL = 'content:oblivion.esm:00000c'
CHEST = 'content:oblivion.esm:04931b'

def read(p):
    return json.loads(p.read_text())

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('course', type=Path)
    ap.add_argument('phase', choices=['repair', 'menu', 'restart'])
    ap.add_argument('--sanitized', action='store_true')
    args = ap.parse_args()
    p = args.course
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
    assert bp['values'][12] == pa['values'][12] == [5.0, 0.0, 0.0, 0.0]
    assert bs[1]['base'] == ass[1]['base'] == 5
    assert all((bs[i] == ass[i] for i in range(27) if i != 1))
    assert bp['values'][:10] == pa['values'][:10] and bp['values'][11:] == pa['values'][11:]
    assert bp['passive_abilities'] == pa['passive_abilities']
    for s in [b, a]:
        assert next((v for v in s['native_actor_life'] if v['actor'] == PLAYER))['phase'] == 0 and math.isclose(sum((v or 0 for v in actor(s)['values'][8])), 80, abs_tol=0.01)
    item = lambda s, key: next((i for i in s['player']['inventory'] if i['base'] == key and i['count']))
    count = lambda s, key: sum((i['count'] for i in s['player']['inventory'] if i['base'] == key))
    bi, ai = (item(b, ITEM), item(a, ITEM))
    assert bi['count'] == ai['count'] == 1 and bi['equipped_slots'] == ai['equipped_slots'] == 4
    log = (p / 'process.log').read_text()
    assert log.count('Writing saved game') == 1
    pre = log.split('Writing saved game', 1)[0]
    matches = re.findall('M13 repair committed: item=' + ITEM + ' condition=([\\d.eE+-]+)->([\\d.eE+-]+) amount=(\\d+) roll=(-?\\d+) break_chance=([\\d.eE+-]+) hammer_broken=(\\w+) experience=([\\d.eE+-]+)', pre)
    assert 'M13 repair rejected:' not in pre
    if args.phase == 'repair':
        assert len(matches) == 1 and pre.count('M13 repair requested') == 1
        before, after, amount, roll, chance, broken, experience = matches[0]
        assert math.isclose(float(before), bi['condition'], abs_tol=0.001) and float(after) == 52 and (int(amount) == 9) and (float(chance) == 29) and (0 <= int(roll) < 100)
        broken = broken in ['1', 'true']
        assert broken == (int(roll) < 29)
        assert math.isclose(float(experience), 1.5, abs_tol=1e-05)
        expected = dict(bi, condition=52.0)
        assert ai == expected
        assert count(b, TOOL) == 1 and count(a, TOOL) == 1 - int(broken)
        assert math.isclose(ass[1]['experience'] - bs[1]['experience'], 1.5, abs_tol=1e-05) and bs[1]['experience'] == 0
    else:
        assert not matches and 'M13 repair requested' not in pre
        assert bi == ai and bs[1] == ass[1] and (count(b, TOOL) == count(a, TOOL))
        broken = False
        if args.phase == 'restart':
            assert bi['condition'] == 52 and math.isclose(ass[1]['experience'], 1.5, abs_tol=1e-05)
    normalize = lambda s: sorted((json.dumps(i, sort_keys=True) for i in s['player']['inventory'] if i['count'] and i['base'] not in [ITEM, TOOL]))
    assert normalize(b) == normalize(a)
    if count(a, TOOL):
        assert item(b, TOOL) == item(a, TOOL)
    chest = lambda s: next((v for v in s['references'] if v['key'] == CHEST))
    assert not chest(a)['custom_state']['locked']
    assert sum((i['count'] for i in chest(a)['inventory'] if i['base'] == TOOL)) == 0
    normalizechest = lambda s: sorted((json.dumps(i, sort_keys=True) for i in chest(s)['inventory'] if i['count']))
    assert normalizechest(b) == normalizechest(a)
    # Original MQ01Script SCVR indices: heavyArmor and heavyArmorEquip.
    def tutorial_flags(state):
        quest = next(v for v in state['script_instances']
                     if v['unit'] == 'content:oblivion.esm:0297ad@oblivion.esm/quest/unit=0')
        return quest['locals'][11:13]
    assert tutorial_flags(b) == tutorial_flags(a)
    assert a['player']['actor_values'].get('skill_advances.12', 0) == b['player']['actor_values'].get('skill_advances.12', 0) == 0
    assert math.isclose(a['player']['actor_values']['encumbrance'], b['player']['actor_values']['encumbrance'] - int(broken), abs_tol=0.01)
    events = [json.loads(l) for l in (p / 'events.jsonl').read_text().splitlines()]
    loads = [e for e in events if e['event'] == 'load-complete']
    saves = [e for e in events if e['event'] == 'save-complete']
    epoch = read(p / 'process-epoch.json')
    assert len(loads) == len(saves) == 1
    assert loads[0]['pid'] == saves[0]['pid'] == epoch['pid']
    assert loads[0]['save_sha256'] == sha(src) and saves[0]['save_sha256'] == sha(save) and (read(p / saves[0]['live']) == a)
    if args.sanitized:
        env = read(p / 'observed-sanitizer-environment.json')['environment']
        assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
        libs = read(p / 'loaded-libraries.json')
        assert any(('libasan' in n for n in libs)) and any(('libubsan' in n for n in libs))
    out = dict(passed=True, phase=args.phase, sanitized=args.sanitized, source_sha256=sha(src), save_sha256=sha(save), binary_sha256=epoch['exe_sha256'], condition_before=bi['condition'], condition_after=ai['condition'], armorer_before=bs[1], armorer_after=ass[1], hammer_before=count(b, TOOL), hammer_after=count(a, TOOL), commits=matches, health=80, security_retained=ass[18], acrobatics_retained=ass[20])
    (p / 'repair-audit.json').write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps(out))
if __name__ == '__main__':
    main()
