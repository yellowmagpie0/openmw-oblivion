import json, hashlib, sys, struct
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'scripts'))
from tes4_runtime_state import load_save
b=Path(sys.argv[1]).resolve()
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text())
def equal(a,b):
    if isinstance(a,dict) and isinstance(b,dict):return a.keys()==b.keys() and all(equal(a[k],b[k]) for k in a)
    if isinstance(a,list) and isinstance(b,list):return len(a)==len(b) and all(equal(x,y) for x,y in zip(a,b))
    if isinstance(a,float) or isinstance(b,float):return type(a) in (float,int) and type(b) in (float,int) and a==b and struct.pack('<d',a)==struct.pack('<d',b)
    return type(a)==type(b) and a==b
recipe=read(b/'preregistration.json');initial=load_save(b/'staged-input.omwsave')
check_initial=initial['schema_version']==69 and initial['combat_controllers']=={} and initial['combat_rng_state']==15
assert check_initial and sha(b/'staged-input.omwsave')==recipe['input_sha256']
def check(condition, message):
    if not condition:raise AssertionError(message)
phases={};states={};pins={};epochs={}
for name in ('first','second'):
    out=b/name
    check(read(out/'scenario.json')['passed'],name+' scenario failed')
    check(read(out/'launcher.json')['scenario_returncode']==0,name+' launcher failed')
    epochs[name]=read(out/'process-epoch.json')
    events=[json.loads(l) for l in (out/'events.jsonl').read_text().splitlines()]
    phases[name]=[e for e in events if e['event']=='ordinary-hit-phases']
    index=read(out/'snapshot-index.json');states[name]=[]
    for k in sorted(index,key=int):
        row=index[k];event=row['event'];p=Path(row['snapshot']);l=out/event['live']
        check(sha(p)==event['save_sha256']==row['sha256'],'snapshot hash')
        check(sha(l)==event['live_sha256'],'live hash')
        s=load_save(p);check(equal(s,read(l)),'binary/live mismatch '+str(p));states[name].append(s)
        pins[str(p)]=sha(p);pins[str(l)]=sha(l)
    for e in events:
        if 'live' in e:check(sha(out/e['live'])==e['live_sha256'],'live event hash')
    check(events[-1]['event']=='run-end' and events[-1]['error_count']==0,'clean run end')
    check(sum('M15 melee hit committed:' in l for l in (out/'process.log').read_text().splitlines())==(1 if name=='first' else 0),'contact count')
    pins[str(out/'scenario.json')]=sha(out/'scenario.json');pins[str(out/'events.jsonl')]=sha(out/'events.jsonl')
check(len(phases['first'])==1 and not phases['second'],'phase count')
p=phases['first'][0];check(p['action']==2,'cause action')
check(p['blocked_dispatch'] and not p['counter_selected'] and not p['counter_prepared'],'cached blocked tail')
check(not p['speech_requested'],'native prefix must skip speech request')
check(p['draws']==recipe['expected_contact_draws']==4 and p['prefix_draws']==2,'native four-draw hit budget')
check(p['rng_before']==recipe['expected_rng_before_contact']==5741206 and p['rng_after']==recipe['expected_rng_after_contact']==4277529146,'constructor and phased-hit RNG')
check(states['first'][0]['combat_controllers']=={} and states['first'][0]['combat_rng_state']==15,'baseline before authored StartCombat')
cursor=p['rng_before']
for _ in range(p['draws']):cursor=(cursor*214013+2531011)&0xffffffff
check(cursor==p['rng_after'],'independent CRT draw count')
middle=states['first'][-2];f=states['first'][-1];z=states['second'][-1]
check(equal(middle,load_save(b/'second/input-pristine.omwsave')),'fresh process source')
check(equal(middle,read(b/'second/live-2-2.json')),'exact initial restoration')
check(middle['physical_actions']=={'next':3,'pending':[]},'consumed single cause')
check(f['physical_actions']==z['physical_actions']=={'next':4,'pending':[]},'next intent cancelled by StopCombat')
for state in (middle,f,z):
    values=next(r['values'] for r in state['native_actor_values'] if r['actor']=='dynamic:player:0000000000000001')
    check(equal(values[8],[80,0,0,-1.009374976158142]),'exact one Player Health debit')
    check(equal(values[10],[140,0,0,0]),'no Player block fatigue debit')
    drop=next(r for r in state['references'] if r['key']=='dynamic:native-reference:0000000000000001')
    check(drop['base']=='content:oblivion.esm:090615' and drop['dropper']=='content:oblivion.esm:01fc43' and drop['item_condition']==55.70000076293945,'native worn sword drop identity and condition')
for state in (f,z):
    values=next(r['values'] for r in state['native_actor_values'] if r['actor']=='content:oblivion.esm:01fc43')
    check(values[8][0]==16 and equal(values[10],[155,0,0,0]),'NPC Health unchanged and fatigue naturally recovered')

check(equal(middle['combat_controllers'],recipe['expected_mid_controller']),'actual script-created controller in recovery save')
check(f['combat_controllers']==z['combat_controllers']=={},'StopCombat removes current controller after recovery')
check(middle['combat_rng_state']==p['rng_after']==f['combat_rng_state']==z['combat_rng_state'],'no random replay')
for actor, kind, action in (('content:oblivion.esm:01fc43',1,7),('dynamic:player:0000000000000001',0,6)):
    row=next(r for r in middle['native_melee_states'] if r['actor']==actor)
    check(row['reaction'] is not None and row['reaction']['contact_id']==2 and row['reaction']['kind']==kind and not row['reaction']['inactive'],'actual active middle recovery '+actor)
    av=next(r for r in middle['native_actor_values'] if r['actor']==actor)
    check(av['process_action']==action,'actual middle process action')
    if kind==0:check(row['first_person']['reaction']['contact_id']==2 and row['first_person']['reaction']['kind']==0,'actual middle first person BlockHit')
