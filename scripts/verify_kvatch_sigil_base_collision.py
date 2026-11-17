from pathlib import Path
import sys,json,math
sys.path.insert(0,str(Path.cwd()/'scripts'))
import tes4_runtime_state as rt
from verify_kvatch_faction_reaction import inventory,reactions,QUEST
from verify_kvatch_topic_learning import known_topics,read,sha
from verify_kvatch_harrada_animation import health
r=Path(sys.argv[1]);cold='cold' in r.name;player='dynamic:player:0000000000000001';ilend='content:oblivion.esm:098138';sigil='content:oblivion.esm:0c48e4';cell='content:oblivion.esm:01d0c8'
d=read(r/'driver-process.json');scenario=read(r/'scenario.json');source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave'
assert d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and d['exit_code']==scenario['exit_code']==0 and scenario['passed'] and scenario['actions_complete'] and all(a['passed'] for a in scenario['actions'])
assert sha(source)==d['input_sha256'] and sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
a,b=rt.load_save(source),rt.load_save(target);assert b['schema_version']==106 and rt.encode_payload(b)==rt._find_runtime_record(target.read_bytes())[3]
assert inventory(a)==inventory(b) and reactions(a)==reactions(b) and a['quests']==b['quests'] and known_topics(source)==known_topics(target)
assert next(q for q in b['quests'] if q['quest']==QUEST)['stage']==25 and abs(health(a)-health(b))<.1
ref=lambda s,key:next(x for x in s['references'] if x['key']==key)
assert ref(a,ilend)['inventory']==ref(b,ilend)['inventory'] and ref(b,ilend)['base']=='content:oblivion.esm:09812f'
assert b['player']['cell']==ref(b,ilend)['cell']==cell and ref(b,ilend)['enabled'] and not ref(b,ilend)['deleted']
for actor in [player,ilend]:assert next(x for x in b['native_actor_life'] if x['actor']==actor)['phase']==0
ai=next(x for x in b['actor_ai'] if x['actor']==ilend)
assert ai['package']=='content:oblivion.esm:1778d1' and ai['target']==ai['companion_group']==player and ai['cell']==ai['last_valid_cell']==cell
# Follow AI resamples its last valid pose while Ilend is still approaching.
assert math.dist(ai['last_valid_position'][:3],ref(b,ilend)['position'][:3])<64
for actor,pos in [(player,b['player']['position']),(ilend,ref(b,ilend)['position'])]:
 movement=next(x for x in b['native_movement']['actors'] if x['actor']==actor)
 assert math.dist(movement['position'],pos[:3])<.1 and movement['collision_mode'] and movement['collision_body']
assert math.dist(b['player']['position'][:2],[-1312,0])<160 and abs(b['player']['position'][2]+240)<64
separation=math.dist(b['player']['position'][:3],ref(b,ilend)['position'][:3]);assert 60<separation<400
before,after=ref(a,sigil),ref(b,sigil)
assert after['base']=='content:oblivion.esm:0c48e3' and after['enabled'] and not after['deleted'] and after['position']==before['position']==[0,0,-208,0,0,0] and after['custom_state']==before['custom_state']
log=(r/'process.log').read_text();assert ('BloodFeast cold collision verified' if cold else 'BloodFeast leg complete') in log and 'ERROR: AddressSanitizer' not in log and 'runtime error:' not in log
ai_events=[json.loads(l) for l in (r/'ai-events.jsonl').read_text().splitlines()]
assert not any(e.get('actor')==ilend and e['event']=='door-transition' for e in ai_events)
epoch=read(r/'process-epoch.json');events=[json.loads(l) for l in (r/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1
for e in loads+saves:assert e['pid']==epoch['pid'] and sha(r/e['live'])==e['live_sha256']
assert loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(target) and read(r/saves[0]['live'])==b
if cold:
 loaded=read(r/loads[0]['live']);assert math.dist(loaded['player']['position'][:3],a['player']['position'][:3])<.1 and math.dist(a['player']['position'][:3],b['player']['position'][:3])<1
 assert ref(loaded,sigil)==before and abs(health(loaded)-health(a))<.001
 prior=read(source.parents[3]/'process-epoch.json');assert (epoch['pid'],epoch['start_ticks'],epoch['boot_id'])!=(prior['pid'],prior['start_ticks'],prior['boot_id'])
san=(r/'observed-sanitizer-environment.json').exists()
if san:
 env=read(r/'observed-sanitizer-environment.json')['environment'];libs=read(r/'loaded-libraries.json')
 assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'] and any('libasan' in p for p in libs) and any('libubsan' in p for p in libs)
result=dict(passed=True,cold=cold,sanitized=san,input_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],player_health=health(b),player_position=b['player']['position'],ilend_position=ref(b,ilend)['position'],separation=separation,sigil_reference_retained=True,scope='Actual original Blood Feast floor lane and lower Rending Halls doorway, genuine own-save continuation and unchanged owned state. No door entry or full tower/gate completion.')
(r/'sigil-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
