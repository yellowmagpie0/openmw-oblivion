"""Native/process audit; rendered captures and implementation gates remain separate."""
import argparse, hashlib, json, re, sys, datetime
from pathlib import Path
from tes4_runtime_state import load_save

def require(condition, message):
    if not condition:
        raise ValueError(message)

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):
            h.update(block)
    return h.hexdigest()

def report(course):
    p=course/'scenario.json'
    require(p.is_file(),f'No terminal scenario report: {course}')
    result=json.loads(p.read_text())
    require(result['passed'] and result['actions_complete'] and result['exit_code']==0,
            f'Course did not pass completely: {course}')
    require(not result['forbidden_log_findings'] and not result['unreviewed_error_log_findings'],
            f'Unreviewed runtime findings: {course}')
    require((course/'sdl-input.txt').read_bytes()==(course/'sdl-input.txt.delivered').read_bytes(),
            f'Issued and delivered ordinary inputs differ: {course}')
    return result

def events(course):
    return [json.loads(x) for x in (course/'combat-events.jsonl').read_text().splitlines()]

def save(course):
    paths=list(course.glob('userdata/saves/*/Quicksave.omwsave'))
    require(len(paths)==1,f'Expected one actual Quicksave: {course}')
    return paths[0],load_save(paths[0])

def check_saved_receipt(course,run,path,state):
    saves=[x for x in run if x['event']=='save-complete']
    require(saves and saves[-1]['save_sha256']==digest(path),
            f'Final F5 has no matching native receipt: {course}')
    live=course/saves[-1]['live']
    require(digest(live)==saves[-1]['live_sha256'],f'Final native snapshot hash mismatch: {course}')
    require(state==json.loads(live.read_text()),f'Parsed save differs from its native receipt snapshot: {course}')
    ends=[x for x in run if x['event']=='run-end']
    require(len(ends)==1 and ends[0]['error_count']==0 and ends[0]['pending_count']==0,
            f'Run did not finish with clear native error/pending counts: {course}')
    return saves[-1]

def compare_entry(expected,actual,path=''):
    if expected==actual:
        return []
    if isinstance(expected,dict) and isinstance(actual,dict):
        require(expected.keys()==actual.keys(),f'Changed state keys at {path}')
        return sum((compare_entry(expected[k],actual[k],path+'/'+k) for k in expected),[])
    if isinstance(expected,list) and isinstance(actual,list):
        require(len(expected)==len(actual),f'Changed state length at {path}')
        return sum((compare_entry(a,b,path+'/'+str(i)) for i,(a,b) in enumerate(zip(expected,actual))),[])
    require(re.fullmatch(r'/native_animated_references/\d+/bodies/\d+/linear_velocity/[012]',path)
            and isinstance(expected,(float,int)) and isinstance(actual,(float,int))
            and abs(expected-actual)<=1e-5,f'Unexpected restored value at {path}: {expected!r} -> {actual!r}')
    return [{'path':path,'expected':expected,'actual':actual}]

def required_course_patterns(manifest):
    """Use the actual selected input gates, including their producer/phase scope."""
    patterns=set()
    def visit(action):
        if action['type'] in ('wait_log','repeat_until_log'):
            patterns.add(action['pattern'])
        for field in ('actions','cleanup_actions'):
            for child in action.get(field,[]):
                visit(child)
        for children in action.get('cases', {}).values():
            for child in children:
                visit(child)
    for action in manifest['actions']:
        visit(action)
    requirements=[
        ('wall_exit','Joined wall exit ready',4),
        ('physical_room_traversal','Joined room approach reached',6),
        ('corpse_loot','Sewer corpse loot transferred',7),
        ('initial_weapon_equipped','Sewer Club equipped',7),
        ('first_chest_loot','Substructure chest loot transferred',9),
        ('armor_equipped','Joined both armor equipped',10),
        ('axe_equipped','Joined axe equipped',10),
        ('initial_potion_healing','Joined potion healed',10),
        ('rolling_logs_released','TES4 animated reference released: content:oblivion.esm:03b49e',None),
        ('healing_chest_loot','Healing chest transferred',39),
        ('later_potion_healing','Substructure potion restored health',40),
        ('physical_final_sewers_entry','Joined Player entered Imperial Sewers03',64),
    ]
    result={}
    for name,needle,segment in requirements:
        matches=[pattern for pattern in patterns if needle in pattern
                 and (pattern.startswith(f'Escape segment {segment}:') if segment is not None
                      else not pattern.startswith('Escape segment'))]
        require(len(matches)==1,f'Expected one selected gate for {name}, found {matches}')
        result[name]=re.compile(matches[0])
    return result

