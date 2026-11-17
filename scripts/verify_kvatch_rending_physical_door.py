from pathlib import Path
import sys,json,math
sys.path.insert(0,str(Path.cwd()/'scripts'))
import tes4_runtime_state as rt
from verify_kvatch_faction_reaction import inventory,reactions
from verify_kvatch_topic_learning import known_topics,read,sha
from verify_kvatch_harrada_animation import health
r=Path(sys.argv[1]);cold='cold' in r.name;d=read(r/'driver-process.json');s=read(r/'scenario.json');source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';door='content:oblivion.esm:093ee0'
assert d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and d['exit_code']==s['exit_code']==0 and s['passed'] and s['actions_complete']
assert sha(source)==d['input_sha256'] and sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
a,b=rt.load_save(source),rt.load_save(target);assert inventory(a)==inventory(b) and reactions(a)==reactions(b) and a['quests']==b['quests'] and known_topics(source)==known_topics(target) and abs(health(a)-health(b))<.1 and a['path_points']==b['path_points']
assert b['schema_version']==106 and rt.encode_payload(b)==rt._find_runtime_record(target.read_bytes())[3]
ref=lambda x,key:next(v for v in x['references'] if v['key']==key)
inst=lambda x:next(v for v in x['script_instances'] if v['context']==door and v['unit'].startswith('content:oblivion.esm:005162@'))
assert inst(b)['locals']==[{'type':'number','value':1},{'type':'number','value':1}]
assert ref(a,door)['position']==ref(b,door)['position'] and ref(b,door)['enabled'] and not ref(b,door)['deleted']
state=ref(b,door)['custom_state'];assert state['obscript.animation_group']=='open' and state['obscript.animation_scripted'] and state['obscript.animation_mode']==1
for key in ['dynamic:player:0000000000000001','content:oblivion.esm:098138']:assert next(v for v in b['native_actor_life'] if v['actor']==key)['phase']==0
assert ref(a,'content:oblivion.esm:098138')['inventory']==ref(b,'content:oblivion.esm:098138')['inventory']
prior_state=ref(a,door)['custom_state'];prior_player=a['player']['position'][:3];prior_script=inst(a);del a
log=(r/'process.log').read_text();calls=log.count('enablelinkedpathpoints object='+door+' changed=false');assert calls==(0 if cold else 1)
events=[json.loads(l) for l in (r/'events.jsonl').read_text().splitlines()];load=next(v for v in events if v['event']=='load-complete');save=next(v for v in events if v['event']=='save-complete');epoch=read(r/'process-epoch.json')
for e in [load,save]:assert e['pid']==epoch['pid'] and sha(r/e['live'])==e['live_sha256']
assert load['save_sha256']==sha(source) and save['save_sha256']==sha(target) and read(r/save['live'])==b
loaded_phase=None
if cold:
 loaded=read(r/load['live']);loaded_phase=ref(loaded,door)['custom_state']['obscript.animation_progress'];assert abs(loaded_phase-prior_state['obscript.animation_progress'])<1e-4 and inst(loaded)==prior_script and math.dist(loaded['player']['position'][:3],prior_player)<.1
 assert state['obscript.animation_progress']>prior_state['obscript.animation_progress'] and state['obscript.animation_progress']>=.999 and 'RendingOpenCold passage clear' in log
 prior=read(source.parents[3]/'process-epoch.json');assert (epoch['pid'],epoch['start_ticks'],epoch['boot_id'])!=(prior['pid'],prior['start_ticks'],prior['boot_id'])
else:assert 0<state['obscript.animation_progress']<1 and 'RendingOpen passage clear' in log
x=dict(passed=True,cold=cold,input_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],health=health(b),player_position=b['player']['position'],phase=state['obscript.animation_progress'],loaded_phase=loaded_phase,activation_noops=calls,scope='Normal-only forward course: original physical door opening and active-phase own-save continuation. No sanitizer physical-opening or full Hall completion claim.')
(r/'physical-door-audit.json').write_text(json.dumps(x,indent=2)+'\n');print(json.dumps(x))
