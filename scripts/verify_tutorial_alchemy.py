"""Inspect actual tutorial-kit gameplay saves; expected poison values come from the original-game receipt."""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'scripts'))
import tes4_runtime_state as runtime
from verify_blade_use import shared as skills
from verify_skill_book_read import shared as progress
PLAYER='dynamic:player:0000000000000001'
MORTAR='content:oblivion.esm:0c7968';WISP='content:oblivion.esm:06251f';STINK='content:oblivion.esm:08446d'
KIT={MORTAR,WISP,STINK};REFS={0xc796f:MORTAR,0xc796e:WISP,0xc796d:STINK};MQ='content:oblivion.esm:01e723'
def read(p):return json.loads(Path(p).read_text())
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def generated_potions(p):
 data=Path(p).read_bytes();out=[]
 for _,_,tag,subs in runtime._save_records(data):
  if tag!=b'ALCH':continue
  fields={n:data[a+8:b] for n,a,b in subs}
  name=fields[b'NAME']
  if len(name)!=9 or name[0]!=4:continue
  serial=struct.unpack('<Q',name[1:])[0]
  weight,value,flags=struct.unpack('<fii',fields[b'ALDT'])
  effect=fields.get(b'ENAM',b'');effect_id=fields.get(b'ENID',b'')
  assert effect_id==b'\x02DamageHealth',repr(effect_id)
  assert len(effect)==20,len(effect)
  out.append(dict(key=f'dynamic:shared-item:{serial+1:016x}',name=fields[b'FNAM'].rstrip(b'\0').decode(),weight=weight,value=value,flags=flags,effect=list(struct.unpack('<iiiii',effect)),model=fields[b'MODL'].rstrip(b'\0').decode(),icon=fields[b'TEXT'].rstrip(b'\0').decode(),raw_sha256=hashlib.sha256(b''.join(n+v for n,v in fields.items())).hexdigest()))
 return out