def audit_early_rat_recovery(course, game_pid, path):
    record=json.loads(path.read_text())
    require(course.name=='full-escape-continuous-18' and game_pid==401988
            and record['game_pid']==game_pid and record['driver_pid']==401964,
            'Early recovery belongs to another run')
    actions=record['actions']
    signature=hashlib.sha256(json.dumps(actions,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    require(len(actions)==45 and signature=='5fb80fa1f27847a2f3810f3ea097f1799d600481f669e82f795603e57fcee922',
            'Unreviewed early recovery input sequence')
    require(all(a.get('returncode',0)==0 for a in actions), 'Manual recovery command failed')
    signals=[(a['process_action'],a['pid']) for a in actions if 'process_action' in a]
    require(signals==[('SIGSTOP',401964),('SIGCONT',401964)] and record['driver_resumed'],
            'Original driver pause/resume differs from reviewed actions')
    handles=json.loads((course/'capture-handles.json').read_text())
    require(handles['driver_pid']==401964 and handles['game_pid']==game_pid, 'Changed original driver/game identity')
    segment=None;casts=[];resumed=None
    with (course/'process.log').open() as stream:
        for line in stream:
            match=re.search(r'Escape outer segment (\d+) ready',line)
            if match:
                segment=int(match[1])
                if segment==4:
                    resumed=line.strip();break
            if segment==3:
                match=re.match(r'^\[[0-9:.]+ I\] M16 native cast release: spell=content:oblivion.esm:000136 applied=1 health=([0-9.eE+-]+) magicka=([0-9.eE+-]+)',line)
                if match:casts.append({'health':float(match[1]),'magicka':float(match[2]),'line':line.strip()})
    require([c['health'] for c in casts]==[40,48,56,64,72,80], 'Manual recovery lacks six real healing effects')
    require(all(c['magicka']>=0 for c in casts), 'Manual cast resource accounting invalid')
    require(resumed is not None, 'Original course did not advance after manual recovery')
    require(int((course/'sdl-input.txt.delivered').read_text().splitlines()[-1].split()[0])>380,
            'Original automated input did not resume')
    receipt=next(e for e in events(course) if e['event']=='save-complete' and e['sequence']==21)
    index=json.loads((course/'retained-saves/index.json').read_text())
    archive=Path(next(r['archive'] for r in index['records'] if r['sequence']==21))
    require(digest(archive)==receipt['save_sha256'], 'Manual F5 archive hash differs')
    live=course/receipt['live'];require(digest(live)==receipt['live_sha256'], 'Manual F5 native hash differs')
    state=load_save(archive)
    require(state==json.loads(live.read_text()), 'Manual F5 is not native-exact')
    require(state['player']['actor_values']['health.current']==80, 'Manual F5 lacks normal full recovery')
    require(record['observed_recovery']=={'entry_health':32,'final_health':80,'native_applied_casts':6,'normal_F5_sequence':21,'no_save_load':True},
            'Recovery report differs from reviewed observation')
    return {'record':str(path),'sha256':digest(path),'action_signature':signature,
            'scope':'Reviewed ordinary X11 inventory/spell selection, six self-heals and normal F5. Driver temporarily paused and resumed; no game restart or world/stat/quest injection.',
            'actions':actions,'native_healing_effects':casts,'manual_save_receipt':receipt,
            'retained_save_sha256':digest(archive),'parsed_native_exact':True,'original_driver_resumed_at':resumed}

def audit_manual_inputs(course, game_pid):
    files=list(course.glob('manual-*-input-*.json'))
    require(all(p.name in ('manual-landing-input-01.json','manual-early-rat-recovery-input-01.json') for p in files), 'Unreviewed manual input record')
    result=[]
    for path in files:
        if path.name=='manual-early-rat-recovery-input-01.json':
            result.append(audit_early_rat_recovery(course,game_pid,path));continue
        record=json.loads(path.read_text())
        require(record['game_pid']==game_pid, 'Manual input used another game process')
        actions=record['actions']
        require(len(actions)==2 and actions[0]['command']==['xdotool','keydown','d']
                and actions[1]['command']==['xdotool','keyup','d']
                and all(x['returncode']==0 for x in actions), 'Unreviewed manual input commands')
        duration=(datetime.datetime.fromisoformat(actions[1]['utc'])
                  -datetime.datetime.fromisoformat(actions[0]['utc'])).total_seconds()
        require(.4<=duration<=.8, 'Manual sidestep duration differs from reviewed input')
        response=record['observed_response'];before=response['before'];after=response['first_after']
        require(response['same_game_pid']==game_pid and response['original_driver_resumed'], 'Manual recovery did not preserve the original course')
        require(after['position'][0]-before['position'][0]>60 and after['position'][1]-before['position'][1]>60,
                'No observed physical sidestep/forward movement')
        patterns={
            'movement':re.compile(re.escape('['+after['local_time'])+r'[^\n]*Tutorial descent position: x='
                +re.escape(str(after['position'][0]))+r' y='+re.escape(str(after['position'][1]))),
            'landing':re.compile(re.escape('['+response['landing_gate_local_time'])+r'[^\n]*Tutorial descent reached landing')}
        found={}
        with (course/'process.log').open() as stream:
            for line in stream:
                for name,pattern in patterns.items():
                    if name not in found and pattern.search(line):found[name]=line.strip()
                if len(found)==len(patterns):break
        require(found.keys()==patterns.keys(), 'Manual input has no matching read-only gameplay response')
        result.append({'record':str(path),'sha256':digest(path),'scope':'Paired ordinary X11 D key events, separately audited from automated SDL receipts',
                       'duration_seconds':duration,'gameplay_observations':found,'actions':actions})
    return result

def audit(course,reload):
    first=report(course)
    require(not any('--load-savegame' in x for x in first['command']), 'Escape was loaded from a save')
    require('new-game=1' in (course/'config/openmw.cfg').read_text(),'Not an original new game')
    run=events(course)
    require(len({x['pid'] for x in run})==1,'Multiple game processes in escape evidence')
    require(sum(x['event']=='run-start' for x in run)==1,'Escape must start exactly once')
    require(not any(x['event']=='load-complete' for x in run),'Escape includes a load/restart')
    allowed_deferred='deferred command=setunconscious unit=content:oblivion.esm:01ecbd@oblivion.esm/quest/unit=0'
    deferred=[x for x in run if x['event']=='unsupported-command']
    require(len(deferred)==5 and all(x['tick']==1 and x['message']==allowed_deferred for x in deferred),
            'Unexpected unsupported command outside the separately audited five Peryite startup deferrals')
    manual_inputs=audit_manual_inputs(course,run[0]['pid'])
    manifest=json.loads(Path(first['manifest']).read_text())
    required_patterns=required_course_patterns(manifest)
    observations={}
    sequence=[];exit_count=0;log_hash=hashlib.sha256()
    story_traces={
        'ownership':'setownership ref=content:oblivion.esm:022812 owner=content:oblivion.esm:0150bc applied=true',
        'faction':'setfactionrank ref=content:oblivion.esm:01e703 faction=content:oblivion.esm:014a20 rank=1',
        'bounty':'setcrimegold ref=dynamic:player:0000000000000001 requested=0'}
    story_counts={key:0 for key in story_traces}
    with (course/'process.log').open('rb') as stream:
        for raw in stream:
            log_hash.update(raw);line=raw.decode('utf-8',errors='replace')
            match=re.search(r'Escape outer segment (\d+) ready',line)
            if match:sequence.append(int(match[1]))
            if 'M5 door activation: result=teleport ref=content:oblivion.esm:04bed7' in line:exit_count+=1
            for key,trace in story_traces.items():story_counts[key]+=trace in line
            for name,pattern in required_patterns.items():
                if name not in observations and pattern.search(line):
                    observations[name]=line.strip()
    require(sequence==list(range(1,68)),f'Not all67 observer segments completed in order: {sequence}')
    require(observations.keys()==required_patterns.keys(),
            f'Missing selected course observations: {required_patterns.keys()-observations.keys()}')
    require(exit_count==1,'Original exterior door must teleport exactly once')
    require(all(value==1 for value in story_counts.values()),f'Original story mutations must execute once: {story_counts}')
    p,state=save(course)
    final_receipt=check_saved_receipt(course,run,p,state)
    cg=next(q for q in state['quests'] if q['quest'].endswith(':02466e'))
    mq=next(q for q in state['quests'] if q['quest'].endswith(':01e723'))
    milestones={19,24,26,30,40,50,54,56,58,65,74,76,80,87,88}
    require(milestones<=set(cg['completed_stages']),'Missing original CharacterGen milestone')
    require(88 in mq['completed_stages'],'Missing distinct MQ01 exterior stage88')
    require(state['player']['class']=='content:oblivion.esm:023787','Warrior class not retained')
    require(state['player']['birthsign']=='content:oblivion.esm:01fd9d','Warrior birthsign not retained')
    count=lambda base:sum(x['count'] for x in state['player']['inventory'] if x['base'].endswith(':'+base))
    require(count('0250a0')==1 and count('01e702')==0,'Amulet/key inventory is wrong')
    equipment={}
    for base in ['0661c1','0661c3','090613']:
        equipped=[x for x in state['player']['inventory'] if x['base'].endswith(':'+base)
                  and x['count']>0 and x['equipped_slots']!=0]
        require(equipped,f'Chosen course equipment not retained/equipped: {base}')
        equipment[base]=equipped
    require(state['player']['actor_values']['health.current']>0,'Player dead at exterior')
    require(state['player']['cell']=='content:oblivion.esm:005e3d','Final save is not in the sewer-exit Tamriel exterior')
    require(state['player']['character_generation']['tracking'] is False,'Chargen tracking still active')
    require(state['player']['character_generation']['skill_uses'] is None,'Chargen skill history not cleared')
    lives={x['actor']:x for x in state['native_actor_life']}
    key=lambda local:'content:oblivion.esm:'+local
    require(lives[state['player']['reference']]['phase']==0,'Player native life is not Alive at exterior')
    require(all(lives[key(local)]['phase']==1 and lives[key(local)]['killer']==state['player']['reference']
                for local in ['037e40','037e41','037e42']), 'Missing native Player victories for the three early rats')
    require(all(lives[key(local)]['phase']==1 for local in ['014a2c','014a30','0178c3','0178c4']),
            'First ambush death history not retained')
    require(not any(lives[x['actor']]['phase']==1 for x in state['native_bound_abilities']),
            'Terminal actor still owns bound equipment')
    require(lives[key('03b52e')]['phase']==1 and lives[key('03b52e')]['killer']=='dynamic:player:0000000000000001','No genuine Player shaman victory')
    require(lives[key('032a15')]['phase']==1 and lives[key('032a15')]['killer']=='null','Renault scripted death not retained')
    require(next(x for x in state['native_actor_bases'] if x['base']==key('02349f'))['essential'] is False,'Renault mortality not retained')
    require(next(x for x in state['native_death_counts'] if x['base']==key('02349f'))['count']==1,'Renault death count is not exactly one')
    for local in ['032a18','032a16']:
        require(lives[key(local)]['phase']==1 and lives[key(local)]['killer']=='null','Emperor/Glenroy scripted death not retained')
    require(lives[key('032a17')]['phase']==0,'Baurus died')
    require(lives[key('01e703')]['phase']==1,'Final assassin survived')
    require(lives[key('0666a7')]['phase']==1
            and lives[key('0666a7')]['killer']==state['player']['reference'], 'No native Player western-rat victory')
    owner=next(x for x in state['references'] if x['key']==key('022812'))
    require(owner['owner']==key('0150bc') and owner['lock_level']==100, 'Original door ownership/lock not retained')
    faction=next(x for x in state['native_actor_bases'] if x['base']==key('01e6ff'))
    require(faction['factions']==[[key('014a20'),1],[key('0227bc'),0]], 'Final assassin faction replacement not retained')
    bounty=next(x for x in state['native_actor_values'] if x['actor']==state['player']['reference'])['bounty']
    require(bounty=={'normal':0,'shivering_isles':0,'player_in_shivering_isles':False}, 'Exit legal bounty reset not retained')
    for k in ['native_combat_engagements','pending_death_events','physical_action_owners','native_spell_casts','native_magic_projectiles']:
        require(not state[k],f'Unfinished native work: {k}')
    require(not any(key.startswith('native.fire.') for ref in state['references'] for key in ref.get('custom_state',{})),
            'Unfinished environmental fire at the exterior checkpoint')
    for entry in (course/'test-data/events.txt').read_text().splitlines():
        require(not entry.strip() or re.fullmatch(r'at \d+ command \S+ (?:GetStage|GetDead)',entry),'World-changing scheduled command')
    second=report(reload);follow=events(reload)
    loads=[x for x in follow if x['event']=='load-complete'];require(len(loads)==1,'Reload must load exactly once')
    require(loads[0]['pid']!=run[0]['pid'],'Reload did not use a fresh game process')
    require(loads[0]['save_sha256']==digest(p),'Reload did not use the new uninterrupted course save')
    live=reload/loads[0]['live'];require(digest(live)==loads[0]['live_sha256'],'Reload live snapshot hash mismatch')
    differences=compare_entry(state,json.loads(live.read_text()))
    rp,restored=save(reload)
    reload_receipt=check_saved_receipt(reload,follow,rp,restored)
    for k in ['class','birthsign','inventory']:
        require(state['player'][k]==restored['player'][k],f'Reload changed Player {k}')
    require(state['player']['actor_values']['health.current']==restored['player']['actor_values']['health.current'],'Reload changed health')
    require(state['native_death_counts']==restored['native_death_counts'],'Reload changed death history')
    require(restored['player']['cell']==state['player']['cell'],'Reload continuation left the exterior')
    require(next(x for x in restored['native_actor_life'] if x['actor']==restored['player']['reference'])['phase']==0,
            'Player native life is not Alive after reload continuation')
    for k in ['native_combat_engagements','pending_death_events','physical_action_owners','native_spell_casts','native_magic_projectiles']:
        require(not restored[k],f'Unfinished native work after reload continuation: {k}')
    require(not any(key.startswith('native.fire.') for ref in restored['references'] for key in ref.get('custom_state',{})),
            'Unfinished environmental fire after exterior reload')
    require(not any(x['event']=='unsupported-command' for x in follow),'Reload reached unsupported command')
    require(next(x for x in run if x['event']=='run-end')['unsupported_count']==5, 'Full native deferral counter differs from reviewed events')
    require(next(x for x in follow if x['event']=='run-end')['unsupported_count']==0, 'Reload native unsupported counter is not clear')
    require(next(x for x in restored['native_actor_bases'] if x['base']==key('01e6ff'))['factions']==faction['factions'], 'Reload changed saved faction membership')
    restored_owner=next(x for x in restored['references'] if x['key']==key('022812'))
    require(restored_owner['owner']==owner['owner'] and restored_owner['lock_level']==100, 'Reload changed door ownership/lock')
    require(next(x for x in restored['native_actor_values'] if x['actor']==restored['player']['reference'])['bounty']==bounty, 'Reload changed legal bounty')
    reload_log=(reload/'process.log').read_text()
    require(not any(trace in reload_log for trace in story_traces.values()), 'Reload replayed an original story mutation')
    return {'passed':True,'scope':'Uninterrupted original new-game process/native-state/exterior/reload audit; manual rendered review and implementation gates are separate',
            'course':str(course),'reload':str(reload),'game_pid':run[0]['pid'],'reload_pid':loads[0]['pid'],
            'segments':sequence,'exterior_teleports':exit_count,'save_sha256':digest(p),'reload_save_sha256':digest(rp),
            'log_sha256':log_hash.hexdigest(),'restore_projection_differences':differences,
            'escape_save_receipt':final_receipt,'reload_save_receipt':reload_receipt,
            'automated_replay_issued_delivered_inputs_match':True,'manual_ordinary_input_recoveries':manual_inputs,
            'entirely_unattended':not manual_inputs,'parsed_saves_equal_native_receipts':True,
            'selected_course_observations':observations,'retained_course_equipment':equipment,
            'player':{k:state['player'][k] for k in ['cell','position','class','birthsign']},
            'health':state['player']['actor_values']['health.current'],'chargen':cg,'mq01':mq,
            'known_remote_startup_deferrals':deferred,'original_story_mutation_counts':story_counts,
            'retained_door_owner':owner['owner'],'final_assassin_factions':faction['factions'],'legal_bounty':bounty}

def audit_course(course, input_save=None, allow_startup_deferrals=False):
    """Reuse the retained-course audits for one focused or reload process."""
    scenario = report(course)
    run = events(course)
    require(run and len({e['pid'] for e in run}) == 1, 'Mixed/missing native process identity')
    endings = [e for e in run if e['event'] == 'run-end']
    require(len(endings) == 1, 'Missing native run-end')
    end = endings[0]
    require(end['error_count'] == 0 and end['pending_count'] == 0, 'Native error/pending work')
    deferred = [e for e in run if e['event'] == 'unsupported-command']
    allowed = 'deferred command=setunconscious unit=content:oblivion.esm:01ecbd@oblivion.esm/quest/unit=0'
    if allow_startup_deferrals:
        require(len(deferred) == 5 and all(e['tick'] == 1 and e['message'] == allowed for e in deferred),
                'Unreviewed startup deferral')
    else:
        require(not deferred and end['unsupported_count'] == 0, 'Unsupported native command')
    require(not list(course.glob('manual-*-input-*.json')), 'Focused artifact requires recorded runner inputs only')
    loads = [e for e in run if e['event'] == 'load-complete']
    entry = None
    if input_save is not None:
        require(len(loads) == 1 and loads[0]['save_sha256'] == digest(input_save), 'Wrong checkpoint load')
        expected = load_save(input_save)
        actual = json.loads((course / loads[0]['live']).read_text())
        migration = None
        if expected['schema_version'] == 89 and actual['schema_version'] == 90:
            require(all(b.get('factions') is None for b in expected['native_actor_bases']), 'Unexpected legacy faction data')
            for base in expected['native_actor_bases']:
                base['factions'] = None
            expected['schema_version'] = 90
            migration = '89 -> 90; absent optional shared-base faction replacements default to None'
        entry = {'source_save_sha256': digest(input_save), 'migration': migration,
                 'differences': compare_entry(expected, actual)}
    else:
        require(not loads, 'New-game run unexpectedly loaded state')
    receipts = [e for e in run if e['event'] == 'save-complete']
    records = json.loads((course / 'retained-saves/index.json').read_text())['records']
    require(receipts and len(receipts) == len(records), 'Missing retained actual saves')
    checked = []
    for receipt, record in zip(receipts, records):
        require(record['status'] == 'copied byte-identically' and record['sequence'] == receipt['sequence'],
                'Save archival failed or was superseded')
        path = Path(record['archive'])
        live = course / receipt['live']
        require(digest(path) == receipt['save_sha256'] and digest(live) == receipt['live_sha256'], 'Save/native hash mismatch')
        require(load_save(path) == json.loads(live.read_text()), 'Parsed save differs from native snapshot')
        checked.append({'sequence': receipt['sequence'], 'archive': str(path), 'sha256': receipt['save_sha256']})
    path, state = save(course)
    check_saved_receipt(course, run, path, state)
    result = {'passed': True, 'scope': 'One course: native, ordinary-input delivery, every retained save and checkpoint restoration; rendered review separate',
              'game_pid': run[0]['pid'], 'native_end': end, 'entry': entry, 'saves': checked,
              'save_sha256': digest(path), 'save': str(path), 'rendered_review': 'pending'}
    (course / 'regression-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    return result, state


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--course',required=True,type=Path);parser.add_argument('--reload',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path);args=parser.parse_args()
    try:
        result=audit(args.course,args.reload)
    except (ValueError,KeyError,StopIteration,OSError) as error:
        result={'passed':False,'reason':str(error),'course':str(args.course),'reload':str(args.reload)}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ['passed','reason'] if k in result}))
    raise SystemExit(0 if result['passed'] else 1)


if __name__ == '__main__':
    main()
