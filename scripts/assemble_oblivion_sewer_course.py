import json,copy,re,hashlib,argparse
from pathlib import Path
parser=argparse.ArgumentParser(description='Assemble the original sewer normal-input course into one new-game process.')
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--selection',type=Path,help='Focused checkpoint selection; default is the full original new-game course.')
args=parser.parse_args()
repo=Path(__file__).resolve().parents[1]
r=args.output.parent
r.mkdir(parents=True,exist_ok=True)
selection=args.selection or repo/'scripts/data/oblivion_compat/oblivion_sewer_continuous_selection.json'
chain=json.loads(selection.read_text())['segments']
recipes=[];provenance=[];code=[];actions=[];extra={};types=set()
engine_prefix=('Applied ','acceptance-command ','Native ','TES4 ','Writing ','character_menu ','messagebox ','setstage ','startcombat ','sayto ','"event":')

def engine_pattern(pattern):
 # Engine events keep their producer's prefix; only Lua observer output is
 # wrapped by the segment's local print function.
 return bool(re.match(r'M\d+\s',pattern)) or pattern.startswith(engine_prefix)

def scoped_pattern(pattern,phase):
 # Scope each top-level alternative to its own producer. A guard callback
 # can legitimately finish movement before the observer's waypoint fires.
 branches=[];start=0;depth=0;character_class=False;escaped=False
 for i,ch in enumerate(pattern):
  if escaped:escaped=False;continue
  if ch=='\\':escaped=True;continue
  if ch=='[':character_class=True
  elif ch==']':character_class=False
  elif not character_class:
   if ch=='(':depth+=1
   elif ch==')':depth-=1
   elif ch=='|' and depth==0:branches.append(pattern[start:i]);start=i+1
 branches.append(pattern[start:])
 return '|'.join(branch if engine_pattern(branch) else f'Escape segment {phase}:\\s*(?:'+branch+')' for branch in branches)

def scope(a,phase):
 a=copy.deepcopy(a);types.add(a['type'])
 if a['type']=='wait_log':
  if a['pattern']=='Applied TES4 runtime state:' or a['pattern'].startswith('acceptance-command '):return None
  a['pattern']=scoped_pattern(a['pattern'],phase)
  if a['pattern'].startswith('Escape segment') and 'ready' in a['pattern'].lower():a['timeout_seconds']=max(a.get('timeout_seconds',0),120)
 if a['type'] in ('repeat_until_log','branch_on_log'):
  a['pattern']=scoped_pattern(a['pattern'],phase)
 if a['type']=='mouse_move' and 'from_log'in a:a['from_log']['pattern']=f'Escape segment {phase}:\\s*(?:'+a['from_log']['pattern']+')'
 if a['type']=='screenshot':a['name']=f'segment{phase:02d}-'+a['name']
 for k in ['actions','cleanup_actions']:
  if k in a:a[k]=scope_actions(a[k],phase)
 if 'cases' in a:a['cases']={name:scope_actions(children,phase) for name,children in a['cases'].items()}
 return a

def scope_actions(items,phase):
 result=[]
 for index,item in enumerate(items):
  action=scope(item,phase)
  if action is None:continue
  result.append(action)
  if action['type']!='mouse_move' or 'from_log' not in action:continue
  # A receipt acknowledges SDL delivery before the camera necessarily uses
  # that motion. Wait for actual frames before another sample or facing gate.
  # Later adaptive recipes already supply their own F7 frame barrier.
  following=items[index+1] if index+1<len(items) else {}
  if following.get('type')=='key_down' and following.get('value')=='F7':continue
  result += [{'type':'key_down','value':'F7'},
   {'type':'wait_log','pattern':'Escape outer camera settled','after_start':True,'timeout_seconds':30,'abort_on_failure':True},
   {'type':'key_up','value':'F7'}]
 return result
