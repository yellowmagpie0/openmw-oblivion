"""Reproduce isolated Player attack hold extensions from hash-pinned stock KFs.

The selected view's controller StopTime and End key are extended, and one
zero-time blend key is inserted to make outgoing retirement observable. A
successful write is structural evidence; NIF parsing and gameplay are separate.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path, PurePosixPath

from tes4_m15_reaction_duration_fixture import build


def insert_blend(data: bytes, asset: dict, frames: int) -> bytes:
    """Insert one declared zero-time key in the final reviewed text-key record.

    NIF 20.0.0.4 uses record indices and no record-size table. This narrow
    fixture requires the final key array followed by the single-root footer.
    """
    offset = asset.get('text_key_count_offset')
    if type(offset) is not int or not 4 <= offset < len(data) - 12:
        raise ValueError('strike text-key count offset outside source')
    count = struct.unpack_from('<I', data, offset)[0]
    if not 3 <= count <= 16:
        raise ValueError('strike text-key inventory differs')
    pos = offset + 4
    keys = []
    end_offset = None
    for _ in range(count):
        if pos + 8 > len(data):
            raise ValueError('truncated strike text key')
        time, size = struct.unpack_from('<fI', data, pos)
        if not math.isfinite(time) or not 1 <= size <= 128 or pos + 8 + size > len(data):
            raise ValueError('invalid strike text key')
        key = data[pos + 8:pos + 8 + size]
        if key.lower().startswith(b'blend:'):
            raise ValueError('strike source already declares blend')
        if key == b'end':
            if end_offset is not None: raise ValueError('duplicate strike End key')
            end_offset = pos
        keys.append((time, key))
        pos += 8 + size
    if (keys[0] != (0., b'start') or keys[-1][1] != b'end'
            or end_offset != asset['end_key_offset']
            or data[pos:] != bytes.fromhex('0100000000000000')
            or asset['stop_offset'] >= offset):
        raise ValueError('strike final key-array/footer layout differs')
    text = ('blend:' + str(frames)).encode('ascii')
    inserted = struct.pack('<fI', 0., len(text)) + text
    return data[:offset] + struct.pack('<I', count + 1) + inserted + data[offset + 4:]


def build_fixture(source_directory: Path, view: str, recipe: dict, output: Path) -> dict:
    fields = {'schema_version', 'purpose', 'source_archive_sha256', 'extended_end', 'blend_frames', 'assets'}
    if (not isinstance(recipe, dict) or set(recipe) != fields or type(recipe['schema_version']) is not int or recipe['schema_version'] != 1
            or type(recipe['blend_frames']) is not int or not 1 <= recipe['blend_frames'] <= 255
            or view not in ('body', 'first') or not isinstance(recipe['purpose'], str)
            or not isinstance(recipe['source_archive_sha256'], str)
            or len(recipe['source_archive_sha256']) != 64
            or any(c not in '0123456789abcdef' for c in recipe['source_archive_sha256'])
            or not isinstance(recipe['assets'], dict) or set(recipe['assets']) != {'body', 'first'}):
        raise ValueError('invalid Player strike duration recipe')
    if output.exists():
        raise ValueError('Player strike duration output already exists')
    assets = recipe['assets'][view]
    prefix = 'meshes/characters/' + ('_male' if view == 'body' else '_1stperson') + '/'
    expected = {prefix + 'handtohandattack' + suffix + '.kf' for suffix in ('left', 'right', 'power')}
    if not isinstance(assets, list) or len(assets) != 3:
        raise ValueError('Player strike duration requires all three selected-view attacks')
    prepared = []
    seen = set()
    # Validate and build every asset before creating the output directory.
    for asset in assets:
        if (not isinstance(asset, dict) or not isinstance(asset.get('path'), str)
                or asset['path'] not in expected or asset['path'] in seen):
            raise ValueError('invalid, duplicate or foreign Player strike asset path')
        path = PurePosixPath(asset['path'])
        if path.is_absolute() or '..' in path.parts or str(path) != asset['path']:
            raise ValueError('noncanonical Player strike asset path')
        seen.add(asset['path'])
        source = (source_directory / path).read_bytes()
        duration_asset = {k: v for k, v in asset.items() if k != 'text_key_count_offset'}
        data, report = build(source, duration_asset, recipe['extended_end'])
        if asset.get('sequence_name') != 'Attack' + path.stem.removeprefix('handtohandattack').title():
            raise ValueError('Player strike path and sequence identity differ')
        data = insert_blend(data, asset, recipe['blend_frames'])
        report['sha256'] = hashlib.sha256(data).hexdigest()
        report['changed_fields'] += ['NiTextKeyExtraData.numKeys', 'NiTextKeyExtraData.blend']
        report['blend_frames'] = recipe['blend_frames']
        prepared.append((path, data, report))
    output.mkdir(parents=True, exist_ok=False)
    for path, data, _ in prepared:
        destination = output / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    report = dict(schema_version=1, view=view, files=[r for _, _, r in prepared],
        declared_source_archive_sha256=recipe['source_archive_sha256'],
        archive_itself_rehashed=False, nif_reader_validation=False, runtime_acceptance=False)
    (output / 'fixture-report.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source_directory', type=Path)
    parser.add_argument('view', choices=('body', 'first'))
    parser.add_argument('output', type=Path, help='New override directory')
    parser.add_argument('--recipe', type=Path, default=Path(__file__).parent /
                        'data/oblivion_compat/m15_player_strike_duration_fixture.json')
    args = parser.parse_args()
    recipe_bytes = args.recipe.read_bytes()
    recipe = json.loads(recipe_bytes)
    report = build_fixture(args.source_directory, args.view, recipe, args.output)
    report['recipe_sha256'] = hashlib.sha256(recipe_bytes).hexdigest()
    (args.output / 'fixture-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
