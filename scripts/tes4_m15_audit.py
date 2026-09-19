"""Native M15 content inventory and independently decoded combat-style data.

This audit classifies data, not gameplay acceptance. Missing runtime/default
oracles remain explicit unresolved policies even when structural reads succeed.
Generated reports may contain installed game data and belong under build/.
"""
from __future__ import annotations

import collections
import hashlib
import math
import struct
from pathlib import Path
from typing import Any

import tes4_m14_audit as binary


class M15AuditError(ValueError):
    pass


# Struct grouping is independent of the C++ decoder's explicit byte offsets.
_CORE = struct.Struct('<2B2x8f2B2x3fB3x2f5B3x2f2B2x')
_CORE_NAMES = ('dodge_chance', 'left_right_chance', 'dodge_lr_min', 'dodge_lr_max',
    'dodge_forward_min', 'dodge_forward_max', 'dodge_back_min', 'dodge_back_max',
    'idle_min', 'idle_max', 'block_chance', 'attack_chance', 'attack_recoil_bonus',
    'attack_unconscious_bonus', 'attack_unarmed_bonus', 'power_attack_chance',
    'power_recoil_bonus', 'power_unconscious_bonus', 'power_normal', 'power_forward',
    'power_back', 'power_left', 'power_right', 'hold_min', 'hold_max', 'flags', 'acrobatic_chance')
_TAILS = ((92, '<2f', ('optimal_range_multiplier', 'maximum_range_multiplier')),
    (104, '<3f', ('melee_switch_distance', 'ranged_switch_distance', 'buff_standoff')),
    (112, '<2f', ('ranged_standoff', 'group_standoff')),
    (120, '<B3xf', ('rush_chance', 'rush_distance_multiplier')),
    (124, '<I', ('do_not_acquire',)))
_ADVANCED_NAMES = ('dodge_fatigue_multiplier', 'dodge_fatigue_base', 'encumbered_speed_base',
    'encumbered_speed_multiplier', 'dodge_under_attack', 'dodge_not_under_attack',
    'back_dodge_under_attack', 'back_dodge_not_under_attack', 'forward_dodge_attacking',
    'forward_dodge_not_attacking', 'block_skill_multiplier', 'block_skill_base',
    'block_under_attack', 'block_not_under_attack', 'attack_skill_multiplier',
    'attack_skill_base', 'attack_under_attack', 'attack_not_under_attack',
    'attack_during_block', 'power_attack_fatigue_base', 'power_attack_fatigue_multiplier')


def combat_style(standard: bytes, advanced: bytes | None = None) -> dict[str, Any]:
    if len(standard) not in (84, 92, 104, 112, 120, 124):
        raise M15AuditError(f'CSTD invalid TES4 size: {len(standard)}')
    values = dict(zip(_CORE_NAMES, _CORE.unpack_from(standard), strict=True))
    offset = _CORE.size
    for end, format_, names in _TAILS:
        if len(standard) >= end:
            values.update(zip(names, struct.unpack_from(format_, standard, offset), strict=True))
        else:
            values.update(dict.fromkeys(names))
        offset = end
    for name, value in values.items():
        if value is None:
            continue
        if isinstance(value, float) and not math.isfinite(value):
            raise M15AuditError(f'CSTD nonfinite {name}')
        if name.endswith('_chance') or name in ('power_normal', 'power_forward', 'power_back', 'power_left', 'power_right'):
            if not 0 <= value <= 100:
                raise M15AuditError(f'CSTD invalid percentage: {name}')
        if isinstance(value, float) and not name.endswith('_bonus') and value < 0:
            raise M15AuditError(f'CSTD negative timer/distance/multiplier: {name}')
    for prefix in ('dodge_lr', 'dodge_forward', 'dodge_back', 'idle', 'hold'):
        if values[prefix + '_min'] > values[prefix + '_max']:
            raise M15AuditError(f'CSTD reversed timer: {prefix}')
    if values['do_not_acquire'] not in (None, 0, 1):
        raise M15AuditError('CSTD invalid Do Not Acquire flag')
    if values['do_not_acquire'] is not None:
        values['do_not_acquire'] = bool(values['do_not_acquire'])
    extra = None
    if advanced is not None:
        if len(advanced) != 84:
            raise M15AuditError('CSAD must contain exactly 21 floats')
        extra = dict(zip(_ADVANCED_NAMES, struct.unpack('<21f', advanced), strict=True))
        if any(not math.isfinite(value) for value in extra.values()):
            raise M15AuditError('CSAD contains nonfinite values')
    return {'standard_size': len(standard), 'standard': values, 'advanced': extra}


