"""Compare original TES4 ingredient effects with typed esmtool output."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys

parser = argparse.ArgumentParser(__doc__)
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--master', type=Path, required=True)
parser.add_argument('--esmtool', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
sys.path.insert(0, str(args.repo / 'scripts'))
from tes4_m15_fixture import records
from tes4_m14_audit import _subrecords

source = args.master.read_bytes()
source_hash = hashlib.sha256(source).hexdigest()
if source_hash != 'a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70':
    raise ValueError('unreviewed master hash')
expected = {}
expected_properties = {}
restoration_definitions = {}
for tag, ident, payload in records(source):
    if tag == 'MGEF':
        fields = {s['name']: s['payload'] for s in _subrecords(payload, str(args.master), tag)}
        code = fields.get('EDID', b'').rstrip(b'\0').decode('cp1252')
        if code in ('REHE', 'RESP', 'REFA'):
            flags, cost, resource, school, resistance = struct.unpack('<IfIIi', fields['DATA'][:20])
            restoration_definitions[code] = {'flags': flags, 'resource': resource,
                                             'school': school, 'resistance': resistance}
    if tag != 'INGR':
        continue
    subs = _subrecords(payload, str(args.master), tag)
    name = next(s['payload'].rstrip(b'\0').decode('cp1252') for s in subs if s['name'] == 'EDID')
    expected[name] = [list(struct.unpack('<6I', s['payload'])) for s in subs if s['name'] == 'EFIT']
    fields = {s['name']: s['payload'] for s in subs}
    expected_properties[name] = (struct.unpack('<II', fields['ENIT']), struct.unpack('<f', fields['DATA'])[0])
for code, resource in (('REHE', 8), ('RESP', 9), ('REFA', 10)):
    if restoration_definitions.get(code) != {'flags': 0x01000870, 'resource': resource,
                                             'school': 5, 'resistance': -1}:
        raise ValueError('reviewed restoration definition changed')
if len(expected) != 173 or sum(map(len, expected.values())) != 656:
    raise ValueError('reviewed ingredient inventory changed')
run = subprocess.run([str(args.esmtool), 'dump', '-t', 'INGR', str(args.master)], capture_output=True, text=True)
args.output.mkdir(parents=True, exist_ok=False)
(args.output / 'typed-dump.txt').write_text(run.stdout)
(args.output / 'stderr.txt').write_text(run.stderr)
run.check_returncode()
actual = {}
for block in run.stdout.split('  Record: INGR')[1:]:
    name = re.search(r'  EditorId: ([^\n]+)', block).group(1).strip()
    actual[name] = [list(map(int, match)) for match in re.findall(
        r'IngredientEffect: code=(\d+) magnitude=(\d+) area=(\d+) duration=(\d+) range=(\d+) actorValue=(\d+)', block)]
actual_properties = {}
for block in run.stdout.split('  Record: INGR')[1:]:
    name = re.search(r'  EditorId: ([^\n]+)', block).group(1).strip()
    match = re.search(r'Ingredient: value=(\d+) weight=([^ ]+) flags=(\d+)', block)
    actual_properties[name] = ((int(match[1]), int(match[3])), float(match[2]))
properties_passed = actual_properties.keys() == expected_properties.keys() and all(
    actual_properties[k][0] == expected_properties[k][0] and math.isclose(actual_properties[k][1], expected_properties[k][1], abs_tol=1e-5)
    for k in expected_properties)
result = {'passed': actual == expected and properties_passed, 'properties_passed': properties_passed, 'source_sha256': source_hash,
          'esmtool_sha256': hashlib.sha256(args.esmtool.read_bytes()).hexdigest(),
          'restoration_definitions': restoration_definitions,
          'records': len(actual), 'effects': sum(map(len, actual.values())),
          'missing': sorted(expected.keys() - actual.keys()),
          'unexpected': sorted(actual.keys() - expected.keys()),
          'different': sorted(k for k in actual.keys() & expected.keys() if actual[k] != expected[k])}
(args.output / 'verification.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result))
raise SystemExit(0 if result['passed'] else 1)
