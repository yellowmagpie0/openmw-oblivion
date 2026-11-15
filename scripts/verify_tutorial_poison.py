"""Audit original Poison of Illness gameplay courses and their actual saved ownership."""
import argparse,copy,hashlib,json,math,re,sys
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'scripts'))
import tes4_runtime_state as runtime
from verify_tutorial_alchemy import generated_potions
PLAYER='dynamic:player:0000000000000001'
POISON='content:oblivion.esm:08dc73';AXE='content:oblivion.esm:090613'
GOBLIN='content:oblivion.esm:03b513';BASE='content:oblivion.esm:03b512'
def read(p):return json.loads(Path(p).read_text())
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def stock(s,key):return sum(i['count'] for i in s['player']['inventory'] if i['base']==key)
def axe(s):
 rows=[i for i in s['player']['inventory'] if i['base']==AXE];assert len(rows)==1 and rows[0]['count']==1;return rows[0]
def life(s):return next(v for v in s['native_actor_life'] if v['actor']==GOBLIN)
def deaths(s):return {v['base']:v['count'] for v in s['native_death_counts']}
def inventory(s,exclude=()):return sorted((i for i in s['player']['inventory'] if i['base'] not in exclude),key=lambda i:json.dumps(i,sort_keys=True))
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('course',type=Path);ap.add_argument('phase',choices=['decline','apply','reload-declined','reload-coated','miss','contact','reload-spent','approach']);ap.add_argument('--sanitized',action='store_true');args=ap.parse_args();r=args.course
 d=read(r/'driver-process.json');assert d['captured'] and d['exit_code']==0 and d['input_unchanged'] and d['manifest_input_unchanged'];assert read(r/'scenario.json')['passed'];assert sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
 source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256']
 events=[json.loads(v) for v in (r/'events.jsonl').read_text().splitlines()];loads=[v for v in events if v['event']=='load-complete'];saves=[v for v in events if v['event']=='save-complete'];assert len(loads)==1 and saves
 epoch=read(r/'process-epoch.json');assert all(v['pid']==epoch['pid'] and v['epoch']==1 for v in loads+saves);assert loads[0]['save_sha256']==sha(source);assert saves[-1]['save_sha256']==sha(target)
 b=read(r/loads[0]['live']);a=read(r/saves[-1]['live']);assert a['schema_version']==b['schema_version']==104
 assert runtime.encode_payload(a)==runtime._find_runtime_record(target.read_bytes())[3]
 assert a['next_dynamic_serial']==b['next_dynamic_serial']==2
 assert generated_potions(source)==generated_potions(target)
 assert stock(a,'dynamic:shared-item:0000000000000001')==1
 before=axe(b);after=axe(a);assert before['equipped_slots']==after['equipped_slots']==65536
 assert before['charge']==after['charge']==-1 and before['script_owner'] is None and after['script_owner'] is None
 assert before['bound_source'] is None and after['bound_source'] is None
 expected_stock=1 if args.phase in ('decline','reload-declined') else 0
 expected_coating=POISON if args.phase in ('apply','reload-coated','miss','approach') else None
 assert stock(a,POISON)==expected_stock and after['weapon_poison']==expected_coating
 log=(r/'process.log').read_text();apply_count=log.count('TES4 weapon poison applied: poison='+POISON);contact_count=log.count('TES4 weapon poison contact committed: poison='+POISON)
 assert apply_count==int(args.phase=='apply') and contact_count==int(args.phase=='contact')
 if args.phase=='miss':
  assert log.count('M15 melee miss committed: actor='+PLAYER)==1
 if args.phase=='apply':
  assert stock(b,POISON)==1 and before['weapon_poison'] is None
  assert inventory(a,(AXE,))==inventory(b,(AXE,POISON))
 elif args.phase=='contact':
  # Active original goblins may land a real hit before the Player's strike.
  # Preserve every inventory field except condition loss on worn armour,
  # witnessed by that goblin's committed hit and matching Player Health debit.
  incoming=re.findall(r'M15 melee hit committed: actor='+re.escape(GOBLIN)+r' .*?health_damage=([0-9.e+-]+).*?victim='+re.escape(PLAYER)+r' .*?armor_condition_writes=([0-9]+)',log)
  damage=sum(float(v[0]) for v in incoming);writes=sum(int(v[1]) for v in incoming)
  assert math.isclose(a['player']['actor_values']['health.current'],b['player']['actor_values']['health.current']-damage,abs_tol=.01)
  remaining=copy.deepcopy(inventory(a,(AXE,)));changed=0
  for before_item in inventory(b,(AXE,)):
   if before_item in remaining:remaining.remove(before_item);continue
   matches=[v for v in remaining if {k:x for k,x in v.items() if k!='condition'}=={k:x for k,x in before_item.items() if k!='condition'}]
   assert len(matches)==1
   after_item=matches[0]
   assert damage>0 and before_item['count']==1 and before_item['equipped_slots']&0xffff
   assert 0<=after_item['condition']<before_item['condition']
   changed+=1;remaining.remove(after_item)
  assert not remaining and changed<=writes
  if len(incoming)==1:assert changed==writes
 else:assert inventory(a,(AXE,))==inventory(b,(AXE,))
 expected=copy.deepcopy(before);expected['weapon_poison']=expected_coating
 if args.phase=='contact':
  assert after['condition']<before['condition'];expected['condition']=after['condition']
  assert life(b)['phase']==0 and life(a)['phase']==1 and life(a)['killer']==PLAYER
  dc=deaths(b);dc[BASE]=dc.get(BASE,0)+1;assert deaths(a)==dc
  hit=re.search(r'TES4 weapon poison contact committed: poison='+re.escape(POISON)+r'.*physical_health_damage=([0-9.e+-]+) poison_health_damage=([0-9.e+-]+) total_health_damage=([0-9.e+-]+)',log)
  assert hit;physical,poison,total=map(float,hit.groups());assert physical>=0 and math.isclose(poison,10,abs_tol=1e-4) and math.isclose(total,physical+poison,abs_tol=1e-4)
 else:
  assert a['native_death_counts']==b['native_death_counts'] and a['quests']==b['quests']
  assert life(a)==life(b)
  if args.phase=='reload-spent':assert life(a)['phase']==1 and life(a)['killer']==PLAYER
  else:assert life(a)['phase']==0
 assert after==expected
 if args.sanitized:
  env=read(r/'observed-sanitizer-environment.json')['environment'];assert env['ASAN_OPTIONS']=='detect_leaks=0:halt_on_error=1' and env['UBSAN_OPTIONS']=='halt_on_error=1:print_stacktrace=1'
  libs=read(r/'loaded-libraries.json');assert any('libasan' in v for v in libs) and any('libubsan' in v for v in libs)
 out=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],save_events=len(saves),axe=after,stock=expected_stock,goblin_life=life(a),apply_events=apply_count,poison_contacts=contact_count);(r/'poison-audit.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
if __name__=='__main__':main()
