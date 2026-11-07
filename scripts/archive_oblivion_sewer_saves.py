"""Retain byte-identical committed saves; never construct or alter game state."""
import argparse, hashlib, json, os, time
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('course',type=Path)
parser.add_argument('--game-pid',type=int,required=True)
args=parser.parse_args()
course=args.course.resolve()
archive=course/'retained-saves'
archive.mkdir(exist_ok=True)
index=archive/'index.json'
records=[]
seen=set()
original_stat=Path('/proc',str(args.game_pid),'stat').read_text()
original_start=original_stat[original_stat.rfind(')')+2:].split()[19]
status={'scope':'Read-only receipt-verified save copies, not gameplay acceptance','pid':os.getpid(),'game_pid':args.game_pid,'running':True,'records':records}
def digest(data):return hashlib.sha256(data).hexdigest()
def publish():index.write_text(json.dumps(status,indent=2)+'\n')
def read_events():
    result=[]
    try:lines=(course/'combat-events.jsonl').read_text().splitlines()
    except FileNotFoundError:return result
    for line in lines:
        try:result.append(json.loads(line))
        except json.JSONDecodeError:break # A writer may still be appending the final line.
    return result
publish()
try:
    deadline=time.monotonic()+24*3600
    while time.monotonic()<deadline:
        events=read_events()
        assert all(e['pid']==args.game_pid for e in events),'Different game PID in source stream'
        saves=[e for e in events if e['event']=='save-complete']
        changed=False
        for e in saves:
            serial=e['sequence']
            if serial in seen:continue
            src=Path(e['save']).resolve()
            src.relative_to(course/'userdata')
            try:data=src.read_bytes()
            except FileNotFoundError:continue
            if digest(data)!=e['save_sha256']:
                if any(x['sequence']>serial and x['save']==e['save'] for x in saves):
                    seen.add(serial)
                    records.append({'sequence':serial,'status':'superseded before archiving; no copy','receipt':e})
                    changed=True
                continue
            dest=archive/f"{e['epoch']}-{serial:04d}-{src.name}"
            if dest.exists():assert digest(dest.read_bytes())==e['save_sha256']
            else:
                with dest.open('xb') as f:f.write(data)
            assert digest(dest.read_bytes())==e['save_sha256']
            seen.add(serial)
            records.append({'sequence':serial,'status':'copied byte-identically','archive':str(dest),'sha256':e['save_sha256'],'receipt':e})
            changed=True
        if changed:publish()
        if any(e['event']=='run-end' for e in events):
            status['stopped_reason']='native run-end observed'
            break
        try:
            raw=Path('/proc',str(args.game_pid),'stat').read_text()
            fields=raw[raw.rfind(')')+2:].split()
            assert fields[19]==original_start,'PID reused'
            if fields[0]=='Z':break
        except (ProcessLookupError,FileNotFoundError):
            status['stopped_reason']='original game PID ended'
            break
        time.sleep(.2)
    else:status['stopped_reason']='24hour archival limit'
except BaseException as exc:
    status['error']=repr(exc)
    raise
finally:
    status['running']=False
    publish()
