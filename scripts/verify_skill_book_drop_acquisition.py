import json,sys,hashlib,math
from pathlib import Path
sys.path.insert(0,'scripts')
import tes4_runtime_state as runtime
from verify_blade_use import shared
from verify_skill_book_read import shared as progression
r=Path(sys.argv[1]);d=json.loads((r/'driver-process.json').read_text());assert d['captured'] and d['exit_code']==0 and d['input_unchanged'] and d['manifest_input_unchanged'];assert json.loads((r/'scenario.json').read_text())['passed']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256'];b=runtime.load_save(source);a=runtime.load_save(target);assert a['schema_version']==b['schema_version']==103;assert runtime.encode_payload(a)==runtime._find_runtime_record(target.read_bytes())[3];assert shared(source)==shared(target) and progression(source)==progression(target)
base='content:oblivion.esm:0243fc';ref='content:oblivion.esm:066cc1';items=lambda s:[i for i in s['player']['inventory'] if i['base']==base];other=lambda s:[i for i in s['player']['inventory'] if i['base']!=base];assert not items(b) and len(items(a))==1 and items(a)[0]['count']==1;assert other(a)==other(b)
assert a['player']['awarded_skill_books']==b['player']['awarded_skill_books']==[base]
assert b['player']['actor_values']['pc_misc_stat.17']==3 and a['player']['actor_values']['pc_misc_stat.17']==4
assert {k:v for k,v in a['player']['actor_values'].items() if k not in ['pc_misc_stat.17','encumbrance']}=={k:v for k,v in b['player']['actor_values'].items() if k not in ['pc_misc_stat.17','encumbrance']}
assert math.isclose(a['player']['actor_values']['encumbrance'],b['player']['actor_values']['encumbrance']+1,abs_tol=1e-4)
for f in ['native_actor_values','native_actor_life','native_death_counts','native_combat_engagements','quests','next_dynamic_serial','native_loose_weapons']:assert a[f]==b[f],f
book=next(v for v in a['references'] if v['key']==ref);assert book['deleted'] and book['custom_state']['count']==0 and book['custom_state']['read'] and book['custom_state']['taken'];assert a['next_dynamic_serial']==1 and not a['native_loose_weapons'];log=(r/'process.log').read_text();assert log.count('M5 interaction: kind=read result=read ref='+ref)==1 and log.count('M5 interaction: kind=take result=taken ref='+ref)==1 and 'awarded=true' not in log
result=dict(passed=True,input_sha256=sha(source),save_sha256=sha(target),books_read_before=3,books_read_after=4,awarded_books=a['player']['awarded_skill_books'],all_native_values_retained=True,all_shared_skills_and_progression_retained=True,next_dynamic_serial=1,process=json.loads((r/'process-epoch.json').read_text()));(r/'acquisition-verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
