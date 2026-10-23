"""Append a class condition to a pinned existing voiced INFO for an isolated runtime check.

The generated override retains licensed response data locally. The editable
recipe and writer belong in Git; the generated plugin does not.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct

from tes4_m15_fixture import records, record, group, sub, string, reader


def build(source: bytes, recipe: dict) -> tuple[bytes, dict]:
    fields = {'schema_version', 'source_sha256', 'source_plugin', 'plugin_name', 'npc', 'topic', 'info',
              'class', 'source_payload_sha256'}
    if (not isinstance(recipe, dict) or set(recipe) != fields or recipe['schema_version'] != 1
            or recipe['source_plugin'] != 'Oblivion.esm'
            or not isinstance(recipe['plugin_name'], str)
            or not recipe['plugin_name'].endswith('.esp')
            or any(c in recipe['plugin_name'] for c in '/\\\x00')
            or any(type(recipe[k]) is not int or not 0 < recipe[k] < 0x1000000
                   for k in ('npc', 'topic', 'info', 'class'))
            or hashlib.sha256(source).hexdigest() != recipe['source_sha256']):
        raise ValueError('class dialogue fixture recipe/source differs')
    wanted = {('NPC_', recipe['npc']), ('DIAL', recipe['topic']), ('INFO', recipe['info']),
              ('CLAS', recipe['class'])}
    selected = {}
    for tag, ident, payload in records(source):
        if (tag, ident) in wanted:
            if tag in selected:
                raise ValueError('duplicate fixture source identity')
            selected[tag] = payload
    if len(selected) != 4:
        raise ValueError('class dialogue fixture source records missing')
    hashes = {tag: hashlib.sha256(payload).hexdigest() for tag, payload in selected.items()}
    if hashes != recipe['source_payload_sha256']:
        raise ValueError('class dialogue fixture source payload differs')
    npc = reader._subrecords(selected['NPC_'], 'fixture source', 'NPC_')
    classes = [s['payload'] for s in npc if s['name'] == 'CNAM']
    if classes != [struct.pack('<I', recipe['class'])]:
        raise ValueError('class fixture NPC does not use the declared class')
    info = reader._subrecords(selected['INFO'], 'fixture source', 'INFO')
    if any(s['name'] == 'CTDA' and struct.unpack_from('<I', s['payload'], 8)[0] == 68 for s in info):
        raise ValueError('class condition already exists in fixture source')
    condition = bytearray(24)
    struct.pack_into('<fII', condition, 4, 1., 68, recipe['class'])
    after = bytearray()
    inserted = False
    for s in info:
        if s['name'] == 'SCHR' and not inserted:
            after += sub('CTDA', condition)
            inserted = True
        after += sub(s['name'], s['payload'])
    if not inserted:
        after += sub('CTDA', condition)
    header = record('TES4', 0, sub('HEDR', struct.pack('<fII', 1., 2, 0x800))
                    + string('CNAM', 'OpenMW class condition runtime fixture')
                    + string('MAST', recipe['source_plugin']) + sub('DATA', struct.pack('<Q', len(source))))
    children = group(struct.pack('<I', recipe['topic']), 7, record('INFO', recipe['info'], after))
    output = header + group(b'DIAL', 0, record('DIAL', recipe['topic'], selected['DIAL']) + children)
    return output, dict(schema_version=1, structural_write_success=True, source_payload_sha256=hashes,
                        appended_condition=dict(function=68, comparison=1., class_id=recipe['class']),
                        plugin_sha256=hashlib.sha256(output).hexdigest(),
                        world_validation=False, semantic_reinspection=False, runtime_acceptance=False)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('recipe', type=Path)
    parser.add_argument('output', type=Path, help='New isolated fixture directory')
    args = parser.parse_args()
    recipe = json.loads(args.recipe.read_text())
    if args.output.exists():
        raise FileExistsError(args.output)
    data, report = build(args.source.read_bytes(), recipe)
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / recipe['plugin_name']).write_bytes(data)
    (args.output / 'fixture-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
