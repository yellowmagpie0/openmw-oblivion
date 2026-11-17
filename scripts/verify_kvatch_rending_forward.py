from pathlib import Path
import json,sys,math
sys.path.insert(0,'scripts');import tes4_runtime_state as rt
from verify_kvatch_topic_learning import read,sha,known_topics
from verify_kvatch_faction_reaction import inventory,reactions
from verify_kvatch_harrada_animation import health
r=Path(sys.argv[1]);d=read(r/'driver-process.json');source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';a,b=rt.load_save(source),rt.load_save(target)
scenario=read(r/'scenario.json')
assert d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and d['exit_code']==scenario['exit_code']==0 and scenario['passed'] and scenario['actions_complete']
assert sha(source)==d['input_sha256'] and sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
assert inventory(a)==inventory(b) and reactions(a)==reactions(b) and a['quests']==b['quests'] and known_topics(source)==known_topics(target) and abs(health(a)-health(b))<.1
for actor in ['dynamic:player:0000000000000001','content:oblivion.esm:098138']:assert next(v for v in b['native_actor_life'] if v['actor']==actor)['phase']==0
assert b['schema_version']==106 and rt.encode_payload(b)==rt._find_runtime_record(target.read_bytes())[3]
key='content:oblivion.esm:093ede';life=lambda x:next(v for v in x['native_actor_life'] if v['actor']==key)
assert life(a)==life(b) and life(b)['phase']==1 and life(b)['killer']=='content:oblivion.esm:098138'
assert b['player']['cell']=='content:oblivion.esm:01d0c8'
ai=next(v for v in b['actor_ai'] if v['actor']=='content:oblivion.esm:098138');assert ai['package']=='content:oblivion.esm:1778d1' and ai['last_transition_door']=='content:oblivion.esm:093edf'
assert a['path_points']==b['path_points'];source_pose=a['player']['position'][:3];prior_life=life(a);del a
events=[json.loads(l) for l in (r/'events.jsonl').read_text().splitlines()];epoch=read(r/'process-epoch.json');load=next(v for v in events if v['event']=='load-complete');save=next(v for v in events if v['event']=='save-complete')
for e in [load,save]:assert e['pid']==epoch['pid'] and sha(r/e['live'])==e['live_sha256']
assert load['save_sha256']==sha(source) and save['save_sha256']==sha(target) and read(r/save['live'])==b
loaded=read(r/load['live']);assert life(loaded)==prior_life and math.dist(loaded['player']['position'][:3],source_pose)<.1
previous=read(source.parents[3]/'process-epoch.json');assert (epoch['pid'],epoch['start_ticks'],epoch['boot_id'])!=(previous['pid'],previous['start_ticks'],previous['boot_id'])
x=dict(passed=True,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],enemy_life=life(b),cell=b['player']['cell'],player_pose=b['player']['position'],scope='Normal forward course only; terminal original Dremora and follower package preserved through actual process restart and upper Blood Feast transition. Combine with road-audit for earned-state invariants.')
(r/'forward-audit.json').write_text(json.dumps(x,indent=2)+'\n');print(json.dumps(x))
