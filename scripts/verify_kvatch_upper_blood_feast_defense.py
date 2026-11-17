from pathlib import Path
import sys,json,re
sys.path.insert(0,'scripts');import tes4_runtime_state as rt
from verify_kvatch_faction_reaction import inventory,reactions
from verify_kvatch_topic_learning import known_topics,read,sha
from verify_kvatch_harrada_animation import health
r=Path(sys.argv[1]);d=read(r/'driver-process.json');s=read(r/'scenario.json');source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave'
assert d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and d['exit_code']==s['exit_code']==0 and s['passed'] and s['actions_complete']
assert sha(source)==d['input_sha256'] and sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
a,b=rt.load_save(source),rt.load_save(target)
assert inventory(a)==inventory(b) and reactions(a)==reactions(b) and a['quests']==b['quests'] and known_topics(source)==known_topics(target)
assert b['schema_version']==106 and rt.encode_payload(b)==rt._find_runtime_record(target.read_bytes())[3]
life=lambda x,k:next(v for v in x['native_actor_life'] if v['actor']==k)
for actor in ['dynamic:player:0000000000000001','content:oblivion.esm:098138']:assert life(b,actor)['phase']==0
assert life(a,'content:oblivion.esm:093ee1')['phase']==0 and life(b,'content:oblivion.esm:093ee1')['phase']!=0
log=(r/'process.log').read_text();damage=sum(float(x) for x in re.findall(r'M15 melee hit committed: actor=content:oblivion.esm:093ee1 .*?health_damage=([0-9.]+) .*?victim=dynamic:player:0000000000000001',log));assert 0<damage<10 and abs(health(a)-health(b)-damage)<.05;assert 'RendingFight checkpoint=target-dead' in log and 'actor=content:oblivion.esm:093ee1 killer=content:oblivion.esm:098138' in log
assert not any('M15 melee hit committed: actor=dynamic:player:' in line for line in log.splitlines())
ref=lambda x,k:next(v for v in x['references'] if v['key']==k)
ai=next(v for v in b['actor_ai'] if v['actor']=='content:oblivion.esm:098138');assert ai['package']=='content:oblivion.esm:1778d1'
x=dict(passed=True,input_sha256=sha(source),save_sha256=sha(target),player_health=health(b),player_pose=b['player']['position'],ilend_pose=ref(b,'content:oblivion.esm:098138')['position'],enemy_life=life(b,'content:oblivion.esm:093ee1'),scope='Normal forward preparation only: existing Ilend defense kills original upper Blood Feast Dremora; no new combat implementation acceptance.')
(r/'upper-defense-audit.json').write_text(json.dumps(x,indent=2)+'\n');print(json.dumps(x))
