from pathlib import Path
import sys,json,math
sys.path.insert(0,'scripts');import tes4_runtime_state as rt
from verify_kvatch_topic_learning import read,sha,known_topics
from verify_kvatch_faction_reaction import inventory,reactions
from verify_kvatch_harrada_animation import health
r=Path(sys.argv[1]);phase=sys.argv[2];producer=read(Path(sys.argv[3]));d=read(r/'driver-process.json');sc=read(r/'scenario.json');source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';a,b=rt.load_save(source),rt.load_save(target)
assert sc['passed'] and sc['actions_complete'] and d['exit_code']==sc['exit_code']==0 and all(x['passed'] for x in sc['actions'])
assert d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and sha(source)==d['input_sha256'] and sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
assert b['schema_version']==106 and rt.encode_payload(b)==rt._find_runtime_record(target.read_bytes())[3]
assert inventory(a)==inventory(b) and reactions(a)==reactions(b) and a['quests']==b['quests'] and known_topics(source)==known_topics(target)
assert abs(health(a)-health(b))<.1
player='dynamic:player:0000000000000001';ilend='content:oblivion.esm:098138';life=lambda s,k:next(x for x in s['native_actor_life'] if x['actor']==k);ref=lambda s,k:next(x for x in s['references'] if x['key']==k)
hp=lambda s,k:sum(v or 0 for v in next(x for x in s['native_actor_values'] if x['actor']==k)['values'][8])
for k in [player,ilend]:assert life(b,k)['phase']==0 and abs(hp(a,k)-hp(b,k))<.1
for k in ['content:oblivion.esm:093ede','content:oblivion.esm:093ee1']:assert life(a,k)==life(b,k) and life(b,k)['phase']==1 and life(b,k)['killer']==ilend
assert ref(a,ilend)['inventory']==ref(b,ilend)['inventory']
assert a['player']['cell']==b['player']['cell']==ref(b,ilend)['cell']=='content:oblivion.esm:021072'
ai=next(x for x in b['actor_ai'] if x['actor']==ilend);assert ai['package']=='content:oblivion.esm:1778d1' and ai['target']==player and not ai['restrained'] and not ai['action_reserved']
origin=a['player']['position'][:3];end=b['player']['position'][:3];v=[3533.3357-origin[0],13487.126-origin[1]];length=math.hypot(*v);direction=[x/length for x in v];delta=[x-y for x,y in zip(end,origin)];progress=delta[0]*direction[0]+delta[1]*direction[1];lateral=abs(delta[0]*direction[1]-delta[1]*direction[0]);assert 490<=progress<650 and lateral<100 and abs(delta[2])<64
ally=ref(b,ilend)['position'][:3];assert math.dist(end,ally)<400 and abs(end[2]-ally[2])<64
corpse='content:oblivion.esm:06bc4e';suffix='content:oblivion.esm:0852fe';age='obscript.shader.age.'+suffix;duration='obscript.shader.duration.'+suffix;assert ref(b,corpse)['custom_state'][duration]==-1 and ref(b,corpse)['custom_state'][age]>ref(a,corpse)['custom_state'][age]
local=lambda s:next(x['locals'] for x in s['script_instances'] if x['context']==corpse and x['unit'].startswith('content:oblivion.esm:0c47ae@'));assert local(a)==local(b)
epoch=read(r/'process-epoch.json');assert epoch['exe_sha256']==producer['sha256'];events=[json.loads(l) for l in (r/'events.jsonl').read_text().splitlines()];load=next(x for x in events if x['event']=='load-complete');save=next(x for x in events if x['event']=='save-complete')
for e in [load,save]:assert e['pid']==epoch['pid'] and sha(r/e['live'])==e['live_sha256']
assert load['save_sha256']==sha(source) and save['save_sha256']==sha(target) and read(r/save['live'])==b
loaded=read(r/load['live']);assert math.dist(loaded['player']['position'][:3],origin)<.1 and math.dist(ref(loaded,ilend)['position'][:3],ref(a,ilend)['position'][:3])<.1
previous=read(source.parents[3]/'process-epoch.json');assert (epoch['pid'],epoch['start_ticks'],epoch['boot_id'])!=(previous['pid'],previous['start_ticks'],previous['boot_id'])
if phase=='entry':assert sha(source)=='0ffb72a796bb60c9204e529985958e3355c1245f565f091917aa0d991868c792'
log=(r/'process.log').read_text();assert 'BridgeYield clear progress=' in log and 'DarkRoute checkpoint=stalled' not in log
if r.name.startswith('sanitized'):
 env=read(r/'observed-sanitizer-environment.json')['environment'];assert env['ASAN_OPTIONS']=='detect_leaks=0:halt_on_error=1' and env['UBSAN_OPTIONS']=='halt_on_error=1:print_stacktrace=1' and env['LD_PRELOAD'].split(':')[0]=='/usr/lib64/libasan.so.8.0.0'
x=dict(passed=True,phase=phase,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],progress=progress,lateral=lateral,player_pose=b['player']['position'],ilend_pose=ref(b,ilend)['position'],player_health=health(b),ilend_health=hp(b,ilend),package=ai['package'],target=ai['target'],scope='Original bridge clearance only: ordinary500-unit advance past initial idle follower, same nativeFollow/target/lives/health and earned state, physical bridge bounds, distinct process/save/live provenance. Not the full bridge, Reapers Sprawl, key, or main-quest acceptance.')
(r/'yield-audit.json').write_text(json.dumps(x,indent=2)+'\n');print(json.dumps(x))
