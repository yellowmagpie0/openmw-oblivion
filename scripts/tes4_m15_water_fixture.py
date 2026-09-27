"""Reproduce a bounded flooded variant of the pinned M15 lifecycle fixture.

Only the selected CELL's water flag and height change. The recipe remains the
editable source; generated licensed records belong in ignored evidence paths.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from tes4_m14_audit import _subrecords
from tes4_m15_fixture import records, sub


def build(source: bytes, recipe: dict) -> tuple[bytes, dict]:
    if set(recipe) != {'source_sha256', 'cell', 'water_height'}:
        raise ValueError('invalid water fixture recipe fields')
    if hashlib.sha256(source).hexdigest() != recipe['source_sha256']:
        raise ValueError('water fixture source revision differs from recipe')
    if type(recipe['cell']) is not int or not 0x800 <= recipe['cell'] <= 0xffffff:
        raise ValueError('invalid water fixture cell identity')
    height = recipe['water_height']
    if type(height) not in (int, float) or not math.isfinite(height) or abs(height) > 4096:
        raise ValueError('invalid water fixture height')
    original = list(records(source))  # Validate every containing record boundary first.
    changed = 0

    def rewrite(data: bytes) -> bytes:
        nonlocal changed
        output = bytearray()
        offset = 0
        while offset < len(data):
            tag, size, flags, ident, extra = struct.unpack_from('<4sIIII', data, offset)
            if tag == b'GRUP':
                body = rewrite(data[offset + 20:offset + size])
                output.extend(tag + struct.pack('<I', 20 + len(body)) + data[offset + 8:offset + 20] + body)
                offset += size
                continue
            body = data[offset + 20:offset + 20 + size]
            if tag == b'CELL' and ident == recipe['cell']:
                if flags & 0x40000:
                    raise ValueError('compressed target cell is outside fixture contract')
                fields = _subrecords(body, 'water fixture', 'CELL')
                cell_data = [s['payload'] for s in fields if s['name'] == 'DATA']
                if len(cell_data) != 1 or len(cell_data[0]) != 1:
                    raise ValueError('invalid water fixture CELL DATA')
                if sum(s['name'] == 'XCLW' for s in fields) > 1:
                    raise ValueError('duplicate water fixture height')
                body = b''.join(sub(s['name'], bytes([s['payload'][0] | 2])
                                   if s['name'] == 'DATA' else s['payload'])
                                for s in fields if s['name'] != 'XCLW')
                body += sub('XCLW', struct.pack('<f', height))
                changed += 1
            output.extend(struct.pack('<4sIIII', tag, len(body), flags, ident, extra) + body)
            offset += 20 + size
        return bytes(output)

    result = rewrite(source)
    if changed != 1:
        raise ValueError('water fixture requires exactly one matching cell')
    emitted = list(records(result))
    if len(emitted) != len(original):
        raise ValueError('water fixture record count changed')
    for before, after in zip(original, emitted):
        if before[:2] != ('CELL', recipe['cell']) and before != after:
            raise ValueError('water fixture changed an unrelated record')
    target = next(payload for tag, ident, payload in emitted if (tag, ident) == ('CELL', recipe['cell']))
    fields = {s['name']: s['payload'] for s in _subrecords(target, 'water readback', 'CELL')}
    if not fields['DATA'][0] & 2 or fields['XCLW'] != struct.pack('<f', height):
        raise ValueError('water fixture semantic readback failed')
    return result, {'structural_readback_passed': True, 'cell_has_water': True,
                    'water_height': struct.unpack('<f', fields['XCLW'])[0],
                    'source_sha256': recipe['source_sha256'],
                    'sha256': hashlib.sha256(result).hexdigest(), 'runtime_accepted': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--recipe', type=Path,
                        default=Path(__file__).parent / 'data/oblivion_compat/m15_water_fixture.json')
    args = parser.parse_args()
    result, report = build(args.source.read_bytes(), json.loads(args.recipe.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as stream:
        stream.write(result)
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