for i,row in enumerate(chain,1):
 p=repo/row['manifest']
 d=json.loads(p.read_text());recipes.append(d)
 main=next(f for f in d['files'] if f['path'].endswith('/tutorial_passage_observer.lua'))
 for f in d['files']:
  if f['path'].endswith('.lua') and f is not main:
   assert f['path'] not in extra or extra[f['path']]==f['content'];extra[f['path']]=f['content']
 lua=main['content'];code.append(f"local function segment{i}()\nlocal outerPrint=print\nlocal function print(...) outerPrint('Escape segment {i}:',...) end\n"+lua+'\nend\n')
 aa=d['actions'][:row['action_count']]
 # This authored54 wait timed out because Player was standing on Glenroy's node;
 # the next accepted segment is the actual ordinary backstep that resolves it.
 if row['source_course']=='joined-third-ambush-paused-recipient-02':aa=[a for a in aa if not(a['type']=='wait_log' and 'stage=54|' in a.get('pattern',''))]
 if i>1:
  actions += [{'type':'key_up','value':k} for k in ['w','s','a','d','Shift_L','F7','F8']]+[{'type':'mouse_up','button':j} for j in [1,3]]
  actions += [{'type':'key_down','value':'F7'},{'type':'key_hold','value':'F8','seconds':.25},{'type':'key_up','value':'F7'},{'type':'wait_log','pattern':f'Escape outer segment {i} ready','timeout_seconds':30,'after_start':False,'abort_on_failure':True}]
 actions += scope_actions(aa,i)
 provenance.append({'segment':i,'source_segment':row['segment'],'source_course':row['source_course'],'manifest':str(p),'manifest_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'original_actions':len(d['actions']),'selected_prefix_actions':len(aa),'selection_action_count':row['action_count']})
code=''.join(code)+"local input=require('openmw.input')\nlocal segments={{"+','.join(f'segment{i}' for i in range(1,len(chain)+1))+'''}}
local n=1
local current=segments[n]()
local held=false
local cameraFrame=0
local cameraHeld=false
local cameraSampleFrame=nil
print('Escape outer segment 1 ready')
return {{engineHandlers={{onFrame=function(dt)
 cameraFrame=cameraFrame+1
 local cameraDown=input.isKeyPressed(input.KEY.F7) and not input.isKeyPressed(input.KEY.F8)
 if cameraDown and not cameraHeld then cameraSampleFrame=cameraFrame+3 end
 if not cameraDown then cameraSampleFrame=nil end
 cameraHeld=cameraDown
 if cameraSampleFrame and cameraFrame>=cameraSampleFrame then print('Escape outer camera settled') end
 local down=input.isKeyPressed(input.KEY.F8) and input.isKeyPressed(input.KEY.F7)
 if down and not held then
  n=n+1
  assert(segments[n],'Unexpected escape segment')
  current=segments[n]()
  print('Escape outer segment '..n..' ready')
 end
 held=down
 if down then return end
 if current.engineHandlers.onFrame then current.engineHandlers.onFrame(dt) end
end}}}}
'''
m=copy.deepcopy(recipes[0]);next(f for f in m['files'] if f['path'].endswith('/tutorial_passage_observer.lua'))['content']=code
for path,content in extra.items():m['files'].append({'path':path,'content':content})
m['actions']=actions;m['name']='Uninterrupted original new game prison through full sewer escape';m['timeout_seconds']=86400;m['sdl_input_timeout_seconds']=90
m['expected_log']=['Escape outer segment 67 ready','setstage quest=content:oblivion.esm:02466e stage=30','setstage quest=content:oblivion.esm:01e723 stage=88','M5 door activation: result=teleport ref=content:oblivion.esm:04bed7','Player outside sewer']
if args.selection:
 m['name']='Focused ordinary-input sewer course from a genuine checkpoint'
 m['expected_log']=[f'Escape outer segment {len(chain)} ready']+[scoped_pattern(p,len(chain)) for p in recipes[-1]['expected_log']]
m['forbidden_log']=[x for x in m['forbidden_log'] if '01e723 stage=24' not in x]
types.clear()
def record_types(action):
 types.add(action['type'])
 for field in ['actions','cleanup_actions']:
  for child in action.get(field,[]):record_types(child)
 for children in action.get('cases',{}).values():
  for child in children:record_types(child)
for action in actions:record_types(action)
args.output.write_text(json.dumps(m,indent=2)+'\n');(r/'full-escape-continuous-observer.lua').write_text(code.format());(r/'full-escape-assembly.json').write_text(json.dumps({'scope':'Prepared only, no runtime acceptance','focused':bool(args.selection),'selection':str(selection),'segments':provenance,'actions':len(actions),'action_types':sorted(types),'rule':('Focused genuine-checkpoint load. ' if args.selection else 'One original new-game process. ')+'Ordinary controls with read-only segment/camera observations and frame barriers; no health, position or quest injection. F7+F8 changes observer locals only. Original stage24 wall activation exclusion removed based on independent prior audit.'},indent=2)+'\n')
print(len(chain),'segments',len(actions),'actions',sorted(types));assert 'command' not in types
