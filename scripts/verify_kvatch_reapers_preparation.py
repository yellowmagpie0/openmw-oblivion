"""Audit actual original Reapers Sprawl preparation; no new engine acceptance."""
from pathlib import Path
import argparse,copy,json,math,re,sys
sys.path.insert(0,str(Path.cwd()/'scripts'))
import tes4_runtime_state as rt
from verify_kvatch_topic_learning import read,sha,known_topics
from verify_kvatch_faction_reaction import inventory,reactions
from verify_kvatch_harrada_animation import health
p=argparse.ArgumentParser(description=__doc__);p.add_argument('case',type=Path);p.add_argument('phase',choices=['greeting','defense','menien']);p.add_argument('--producer',type=Path,required=True);args=p.parse_args();r=args.case
producer=read(args.producer);driver=read(r/'driver-process.json');scenario=read(r/'scenario.json');source=Path(driver['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';a,b=rt.load_save(source),rt.load_save(target)
assert scenario['passed'] and scenario['actions_complete'] and driver['exit_code']==scenario['exit_code']==0 and all(x['passed'] for x in scenario['actions'])
assert driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged'] and sha(source)==driver['input_sha256'] and sha(r/'captured-input-manifest.json')==driver['manifest_input_sha256']
assert b['schema_version']==106 and rt.encode_payload(b)==rt._find_runtime_record(target.read_bytes())[3]
player='dynamic:player:0000000000000001';ilend='content:oblivion.esm:098138';guardian='content:oblivion.esm:0878d5';key='content:oblivion.esm:093ef5';quest='content:oblivion.esm:0224d8'
ref=lambda s,k:next(x for x in s['references'] if x['key']==k)
life=lambda s,k:next(x for x in s['native_actor_life'] if x['actor']==k)
hp=lambda s,k:sum(x or 0 for x in next(x for x in s['native_actor_values'] if x['actor']==k)['values'][8])
assert a['player']['cell']==b['player']['cell']==ref(b,ilend)['cell']=='content:oblivion.esm:033b5e'
assert reactions(a)==reactions(b) and known_topics(source)==known_topics(target)
assert inventory({'player':{'inventory':ref(a,ilend)['inventory']}})==inventory({'player':{'inventory':ref(b,ilend)['inventory']}})
for actor in [player,ilend]:assert life(b,actor)['phase']==0 and hp(b,actor)>0
for actor in ['content:oblivion.esm:093ede','content:oblivion.esm:093ee1']:assert life(a,actor)==life(b,actor) and life(b,actor)['phase']==1 and life(b,actor)['killer']==ilend
ai=next(x for x in b['actor_ai'] if x['actor']==ilend);assert ai['package']=='content:oblivion.esm:1778d1' and ai['target']==player and not ai['restrained'] and not ai['action_reserved']
assert math.dist(ref(b,ilend)['position'][:3],b['player']['position'][:3])<400
log=(r/'process.log').read_text();expected_quests=copy.deepcopy(a['quests']);q=next(x for x in expected_quests if x['quest']==quest)
if args.phase=='menien':
 before,after=25,30
 assert q['stage']==before;q['stage']=after;q['completed_stages'].append(after)
 assert f'setstage quest={quest} stage={after}' in log
assert expected_quests==b['quests']
assert inventory(a)==inventory(b)
if args.phase!='defense':assert ref(a,guardian)['inventory']==ref(b,guardian)['inventory']
if args.phase=='defense':
 assert life(a,guardian)['phase']==0 and life(b,guardian)['phase']==1 and life(b,guardian)['killer']==ilend
 damage=sum(float(x) for x in re.findall(r'M15 melee hit committed: actor='+guardian+r' .*?health_damage=([0-9.]+) .*?victim='+player,log))
 assert 0<damage<10 and abs(health(a)-health(b)-damage)<.05 and hp(b,ilend)<=hp(a,ilend)
 assert 'RendingFight checkpoint=target-dead' in log
else:
 assert abs(health(a)-health(b))<.1 and abs(hp(a,ilend)-hp(b,ilend))<.1
 assert life(a,guardian)==life(b,guardian)
if args.phase=='greeting':
 assert life(b,guardian)['phase']==0 and any(x['target']==player for x in b['combat_controllers'][guardian])
 assert 'conversation-player ref='+guardian in log and 'startcombat actor='+guardian in log
if args.phase=='menien':assert '089938@oblivion.esm/dialogue-result/' in log
corpse='content:oblivion.esm:06bc4e';suffix='content:oblivion.esm:0852fe';age='obscript.shader.age.'+suffix;duration='obscript.shader.duration.'+suffix
assert ref(b,corpse)['custom_state'][duration]==-1 and ref(b,corpse)['custom_state'][age]>ref(a,corpse)['custom_state'][age]
locals_=lambda s:next(x['locals'] for x in s['script_instances'] if x['context']==corpse and x['unit'].startswith('content:oblivion.esm:0c47ae@'))
assert locals_(a)==locals_(b)
epoch=read(r/'process-epoch.json');assert epoch['exe_sha256']==producer['sha256']==sha(Path(producer['path']))
events=[json.loads(x) for x in (r/'events.jsonl').read_text().splitlines()];load=next(x for x in events if x['event']=='load-complete');save=next(x for x in events if x['event']=='save-complete')
for e in [load,save]:assert e['pid']==epoch['pid'] and sha(r/e['live'])==e['live_sha256']
assert load['save_sha256']==sha(source) and save['save_sha256']==sha(target) and read(r/save['live'])==b
previous=read(source.parents[3]/'process-epoch.json');assert tuple(epoch[k] for k in ['pid','start_ticks','boot_id'])!=tuple(previous[k] for k in ['pid','start_ticks','boot_id'])
result=dict(passed=True,phase=args.phase,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],cell=b['player']['cell'],player_pose=b['player']['position'],ilend_pose=ref(b,ilend)['position'],player_health=health(b),ilend_health=hp(b,ilend),guardian_life=life(b,guardian),quest=next(x for x in b['quests'] if x['quest']==quest),scope='Normal forward preparation using existing implementations; no new C++ or sanitizer/keyed-door/full-course acceptance.')
(r/'preparation-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
