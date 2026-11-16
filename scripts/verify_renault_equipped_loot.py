import collections,hashlib,json,re,sys
from pathlib import Path
sys.path.insert(0,'scripts');import tes4_runtime_state as io
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path('build/oblivion-compat/windows-merge');ref='content:oblivion.esm:032a15';rows=[];fields=['base','condition','charge','owner','remaining_usage_time','ownership_rank','ownership_global','extra_scale','bound_source']
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def counts(items):
 c=collections.Counter()
 for item in items:c[json.dumps({k:item.get(k) for k in fields},sort_keys=True)]+=item['count']
 return c
for phase in ['loot','cold']:
 p=root/f'normal-merged-renault-{phase}-04';driver=json.loads((p/'driver-process.json').read_text());assert driver['exit_code']==0 and driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged'];assert json.loads((p/'scenario.json').read_text())['passed'];source=Path(driver['input_save']);save=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';a=io.load_save(source);b=io.load_save(save);old=next(r for r in a['references'] if r['key']==ref);new=next(r for r in b['references'] if r['key']==ref);assert old['base']==new['base'] and new['enabled'] and not new['deleted'] and not new['inventory'];assert b['native_player_furniture'] is None
 deaths=lambda state:{v['base']:v['count'] for v in state['native_death_counts']}
 before_deaths=deaths(a);after_deaths=deaths(b);log=(p/'process.log').read_text(errors='replace')
 if phase=='loot':
  expected_deaths=dict(before_deaths);expected_deaths['content:oblivion.esm:027338']=1;assert after_deaths==expected_deaths
  callbacks=re.findall(r'M15 native death callback: id=(\d+) actor=(\S+) killer=(\S+)',log);assert callbacks==[('6','content:oblivion.esm:026be1','content:oblivion.esm:037e40')]
 else:assert before_deaths==after_deaths and 'M15 native death callback:' not in log
 for old_life in a['native_actor_life']:
  if phase=='loot' and old_life['actor']=='content:oblivion.esm:026be1':continue
  assert next(v for v in b['native_actor_life'] if v['actor']==old_life['actor'])==old_life
 assert next(v for v in b['native_actor_life'] if v['actor']=='dynamic:player:0000000000000001')['phase']==0
 expected=counts(a['player']['inventory'])+(counts(old['inventory']) if phase=='loot' else collections.Counter());assert counts(b['player']['inventory'])==expected
 if phase=='loot':
  assert len(old['inventory'])==8 and sum(bool(i['equipped_slots']) for i in old['inventory'])==5
  bases={i['base'] for i in old['inventory']};assert all(i['equipped_slots']==0 for i in b['player']['inventory'] if i['base'] in bases)
 else:assert a['player']['inventory']==b['player']['inventory']
 epoch=json.loads((p/'process-epoch.json').read_text());row={'case':str(p),'phase':phase,'passed':True,'input_sha256':sha(source),'save_sha256':sha(save),'process':epoch,'transferred_original_stacks':8 if phase=='loot' else 0,'exact_fields':fields,'previous_death_counts_unchanged':True,'declared_new_natural_death':{'actor':'content:oblivion.esm:026be1','killer':'content:oblivion.esm:037e40','callback_id':6} if phase=='loot' else None};rows.append(row);(p/'audit.json').write_text(json.dumps(row,indent=2)+'\n')
assert rows[0]['save_sha256']==rows[1]['input_sha256'];assert rows[0]['process']['pid']!=rows[1]['process']['pid'];(root/'normal-merged-renault-chain.json').write_text(json.dumps({'passed':True,'phases':rows,'scope':'Ordinary approach and Take All on original equipped Renault corpse, retained rendering, exact transfer fields and cold empty-corpse continuation. Movement-only corpse pass-through is additionally covered by actual Bullet worker-partition engine tests; no new manual crossing claim.'},indent=2)+'\n');print(json.dumps({'passed':True,'save_sha256':rows[-1]['save_sha256']}))
