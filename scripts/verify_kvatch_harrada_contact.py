"""Audit original Harrada contact damage and own-save impulse continuation."""
from pathlib import Path
import sys,json,math
sys.path.insert(0,str(Path.cwd()/'scripts'))
import tes4_runtime_state as rt
from verify_kvatch_faction_reaction import inventory,reactions,QUEST
from verify_kvatch_topic_learning import known_topics,read,sha
from verify_kvatch_harrada_animation import health
r=Path(sys.argv[1]);cold='cold' in r.name
s=read(r/'scenario.json');d=read(r/'driver-process.json')
assert d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and d['exit_code']==0
assert sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
source=Path(d['input_save']);assert sha(source)==d['input_sha256']
target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave'
assert s['exit_code']==0 and s['passed'] and s['actions_complete'] and all(a['passed'] for a in s['actions'])
before,after=rt.load_save(source),rt.load_save(target)
assert after['schema_version']==106 and rt.encode_payload(after)==rt._find_runtime_record(target.read_bytes())[3]
assert inventory(before)==inventory(after) and reactions(before)==reactions(after) and before['quests']==after['quests'] and known_topics(source)==known_topics(target)
assert next(q for q in after['quests'] if q['quest']==QUEST)['stage']==25
player='dynamic:player:0000000000000001';ilend='content:oblivion.esm:098138'
for actor in (player,ilend):assert next(a for a in after['native_actor_life'] if a['actor']==actor)['phase']==0
ref=lambda state:next(a for a in state['references'] if a['key']==ilend)
assert ref(before)['inventory']==ref(after)['inventory'] and ref(after)['enabled'] and not ref(after)['deleted']
log=(r/'process.log').read_text();hits=[l for l in log.splitlines() if 'TES4 trap impact: reference=content:oblivion.esm:068733 body=182 actor='+player in l]
assert abs(health(before)-health(after)-(0 if cold else 5))<.1
assert len(hits)==(0 if cold else 1)
pulses=lambda state:[x for x in state['native_actor_knockback'] if x['actor']==player]
if not cold:
 assert len(pulses(after))==1 and 0<pulses(after)[0]['remaining']<=.5
 # The observed response must push horizontally away from the authored plant.
 delta=[after['player']['position'][0]+1393.248779296875,after['player']['position'][1]-2230.81298828125]
 assert sum(a*b for a,b in zip(pulses(after)[0]['acceleration'][:2],delta))>0
if cold:
 assert len(pulses(before))==len(pulses(after))==1
 assert pulses(after)[0]['acceleration']==pulses(before)[0]['acceleration']
 assert 0<pulses(after)[0]['remaining']<pulses(before)[0]['remaining']
epoch=read(r/'process-epoch.json');events=[json.loads(l) for l in (r/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1
for e in loads+saves: assert e['pid']==epoch['pid'] and sha(r/e['live'])==e['live_sha256']
assert loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(target) and read(r/saves[0]['live'])==after
if cold:
 prior=read(source.parents[3]/'process-epoch.json');assert (epoch['pid'],epoch['start_ticks'],epoch['boot_id'])!=(prior['pid'],prior['start_ticks'],prior['boot_id'])
 loaded=read(r/loads[0]['live']);assert pulses(loaded)==pulses(before) and loaded['native_trap_contacts']==before['native_trap_contacts'] and abs(health(loaded)-health(before))<1e-4
if 'reentry' in r.name: assert 'Harrada separated far' in log
san=(r/'observed-sanitizer-environment.json').exists()
if san:
 env=read(r/'observed-sanitizer-environment.json')['environment'];libs=read(r/'loaded-libraries.json')
 assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
 assert any('libasan' in p for p in libs) and any('libubsan' in p for p in libs)
result=dict(passed=True,sanitized=san,cold=cold,input_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],health_before=health(before),health_after=health(after),player_impacts=len(hits),pending_push=pulses(after),saved_contacts=after['native_trap_contacts'],moved=math.dist(before['player']['position'][:3],after['player']['position'][:3]))
(r/'contact-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