compared=[]
def without_idle_clock(rows):
    rows=json.loads(json.dumps(rows))
    for row in rows:
        if row.get('first_person'):
            row['first_person'].pop('clock',None)
            for held in row['first_person']['held_blocks']:
                check(held['inactive'], 'no active first-person held block after release')
                held.pop('animation_time',None)
                held.pop('sequence_timing',None)
    return rows
check(equal(without_idle_clock(f['native_melee_states']),without_idle_clock(z['native_melee_states'])),'final melee authority except idle clock and inactive held sample times')
for key in f:
    if key.startswith('native_') and key not in ('native_movement','native_loose_weapons','native_actor_manager_time','native_actor_update_times','native_animation_clocks','native_physical_blend_time_cache','native_melee_states') or key in ('physical_actions','physical_action_owners','combat_rng_state','spoken_dialogue_infos','disposition_modifiers','ignore_friendly_hits','pending_death_events','next_death_event','script_instances','combat_controllers'):
        check(equal(f[key],z[key]),'final semantic mismatch '+key);compared.append(key)
for actor in ('dynamic:player:0000000000000001','content:oblivion.esm:01fc43'):
    for s in (f,z):
        for row in s['native_melee_states']:
            if row['actor']==actor:
                check(not row['strike'] and not row['reaction'] and not row['retiring_sequences'] and not row['held_blocks'],'actor recovery')
                if row['first_person']:check(not row['first_person']['reaction'] and not row['first_person']['strike'],'first person recovery')
    rows=[next((r for r in s['references'] if r['key']==actor),None) for s in (f,z)]
    check((rows[0] is None)==(rows[1] is None),'reference availability')
    if rows[0]:check(equal({k:v for k,v in rows[0].items() if k!='position'},{k:v for k,v in rows[1].items() if k!='position'}),'full actor inventory/ref state')
check(len(f['native_loose_weapons'])==len(z['native_loose_weapons'])==len(middle['native_loose_weapons']),'drop count')
for a,c in zip(f['native_loose_weapons'],z['native_loose_weapons']):
    check(equal({k:v for k,v in a.items() if k not in ('body','root_local')},{k:v for k,v in c.items() if k not in ('body','root_local')}),'drop identity')
    check(all(equal(a['body'][k],c['body'][k]) for k in ('record','node_record','native_motion')),'drop body identity')
check((epochs['first']['pid'],epochs['first']['start_ticks'])!=(epochs['second']['pid'],epochs['second']['start_ticks']),'fresh epoch')
check(epochs['first']['executable_sha256']==epochs['second']['executable_sha256']==read(b/'binary-pin.json')['sha256'],'same binary')
for phase,expected_state in [('first',initial),('second',middle)]:
    out=b/phase;events=[json.loads(line) for line in (out/'events.jsonl').read_text().splitlines()]
    loads=[e for e in events if e['event']=='load-complete'];check(len(loads)==1,'one actual load per process')
    restored=read(out/loads[0]['live'])
    if phase=='second':check(equal(restored,expected_state),'full fresh recovery initial logical state')
    else:
        for key in expected_state:
            if (key.startswith('native_') and key not in ('native_movement','native_physical_blend_time_cache')) or key in ('physical_actions','physical_action_owners','combat_rng_state','combat_controllers'):
                check(equal(restored[key],expected_state[key]),'legacy import preserves supplied authority '+key)
    check(sum('startcombat actor=content:oblivion.esm:01fc43' in line for line in (out/'process.log').read_text().splitlines())==(1 if phase=='first' else 0),'no repeated constructor after load')
    if 'sanitizer_options' in recipe:
        env=read(out/'actual-engine-environment.json')
        for k,v in recipe['sanitizer_options'].items():check((env[k].split(':')[0]==v if k=='LD_PRELOAD' else env[k]==v),'actual sanitizer options')
    for name in ('input-pristine.omwsave','actual-engine-environment.json','process-epoch.json','process.log'):
        pins[str(out/name)]=sha(out/name)
check(sha(recipe['source'])==recipe['source_sha256'],'source preserved')
check(sha(read(b/'binary-pin.json')['path'])==read(b/'binary-pin.json')['sha256'],'runtime binary preserved')
r={'passed':True,'scope':'One normal contact after authored StartCombat creates current controller; exact binary/live serialization and fresh restoration; logical actor recovery, complete target inventory and loose-item identity. World clock/AI scheduling, actor float Z, loose trajectory and inactive first-person held-block sample times are not compared. Inactive held-block group, retirement, inactivity and pose identity still match.','phase':p,'runtime_schema':69,'controller_in_recovery':middle['combat_controllers'],'constructor_draws':1,'native_oracles':recipe['native_expectation_evidence'],'compared_final_fields':compared,'first_snapshot_count':len(states['first']),'second_snapshot_count':len(states['second']),'physical_actions':f['physical_actions'],'drop_count':len(f['native_loose_weapons']),'epochs':epochs,'pins':pins,'library_identity_scope':'Executable pinned in each actual process; this course monitor did not capture mapped library identities. Prior producer course library evidence is separate.','first_import_scope':'Legacy schema61 fixture upgraded by codec then normal loader; supplied native authority preserved except absent movement/blend caches. Generic projection/default/content normalization is not claimed byte-exact. Second recovery load is wholly exact.','full_M15S4_acceptance':False}
(b/'semantic-verification.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({k:r[k] for k in ('passed','phase','drop_count','physical_actions')}))
