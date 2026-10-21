"""Reproduce two pinned BlockHit holds with an observable captured blend.

Only StopTime, End and the reviewed existing Blend modifier change. The
synthetic clips provide a restart observation window, not motion parity.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

from tes4_m15_reaction_duration_fixture import build


def replace_blend(data: bytes, asset: dict) -> bytes:
    offset = asset.get('text_key_count_offset')
    if (type(offset) is not int or not 4 <= offset < len(data)
            or asset.get('blend_text_offset') != offset + 25
            or asset['end_key_offset'] != offset + 33):
        raise ValueError('cached BlockHit key-array offsets differ')
    expected = (struct.pack('<IfI', 3, 0., 5) + b'start'
                + struct.pack('<II', 0x3d088880, 8) + b'Blend: 0'
                + struct.pack('<fI', 4., 3) + b'end'
                + bytes.fromhex('0100000000000000'))
    if data[offset:] != expected:
        raise ValueError('cached BlockHit reviewed key-array/footer differs')
    blend = asset['blend_text_offset']
    return data[:blend] + b'Blend:30' + data[blend + 8:]


def build_fixture(source_directory: Path, recipe: dict, output: Path) -> dict:
    fields = {'schema_version', 'purpose', 'source_archive_sha256', 'extended_end', 'blend_frames', 'assets'}
    if (not isinstance(recipe, dict) or set(recipe) != fields
            or type(recipe['schema_version']) is not int or recipe['schema_version'] != 1
            or recipe['extended_end'] != 4 or type(recipe['blend_frames']) is not int
            or recipe['blend_frames'] != 30 or not isinstance(recipe['purpose'], str)
            or not isinstance(recipe['source_archive_sha256'], str)
            or len(recipe['source_archive_sha256']) != 64
            or any(c not in '0123456789abcdef' for c in recipe['source_archive_sha256'])
            or not isinstance(recipe['assets'], dict) or set(recipe['assets']) != {'body', 'first'}):
        raise ValueError('unsupported cached BlockHit hold recipe')
    if output.exists():
        raise FileExistsError(output)
    prepared = []
    for view, directory in [('body', '_male'), ('first', '_1stperson')]:
        asset = recipe['assets'][view]
        expected = 'meshes/characters/' + directory + '/onehandblockhit.kf'
        if not isinstance(asset, dict) or asset.get('path') != expected:
            raise ValueError('cached BlockHit asset is outside its declared view')
        duration_asset = {k: v for k, v in asset.items()
                          if k not in ('text_key_count_offset', 'blend_text_offset')}
        data, report = build((source_directory / expected).read_bytes(), duration_asset, 4.)
        data = replace_blend(data, asset)
        report['changed_fields'] += ['NiTextKeyExtraData.Blend.value']
        report.update(sha256=hashlib.sha256(data).hexdigest(), blend_frames=30)
        prepared.append((expected, data, report))
    output.mkdir(parents=True, exist_ok=False)
    for name, data, _ in prepared:
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    report = dict(schema_version=1, files=[r for _, _, r in prepared],
                  structural_write_success=True,
                  declared_source_archive_sha256=recipe['source_archive_sha256'],
                  archive_itself_rehashed=False, nif_reader_validation=False, runtime_acceptance=False)
    (output / 'fixture-report.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source_directory', type=Path)
    parser.add_argument('output', type=Path, help='New override directory')
    parser.add_argument('--recipe', type=Path, default=Path(__file__).parent /
                        'data/oblivion_compat/m15_player_cached_block_hold_fixture.json')
    args = parser.parse_args()
    print(json.dumps(build_fixture(args.source_directory, json.loads(args.recipe.read_text()), args.output), indent=2))


if __name__ == '__main__':
    main()
