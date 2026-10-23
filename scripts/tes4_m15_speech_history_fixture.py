"""Isolate one pinned, originally SayOnce voiced INFO under an authored topic/quest.

Licensed response bytes stay in the locally generated plugin. The editable
recipe and writer describe the source identities and exact allowed mutations.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
from tes4_m15_fixture import records, record, group, sub, string, reader


def build(source: bytes, recipe: dict) -> tuple[bytes, dict]:
    fields = {'schema_version', 'source_sha256', 'source_plugin', 'plugin_name', 'source_info',
              'source_payload_sha256', 'topic', 'quest', 'topic_editor_id', 'quest_editor_id'}
    if (not isinstance(recipe, dict) or set(recipe) != fields or type(recipe['schema_version']) is not int
            or recipe['schema_version'] != 1 or recipe['source_plugin'] != 'Oblivion.esm'
            or not isinstance(recipe['plugin_name'], str) or not recipe['plugin_name'].endswith('.esp')
            or any(c in recipe['plugin_name'] for c in '/\\\x00')
            or type(recipe['source_info']) is not int or not 0 < recipe['source_info'] < 0x1000000
            or any(type(recipe[k]) is not int or not 0x1000800 <= recipe[k] < 0x2000000 for k in ('topic', 'quest'))
            or recipe['topic'] == recipe['quest']
            or any(not isinstance(recipe[k], str) or not recipe[k].isascii() or not recipe[k].isalnum()
                   for k in ('topic_editor_id', 'quest_editor_id'))
            or hashlib.sha256(source).hexdigest() != recipe['source_sha256']):
        raise ValueError('speech history fixture recipe/source differs')
    selected = [payload for tag, ident, payload in records(source) if tag == 'INFO' and ident == recipe['source_info']]
    if len(selected) != 1 or hashlib.sha256(selected[0]).hexdigest() != recipe['source_payload_sha256']:
        raise ValueError('speech history fixture source INFO differs')
    parts = reader._subrecords(selected[0], 'fixture source', 'INFO')
    data = [s['payload'] for s in parts if s['name'] == 'DATA']
    if len(data) != 1 or len(data[0]) != 3 or not data[0][2] & 4 or data[0][2] & (8 | 16):
        raise ValueError('speech history fixture requires an ordinary originally SayOnce INFO')
    if any(s['name'] in ('NAME', 'PNAM') for s in parts):
        raise ValueError('speech history fixture excludes additional topics and predecessor links')
    if sum(s['name'] == 'QSTI' and len(s['payload']) == 4 for s in parts) != 1:
        raise ValueError('speech history fixture requires one quest binding')
    after = b''.join(sub(s['name'], struct.pack('<I', recipe['quest']) if s['name'] == 'QSTI' else s['payload'])
                     for s in parts) + sub('PNAM', struct.pack('<I', 0))
    topic = record('DIAL', recipe['topic'], string('EDID', recipe['topic_editor_id'])
                   + sub('QSTI', struct.pack('<I', recipe['quest']))
                   + string('FULL', 'Speech history test') + sub('DATA', bytes([data[0][0]])))
    quest = record('QUST', recipe['quest'], string('EDID', recipe['quest_editor_id']) + sub('DATA', bytes([1, 50])))
    header = record('TES4', 0, sub('HEDR', struct.pack('<fII', 1., 3, (max(recipe['topic'], recipe['quest']) & 0xffffff) + 1))
                    + string('CNAM', 'OpenMW isolated speech history fixture')
                    + string('MAST', recipe['source_plugin']) + sub('DATA', struct.pack('<Q', len(source))))
    output = header + group(b'QUST', 0, quest) + group(b'DIAL', 0, topic
                + group(struct.pack('<I', recipe['topic']), 7, record('INFO', recipe['source_info'], after)))
    return output, dict(schema_version=1, structural_write_success=True,
        source_info_sha256=recipe['source_payload_sha256'], plugin_sha256=hashlib.sha256(output).hexdigest(),
        mutations=['New DIAL and start-enabled QUST', 'Existing INFO reparented to new DIAL; QSTI replaced; explicit PNAM zero inserts at head'],
        original_SayOnce_flags_preserved=data[0][2], voice_owner_and_response_bytes_preserved=True,
        compiler_world_validation=False, semantic_reinspection=False, runtime_acceptance=False)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('recipe', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.output.exists(): raise FileExistsError(args.output)
    recipe = json.loads(args.recipe.read_text())
    output, report = build(args.source.read_bytes(), recipe)
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / recipe['plugin_name']).write_bytes(output)
    (args.output / 'fixture-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__': main()
