"""Audit real Cairn Bolete use and partial/completed cold reloads."""
import hashlib, json, math, re, sys
from pathlib import Path
import tes4_runtime_state as runtime
from verify_blade_use import shared
from verify_skill_book_read import shared as book_progress
PLAYER='dynamic:player:0000000000000001'
CAP='content:oblivion.esm:06251e'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def actor(s):return next(v for v in s['native_actor_values'] if v['actor']==PLAYER)
def main():
 p=Path(sys.argv[1]);phase=sys.argv[2];assert phase in ['baseline','use','pending','complete']
 d=read(p/'driver-process.json');assert d['captured'] and d['exit_code']==0 and d['input_unchanged'] and d['manifest_input_unchanged'] and read(p/'scenario.json')['passed']
 source=Path(d['input_save']);save=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256']
 b,a=runtime.load_save(source),runtime.load_save(save);assert b['schema_version']==a['schema_version']==101 and runtime.encode_payload(a)==runtime._find_runtime_record(save.read_bytes())[3]
 bp,ap=actor(b),actor(a);bs,ass=shared(source),shared(save)
 assert all(bs[i]==ass[i] for i in range(27) if i!=16)
 assert ass[16]==dict(bs[16],experience=bs[16]['experience']+(.5 if phase=='use' else 0))
 assert book_progress(source)==book_progress(save)
 assert all(x==y for i,(x,y) in enumerate(zip(bp['values'],ap['values'])) if i not in [8,10])
 assert bp['values'][8][:3]==ap['values'][8][:3]
 assert bp['passive_abilities']==ap['passive_abilities'] and bp['used_powers']==ap['used_powers']
 assert b['player']['character_generation']==a['player']['character_generation'] and b['player']['awarded_skill_books']==a['player']['awarded_skill_books']
 items=lambda s:sorted(json.dumps(i,sort_keys=True) for i in s['player']['inventory'] if i['count'] and i['base']!=CAP)
 count=lambda s:sum(i['count'] for i in s['player']['inventory'] if i['base']==CAP)
 assert items(b)==items(a) and count(b)==count(a)+(phase=='use')
 if phase=='use':assert count(b)==1 and count(a)==0
 lives={v['actor']:v for v in a['native_actor_life']};credited=[v for v in b['native_actor_life'] if v['phase']==1 and v['killer']==PLAYER]
 assert len(credited)==33 and all(lives[v['actor']]['phase']==1 and lives[v['actor']]['killer']==PLAYER for v in credited)
 assert lives[PLAYER]['phase']==0
 for plant in b['references']:
  if plant.get('custom_state',{}).get('harvested'):
   assert next(v for v in a['references'] if v['key']==plant['key'])==plant
 effects=lambda v:v.get('ingredient_restoration', []);be,ae=effects(bp),effects(ap)
 for e in be+ae:assert e['ingredient']==CAP and e['magnitude']==1 and e['duration']==3 and e['actor_value']==8 and 0<=e['elapsed']<3
 if phase=='use':assert not be and len(ae)==1;start=0;finish=ae[0]['elapsed'];assert 0<finish<3
 elif phase=='pending':assert len(be)==1 and not ae;start=be[0]['elapsed'];finish=3
 else:assert not be and not ae;start=finish=0
 log=(p/'process.log').read_text();assert log.count('Writing saved game')==1
 pre=log.split('Writing saved game',1)[0]
 consumed=re.findall(r'TES4 ingredient consumed: ingredient=(\S+) magnitude=(\S+) duration=(\S+)',pre)
 assert consumed==([(CAP,'1','3')] if phase=='use' else [])
 uses=re.findall(r'M16 Alchemy ingredient use: ingredient=(\S+)',pre);assert uses==([CAP] if phase=='use' else [])
 ticks=re.findall(r'TES4 ingredient restoration: ingredient='+CAP+r' amount=([\d.eE+-]+) elapsed=([\d.eE+-]+) actor_value=8',pre)
 contribution=sum(float(v) for v,_ in ticks);assert math.isclose(contribution,finish-start,abs_tol=.002)
 health=lambda v:sum(x or 0 for x in v['values'][8]);assert math.isclose(health(ap),min(110,health(bp)+finish-start),abs_tol=.01)
 assert math.isclose(a['player']['actor_values']['health.current'],health(ap),abs_tol=.01)
 es=[json.loads(l) for l in (p/'events.jsonl').read_text().splitlines()];loads=[e for e in es if e['event']=='load-complete'];saves=[e for e in es if e['event']=='save-complete'];epoch=read(p/'process-epoch.json')
 assert len(loads)==len(saves)==1 and loads[0]['pid']==saves[0]['pid']==epoch['pid']
 assert loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(save)
 assert read(p/loads[0]['live'])==b and read(p/saves[0]['live'])==a
 sanitized=(p/'observed-sanitizer-environment.json').exists()
 if sanitized:
  env=read(p/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
  libs=read(p/'loaded-libraries.json');assert any('libasan' in x for x in libs) and any('libubsan' in x for x in libs)
 out=dict(passed=True,phase=phase,sanitized=sanitized,source_sha256=sha(source),save_sha256=sha(save),binary_sha256=epoch['exe_sha256'],health_before=health(bp),health_after=health(ap),elapsed_before=start,elapsed_after=finish,contribution=contribution,pending=ae,alchemy_before=bs[16],alchemy_after=ass[16],credited_deaths_retained=len(credited),full_other_item_metadata_retained=True,all_unrelated_skills_retained=True,book_progress_retained=True)
 (p/'health-use-audit.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
if __name__=='__main__':main()