def _one(subs: list[dict], name: str, required: bool = False) -> bytes | None:
    values = [s['payload'] for s in subs if s['name'] == name]
    if len(values) > 1 or (required and not values):
        raise M15AuditError(f'missing or duplicate {name}')
    return values[0] if values else None


def read_plugin(path: Path) -> tuple[dict, list[dict]]:
    """Read TES4 records with group containment and original owning-file keys."""
    data = path.read_bytes()
    if len(data) < 20 or data[:4] != b'TES4':
        raise M15AuditError(f'{path.name}: missing TES4 header')
    header_end = 20 + binary._u32(data, 4)
    if header_end > len(data):
        raise M15AuditError(f'{path.name}: truncated header')
    header = binary._subrecords(data[20:header_end], path.name, 'TES4')
    hedr = _one(header, 'HEDR', True)
    if len(hedr) != 12 or struct.unpack_from('<f', hedr)[0] not in (struct.unpack('<f', struct.pack('<f', 0.8))[0], 1.0):
        raise M15AuditError(f'{path.name}: not a supported TES4 header version')
    masters = [binary._decode_string(s['payload']).casefold() for s in header if s['name'] == 'MAST']
    if len(set(masters)) != len(masters):
        raise M15AuditError(f'{path.name}: duplicate masters')
    records = []
    wanted = {'CSTY', 'NPC_', 'CREA', 'GMST', 'FACT', 'WEAP', 'AMMO', 'ARMO', 'ACHR', 'ACRE', 'REFR', 'CONT', 'CELL'}

    def walk(start: int, end: int, cell: str | None = None, depth: int = 0):
        if depth > 64:
            raise M15AuditError('excessive group nesting')
        while start < end:
            if end - start < 20:
                raise M15AuditError('truncated record header')
            tag, size, flags, ident, _ = struct.unpack_from('<4sIIII', data, start)
            finish = start + size if tag == b'GRUP' else start + 20 + size
            if not start + 20 <= finish <= end:
                raise M15AuditError('record exceeds containing group')
            if tag == b'GRUP':
                group_type = struct.unpack_from('<i', data, start + 12)[0]
                parent = binary._stable_key(path.name, flags, masters) if group_type in binary.CELL_CHILD_GROUPS else cell
                walk(start + 20, finish, parent, depth + 1)
            else:
                tag = tag.decode('ascii')
                key = binary._stable_key(path.name, ident, masters)
                record = {'key': key, 'type': tag, 'plugin': path.name, 'flags': flags,
                          'deleted': bool(flags & binary.DELETED_FLAG), 'cell': cell, 'masters': masters}
                if tag in wanted and not record['deleted']:
                    payload = binary._payload(data, start + 20, size, flags, path.name, tag)
                    record['subrecords'] = binary._subrecords(payload, path.name, tag)
                records.append(record)
            start = finish
    walk(header_end, len(data))
    return {'name': path.name, 'sha256': hashlib.sha256(data).hexdigest(), 'masters': masters,
            'record_count': len(records)}, records


