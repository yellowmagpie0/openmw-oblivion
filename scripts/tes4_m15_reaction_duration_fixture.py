"""Create an isolated one-view reaction hold extension from a pinned stock KF.

Only the controller StopTime and End text-key time change. Transform tracks,
priorities and all other bytes remain intact. Actual NIF reader and runtime
acceptance are separate checks; no plugin, contact or resource value is written.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct

HEADER = b'Gamebryo File Format, Version 20.0.0.4\n'


def build(source: bytes, asset: dict, end: float) -> tuple[bytes, dict]:
    fields = {'path', 'sha256', 'stop_offset', 'end_key_offset', 'original_end_bits',
              'sequence_name_offset', 'controlled_blocks'}
    if set(asset) not in (fields, fields | {'sequence_name'}) or hashlib.sha256(source).hexdigest() != asset['sha256']:
        raise ValueError('reaction duration source differs from recipe')
    if not source.startswith(HEADER):
        raise ValueError('reaction duration requires the reviewed KF version')
    positions = [asset[key] for key in ('stop_offset', 'end_key_offset', 'sequence_name_offset')]
    if any(type(v) is not int or not 4 <= v <= len(source) - 12 for v in positions):
        raise ValueError('reaction duration offset outside source')
    stop, key, name = positions
    sequence_name = asset.get('sequence_name', 'BlockHit')
    if sequence_name not in ('BlockHit', 'AttackLeft', 'AttackRight', 'AttackPower'):
        raise ValueError('unsupported native hold sequence identity')
    sequence = sequence_name.encode('ascii')
    count = asset['controlled_blocks']
    if type(count) is not int or not 1 <= count <= 256 or stop != name + 28 + len(sequence) + count * 33:
        raise ValueError('reaction duration sequence layout differs')
    if source[name - 4:name + len(sequence)] != struct.pack('<I', len(sequence)) + sequence:
        raise ValueError('reaction duration sequence identity differs')
    if struct.unpack_from('<I', source, name + len(sequence))[0] != count:
        raise ValueError('reaction duration controlled-block inventory differs')
    weight, _, cycle, frequency, start = struct.unpack_from('<fIIff', source, stop - 20)
    if (weight, cycle, frequency, start) != (1., 2, 1., 0.):
        raise ValueError('reaction duration controller layout differs')
    if source[key + 4:key + 11] != struct.pack('<I', 3) + b'end' or abs(stop - key) < 4:
        raise ValueError('reaction duration End key identity differs')
    if type(asset['original_end_bits']) is not int or not 0 <= asset['original_end_bits'] <= 0xffffffff:
        raise ValueError('reaction duration requires exact original float bits')
    for offset in (stop, key):
        if struct.unpack_from('<I', source, offset)[0] != asset['original_end_bits']:
            raise ValueError('reaction duration original end differs')
    original = struct.unpack('<f', struct.pack('<I', asset['original_end_bits']))[0]
    if type(end) not in (int, float) or not math.isfinite(end) or not original < end <= 4:
        raise ValueError('reaction hold extension outside supported range')
    end = struct.unpack('<f', struct.pack('<f', end))[0]
    if end <= original:
        raise ValueError('reaction hold extension disappears after float storage')
    output = bytearray(source)
    for offset in (stop, key):
        struct.pack_into('<f', output, offset, end)
    if any(a != b and not (stop <= i < stop + 4 or key <= i < key + 4)
           for i, (a, b) in enumerate(zip(source, output))):
        raise AssertionError('reaction duration changed unrelated bytes')
    result = bytes(output)
    return result, dict(source_sha256=asset['sha256'], sha256=hashlib.sha256(result).hexdigest(),
                        path=asset['path'], original_end=original,
                        end=struct.unpack_from('<f', result, stop)[0],
                        changed_fields=['NiControllerSequence.StopTime', 'NiTextKeyExtraData.End.time'],
                        transform_tracks_and_other_bytes_preserved=True,
                        nif_reader_validation_performed=False, runtime_acceptance=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source_directory', type=Path)
    parser.add_argument('view', choices=('body', 'first'))
    parser.add_argument('output', type=Path, help='New override directory')
    parser.add_argument('--recipe', type=Path, default=Path(__file__).parent /
                        'data/oblivion_compat/m15_reaction_duration_fixture.json')
    args = parser.parse_args()
    recipe = json.loads(args.recipe.read_text())
    if recipe['schema_version'] != 1:
        raise ValueError('unsupported reaction duration recipe')
    asset = recipe['assets'][args.view]
    expected = 'meshes/characters/' + ('_male' if args.view == 'body' else '_1stperson') + '/onehandblockhit.kf'
    if asset['path'] != expected:
        raise ValueError('reaction duration asset is outside the selected view')
    output, report = build((args.source_directory / expected).read_bytes(), asset, recipe['extended_end'])
    args.output.mkdir(parents=True, exist_ok=False)
    target = args.output / expected
    target.parent.mkdir(parents=True)
    target.write_bytes(output)
    (args.output / 'reaction-duration.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