def item(s,key):return [i for i in s['player']['inventory'] if i['base']==key and i['count']]
def actor(s):return next(v for v in s['native_actor_values'] if v['actor']==PLAYER)
def mq(s):return next(v for v in s['quests'] if v['quest']==MQ)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('course',type=Path);ap.add_argument('phase',choices=['acquire','brew','reload']);ap.add_argument('--sanitized',action='store_true');args=ap.parse_args();r=args.course
 d=read(r/'driver-process.json');assert d['captured'] and d['exit_code']==0 and d['input_unchanged'] and d['manifest_input_unchanged'];assert read(r/'scenario.json')['passed'];assert sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
 source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256']
 events=[json.loads(v) for v in (r/'events.jsonl').read_text().splitlines()];loads=[v for v in events if v['event']=='load-complete'];saves=[v for v in events if v['event']=='save-complete'];assert len(loads)==1 and saves
 epoch=read(r/'process-epoch.json');assert all(v['pid']==epoch['pid'] and v['epoch']==1 for v in loads+saves);assert loads[0]['save_sha256']==sha(source);assert saves[-1]['save_sha256']==sha(target)
 b=read(r/loads[0]['live']);a=read(r/saves[-1]['live']);assert a['schema_version']==b['schema_version']==103
 assert runtime.encode_payload(a)==runtime._find_runtime_record(target.read_bytes())[3]
 assert a['native_death_counts']==b['native_death_counts'];assert a['player'].get('awarded_skill_books',[])==b['player'].get('awarded_skill_books',[]);assert a['player']['character_generation']==b['player']['character_generation']
 assert a['player']['actor_values']['health.current']==b['player']['actor_values']['health.current']==80
 assert actor(a)['passive_abilities']==actor(b)['passive_abilities']
 log=(r/'process.log').read_text();before_skills,after_skills=skills(source),skills(target);before_progress,after_progress=progress(source),progress(target)
 if args.phase=='acquire':
  assert not any(item(b,key) for key in KIT)
  assert [i for i in a['player']['inventory'] if i['base'] not in KIT]==b['player']['inventory']
  for key in KIT:assert len(item(a,key))==1 and item(a,key)[0]['count']==1
  serial=b['next_dynamic_serial'];owner=f'dynamic:inventory-script:{serial:016x}'
  assert a['next_dynamic_serial']==serial+1 and item(a,MORTAR)[0]['script_owner']==owner
  assert all(item(a,k)[0]['script_owner'] is None for k in [WISP,STINK])
  assert not any(v['key']==f'dynamic:native-reference:{serial:016x}' for v in a['references'])
  assert 64 not in mq(b)['completed_stages'] and set(mq(a)['completed_stages'])==set(mq(b)['completed_stages'])|{64}
  assert mq(a)['stage']==mq(b)['stage']==80
  assert log.count('M7 inventory add event: item='+MORTAR)==1
  for number,key in REFS.items():
   ref=f'content:oblivion.esm:{number:06x}';old=next(v for v in b['references'] if v['key']==ref);new=next(v for v in a['references'] if v['key']==ref)
   assert not old['deleted'] and old['custom_state'].get('count',1)==1
   assert new['deleted'] and new['custom_state']['count']==0 and new['custom_state']['taken']
   assert log.count('M5 interaction: kind=take result=taken ref='+ref+' base='+key)==1
  assert before_skills==after_skills and before_progress==after_progress
  assert not generated_potions(source) and not generated_potions(target)
  assert math.isclose(a['player']['actor_values']['encumbrance'],b['player']['actor_values']['encumbrance']+1.2,abs_tol=1e-4)
 else:
  assert a['next_dynamic_serial']==b['next_dynamic_serial'];assert a['quests']==b['quests']
  assert item(a,MORTAR)==item(b,MORTAR) and item(a,MORTAR)[0]['script_owner'] is not None
  for number in REFS:assert next(v for v in a['references'] if v['key']==f'content:oblivion.esm:{number:06x}')==next(v for v in b['references'] if v['key']==f'content:oblivion.esm:{number:06x}')
  mortar_instances=lambda s:[v for v in s['script_instances'] if '0c7966' in v['unit']]
  assert len(mortar_instances(a))==1 and mortar_instances(a)==mortar_instances(b)
  assert 'M7 inventory add event: item='+MORTAR not in log
  made=generated_potions(target);assert len(made)==1;poison=made[0];key=poison['key']
  expected=read('docs/oblivion/TUTORIAL-ALCHEMY-ORACLE.json')['original_game_probe']['observations']
  assert poison['name']==expected['default_name'] and poison['value']==expected['value_ui'] and math.isclose(poison['weight'],expected['weight_ui'],abs_tol=.01)
  assert poison['flags']==0x100 and poison['effect']==[0,0,expected['duration_seconds_ui'],expected['magnitude_ui'],expected['magnitude_ui']]
  assert poison['model'].lower().replace('\\','/')=='clutter/potions/potionpoison.nif'
  assert poison['icon'].lower().replace('\\','/')=='clutter/potions/iconpotionpoison01.dds'
  assert len(item(a,key))==1 and item(a,key)[0]['count']==1 and item(a,key)[0]['script_owner'] is None
  assert not item(a,WISP) and not item(a,STINK)
  if args.phase=='brew':
   assert not generated_potions(source) and not item(b,key)
   assert item(b,WISP)[0]['count']==item(b,STINK)[0]['count']==1
   assert [i for i in a['player']['inventory'] if i['base']!=key]==[i for i in b['player']['inventory'] if i['base'] not in [WISP,STINK]]
   assert before_skills[16]['base']==5 and before_skills[16]['experience']==0 and after_skills[16]['base']==6
   # Independent authored factor/exponent and existing compiled minor default;
   # original native SKIL19 use0=5. This does not invoke production arithmetic.
   settings=read('docs/oblivion/TUTORIAL-ALCHEMY-ORACLE.json')['skill_training']['settings'];requirement=(5*settings['factor'])**settings['exponent']*settings['minor_multiplier']
   assert math.isclose(after_skills[16]['experience'],5-requirement,abs_tol=1e-5)
   assert before_skills[:16]+before_skills[17:]==after_skills[:16]+after_skills[17:]
   assert after_progress['level_progress']==before_progress['level_progress']
   assert after_progress['attributes']==[v+int(i==1) for i,v in enumerate(before_progress['attributes'])]
   assert after_progress['specializations']==[v+int(i==1) for i,v in enumerate(before_progress['specializations'])]
   assert actor(a)['values'][19]==[6.,0.,0.,0.]
   assert math.isclose(a['player']['actor_values']['encumbrance'],b['player']['actor_values']['encumbrance']-.1,abs_tol=1e-4)
  else:
   assert a['player']['inventory']==b['player']['inventory'];assert made==generated_potions(source)
   assert before_skills==after_skills and before_progress==after_progress
   assert actor(a)['values']==actor(b)['values']
 if args.sanitized:
  env=read(r/'observed-sanitizer-environment.json')['environment'];assert env['ASAN_OPTIONS']=='detect_leaks=0:halt_on_error=1' and env['UBSAN_OPTIONS']=='halt_on_error=1:print_stacktrace=1'
  libs=read(r/'loaded-libraries.json');assert any('libasan' in v for v in libs) and any('libubsan' in v for v in libs)
 out=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],save_events=len(saves),generated_potions=generated_potions(target),alchemy_before=before_skills[16],alchemy_after=after_skills[16],mortar=item(a,MORTAR),quest=mq(a));(r/'alchemy-audit.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
if __name__=='__main__':main()
