"""Compare typed esmtool EFSH output with independent original-master decoding.

This verifies data ingestion, not command playback or rendered effect acceptance.
Run with --esmtool build/esmtool --master /path/to/Oblivion.esm --output NEW_DIR.
"""
from pathlib import Path
import argparse
import collections
import hashlib
import json
import math
import re
import shlex
import struct
import subprocess
import zlib

from tes4_m14_audit import _subrecords

MASTER_SHA256 = 'a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70'


def decode(data):
    if len(data) not in (96, 224):
        raise ValueError('Invalid TES4 EFSH DATA length')
    def values(fmt, offset):
        result = list(struct.unpack_from('<' + fmt, data, offset))
        if any(isinstance(v, float) and not math.isfinite(v) for v in result):
            raise ValueError('Nonfinite EFSH value')
        return result
    groups = {
        'MembraneBlend': values('3I', 4) + values('I', 92),
        'FillColor': values('4B', 16), 'FillAlpha': values('6f', 20),
        'TextureSpeed': values('2f', 44), 'EdgeFalloff': values('f', 52),
        'EdgeColor': values('4B', 56), 'EdgeAlpha': values('6f', 60),
        'FullAlpha': values('2f', 84),
    }
    if len(data) == 224:
        groups.update({
            'ParticleBlend': values('4I', 96), 'Birth': values('5f', 112),
            'Lifetime': values('2f', 132), 'NormalMotion': values('2f', 140),
            'Velocity': values('3f', 148), 'Acceleration': values('3f', 160),
            'Scale': values('2f', 172), 'ScaleTime': values('2f', 180),
            'ParticleColor0': values('4B', 188), 'ParticleColor1': values('4B', 192),
            'ParticleColor2': values('4B', 196), 'ParticleAlpha': values('3f', 200),
            'ColorTime': values('3f', 212),
        })
    return {'flags': data[0], 'size': len(data), 'groups': groups}


def original_records(path):
    data = path.read_bytes()
    assert hashlib.sha256(data).hexdigest() == MASTER_SHA256, 'Unreviewed master identity'
    records = {}
    def walk(begin, end):
        while begin < end:
            tag, size, flags, ident, _ = struct.unpack_from('<4s4I', data, begin)
            finish = begin + (size if tag == b'GRUP' else 20 + size)
            assert begin + 20 <= finish <= end
            if tag == b'GRUP':
                walk(begin + 20, finish)
            elif tag == b'EFSH':
                raw = data[begin + 20:finish]
                if flags & 0x40000:
                    unpacked = zlib.decompress(raw[4:])
                    assert len(unpacked) == struct.unpack_from('<I', raw)[0]
                    raw = unpacked
                rows = _subrecords(raw, path.name, 'EFSH')
                subs = {v['name']: v['payload'] for v in rows}
                assert len(subs) == len(rows) and ident not in records
                record = decode(subs['DATA'])
                record['editor_id'] = subs['EDID'].rstrip(b'\0').decode('cp1252')
                record['fillTexture'] = subs.get('ICON', b'\0').rstrip(b'\0').decode('cp1252')
                record['particleTexture'] = subs.get('ICO2', b'\0').rstrip(b'\0').decode('cp1252')
                records[ident] = record
            begin = finish
        assert begin == end
    walk(0, len(data))
    assert len(records) == 102
    return records


def check_dump(text, expected):
    actual = {}
    for block in re.split(r'\n\s*Record: EFSH\s*\n', text)[1:]:
        ident = int(re.search(r'\bId: (0x[0-9a-fA-F]+)', block)[1], 16)
        assert ident not in actual and ident in expected
        e = expected[ident]
        assert re.search(r'EditorId: ([^\n]*)', block)[1].strip() == e['editor_id']
        header = re.search(r'^\s*EffectShader: (.+)$', block, re.M)
        assert header, f'EFSH {ident:06x} is still raw'
        props = dict(v.split('=', 1) for v in shlex.split(header[1]))
        assert int(props['flags']) == e['flags'] and int(props['size']) == e['size']
        for key in ['fillTexture', 'particleTexture']:
            assert props[key] == e[key]
        groups = {name: [float(v) for v in values.split(',')]
                  for name, values in re.findall(r'^\s*EffectShader(\w+): ([^\n]+)$', block, re.M)}
        assert groups.keys() == e['groups'].keys(), (ident, groups.keys())
        for name, values in e['groups'].items():
            assert len(groups[name]) == len(values)
            assert all(math.isclose(a, b, rel_tol=1e-6, abs_tol=1e-6)
                       for a, b in zip(groups[name], values)), (ident, name)
        actual[ident] = e
    assert actual.keys() == expected.keys()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--esmtool', type=Path, required=True)
    parser.add_argument('--master', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    expected = original_records(args.master)
    command = [str(args.esmtool.resolve()), 'dump', '-t', 'EFSH', str(args.master.resolve())]
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    (args.output / 'dump.txt').write_text(result.stdout)
    (args.output / 'stderr.txt').write_text(result.stderr)
    assert result.returncode == 0, result.stderr
    check_dump(result.stdout, expected)
    fire = expected[0x852fe]
    report = dict(passed=True, command=command, master_sha256=MASTER_SHA256,
                  esmtool_sha256=hashlib.sha256(args.esmtool.read_bytes()).hexdigest(),
                  records=len(expected), data_sizes=dict(collections.Counter(v['size'] for v in expected.values())),
                  field_groups_checked=sum(len(v['groups']) for v in expected.values()),
                  fire_damage=fire,
                  scope='Typed original-master ingestion only. No shader command, rendering, or save/restart acceptance.')
    (args.output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'fire_damage'}))


if __name__ == '__main__':
    main()
