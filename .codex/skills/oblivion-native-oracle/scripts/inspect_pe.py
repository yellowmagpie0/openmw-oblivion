#!/usr/bin/env python3
"""Hash-check an original x86 PE32 executable and emit metadata, never its bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def inspect(path, expected):
    data = path.read_bytes()
    actual = hashlib.sha256(data).hexdigest()
    if actual != expected.lower():
        raise ValueError(f'executable SHA-256 mismatch: expected {expected}, found {actual}')
    def unpack(fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(data):
            raise ValueError('truncated PE header or section table')
        return struct.unpack_from(fmt, data, offset)
    if data[:2] != b'MZ':
        raise ValueError('missing MZ header')
    pe, = unpack('<I', 0x3c)
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError('missing PE signature')
    machine, count = unpack('<HH', pe+4)
    optional_size, = unpack('<H', pe+20)
    optional = pe+24
    magic, = unpack('<H', optional)
    if machine != 0x14c or magic != 0x10b or optional_size < 60 or not 1 <= count <= 96:
        raise ValueError('expected PE32/i386 executable with a bounded section table')
    base, = unpack('<I', optional+28)
    image_size, = unpack('<I', optional+56)
    if not image_size or base + image_size > 2**32:
        raise ValueError('invalid image address range')
    sections, ranges = [], []
    for index in range(count):
        name, virtual_size, rva, raw_size, raw = unpack('<8sIIII', optional+optional_size+40*index)
        unpack('<16s', optional+optional_size+40*index+24)
        extent = max(virtual_size, raw_size)
        if raw + raw_size > len(data) or rva + extent > image_size:
            raise ValueError('section extends beyond executable or image')
        if extent and any(rva < end and start < rva+extent for start, end in ranges):
            raise ValueError('overlapping image sections')
        if extent:
            ranges.append((rva, rva+extent))
        sections.append([name.rstrip(b'\0').decode('ascii'), base+rva, raw, raw_size])
    return {'sha256': actual, 'path': str(path.resolve()), 'image_base': base,
            'image_size': image_size, 'sections': sections}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.output.exists():
            raise ValueError('output exists; preserve evidence and choose a new path')
        result = inspect(args.exe, args.sha256)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('x') as stream:
            json.dump(result, stream, indent=2)
            stream.write('\n')
        print(json.dumps({'sha256': result['sha256'], 'sections': len(result['sections']), 'output': str(args.output)}))
    except (OSError, ValueError, UnicodeDecodeError) as error:
        parser.exit(1, f'inspection failed: {error}\n')


if __name__ == '__main__':
    main()
