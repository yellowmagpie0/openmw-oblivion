"""Reproduce the faster two-contact S4 stationary fixture from editable sources.

Preserves the hash-pinned stationary master and overrides its script plus the
declared stock weapon's speed. No scripted hits or resource writes.
TES4 structure/readback, fork SCTX compilation and runtime acceptance are
separate checks; this is unrelated to the TES3-only OpenMW-CS MCP author.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import struct
from pathlib import Path

from tes4_m15_fixture import group, record, records, sub, string
from tes4_m14_audit import _subrecords, COMPRESSED_FLAG
import tes4_m15_stationary_fixture as stationary


def build(source: bytes, stationary_source: bytes, recipe: dict, script: str) -> tuple[bytes, dict]:
    stationary.fields(recipe, 'version purpose source_sha256 master npc style script weapon stationary_master')
    if type(recipe['version']) is not int or recipe['version'] != 3:
        raise ValueError('unsupported cached-block fixture version')
    parent = recipe['stationary_master']
    stationary.fields(parent, 'name sha256')
    if parent['name'] != 'M15StationaryOpponent.esp' or hashlib.sha256(stationary_source).hexdigest() != parent['sha256']:
        raise ValueError('cached-block stationary master differs from reviewed recipe')
    weapon = recipe['weapon']
    stationary.fields(weapon, 'id editor_id speed')
    stationary.identifier(weapon['editor_id'])
    if type(weapon['id']) is not int or not 1 <= weapon['id'] <= 0xffffff:
        raise ValueError('cached-block weapon requires a master FormID')
    if type(weapon['speed']) not in (int, float) or not math.isfinite(weapon['speed']) or not 0 < weapon['speed'] <= 20:
        raise ValueError('cached-block fixture weapon speed outside supported range')
    base_recipe = copy.deepcopy(recipe)
    del base_recipe['weapon']
    del base_recipe['stationary_master']
    base_recipe['version'] = 1
    base, base_report = stationary.build(source, base_recipe, script)
    base_rows = list(records(base))
    parent_rows = list(records(stationary_source))
    if len(parent_rows) != 4 or {(tag, ident) for tag, ident, _ in parent_rows} != {(tag, ident) for tag, ident, _ in base_rows}:
        raise ValueError('cached-block stationary master has unexpected records')
    for tag, ident, payload in base_rows[1:]:
        old = next(data for kind, form, data in parent_rows if (kind, form) == (tag, ident))
        if tag != 'SCPT':
            if old != payload:
                raise ValueError('cached-block stationary master NPC/style differs')
        else:
            before = _subrecords(old, 'cached-block old script', 'SCPT')
            after = _subrecords(payload, 'cached-block new script', 'SCPT')
            if [s for s in before if s['name'] != 'SCTX'] != [s for s in after if s['name'] != 'SCTX']:
                raise ValueError('cached-block script changes its saved local layout')
    matches = [(ident, payload) for tag, ident, payload in records(source)
               if tag == 'WEAP' and ident == weapon['id']]
    if len(matches) != 1:
        raise ValueError('missing or ambiguous cached-block weapon')
    original = _subrecords(matches[0][1], 'cached-block source', 'WEAP')
    if any(sum(s['name'] == key for s in original) != 1 for key in ('EDID', 'DATA')):
        raise ValueError('ambiguous cached-block weapon identity/data')
    if next(s['payload'] for s in original if s['name'] == 'EDID') != weapon['editor_id'].encode('cp1252') + b'\0':
        raise ValueError('cached-block weapon editor identity mismatch')
    data = next(s['payload'] for s in original if s['name'] == 'DATA')
    if len(data) != 30 or data[0] > 3:
        raise ValueError('cached-block fixture requires a TES4 melee weapon')
    changed = bytearray(data)
    struct.pack_into('<f', changed, 4, weapon['speed'])
    fields = [(s['name'], bytes(changed) if s['name'] == 'DATA' else s['payload']) for s in original]
    weapon_payload = b''.join(sub(name, value) for name, value in fields)
    # Preserve record flags even when the winning source used compression.
    def flags(start, end):
        found = []
        while start < end:
            if end - start < 20:
                raise ValueError('truncated cached-block source header')
            tag, size, value, ident, _ = struct.unpack_from('<4sIIII', source, start)
            finish = start + size if tag == b'GRUP' else start + 20 + size
            if finish < start + 20 or finish > end:
                raise ValueError('cached-block source record exceeds parent')
            if tag == b'GRUP':
                found.extend(flags(start + 20, finish))
            elif tag == b'WEAP' and ident == weapon['id']:
                found.append(value & ~COMPRESSED_FLAG)
            start = finish
        return found
    header_flags = flags(0, len(source))
    if len(header_flags) != 1:
        raise ValueError('ambiguous cached-block weapon flags')
    header_payload = sub('HEDR', struct.pack('<fII', 1, 2, 0x800))
    header_payload += string('CNAM', 'OpenMW synthetic cached-block overlay')
    for master in (recipe['master'], parent['name']):
        header_payload += string('MAST', master) + sub('DATA', bytes(8))
    groups = {tag: record(tag, ident, payload) for tag, ident, payload in base_rows[1:] if tag == 'SCPT'}
    groups['WEAP'] = record('WEAP', weapon['id'], weapon_payload, header_flags[0])
    output = record('TES4', 0, header_payload) + b''.join(group(tag.encode('ascii'), 0, payload)
                                                       for tag, payload in sorted(groups.items()))
    emitted = list(records(output))
    if len(emitted) != 3:
        raise ValueError('cached-block emitted record count differs')
    rebuilt = _subrecords(next(payload for tag, _, payload in emitted if tag == 'WEAP'), 'cached-block output', 'WEAP')
    if [(s['name'], s['payload']) for s in rebuilt] != fields:
        raise ValueError('cached-block weapon reinspection failed')
    return output, dict(base_report, kind='synthetic-native-cached-block-overlay', record_count=2,
                        sha256=hashlib.sha256(output).hexdigest(), weapon_fields_changed=['DATA.speed'],
                        weapon_original_speed=struct.unpack_from('<f', data, 4)[0],
                        weapon_speed=struct.unpack_from('<f', changed, 4)[0], weapon_flags=header_flags[0],
                        stationary_master_sha256=parent['sha256'], script_local_layout_preserved=True,
                        npc_fields_changed=[])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    directory = Path(__file__).parent / 'data/oblivion_compat'
    parser.add_argument('source', type=Path)
    parser.add_argument('stationary_master', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--recipe', type=Path, default=directory / 'm15_cached_block_opponent_fixture.json')
    parser.add_argument('--script', type=Path, default=directory / 'm15_cached_block_opponent.obscript')
    args = parser.parse_args()
    output, report = build(args.source.read_bytes(), args.stationary_master.read_bytes(),
                           json.loads(args.recipe.read_text()), args.script.read_text())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as stream:
        stream.write(output)
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
