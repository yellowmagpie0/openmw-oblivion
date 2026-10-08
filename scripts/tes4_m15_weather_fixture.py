"""Build one bounded TES4 weather clone from an editable, hash-pinned recipe.

Only its own FormID and EDID change. Licensed template payloads remain generated
artifacts. This is a TES4 fixture writer, separate from the TES3-only CS MCP.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

try:
    from .tes4_m15_fixture import group, record, records, string, sub
    from .tes4_m14_audit import _subrecords
except ImportError:
    from tes4_m15_fixture import group, record, records, string, sub
    from tes4_m14_audit import _subrecords


def build(source: bytes, recipe: dict) -> tuple[bytes, dict]:
    required = {'version', 'purpose', 'source_sha256', 'master', 'template', 'weather'}
    if not isinstance(recipe, dict) or set(recipe) != required:
        raise ValueError('unexpected or missing weather fixture fields')
    if type(recipe['version']) is not int or recipe['version'] != 1 or recipe['master'] != 'Oblivion.esm':
        raise ValueError('unsupported weather fixture version/master')
    if not isinstance(recipe['purpose'], str) or not recipe['purpose'].strip():
        raise ValueError('missing weather fixture purpose')
    if hashlib.sha256(source).hexdigest() != recipe['source_sha256']:
        raise ValueError('weather fixture source differs from reviewed recipe')
    if type(recipe['template']) is not int or not 1 <= recipe['template'] <= 0xffffff:
        raise ValueError('weather template must belong to the master')
    weather = recipe['weather']
    if not isinstance(weather, dict) or set(weather) != {'id', 'editor_id'}:
        raise ValueError('unexpected weather declaration fields')
    if type(weather['id']) is not int or not 0x01000800 <= weather['id'] <= 0x01ffffff:
        raise ValueError('weather fixture FormID must belong to its own plugin')
    if not isinstance(weather['editor_id'], str) or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,63}', weather['editor_id']):
        raise ValueError('invalid weather fixture editor ID')
    templates = [payload for tag, ident, payload in records(source)
                 if tag == 'WTHR' and ident == recipe['template']]
    if len(templates) != 1:
        raise ValueError('missing or duplicate weather template')
    fields = _subrecords(templates[0], 'weather source', 'WTHR')
    if sum(x['name'] == 'EDID' for x in fields) != 1:
        raise ValueError('ambiguous weather template editor ID')
    preserved = [(x['name'], x['payload']) for x in fields if x['name'] != 'EDID']
    payload = b''.join(string('EDID', weather['editor_id']) if x['name'] == 'EDID'
                       else sub(x['name'], x['payload']) for x in fields)
    header = sub('HEDR', struct.pack('<fII', 1.0, 1, (weather['id'] & 0xffffff) + 1))
    header += string('CNAM', 'M15S3 weather persistence fixture')
    header += string('SNAM', recipe['purpose'])
    header += string('MAST', recipe['master']) + sub('DATA', struct.pack('<Q', len(source)))
    output = record('TES4', 0, header) + group(b'WTHR', 0, record('WTHR', weather['id'], payload))
    reopened = list(records(output))
    if [(tag, ident) for tag, ident, _ in reopened] != [('TES4', 0), ('WTHR', weather['id'])]:
        raise ValueError('weather fixture readback record mismatch')
    check = _subrecords(reopened[1][2], 'weather output', 'WTHR')
    if [(x['name'], x['payload']) for x in check if x['name'] != 'EDID'] != preserved:
        raise ValueError('weather template fields changed during generation')
    return output, {'source_sha256': hashlib.sha256(source).hexdigest(),
                    'output_sha256': hashlib.sha256(output).hexdigest(),
                    'template': recipe['template'], 'form_id': weather['id'],
                    'editor_id': weather['editor_id'], 'masters': [recipe['master']],
                    'preserved_subrecords': [name for name, _ in preserved],
                    'scope': 'structural and exact-field readback; engine acceptance requires a runtime course'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('master', type=Path)
    parser.add_argument('recipe', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    output, report = build(args.master.read_bytes(), json.loads(args.recipe.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as stream:
        stream.write(output)
    args.output.with_suffix(args.output.suffix + '.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
