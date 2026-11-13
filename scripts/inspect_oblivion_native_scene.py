"""Read-only, hash-pinned original reference and NiAVObject scene poses."""
import pathlib,os,json,struct,sys,hashlib,time,math
root=pathlib.Path(sys.argv[1]);label=sys.argv[2];wrapper=json.loads((root/'game-wrapper.json').read_text());wanted=('OPENMW_ORACLE_OWNER='+wrapper['owner']).encode()+b'\0'
procs=[]
for proc in pathlib.Path('/proc').iterdir():
 if not proc.name.isdigit():continue
 try:
  args=proc.joinpath('cmdline').read_bytes().split(b'\0')
  if args[0]==b'./Oblivion.exe' and wanted in proc.joinpath('environ').read_bytes():procs.append(proc)
 except (FileNotFoundError,PermissionError,ProcessLookupError):pass
assert len(procs)==1,[str(p) for p in procs];proc=procs[0];fd=os.open(proc/'mem',os.O_RDONLY)
def read(a,n):
 assert 0x10000<=a<0xfff00000 and n<=1024*1024
 b=os.pread(fd,n,a);assert len(b)==n;return b
def u(a):return struct.unpack('<I',read(a,4))[0]
def h(a):return struct.unpack('<H',read(a,2))[0]
def floats(a,n):
 v=struct.unpack('<'+'f'*n,read(a,4*n));assert all(math.isfinite(x) for x in v);return v
def text(a):return read(a,160).split(b'\0')[0].decode('cp1252',errors='replace') if a else ''
exe=pathlib.Path('/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Oblivion.exe');data=exe.read_bytes();assert hashlib.sha256(data).hexdigest()=='a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6';meta=json.loads(pathlib.Path('build/oblivion-compat/m15/S2/sources-01/original-exe.json').read_text());a=0x46b250
for _,va,off,size in meta['sections']:
 if va<=a<va+size:assert read(a,32)==data[off+a-va:off+a-va+32]
base=0xb0613c;vtable,n,buckets,count=struct.unpack('<4I',read(base,16));assert 1<=n<=262144 and count>1000;array=struct.unpack('<'+'I'*n,read(buckets,4*n))
def lookup(id):
 for slot in (id%n,):
  ptr=array[slot];seen=set()
  while ptr:
   assert ptr not in seen and len(seen)<10000;seen.add(ptr);nxt,key,value=struct.unpack('<3I',read(ptr,12))
   if key==id:assert u(value+0xc)==id;return value
   ptr=nxt
 raise AssertionError('Not found '+hex(id))
def node(a,depth=0,seen=None):
 if seen is None:seen=set()
 assert a not in seen and len(seen)<512 and depth<32;seen.add(a)
 vt=u(a);name=text(u(a+8));result=dict(address=hex(a),name=name,world_translation=floats(a+0x88,3),world_rotation=floats(a+0x64,9),world_scale=floats(a+0x94,1)[0],local_translation=floats(a+0x54,3))
 collision=u(a+0xa8)
 if collision:
  assert u(collision+8)==a;body=u(collision+0x10);result['collision']=dict(address=hex(collision),flags=h(collision+0xc),body=hex(body),hk=hex(u(body+8)))
 if u(vt+0x58)==0x70ac00:
  used=h(a+0xb6);assert used<256;children=u(a+0xb0)
  result['children']=[node(p,depth+1,seen) for p in struct.unpack('<'+'I'*used,read(children,4*used)) if p] if used else []
 return result
forms=[]
for id in (0x6c605,0xc4986,0x6bf50):
 ref=lookup(id);forms.append(dict(id=hex(id),address=hex(ref),position=floats(ref+0x2c,3),rotation=floats(ref+0x20,3),scale=floats(ref+0x38,1)[0],scene=node(u(ref+0x3c))))
stat=proc.joinpath('stat').read_text().rsplit(')',1)[1].split();out=dict(pid=int(proc.name),start_ticks=int(stat[19]),wall_time_ns=time.time_ns(),read_only=True,lookup=dict(entry=hex(a),global_map=hex(base),hash_entry=hex(u(vtable+4)),buckets=n,count=count),scope='Observed actual scene transforms; TESObjectREFR and NiAVObject layout from local xOBSE, lookup code bytes crosschecked against pinned original executable; no memory writes',forms=forms,player=dict(address=hex(lookup(0x14)),position=floats(lookup(0x14)+0x2c,3)))
(root/(label+'.json')).write_text(json.dumps(out,indent=2)+'\n')
def show(n):
 if 'collision' in n:print(n['name'],n['world_translation'],n['collision'])
 for c in n.get('children',[]):show(c)
for f in forms:print('FORM',f['id'],f['position']);show(f['scene'])
os.close(fd)
