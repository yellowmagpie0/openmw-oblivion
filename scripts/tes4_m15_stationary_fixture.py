"""Reproduce the S4 NPC activation fixture from a hash-pinned TES4 master.

Only the NPC's CSTY/SCRI links change. Stats, appearance, packages, factions and
other native fields stay intact. Editable JSON/ObScript are source; generated
licensed NPC payloads stay in ignored evidence. This is fork SCTX compilation,
not a claim of original-game acceptance or TES3 MCP authoring support.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

from tes4_m15_fixture import group, record, records, string, sub
from tes4_m14_audit import _subrecords
from tes4_m15_audit import combat_style


def fields(value, names):
    if not isinstance(value, dict) or set(value) != set(names.split()):
        raise ValueError('missing or unexpected stationary fixture fields')


def identifier(value):
    if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', value):
        raise ValueError('invalid stationary fixture editor ID')
    return value


def build(source: bytes, recipe: dict, script: str) -> tuple[bytes, dict]:
    fields(recipe, 'version purpose source_sha256 master npc style script')
    if type(recipe['version']) is not int or recipe['version'] != 1 or recipe['master'] != 'Oblivion.esm':
        raise ValueError('unsupported stationary fixture version/master')
    if hashlib.sha256(source).hexdigest() != recipe['source_sha256']:
        raise ValueError('stationary fixture master differs from reviewed recipe')
    if not isinstance(recipe['purpose'], str) or not recipe['purpose'].strip():
        raise ValueError('missing stationary fixture purpose')
    fields(recipe['npc'], 'id editor_id')
    fields(recipe['style'], 'id editor_id attack_chance block_chance dodge_chance power_attack_chance idle hold flags do_not_acquire')
    fields(recipe['script'], 'id editor_id sha256')
    npc, style, program = (recipe[k] for k in ('npc', 'style', 'script'))
    for item in (npc, style, program):
        identifier(item['editor_id'])
        if type(item['id']) is not int:
            raise ValueError('stationary fixture requires integer FormIDs')
    if not 1 <= npc['id'] <= 0xffffff or any(not 0x01000800 <= x['id'] <= 0x01ffffff for x in (style, program)):
        raise ValueError('stationary fixture FormID outside master/own domains')
    if style['id'] == program['id']:
        raise ValueError('stationary fixture own identity collision')
    profile = {'attack_chance': 100, 'block_chance': 0, 'dodge_chance': 0, 'power_attack_chance': 0,
               'idle': [0, 0], 'hold': [0, 0], 'flags': 34, 'do_not_acquire': True}
    for key, expected in profile.items():
        value = style[key]
        if isinstance(expected, bool):
            valid = type(value) is bool and value == expected
        elif isinstance(expected, int):
            valid = type(value) is int and value == expected
        else:
            valid = isinstance(value, list) and len(value) == 2 and all(type(x) in (int, float) and x == 0 for x in value)
        if not valid:
            raise ValueError('stationary fixture requires the supported deterministic style: ' + key)
    if not isinstance(script, str) or '\0' in script:
        raise ValueError('invalid stationary fixture script source')
    script_bytes = script.encode('cp1252')
    if hashlib.sha256(script_bytes).hexdigest() != program['sha256']:
        raise ValueError('stationary fixture script differs from reviewed source')
    if not re.match(r'\s*scn\s+' + re.escape(program['editor_id']) + r'\s', script, re.IGNORECASE):
        raise ValueError('stationary fixture script/editor identity mismatch')
    templates = [payload for tag, form, payload in records(source) if tag == 'NPC_' and form == npc['id']]
    if len(templates) != 1:
        raise ValueError('missing or duplicate stationary NPC template')
    original = _subrecords(templates[0], 'stationary source', 'NPC_')
    names = [x['name'] for x in original]
    if names.count('EDID') != 1 or any(names.count(k) > 1 for k in ('ZNAM', 'SCRI')):
        raise ValueError('ambiguous stationary NPC identity/style/script')
    editor = next(x['payload'] for x in original if x['name'] == 'EDID')
    if editor != npc['editor_id'].encode('cp1252') + b'\0':
        raise ValueError('stationary NPC template editor identity mismatch')
    preserved = [x for x in original if x['name'] not in ('ZNAM', 'SCRI')]
    npc_payload = b''.join(sub(x['name'], x['payload']) for x in preserved)
    npc_payload += sub('ZNAM', struct.pack('<I', style['id'])) + sub('SCRI', struct.pack('<I', program['id']))
    standard = bytearray(124)
    standard[0], standard[36], standard[37], standard[52], standard[80] = (
        style['dodge_chance'], style['block_chance'], style['attack_chance'], style['power_attack_chance'], style['flags'])
    struct.pack_into('<2f', standard, 28, *style['idle'])
    struct.pack_into('<2f', standard, 72, *style['hold'])
    struct.pack_into('<2f', standard, 84, 1, 1)
    struct.pack_into('<3f', standard, 92, 250, 1000, 325)
    struct.pack_into('<2f', standard, 104, 500, 325)
    standard[112] = 1  # Preserve the declared tail; original zero loads default25.
    struct.pack_into('<fI', standard, 116, 1, int(style['do_not_acquire']))
    semantic_style = combat_style(bytes(standard))
    script_payload = string('EDID', program['editor_id'])
    script_payload += sub('SCHR', struct.pack('<4I2H', 0, 1, 0, 1, 0, 0))
    script_payload += sub('SLSD', struct.pack('<6I', 1, 0, 0, 0, 1, 0)) + string('SCVR', 'fighting')
    script_payload += sub('SCRO', struct.pack('<I', 0x14)) + string('SCTX', script)
    payloads = {'NPC_': record('NPC_', npc['id'], npc_payload),
                'CSTY': record('CSTY', style['id'], string('EDID', style['editor_id']) + sub('CSTD', bytes(standard))),
                'SCPT': record('SCPT', program['id'], script_payload)}
    header = sub('HEDR', struct.pack('<fII', 1, 3, (max(style['id'], program['id']) & 0xffffff) + 1))
    header += string('CNAM', 'OpenMW synthetic S4 stationary opponent')
    header += string('MAST', recipe['master']) + sub('DATA', bytes(8))
    output = record('TES4', 0, header) + b''.join(group(tag.encode('ascii'), 0, data) for tag, data in sorted(payloads.items()))
    emitted = list(records(output))
    if len(emitted) != 4:
        raise ValueError('stationary fixture emitted record count differs')
    reinspected = _subrecords(next(p for t, _, p in emitted if t == 'NPC_'), 'stationary output', 'NPC_')
    if [(x['name'], x['payload']) for x in reinspected if x['name'] not in ('ZNAM', 'SCRI')] != [(x['name'], x['payload']) for x in preserved]:
        raise ValueError('stationary fixture altered unrelated NPC fields')
    return output, {'kind': 'synthetic-native-stationary-opponent', 'record_count': 3,
                    'master_sha256': recipe['source_sha256'], 'script_sha256': program['sha256'],
                    'sha256': hashlib.sha256(output).hexdigest(), 'style': semantic_style,
                    'npc_fields_changed': ['ZNAM', 'SCRI'], 'scope': 'Structural/semantic artifact; actual fork compilation and normal activation/contact/restart are separate gates.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    data = Path(__file__).parent / 'data/oblivion_compat'
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--recipe', type=Path, default=data / 'm15_stationary_opponent_fixture.json')
    parser.add_argument('--script', type=Path, default=data / 'm15_stationary_opponent.obscript')
    args = parser.parse_args()
    output, metadata = build(args.source.read_bytes(), json.loads(args.recipe.read_text()), args.script.read_text())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as stream:
        stream.write(output)
    args.output.with_suffix('.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(json.dumps(metadata, indent=2))


if __name__ == '__main__':
    main()