def inventory(paths: list[Path]) -> dict[str, Any]:
    if not paths or len({p.name.casefold() for p in paths}) != len(paths):
        raise M15AuditError('content list must be nonempty and unique')
    winners = {}
    plugins = []
    loaded = set()
    for path in paths:
        metadata, records = read_plugin(path)
        if set(metadata['masters']) - loaded:
            raise M15AuditError(f'{path.name}: masters must precede their dependent plugin')
        plugins.append(metadata)
        loaded.add(path.name.casefold())
        for record in records:
            key = record['key']
            if key in winners and winners[key]['type'] != record['type']:
                raise M15AuditError(f'{key}: override changes record type')
            winners[key] = record
    styles, actors, settings, factions = {}, {}, {}, {}
    failures = []
    for key, record in winners.items():
        if record['deleted'] or 'subrecords' not in record:
            continue
        subs = record['subrecords']
        edid = binary._decode_string(_one(subs, 'EDID') or b'')
        try:
            if record['type'] == 'CSTY':
                style = combat_style(_one(subs, 'CSTD', True), _one(subs, 'CSAD'))
                style['editor_id'] = edid
                style['raw_sha256'] = hashlib.sha256(_one(subs, 'CSTD', True) + (_one(subs, 'CSAD') or b'')).hexdigest()
                styles[key] = style
                if style['standard']['flags'] & 1 and style['advanced'] is None:
                    failures.append(f'{key}: Advanced flag lacks CSAD')
            elif record['type'] in ('NPC_', 'CREA'):
                style = _one(subs, 'ZNAM')
                if style is not None and len(style) != 4:
                    raise M15AuditError('ZNAM must be one FormID')
                style_key = binary._stable_key(record['plugin'], struct.unpack('<I', style)[0], record['masters']) if style else 'null'
                ai = _one(subs, 'AIDT')
                flags = _one(subs, 'ACBS')
                if ai is None or len(ai) != 12 or flags is None or len(flags) != 16:
                    raise M15AuditError('actor requires TES4 AIDT and ACBS layouts')
                actor_flags = struct.unpack_from('<I', flags)[0]
                actors[key] = {'editor_id': edid, 'type': record['type'], 'style': style_key,
                    'aggression': ai[0], 'confidence': ai[1], 'energy': ai[2], 'responsibility': ai[3],
                    'flags': actor_flags, 'essential': bool(actor_flags & 2), 'respawn': bool(actor_flags & 8)}
            elif record['type'] == 'GMST':
                payload = _one(subs, 'DATA', True)
                if not edid or edid[0] not in 'fis':
                    raise M15AuditError('GMST lacks typed editor ID')
                if edid[0] == 's':
                    value = binary._decode_string(payload)
                else:
                    if len(payload) != 4:
                        raise M15AuditError('numeric GMST must be four bytes')
                    value = struct.unpack('<f' if edid[0] == 'f' else '<i', payload)[0]
                    if isinstance(value, float) and not math.isfinite(value):
                        raise M15AuditError('nonfinite GMST')
                settings[edid] = {'key': key, 'type': edid[0], 'value': value}
            elif record['type'] == 'FACT':
                payload = _one(subs, 'DATA', True)
                if len(payload) != 1 or payload[0] & ~7:
                    raise M15AuditError('invalid native faction flags')
                multiplier = _one(subs, 'CNAM')
                if multiplier is not None and len(multiplier) != 4:
                    raise M15AuditError('invalid faction crime multiplier size')
                value = struct.unpack('<f', multiplier)[0] if multiplier else None
                if value is not None and (not math.isfinite(value) or value < 0):
                    raise M15AuditError('invalid faction crime multiplier')
                factions[key] = {'editor_id': edid, 'flags': payload[0], 'crime_multiplier': value}
        except (ValueError, TypeError, struct.error) as error:
            failures.append(f'{key}: {error}')
    for key, actor in actors.items():
        if actor['style'] != 'null' and actor['style'] not in styles:
            failures.append(f'{key}: missing/deleted/wrong-type combat style {actor["style"]}')
    unresolved = [key for key, actor in actors.items() if actor['style'] == 'null']
    return {'kind': 'm15-native-data-inventory', 'plugins': plugins, 'styles': styles, 'actors': actors,
        'settings': settings, 'factions': factions, 'failures': failures, 'data_passed': not failures,
        'unresolved_default_actors': unresolved, 'runtime_rules_verified': False,
        'open_gates': ['original-game default policy verification', 'independent physical/crime rule matrix'],
        'summary': {'styles': len(styles), 'actors': len(actors), 'settings': len(settings),
                    'factions': len(factions), 'default_actors': len(unresolved),
                    'style_size_distribution': dict(sorted(collections.Counter(str(s['standard_size']) for s in styles.values()).items()))},
        'passed': False} # Data inventory alone never closes the M15 rule/oracle gate.
