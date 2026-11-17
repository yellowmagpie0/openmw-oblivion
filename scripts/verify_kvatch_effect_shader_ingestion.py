from pathlib import Path
import sys,json,subprocess
sys.path.insert(0,'scripts');import tes4_runtime_state as rt
from verify_kvatch_topic_learning import read,sha
from verify_kvatch_harrada_animation import health
r=Path(sys.argv[1]);repo=Path.cwd();root=r.parent;mode='sanitized' if r.name.startswith('sanitized') else 'normal'
# Reuse the current public checker for codec/live, earned state, follow package,
# original first corpse and distinct-process/source provenance checks.
subprocess.run([sys.executable,str(repo/'scripts/verify_kvatch_rending_forward.py'),str(r)],check=True)
d=read(r/'driver-process.json');source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';a,b=rt.load_save(source),rt.load_save(target)
life=lambda s,k:next(v for v in s['native_actor_life'] if v['actor']==k)
corpses={}
for key in ['content:oblivion.esm:093ede','content:oblivion.esm:093ee1']:
 assert life(a,key)==life(b,key) and life(b,key)['phase']==1 and life(b,key)['killer']=='content:oblivion.esm:098138'
 corpses[key]=life(b,key)
ally=lambda s:next(v for v in s['references'] if v['key']=='content:oblivion.esm:098138')
assert ally(a)['inventory']==ally(b)['inventory']
epoch=read(r/'process-epoch.json');producer=read(root/(mode+'-producer-02.json'));assert epoch['exe_sha256']==producer['sha256']
assert 'ShaderLoader earned source ready' in (r/'process.log').read_text()
if mode=='sanitized':
 env=read(r/'observed-sanitizer-environment.json')['environment'];assert env['ASAN_OPTIONS']=='detect_leaks=0:halt_on_error=1' and env['UBSAN_OPTIONS']=='halt_on_error=1:print_stacktrace=1' and env['LD_PRELOAD'].split(':')[0]=='/usr/lib64/libasan.so.8.0.0'
x=dict(passed=True,mode=mode,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],health=health(b),player_pose=b['player']['position'],ilend_pose=ally(b)['position'],corpses=corpses,earned_state_codec_live_and_restart_checks=sha(r/'forward-audit.json'),scope='Typed EFSH ingestion on an earned save and own-save cold continuation only. No shader playback, new door traversal or full course acceptance.')
(r/'ingestion-audit.json').write_text(json.dumps(x,indent=2)+'\n');print(json.dumps(x))
