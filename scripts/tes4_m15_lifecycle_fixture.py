"""Build a native NPC callback fixture; licensed boot data stays outside Git.

The separate observation builder remains script-free. This fixture attaches an
editable observer script, never a script that supplies the death transition.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from tes4_m15_fixture import build as observation, records, record, group, sub, string
from tes4_m14_audit import _subrecords


def build(source: bytes, boot: dict, recipe: dict, script: str):
    fields = {'version', 'purpose', 'npc_template', 'npc', 'reference', 'script', 'position', 'rotation'}
    if set(recipe) != fields or type(recipe['version']) is not int or recipe['version'] != 1:
        raise ValueError('invalid lifecycle recipe fields/version')
    ids = [recipe[k] for k in ('npc', 'reference', 'script')]
    if any(type(i) is not int or not 0x800 <= i <= 0xffffff for i in ids) or len(set(ids)) != 3:
        raise ValueError('invalid lifecycle identities')
    for key, limit in (('position', 4096), ('rotation', 2 * math.pi)):
        values = recipe[key]
        if not isinstance(values, list) or len(values) != 3 or any(
                type(v) not in (float, int) or not math.isfinite(v) or abs(v) > limit for v in values):
            raise ValueError('invalid lifecycle transform')
    if not isinstance(script, str) or '\0' in script or not script.strip():
        raise ValueError('invalid observer source')
    if type(recipe['npc_template']) is not int:
        raise ValueError('invalid NPC template')
    base, _ = observation(source, boot)
    rows = list(records(base))
    if set(ids) & {ident for _, ident, _ in rows}:
        raise ValueError('lifecycle identity collision')
    template = [payload for tag, ident, payload in rows if tag == 'NPC_' and ident == recipe['npc_template']]
    if len(template) != 1:
        raise ValueError('missing lifecycle NPC template')
    appearance = {'MODL', 'MODB', 'RNAM', 'CNAM', 'HNAM', 'LNAM', 'ENAM', 'HCLR', 'FGGS', 'FGGA', 'FGTS', 'FNAM'}
    source_fields = _subrecords(template[0], 'lifecycle template', 'NPC_')
    if not {'MODL', 'RNAM', 'CNAM'} <= {s['name'] for s in source_fields}:
        raise ValueError('NPC template lacks appearance dependencies')
    npc = string('EDID', 'M15LifecycleActor') + string('FULL', 'M15 callback observer')
    npc += b''.join(sub(s['name'], s['payload']) for s in source_fields if s['name'] in appearance)
    npc += sub('ACBS', struct.pack('<IHHHhHH', 512, 0, 100, 0, 1, 0, 0))
    npc += sub('AIDT', struct.pack('<4BIbB2x', 0, 100, 50, 0, 0, -1, 0))
    npc += sub('DATA', bytes([5]) * 21 + struct.pack('<I', 100) + bytes([50]) * 8)
    npc += sub('SCRI', struct.pack('<I', recipe['script']))
    if not any(s['name'] == 'FNAM' for s in source_fields):
        npc += sub('FNAM', bytes(2))
    observer = string('EDID', 'M15DeathObserver')
    observer += sub('SCHR', struct.pack('<4I2H', 0, 0, 0, 1, 0, 0))
    observer += sub('SLSD', struct.pack('<6I', 1, 0, 0, 0, 0, 0)) + string('SCVR', 'deaths')
    observer += string('SCTX', script)
    groups = {}
    for tag, ident, payload in rows:
        if tag not in {'TES4', 'CELL', 'REFR'}:
            groups.setdefault(tag, []).append(record(tag, ident, payload))
    groups.setdefault('NPC_', []).append(record('NPC_', recipe['npc'], npc))
    groups['SCPT'] = [record('SCPT', recipe['script'], observer)]
    cell = boot['cell']['id']
    label = struct.pack('<I', cell)
    cell_rows = [record(tag, ident, payload) for tag, ident, payload in rows if tag == 'CELL']
    if len(cell_rows) != 1:
        raise ValueError('lifecycle fixture requires one cell')
    actor = record('ACHR', recipe['reference'], string('EDID', 'M15LifecycleReference')
                   + sub('NAME', struct.pack('<I', recipe['npc']))
                   + sub('DATA', struct.pack('<6f', *recipe['position'], *recipe['rotation'])), 0x400)
    floors = b''.join(record(tag, ident, payload) for tag, ident, payload in rows if tag == 'REFR')
    children = group(label, 6, group(label, 8, actor) + group(label, 9, floors))
    cells = group(struct.pack('<I', cell % 10), 2,
                  group(struct.pack('<I', cell // 10 % 10), 3, cell_rows[0] + children))
    count = len(rows) - 1 + 3
    header = sub('HEDR', struct.pack('<fII', 1, count, max([i for _, i, _ in rows] + ids) + 1))
    header += string('CNAM', 'OpenMW synthetic M15 lifecycle observer')
    output = record('TES4', 0, header, 1)
    output += b''.join(group(tag.encode('ascii'), 0, b''.join(values)) for tag, values in sorted(groups.items()))
    output += group(b'CELL', 0, cells)
    if len(list(records(output))) != count + 1:
        raise ValueError('lifecycle fixture readback count mismatch')
    return output, {'kind': 'synthetic-callback-observer', 'record_count': count, 'script_count': 1,
                    'source_sha256': boot['source_sha256'], 'sha256': hashlib.sha256(output).hexdigest(),
                    'script_sha256': hashlib.sha256(script.encode('cp1252')).hexdigest(),
                    'runtime_accepted': False}


def main():
    data = Path(__file__).parent / 'data/oblivion_compat'
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--boot-recipe', type=Path, default=data / 'm15_observation_fixture.json')
    parser.add_argument('--recipe', type=Path, default=data / 'm15_lifecycle_fixture.json')
    parser.add_argument('--script', type=Path, default=data / 'm15_lifecycle_observer.obscript')
    args = parser.parse_args()
    output, report = build(args.source.read_bytes(), json.loads(args.boot_recipe.read_text()),
                           json.loads(args.recipe.read_text()), args.script.read_text())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as stream:
        stream.write(output)
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
