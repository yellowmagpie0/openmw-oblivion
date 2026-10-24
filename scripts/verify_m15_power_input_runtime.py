from pathlib import Path
import json,sys,hashlib,re,copy,struct
sys.path.insert(0,'scripts');import tes4_runtime_state as r
base=Path(sys.argv[1]);player='dynamic:player:0000000000000001';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();rows=[];states=[];epochs=[]
for phase,expected_saves in [('first',2),('second',1)]:
 d=base/phase;index=json.loads((d/'snapshot-index.json').read_text());assert len(index)==expected_saves
 scenario=json.loads((d/'scenario.json').read_text());assert scenario['passed'] and scenario['shutdown_requested'] and not scenario['timed_out'] and not scenario['unreviewed_error_log_findings']
 launch=json.loads((d/'launcher.json').read_text());assert launch['scenario_returncode']==launch['monitor_returncode']==0
 epoch=json.loads((d/'process-epoch.json').read_text());epochs.append(epoch);assert epoch['executable_sha256']==launch['binary_sha256'];assert len(json.loads((d/'loaded-libraries.json').read_text())['libraries'])==1
 report=json.loads((d/'script-report.json').read_text());assert report['compilation_failures']==0
 for diagnostic in report['diagnostics']:
  assert (diagnostic['code'],diagnostic['message'],diagnostic['unit'],diagnostic['event'])==('OBSV003','ObScript value is not numeric','content:oblivion.esm:026eac@oblivion.esm/quest/unit=0','gamemode')
 for k,v in sorted(index.items(),key=lambda x:int(x[0])):
  p=Path(v['snapshot']);s=r.load_save(p);live=d/v['event']['live'];assert s==json.loads(live.read_text()),(phase,'binary/live');assert r.encode_payload(s)==r._find_runtime_record(p.read_bytes())[3]
  states.append(s);rows.append(dict(phase=phase,path=str(p),sha256=sha(p),live_sha256=sha(live)))
assert len({(x['boot_id'],x['pid'],x['start_ticks']) for x in epochs})==2
initial,windup,final=states;assert all(x['schema_version']==69 for x in states)
assert initial['combat_rng_state']==415139642 and initial['physical_actions']=={'next':1,'pending':[]}
assert windup['combat_rng_state']==final['combat_rng_state']==1030492215
assert set(windup['spoken_dialogue_infos'])==set(initial['spoken_dialogue_infos'])|{'content:oblivion.esm:0c4548'}
assert final['spoken_dialogue_infos']==windup['spoken_dialogue_infos']
strike=next(x for x in windup['native_melee_states'] if x['actor']==player)['strike'];assert all(strike[k]==v for k,v in dict(id=2,kind=2,contact_committed=False,power_phase=0).items());assert windup['physical_actions']=={'next':3,'pending':[2]};assert final['physical_actions']=={'next':3,'pending':[]};assert all(x['strike'] is None for x in final['native_melee_states'])
log=(base/'first/process.log').read_text();second=(base/'second/process.log').read_text();rolls=re.findall(r'M15 power input speech roll: raw=(\d+) selected=(\d+) rng_before=(\d+) rng_after=(\d+)',log);assert rolls==[('26500','1','415139642','3884216597')]
assert log.count('automatic-speech topic=content:oblivion.esm:0000e6')==1 and 'info=content:oblivion.esm:0c4548 ref='+player+' responses=1 voice=sound/voice/oblivion.esm/imperial/m/generic_powerattack_000c4548_1.mp3' in log
assert 'Native dialogue condition: function=77 subject=0 actual=69 ' in log
assert 'M15 power input speech roll:' not in second and 'automatic-speech topic=content:oblivion.esm:0000e6' not in second and 'M15 melee selection:' not in second
assert second.count('M15 melee restore: actor='+player+' id=2 ')==1
hits=re.findall(r'M15 melee (?:hit|miss) committed:.*',second);assert len(hits)==1 and 'id=2 fatigue_cost=35 before=140 after=105 action=3 health_damage=0 fatigue_damage=0' in hits[0]
assert 'combat_random_draws=0' in hits[0]
events=[json.loads(x) for x in (base/'second/events.jsonl').read_text().splitlines()];load=next(x for x in events if x['event']=='load-complete');assert json.loads((base/'second'/load['live']).read_text())==windup
assert load['save_sha256']==rows[1]['sha256']==sha(base/'second/input-pristine.omwsave')
for state in [windup,final]:
 actors={x['actor']:x for x in state['native_actor_values']}
 for old in initial['native_actor_values']:
  now=actors[old['actor']];expected=copy.deepcopy(old)
  if old['actor']==player:
   expected['process_action']=2 if state is windup else -1
   assert -35<=now['values'][10][3]<=0
   expected['values'][10][3]=now['values'][10][3]
  assert now==expected,old['actor']
 refs={x['key']:x for x in state['references']}
 for old in initial['references']:assert refs[old['key']]['inventory']==old['inventory'],old['key']
 for field in ['native_actor_bases','native_actor_life','native_death_counts','native_combat_engagements','combat_controllers']:
  assert state[field]==initial[field],field
result=dict(passed=True,epochs=epochs,saves=rows,whole_initial_recovery_exact=True,full_binary_live_exact=True,first_saved_strike=strike,expected_rng=dict(start=415139642,power_draw=dict(raw=26500,state=3884216597),random_condition=dict(raw=19169,percent=69,state=3403800452),info_selection=dict(raw=15724,candidates=10,reversed_index=5,state=1030492215,info='content:oblivion.esm:0c4548')),background_quest_diagnostics={phase:len(json.loads((base/phase/'script-report.json').read_text())['diagnostics']) for phase in ('first','second')},scope='Known source-run quest026eac OBSV003 numeric diagnostics remain; this is isolated power-input acceptance, not clean campaign script execution. Actual configured keyboard ordinary-to-power input; windup F5 before contact; fresh-process exact restore, one resumed miss/cost35 and no power-roll/voice replay. Supplied native AV/base/life/controller/inventory preserved except Player Fatigue regeneration bounded[-35,0] and explicit action2->-1. All three saved binary/live states and whole fresh initial recovery exact. Clocks, general positions, ambient AI state and new cold-cell references are outside endpoint equivalence. No hit, directional mastery, device/FPS matrix, hardware listening or original live-game acceptance.')
(base/'verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(passed=True,epochs=2,saves=3)))
