"""Build a bounded, script-free TES4 damage-reference plugin from an editable recipe.

This is an isolated original-game/pure-rule oracle fixture, not campaign content.
Generated licensed appearance/model payloads must stay outside version control.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from tes4_m15_fixture import sub, string, record, group, records
from tes4_m14_audit import _subrecords


def _fields(value: dict, fields: str):
    if not isinstance(value, dict) or set(value) != set(fields.split()):
        raise ValueError('unexpected or missing reference fixture fields')


def _integer(value, minimum, maximum):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError('reference fixture integer outside domain')
    return value


def _number(value, minimum, maximum):
    if type(value) not in (int, float) or not math.isfinite(value) or not minimum <= value <= maximum:
        raise ValueError('reference fixture number outside domain')
    return value


def _transform(item):
    for field, limit in (('position', 4096), ('rotation', 2 * math.pi)):
        if not isinstance(item[field], list) or len(item[field]) != 3:
            raise ValueError('reference fixture transform requires three coordinates')
        for value in item[field]:
            _number(value, -limit, limit)
    return struct.pack('<6f', *item['position'], *item['rotation'])


def build(source: bytes, recipe: dict) -> tuple[bytes, dict]:
    _fields(recipe, 'version purpose source_sha256 master npc_template weapon_template cell room target weapon')
    if recipe['version'] != 1 or hashlib.sha256(source).hexdigest() != recipe['source_sha256']:
        raise ValueError('reference fixture source revision differs from recipe')
    if recipe['master'] != 'Oblivion.esm':
        raise ValueError('reference fixture supports the reviewed Oblivion master only')
    cell, room, target, weapon = (recipe[k] for k in ('cell', 'room', 'target', 'weapon'))
    _fields(cell, 'id editor_id name ambient')
    _fields(room, 'id reference model')
    _fields(target, 'id reference editor_id name health fatigue skill attribute position rotation style')
    _fields(weapon, 'id reference editor_id name damage condition speed reach weight position rotation')
    ids = [cell['id'], room['id'], room['reference'], target['id'], target['reference'],
           target['style'], weapon['id'], weapon['reference']]
    for value in ids:
        _integer(value, 0x01000800, 0x01ffffff)
    if len(set(ids)) != len(ids):
        raise ValueError('reference fixture identity collision')
    for item in (cell, target, weapon):
        if not isinstance(item['editor_id'], str) or not item['editor_id'].isascii() or not item['editor_id'].isalnum():
            raise ValueError('reference fixture editor ID must be an ASCII identifier')
        if not isinstance(item['name'], str) or not item['name'] or '\0' in item['name']:
            raise ValueError('reference fixture name is invalid')
    if room['model'] != 'Architecture/ImperialCity/Interior/ICGroundFloor01.NIF':
        raise ValueError('reference fixture requires the reviewed room shell')
    if not isinstance(cell['ambient'], list) or len(cell['ambient']) != 3:
        raise ValueError('reference fixture requires three ambient color bytes')
    for value in cell['ambient']:
        _integer(value, 0, 255)
    for name, maximum in (('health', 2**32-1), ('fatigue', 65535), ('skill', 255), ('attribute', 255)):
        _integer(target[name], 1 if name in ('health', 'fatigue') else 0, maximum)
    for name, maximum in (('damage', 65535), ('condition', 2**32-1)):
        _integer(weapon[name], 1, maximum)
    _number(weapon['speed'], .01, 10)
    _number(weapon['reach'], .01, 10)
    _number(weapon['weight'], 0, 10000)
    transforms = {k: _transform(recipe[k]) for k in ('target', 'weapon')}
    templates = {('NPC_', _integer(recipe['npc_template'], 1, 0xffffff)): None,
                 ('WEAP', _integer(recipe['weapon_template'], 1, 0xffffff)): None}
    for tag, ident, payload in records(source):
        if (tag, ident) in templates:
            if templates[tag, ident] is not None:
                raise ValueError('duplicate reference fixture template')
            templates[tag, ident] = _subrecords(payload, 'reference source', tag)
    if any(value is None for value in templates.values()):
        raise ValueError('missing reference fixture template')
    appearance = {'MODL', 'MODB', 'RNAM', 'CNAM', 'HNAM', 'LNAM', 'ENAM', 'HCLR', 'FGGS', 'FGGA', 'FGTS', 'FNAM'}
    npc_source = templates['NPC_', recipe['npc_template']]
    weapon_source = templates['WEAP', recipe['weapon_template']]
    if not {'MODL', 'RNAM', 'CNAM'} <= {s['name'] for s in npc_source} or not any(s['name'] == 'MODL' for s in weapon_source):
        raise ValueError('template lacks required appearance/model inputs')
    npc = string('EDID', target['editor_id']) + string('FULL', target['name'])
    npc += b''.join(sub(s['name'], s['payload']) for s in npc_source if s['name'] in appearance)
    # Explicit nonessential, nonrespawning, fixed-level stats. AI remains active.
    npc += sub('ACBS', struct.pack('<IHHHhHH', 512, 0, target['fatigue'], 0, 1, 0, 0))
    npc += sub('AIDT', struct.pack('<4BIbB2x', 0, 100, 50, 0, 0, -1, 0))
    npc += sub('DATA', bytes([target['skill']]) * 21 + struct.pack('<I', target['health'])
               + bytes([target['attribute']]) * 8)
    npc += sub('ZNAM', struct.pack('<I', target['style']))
    if not any(s['name'] == 'FNAM' for s in npc_source):
        npc += sub('FNAM', bytes(2))
    style = bytearray(124)
    # An authored nonattacking/nonblocking style isolates the first incoming hit.
    # The target is mortal and collision/AI processing are not disabled.
    style[80] = 2 | 32
    struct.pack_into('<ff', style, 28, .5, 1)
    struct.pack_into('<ff', style, 84, 1, 1)
    struct.pack_into('<fff', style, 92, 250, 1000, 325)
    struct.pack_into('<ff', style, 104, 500, 325)
    style[112] = 1  # Zero would invoke the original load-time default of 25.
    struct.pack_into('<f', style, 116, 1)
    item = string('EDID', weapon['editor_id']) + string('FULL', weapon['name'])
    item += b''.join(sub(s['name'], s['payload']) for s in weapon_source if s['name'] in {'MODL', 'MODB', 'MODT', 'ICON'})
    item += sub('DATA', struct.pack('<B3xffIIIfH', 0, weapon['speed'], weapon['reach'],
                                   0, 0, weapon['condition'], weapon['weight'], weapon['damage']))
    bases = {'CSTY': record('CSTY', target['style'], string('EDID', 'M15ReferenceStyle') + sub('CSTD', style)),
             'NPC_': record('NPC_', target['id'], npc),
             'STAT': record('STAT', room['id'], string('EDID', 'M15ReferenceRoom') + string('MODL', room['model'])),
             'WEAP': record('WEAP', weapon['id'], item)}
    lighting = bytes(cell['ambient'] + [0]) + bytes(8) + struct.pack('<ffii ff', 0, 10000, 0, 0, 0, 1)
    cell_record = record('CELL', cell['id'], string('EDID', cell['editor_id']) + string('FULL', cell['name'])
                         + sub('DATA', b'\x01') + sub('XCLL', lighting))
    floor = record('REFR', room['reference'], sub('NAME', struct.pack('<I', room['id'])) + sub('DATA', bytes(24)))
    placed_weapon = record('REFR', weapon['reference'], sub('NAME', struct.pack('<I', weapon['id']))
                           + sub('DATA', transforms['weapon']))
    placed_target = record('ACHR', target['reference'], string('EDID', 'M15ReferenceOpponent')
                           + sub('NAME', struct.pack('<I', target['id'])) + sub('DATA', transforms['target']), 0x400)
    label = struct.pack('<I', cell['id'])
    children = group(label, 6, group(label, 8, placed_target) + group(label, 9, floor + placed_weapon))
    # Official interior groups use local FormID decimal units/tens, not the file index.
    local = cell['id'] & 0xffffff
    cells = group(struct.pack('<I', local % 10), 2,
                  group(struct.pack('<I', local // 10 % 10), 3, cell_record + children))
    header = sub('HEDR', struct.pack('<fII', 1, 8, (max(ids) & 0xffffff) + 1))
    header += string('CNAM', 'OpenMW M15 isolated reference fixture')
    header += string('MAST', recipe['master']) + sub('DATA', struct.pack('<Q', len(source)))
    output = record('TES4', 0, header) + b''.join(group(tag.encode('ascii'), 0, value) for tag, value in sorted(bases.items()))
    output += group(b'CELL', 0, cells)
    if len(list(records(output))) != 9:
        raise ValueError('reference fixture readback count mismatch')
    return output, {'kind': 'isolated-reference-fixture', 'record_count': 8, 'script_count': 0, 'quest_count': 0,
                    'source_sha256': recipe['source_sha256'], 'sha256': hashlib.sha256(output).hexdigest(),
                    'runtime_accepted': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--recipe', type=Path,
                        default=Path(__file__).parent / 'data/oblivion_compat/m15_reference_fixture.json')
    args = parser.parse_args()
    data, report = build(args.source.read_bytes(), json.loads(args.recipe.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as stream:
        stream.write(data)
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
