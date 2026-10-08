"""Create a bounded, hash-pinned wrong-size local-fog save fixture.

This mutates one OpenMW save image, not a TES4 plugin. Generated saves remain
private diagnostic artifacts. Structural readback is not engine acceptance.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.tes4_runtime_state import _save_records


def build(source: bytes, recipe: dict) -> tuple[bytes, dict]:
    if not isinstance(recipe, dict) or set(recipe) != {'version', 'source_sha256', 'native_schema', 'pixel'}:
        raise ValueError('unexpected local-fog fixture fields')
    if type(recipe['version']) is not int or recipe['version'] != 1:
        raise ValueError('unsupported local-fog recipe version')
    if recipe['source_sha256'] != hashlib.sha256(source).hexdigest():
        raise ValueError('local-fog source differs from reviewed recipe')
    if type(recipe['native_schema']) is not int or recipe['native_schema'] != 46:
        raise ValueError('unsupported local-fog native schema')
    pixel = recipe['pixel']
    if not isinstance(pixel, list) or len(pixel) != 4 or any(type(x) is not int or not 0 <= x <= 255 for x in pixel):
        raise ValueError('local-fog pixel must contain four byte channels')
    rows = list(_save_records(source))
    native = [subs for _, _, tag, subs in rows if tag == b'T4ST']
    if len(native) != 1 or not native[0] or native[0][0][0] != b'VERS':
        raise ValueError('local-fog fixture requires one versioned native record')
    _, a, b = native[0][0]
    if source[a + 8:b] != struct.pack('<I', recipe['native_schema']):
        raise ValueError('local-fog source native version mismatch')
    targets = [(s, e, a, b) for s, e, tag, subs in rows if tag == b'CSTA'
               for name, a, b in subs if name == b'FTEX']
    if len(targets) != 1:
        raise ValueError('local-fog fixture requires exactly one texture')
    start, end, a, b = targets[0]
    original = source[a + 16:b]
    if (len(original) < 33 or original[:8] != b'\x89PNG\r\n\x1a\n'
            or original[12:16] != b'IHDR' or struct.unpack('>II', original[16:24]) != (32, 32)
            or original[24:26] != b'\x08\x06'):
        raise ValueError('local-fog source texture must declare 32x32 RGBA8 PNG')

    def chunk(tag, payload):
        return struct.pack('>I', len(payload)) + tag + payload + struct.pack('>I', zlib.crc32(tag + payload) & 0xffffffff)

    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(b'\0' + bytes(pixel))) + chunk(b'IEND', b'')
    payload = source[a + 8:a + 16] + png
    body = source[start + 16:a] + b'FTEX' + struct.pack('<I', len(payload)) + payload + source[b:end]
    header = bytearray(source[start:start + 16]); struct.pack_into('<I', header, 4, len(body))
    output = source[:start] + header + body + source[end:]
    after = list(_save_records(output))
    if [source[s:e] for s, e, tag, _ in rows if tag != b'CSTA'] != [output[s:e] for s, e, tag, _ in after if tag != b'CSTA']:
        raise ValueError('local-fog fixture changed a non-cell record')
    old_fields = [source[x:y] for name, x, y in next(subs for s, _, _, subs in rows if s == start) if name != b'FTEX']
    new_fields = [output[x:y] for name, x, y in next(subs for s, _, _, subs in after if s == start) if name != b'FTEX']
    if old_fields != new_fields:
        raise ValueError('local-fog fixture changed a non-image cell field')
    return output, {'source_sha256': hashlib.sha256(source).hexdigest(),
                    'output_sha256': hashlib.sha256(output).hexdigest(), 'native_record_exact': True,
                    'non_cell_records_exact': sum(tag != b'CSTA' for _, _, tag, _ in rows),
                    'old_dimensions': [32, 32], 'new_dimensions': [1, 1],
                    'scope': 'structural diagnostic input; engine rejection/preservation requires runtime verification'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path); parser.add_argument('recipe', type=Path); parser.add_argument('output', type=Path)
    args = parser.parse_args()
    output, report = build(args.source.read_bytes(), json.loads(args.recipe.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as stream:
        stream.write(output)
    args.output.with_suffix(args.output.suffix + '.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
