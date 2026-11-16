"""Audit original Wellspring Stool input/cold saves against its independent oracle."""
import hashlib,json,math,sys
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'scripts'))
import tes4_runtime_state as io
from verify_blade_use import shared as skills
from verify_skill_book_read import shared as progress
REF='content:oblivion.esm:06efc7';BASE='content:oblivion.esm:02c46d';WEIGHT='content:oblivion.esm:01c5ed';PLAYER='dynamic:player:0000000000000001'
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(p.read_text())
r=Path(sys.argv[1]);phase=sys.argv[2];assert phase in ('seat','weight','view','exit','standing')
d=read(r/'driver-process.json');assert d['captured'] and d['exit_code']==0 and d['input_unchanged'] and d['manifest_input_unchanged'];assert read(r/'scenario.json')['passed'];assert sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256'];b=io.load_save(source);a=io.load_save(target)
assert b['schema_version'] in (105,106) and a['schema_version']==106;assert io.encode_payload(a)==io._find_runtime_record(target.read_bytes())[3]
raw=target.read_bytes();camera_flags=[raw[start+8:end] for _,_,tag,subs in io._save_records(raw) if tag==b'CAM_' for name,start,end in subs if name==b'FIRS'];assert camera_flags==[b'\x00'], 'Primary course must actually switch to and retain third-person mode'

assert a['next_dynamic_serial']==b['next_dynamic_serial']==20
assert skills(source)==skills(target) and progress(source)==progress(target)
assert a['quests']==b['quests'] and a['native_actor_life']==b['native_actor_life'] and a['native_death_counts']==b['native_death_counts']
ref=lambda s:next(v for v in s['references'] if v['key']==REF)
assert ref(a)==ref(b) and ref(a)['enabled'] and not ref(a)['deleted'] and ref(a)['custom_state']['count']==1
assert a['player']['cell']==b['player']['cell']=='content:oblivion.esm:0352a9'
others=lambda s:[i for i in s['player']['inventory'] if i['base']!=WEIGHT]
assert others(a)==others(b)
count=lambda s:sum(i['count'] for i in s['player']['inventory'] if i['base']==WEIGHT)
assert count(a)==count(b)+(phase=='weight')
if phase!='weight':assert a['player']['inventory']==b['player']['inventory']
else:
 changed=[]
 for old in b['references']:
  if old['base']!=WEIGHT:continue
  new=next(v for v in a['references'] if v['key']==old['key'])
  if new!=old:changed.append((old,new))
 assert len(changed)==1 and changed[0][0]['key'] in ['content:oblivion.esm:'+v for v in ['06efbe','06efbf','06efc0','06efc1']]
 assert changed[0][0]['custom_state']['count']==1 and changed[0][1]['custom_state']['count']==0
assert math.isclose(a['player']['actor_values']['encumbrance'], b['player']['actor_values']['encumbrance']+(phase=='weight'), abs_tol=1e-4)
for key,value in b['player']['actor_values'].items():
 if key!='encumbrance':assert a['player']['actor_values'][key]==value,(key,value,a['player']['actor_values'][key])
old=b.get('native_player_furniture');new=a['native_player_furniture']
if phase in ('seat','weight','view'):
 assert new['reference']==REF and new['base']==BASE and new['marker']==2 and new['phase']==3
 assert math.dist(a['player']['position'][:3],[-2718.18,-1733.88,-640.34])<2
 assert abs(math.remainder(a['player']['position'][5]-math.radians(257.83),2*math.pi))<.01
 if phase=='seat':assert old is None
 else:
  assert old['phase']==3
  for key in ['reference','base','cell','placement','scale','marker','actor_model','model_hash','clip_hashes']:assert new[key]==old[key]
  assert a['player']['position'][:3]==b['player']['position'][:3]
else:
 assert new is None
 if phase=='exit':
  assert old['phase']==3
  assert math.dist(a['player']['position'][:3],[-2667.85,-1720.06,-642.54])<12
  assert abs(math.remainder(a['player']['position'][5]-math.radians(77.83),2*math.pi))<.01
 else:assert old is None
if phase=='view':assert .6 < new['look_yaw'] < 1.0 and .1 < a['player']['position'][3] < .3
log=(r/'process.log').read_text();assert 'Native Player furniture: cancelled' not in log
if phase=='seat':assert 'Native Player furniture: phase=2' in log and 'Native Player furniture: phase=3' in log
if phase=='exit':assert 'M5 activation focus: result=empty' in log and 'Native Player furniture: phase=4' in log and 'Native Player furniture: phase=0' in log
if phase in ('weight','view','standing'):assert not any('Native Player furniture: phase='+str(n) in log for n in [2,4])
epoch=read(r/'process-epoch.json');events=[json.loads(v) for v in (r/'events.jsonl').read_text().splitlines()];loads=[v for v in events if v['event']=='load-complete'];saves=[v for v in events if v['event']=='save-complete'];assert len(loads)==len(saves)==1
for event in loads+saves:assert event['pid']==epoch['pid'] and sha(r/event['live'])==event['live_sha256']
loaded=read(r/loads[0]['live']);saved=read(r/saves[0]['live']);assert loaded.get('native_player_furniture')==b.get('native_player_furniture');assert saved['native_player_furniture']==new
assert loaded['player']['inventory']==b['player']['inventory'] and saved['player']['inventory']==a['player']['inventory']
assert saved['player']['position']==a['player']['position']
result=dict(passed=True,phase=phase,input_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],process=epoch,seat=new,player_pose=a['player']['position'],weight_count=count(a),allocator=20,strict_fields=['inventory except declared one Weight pickup','quests','actor life','death counts','actor values except encumbrance','skills and level progress','original Stool reference','dynamic serial','cold-load furniture identity/clock and saved live state'],tolerances='seated anchor <2 units and heading .01rad; clean original getup endpoint <12 units and heading .01rad; exact state/asset ownership',runtime_scope='Single original Player forward Stool lifecycle; no NPC/all-chair/M14 completion claim')
(r/'audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result[k] for k in ['passed','phase','input_sha256','save_sha256','weight_count','player_pose']}))
