"""Apply a hash-pinned diagnostic regional-weather recipe to a private save copy.

Only WTHR changes. This creates restart inputs, not gameplay/oracle outcomes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.saved_weather_state import decode_weather_state
from scripts.tes4_runtime_state import _save_records


def build(source: bytes, recipe: dict) -> tuple[bytes, dict]:
    required = {'version', 'source_sha256', 'update_time', 'regions'}
    if not isinstance(recipe, dict) or set(recipe) != required or type(recipe['version']) is not int or recipe['version'] != 1:
        raise ValueError('unsupported regional weather recipe')
    if recipe['source_sha256'] != hashlib.sha256(source).hexdigest():
        raise ValueError('regional weather source differs from reviewed recipe')
    delay = recipe['update_time']
    if type(delay) not in (int, float) or not math.isfinite(delay) or not 0 < delay <= 10000:
        raise ValueError('invalid regional weather update countdown')
    state = decode_weather_state(source)
    if state['identity_version'] != 1:
        raise ValueError('regional fixture requires a native identity catalog')
    catalog = state['catalog']
    declarations = recipe['regions']
    if not isinstance(declarations, list) or not declarations:
        raise ValueError('missing regional weather declarations')
    changes = {}
    for item in declarations:
        if not isinstance(item, dict) or set(item) != {'region', 'weather', 'buckets', 'fallback'}:
            raise ValueError('unexpected regional weather declaration')
        region = item['region']
        if any(not isinstance(item[key], str) for key in ('region', 'weather', 'fallback')):
            raise ValueError('invalid regional weather identity')
        if region not in state['regions'] or region in changes or item['weather'] not in catalog or item['fallback'] not in catalog:
            raise ValueError('unknown or duplicate region/weather identity')
        buckets = item['buckets']
        if not isinstance(buckets, list) or not buckets:
            raise ValueError('missing regional weather buckets')
        chances = [0] * len(catalog)
        seen = set()
        for bucket in buckets:
            if not isinstance(bucket, dict) or set(bucket) != {'weather', 'chance'}:
                raise ValueError('unexpected regional weather bucket')
            key, chance = bucket['weather'], bucket['chance']
            if not isinstance(key, str) or key not in catalog or key in seen or type(chance) is not int or not 1 <= chance <= 100:
                raise ValueError('invalid regional weather bucket')
            seen.add(key)
            chances[catalog.index(key)] = chance
        if sum(chances) > 100:
            raise ValueError('regional fixture chance total exceeds100')
        changes[region] = (catalog.index(item['weather']), catalog.index(item['fallback']), chances)

    rows = list(_save_records(source))
    start, end, _, subs = next(row for row in rows if row[2] == b'WTHR')
    fields = [(tag, source[a + 8:b]) for tag, a, b in subs]
    region_names = iter(state['regions'])
    rewritten = []
    offset = 0
    while offset < len(fields):
        tag, value = fields[offset]
        if tag == b'WUPD':
            value = struct.pack('<f', delay)
        if tag != b'RGNN':
            rewritten.append((tag, value)); offset += 1
            continue
        name = next(region_names)
        stop = offset + 1
        while stop < len(fields) and fields[stop][0] != b'RGNN':
            stop += 1
        if name not in changes:
            rewritten.extend(fields[offset:stop])
        else:
            active, fallback, chances = changes[name]
            rewritten += [(tag, value), (b'RGNW', struct.pack('<i', active))]
            if fallback:
                rewritten.append((b'RGDF', struct.pack('<i', fallback)))
            rewritten += [(b'RGNC', bytes([chance])) for chance in chances]
        offset = stop
    body = b''.join(tag + struct.pack('<I', len(value)) + value for tag, value in rewritten)
    header = bytearray(source[start:start + 16]); struct.pack_into('<I', header, 4, len(body))
    output = source[:start] + header + body + source[end:]
    check = decode_weather_state(output)
    old_other = [source[a:b] for a,b,tag,_ in rows if tag != b'WTHR']
    new_other = [output[a:b] for a,b,tag,_ in _save_records(output) if tag != b'WTHR']
    if old_other != new_other:
        raise ValueError('regional fixture changed a non-weather record')
    expected = dict(state)
    expected['update_time'] = struct.unpack('<f', struct.pack('<f', delay))[0]
    expected['regions'] = dict(state['regions'])
    for name, (active, fallback, chances) in changes.items():
        expected['regions'][name] = dict(weather=catalog[active], fallback=catalog[fallback],
                                        chances=chances, selection_order=catalog)
    if check != expected:
        raise ValueError('regional fixture semantic readback mismatch')
    return output, {'source_sha256': hashlib.sha256(source).hexdigest(),
                    'output_sha256': hashlib.sha256(output).hexdigest(),
                    'changed_regions': list(changes), 'non_weather_records_exact': len(old_other),
                    'weather': check, 'scope': 'diagnostic setup only; runtime resave required'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('recipe', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    output, report = build(args.source.read_bytes(), json.loads(args.recipe.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as stream:
        stream.write(output)
    args.output.with_suffix(args.output.suffix + '.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
