"""Audit a real original Voice of the Emperor application or continuation."""
import argparse, copy, hashlib, json, re, sys
from pathlib import Path
sys.path.insert(0, str(Path.cwd() / 'scripts'))
import tes4_runtime_state as runtime
P = 'dynamic:player:0000000000000001'
T = 'content:oblivion.esm:04bb63'
SPELL = 'content:oblivion.esm:047add'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('course', type=Path)
    parser.add_argument('--phase', choices=['apply', 'restart', 'expire'], required=True)
    args = parser.parse_args()
    root = args.course
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    driver = json.loads((root / 'driver-process.json').read_text())
    source = Path(driver['input_save'])
    output = root / 'userdata/saves/Bendu_Olo/Quicksave.omwsave'
    assert driver['exit_code'] == 0 and driver['captured'] and driver['input_unchanged']
    assert sha(source) == driver['input_sha256']
    assert json.loads((root / 'scenario.json').read_text())['passed']
    initial, final = (runtime.load_save(source), runtime.load_save(output))
    assert initial['schema_version'] == final['schema_version'] == 97
    assert runtime.encode_payload(final) == runtime._find_runtime_record(output.read_bytes())[3]
    events = [json.loads(x) for x in (root / 'events.jsonl').read_text().splitlines()]
    loaded = next((x for x in events if x['event'] == 'load-complete'))
    saved = [x for x in events if x['event'] == 'save-complete'][-1]
    assert loaded['save_sha256'] == sha(source) and saved['save_sha256'] == sha(output)
    live = json.loads((root / loaded['live']).read_text())
    assert live['player']['inventory'] == initial['player']['inventory']
    assert live['clock'] == initial['clock']
    assert json.loads((root / saved['live']).read_text()) == final
    epoch = json.loads((root / 'process-epoch.json').read_text())
    assert loaded['pid'] == saved['pid'] == epoch['pid']
    before, after = (copy.deepcopy(initial['player']['inventory']), copy.deepcopy(final['player']['inventory']))
    for left, right in zip(before, after):
        if left['remaining_usage_time'] != right['remaining_usage_time']:
            assert left['base'] == right['base'] == 'content:oblivion.esm:02cf9f'
            assert left['equipped_slots'] == right['equipped_slots'] == 262144
            assert 0 < right['remaining_usage_time'] <= left['remaining_usage_time']
            left['remaining_usage_time'] = right['remaining_usage_time']
    assert before == after
    amulet = next((x for x in final['player']['inventory'] if x['base'] == 'content:oblivion.esm:0250a0'))
    assert amulet['script_owner'] == 'dynamic:inventory-script:0000000000000001'
    assert amulet['count'] == 1 and amulet['equipped_slots'] == 0
    assert final['next_dynamic_serial'] == initial['next_dynamic_serial']
    context = amulet['script_owner']
    assert [x for x in initial['script_instances'] if x['context'] == context] == [x for x in final['script_instances'] if x['context'] == context]
    corpse = next((x for x in final['native_actor_life'] if x['actor'] == 'content:oblivion.esm:06bfc2'))
    assert corpse['phase'] == 1 and corpse['killer'] == P
    assert final['native_death_counts'] == initial['native_death_counts']
    for state in [initial, final]:
        assert state['player']['actor_values']['health.current'] > 0
        av = {a['actor']: a for a in state['native_actor_values']}
        assert T in av
    av0 = {a['actor']: a for a in initial['native_actor_values']}
    av1 = {a['actor']: a for a in final['native_actor_values']}
    for actor in [P, T]:
        assert av0[actor]['values'][6] == av1[actor]['values'][6], 'Charm rewrote Personality'
        assert av0[actor]['values'][8] == av1[actor]['values'][8], 'Unexpected Health change'
    assert av1[T]['base'] == av0[T]['base'] == 'content:oblivion.esm:084b1a'
    owner = lambda s: next((r for r in s['references'] if r['key'] == T))
    charm = lambda s: {k: v for k, v in owner(s)['custom_state'].items() if k.startswith('native.charm.')}
    old, new = (charm(initial), charm(final))
    c0, c1 = (initial['clock'], final['clock'])
    assert (c0['year'], c0['month'], c0['time_scale']) == (c1['year'], c1['month'], c1['time_scale'])
    seconds = ((c1['day'] - c0['day']) * 24 + c1['hour'] - c0['hour']) * 3600 / c0['time_scale']
    # Compare the saved effect clock with elapsed simulation time.
    # The expiration course allows1.5s plus one query interval after expiry;
    # reserve3s total for observation/input, excluding save wall time.
    if args.phase == 'restart':
        assert abs(new['native.charm.elapsed'] - old['native.charm.elapsed'] - seconds) < 0.5
    if args.phase == 'expire':
        assert 30 - old['native.charm.elapsed'] - 0.5 <= seconds <= 30 - old['native.charm.elapsed'] + 3
    uses = lambda s: {x['spell']: x['used_at'] for x in next((a for a in s['native_actor_values'] if a['actor'] == P))['used_powers']}
    use0, use1 = (uses(initial), uses(final))
    log = (root / 'process.log').read_text()
    assert 'M7 inventory equip event:' not in log
    assert final['disposition_modifiers'] == initial['disposition_modifiers']
    assert not final['native_magic_projectiles']
    if args.phase == 'apply':
        assert not old and SPELL not in use0 and (SPELL in use1)
        assert {k: v for k, v in use1.items() if k != SPELL} == use0
        assert final['next_magic_projectile'] == initial['next_magic_projectile'] + 1
        assert log.count('M16 Charm applied: target=' + T) == 1
    else:
        assert old and use1 == use0 and (final['next_magic_projectile'] == initial['next_magic_projectile'])
        assert 'M16 Charm applied:' not in log and 'M16 native cast begin:' not in log
    if args.phase == 'expire':
        assert not new and log.count('M16 Charm expired: target=' + T) == 1
    else:
        assert set(new) == {'native.charm.spell', 'native.charm.caster', 'native.charm.magnitude', 'native.charm.elapsed'}
        assert new['native.charm.spell'] == SPELL and new['native.charm.caster'] == P
        assert new['native.charm.magnitude'] == 30 and 0 <= new['native.charm.elapsed'] < 30
        if old:
            assert new['native.charm.elapsed'] >= old['native.charm.elapsed']
        if args.phase == 'restart':
            assert 'M16 Voice of the Emperor rejected: reason=daily-use' in log
    values = [int(x) for x in re.findall('acceptance-command actor=' + re.escape(T) + ' name=GetDisposition result=(-?\\d+)', log)]
    assert values, 'No actual disposition queries executed'
    if args.phase == 'apply':
        assert values[0] == 20 and values[-1] == 50 and (set(values) <= {20, 50})
    elif args.phase == 'restart':
        assert set(values) == {50}
    else:
        assert values[0] == 50 and values[-1] == 20 and (set(values) <= {20, 50})
    result = dict(passed=True, auditor_sha256=sha(Path(__file__)), phase=args.phase, elapsed_simulation_seconds=seconds, process=epoch, input_sha256=sha(source), output_sha256=sha(output), charm_before=old, charm_after=new, disposition_queries=values, used_powers=use1, scope='Original Player power and concrete Skeleton084b1a recipient; no Player attack after Charm; no general Charm/combat cancellation claim.')
    (root / 'course-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
if __name__ == '__main__':
    main()
