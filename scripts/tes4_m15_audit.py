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


def skill_definition(index: bytes, data: bytes) -> dict[str, Any]:
    if len(index) != 4 or len(data) != 20:
        raise M15AuditError('SKIL requires four-byte INDX and 20-byte DATA')
    actor_value, attribute, specialization, use0, use1 = struct.unpack('<III2f', data)
    if (struct.unpack('<I', index)[0] != actor_value or not 12 <= actor_value <= 32
            or not 0 <= attribute <= 7 or not 0 <= specialization <= 2
            or not math.isfinite(use0) or not math.isfinite(use1)):
        raise M15AuditError('invalid or inconsistent native SKIL definition')
    return {'actor_value': actor_value, 'governing_attribute': attribute,
            'specialization': specialization, 'use_values': [use0, use1]}


def effect_definition(record: dict) -> dict[str, Any]:
    """Authored DATA prefix only; compiled/loaded flags are a separate layer."""
    subs = record['subrecords']
    code = _one(subs, 'EDID', True)
    if len(code) not in (4,5) or b'\0' in code[:4] or (len(code)==5 and code[4]!=0):
        raise M15AuditError('MGEF requires one four-character EDID')
    data = _one(subs, 'DATA', True)
    if len(data)<24 or len(data)>68 or len(data)%4:
        raise M15AuditError('MGEF DATA requires24-68 bytes in four-byte increments')
    flags, cost_bits, associated, school, resistance, counters, padding = struct.unpack_from('<4IiHH', data)
    return dict(code=struct.unpack('<I', code[:4])[0], data_length=len(data), flags=flags,
                base_cost_bits=cost_bits, associated_data=associated, school=school,
                resistance_actor_value=resistance, counter_count=counters, counter_padding=padding,
                associated_form=_stable_key(record['plugin'],associated,record['masters'])
                    if flags & ((1<<16)|(1<<17)|(1<<18)) else None)


def spell_definition(record: dict) -> dict[str, Any]:
    """Decode original TES4 SPEL bytes; effect execution is deliberately absent."""
    subs = record['subrecords']
    kind, cost, level, flags, padding = _unpack(_one(subs, 'SPIT', True), '<IIIB3s', 'SPIT')
    effects = []
    pending = None
    name = ''
    for subrecord in subs:
        tag, payload = subrecord['name'], subrecord['payload']
        if tag == 'EFID':
            if pending is not None or len(payload) != 4:
                raise M15AuditError('EFID requires four bytes and a paired EFIT')
            pending = payload
        elif tag == 'EFIT':
            code, magnitude, area, duration, range_, av = _unpack(payload, '<4s5I', 'EFIT')
            if pending is None or code != pending:
                raise M15AuditError('EFIT disagrees with or lacks EFID')
            effects.append(dict(code=struct.unpack('<I', code)[0], magnitude=magnitude,
                                area=area, duration=duration, range=range_, actor_value=av, script=None))
            pending = None
        elif tag == 'SCIT':
            if pending is not None or not effects or effects[-1]['script'] is not None or len(payload) not in (4, 12, 16):
                raise M15AuditError('invalid SCIT length/order/duplicate')
            script_id, = struct.unpack_from('<I', payload)
            script = dict(key=_stable_key(record['plugin'], script_id, record['masters']),
                          school=None, visual=None, flags=None, padding=[], name=None)
            if len(payload) >= 12:
                script['school'], script['visual'] = struct.unpack_from('<II', payload, 4)
            if len(payload) == 16:
                script['flags'] = payload[12]
                script['padding'] = list(payload[13:16])
            effects[-1]['script'] = script
        elif tag == 'FULL':
            if not effects and pending is None:
                name = binary._decode_string(payload)
            elif effects and pending is None and effects[-1]['script'] is not None:
                script = effects[-1]['script']
                if script['name'] is not None:
                    raise M15AuditError('duplicate script-effect FULL')
                script['name'] = binary._decode_string(payload)
    if pending is not None:
        raise M15AuditError('EFID has no EFIT')
    return dict(type=kind, cost=cost, level=level, flags=flags, padding=list(padding),
                full_name=name, effects=effects)


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


# Reviewed original DefaultCombatStyle getter-to-setting associations. This is
# an independent audit projection, never a mutable gameplay authority.
_STANDARD_DEFAULT_SETTINGS = {
    'dodge_chance': 'iAIDefaultDodgeChance',
    'left_right_chance': 'iAIDefaultDodgeLeftRightChance',
    'block_chance': 'iAIDefaultBlockChance',
    'attack_chance': 'iAIDefaultAttackChance',
    'power_attack_chance': 'iAIDefaultPowerAttackChance',
    'acrobatic_chance': 'iAIDefaultAcrobaticDodgeChance',
    'rush_chance': 'iAIDefaultRushingAttackPercentChance',
    'dodge_lr_min': 'fAIDefaultDodgeLeftRightMinTime',
    'dodge_lr_max': 'fAIDefaultDodgeLeftRightMaxTime',
    'dodge_forward_min': 'fAIDefaultDodgeForwardMinTime',
    'dodge_forward_max': 'fAIDefaultDodgeForwardMaxTime',
    'dodge_back_min': 'fAIDefaultDodgeBackwardMinTime',
    'dodge_back_max': 'fAIDefaultDodgeBackwardMaxTime',
    'idle_min': 'fAIDefaultIdleMinTime', 'idle_max': 'fAIDefaultIdleMaxTime',
    'hold_min': 'fAIDefaultHoldMinTime', 'hold_max': 'fAIDefaultHoldMaxTime',
    'attack_recoil_bonus': 'fAIDefaultAttackDuringRecoilStaggerBonus',
    'attack_unconscious_bonus': 'fAIDefaultAttackDuringUnconsciousBonus',
    'attack_unarmed_bonus': 'fAIDefaultAttackHandBonus',
    'power_recoil_bonus': 'fAIDefaultPowerAttackRecoilStaggerBonus',
    'power_unconscious_bonus': 'fAIDefaultPowerAttackUnconsciousBonus',
    'power_normal': 'iAIDefaultPowerAttackNormalChance',
    'power_forward': 'iAIDefaultPowerAttackForwardChance',
    'power_back': 'iAIDefaultPowerAttackBackwardChance',
    'power_left': 'iAIDefaultPowerAttackLeftChance',
    'power_right': 'iAIDefaultPowerAttackRightChance',
    'optimal_range_multiplier': 'fAIDefaultOptimalRangeMult',
    'maximum_range_multiplier': 'fAIDefaultMaximumRangeMult',
    'melee_switch_distance': 'fAIDefaultSwitchToMeleeDistance',
    'ranged_switch_distance': 'fAIDefaultSwitchToRangedDistance',
    'buff_standoff': 'fAIDefaultBuffStandoffDistance',
    'ranged_standoff': 'fAIDefaultRangedStandoffDistance',
    'group_standoff': 'fAIDefaultGroupStandoffDistance',
    'rush_distance_multiplier': 'fAIDefaultRushingAttackDistanceMult',
}
_ADVANCED_DEFAULT_SETTINGS = dict(zip(_ADVANCED_NAMES, (
    'fAIDefaultDodgeFatigueMult', 'fAIDefaultDodgeFatigueBase',
    'fAIDefaultDodgeSpeedBase', 'fAIDefaultDodgeSpeedMult',
    'fAIDefaultDodgeDuringAttackMult', 'fAIDefaultDodgeNoAttackMult',
    'fAIDefaultDodgeBackDuringAttackMult', 'fAIDefaultDodgeBackNoAttackMult',
    'fAIDefaultDodgeForwardWhileAttackingMult', 'fAIDefaultDodgeForwardNotAttackingMult',
    'fAIDefaultBlockSkillMult', 'fAIDefaultBlockSkillBase',
    'fAIDefaultBlockDuringAttackMult', 'fAIDefaultBlockNoAttackMult',
    'fAIDefaultAttackSkillMult', 'fAIDefaultAttackSkillBase',
    'fAIDefaultAttackDuringAttackMult', 'fAIDefaultAttackNoAttackMult',
    'fAIDefaultAttackDuringBlockMult', 'fAIDefaultPowerAttackFatigueBase',
    'fAIDefaultPowerAttackFatigueMult'), strict=True))
_FLAG_DEFAULT_SETTINGS = {
    4: 'iAIDefaultIgnoreAlliesInArea', 8: 'iAIDefaultYieldEnabled',
    16: 'iAIDefaultRejectYield', 32: 'iAIDefaultFleeDisabled',
    64: 'iAIDefaultPrefersRangedAttacks', 128: 'iAIDefaultMeleeAlertAllowed',
}


def policy_inventory(report: dict, catalog: dict) -> dict:
    """Resolve reviewed data inputs without claiming a combat simulation ran."""
    if catalog.get('schema_version') != 1 or catalog.get('game_version') != 'Oblivion 1.2.0416':
        raise M15AuditError('unsupported native default-style catalog')
    compiled = {}
    for row in catalog['settings']:
        name = row['name'].casefold()
        if name in compiled:
            raise M15AuditError('ambiguous compiled combat setting')
        compiled[name] = row
    winning = {}
    for name, value in report['settings'].items():
        if name.casefold() in winning:
            raise M15AuditError('ambiguous winning combat setting')
        winning[name.casefold()] = value

    def number(name):
        fallback = compiled.get(name.casefold())
        if fallback is None or fallback.get('type') != ('int32' if name[0] == 'i' else 'float32'):
            raise M15AuditError(f'missing/invalid compiled combat setting {name}')
        row = winning.get(name.casefold())
        if row is not None and row.get('type') != name[0]:
            raise M15AuditError(f'incorrect winning combat setting type {name}')
        value = (row if row is not None else fallback)['value']
        if name[0] == 'i':
            if type(value) is not int or not -2**31 <= value < 2**31:
                raise M15AuditError(f'invalid integer combat setting {name}')
        elif type(value) not in (int, float) or not math.isfinite(value):
            raise M15AuditError(f'invalid floating combat setting {name}')
        else:
            try:
                value = struct.unpack('<f', struct.pack('<f', value))[0]
            except (OverflowError, struct.error) as error:
                raise M15AuditError(f'floating combat setting overflow {name}') from error
        return value

    standard = {field: number(name) for field, name in _STANDARD_DEFAULT_SETTINGS.items()}
    standard['flags'] = sum(bit for bit, name in _FLAG_DEFAULT_SETTINGS.items() if number(name) != 0)
    standard['do_not_acquire'] = number('iAIDefaultDoNotAcquire') != 0
    advanced = {field: number(name) for field, name in _ADVANCED_DEFAULT_SETTINGS.items()}

    def checked(standard, advanced):
        # Reuse the independent binary layout/domain checker to validate every
        # field and timer, including resolved historical tails and signed CSAD.
        try:
            raw = _CORE.pack(*(standard[name] for name in _CORE_NAMES))
            for _, layout, names in _TAILS:
                raw += struct.pack(layout, *(standard[name] for name in names))
            return combat_style(raw, struct.pack('<21f', *(advanced[name] for name in _ADVANCED_NAMES)))
        except (KeyError, OverflowError, struct.error) as error:
            raise M15AuditError(f'invalid resolved combat policy: {error}') from error

    defaults = checked(standard, advanced)
    policies = {'null': dict(defaults, source='native-default-getters-and-winning-GMSTs')}
    failures = []
    fallback_zero = {'melee_switch_distance', 'ranged_switch_distance', 'rush_chance', 'rush_distance_multiplier'}
    for key, style in report['styles'].items():
        try:
            resolved = dict(style['standard'])
            for name, value in resolved.items():
                if value is None:
                    resolved[name] = (1 if name in ('optimal_range_multiplier', 'maximum_range_multiplier')
                                      else False if name == 'do_not_acquire' else standard[name])
                elif name in fallback_zero and value == 0:
                    resolved[name] = standard[name]
            extra = style['advanced'] if resolved['flags'] & 1 else advanced
            if extra is None:
                raise M15AuditError('Advanced flag lacks CSAD')
            policies[key] = dict(checked(resolved, extra), source='authored-style-with-native-tail-rules')
        except (KeyError, M15AuditError) as error:
            failures.append(f'{key}: {error}')
    actors = {}
    for key, actor in report['actors'].items():
        if actor['style'] not in policies:
            failures.append(f'{key}: unresolved combat policy {actor["style"]}')
        else:
            actors[key] = actor['style']
    return {'scope': 'resolved data inputs only; runtime combat and magic remain separate gates',
            'policies': policies, 'actor_policy_keys': actors, 'failures': failures, 'passed': not failures}


def _one(subs: list[dict], name: str, required: bool = False) -> bytes | None:
    values = [s['payload'] for s in subs if s['name'] == name]
    if len(values) > 1 or (required and not values):
        raise M15AuditError(f'missing or duplicate {name}')
    return values[0] if values else None


def _stable_key(plugin: str, ident: int, masters: list[str]) -> str:
    # Match FormKeyResolver: indices outside the declared masters name the
    # source file. The shipped master itself contains such local indices.
    return binary._stable_key(plugin, ident, masters)


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
    wanted = {'CSTY', 'SKIL', 'SPEL', 'MGEF', 'NPC_', 'CREA', 'GMST', 'FACT', 'WEAP', 'AMMO', 'ARMO', 'ACHR', 'ACRE', 'REFR', 'CONT', 'CELL'}

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
                parent = _stable_key(path.name, flags, masters) if group_type in binary.CELL_CHILD_GROUPS else cell
                walk(start + 20, finish, parent, depth + 1)
            else:
                tag = tag.decode('ascii')
                key = _stable_key(path.name, ident, masters)
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


def _unpack(payload: bytes | None, format_: str, name: str) -> tuple:
    if payload is None or len(payload) != struct.calcsize(format_):
        raise M15AuditError(f'{name}: incorrect or missing layout')
    return struct.unpack(format_, payload)


def _reference(record: dict, payload: bytes | None, name: str) -> str:
    ident, = _unpack(payload, '<I', name)
    return _stable_key(record['plugin'], ident, record['masters'])


def _equipment(record: dict) -> dict:
    subs = record['subrecords']
    tag = record['type']
    data = _one(subs, 'DATA', True)
    if tag == 'WEAP':
        names = ('weapon_type', 'speed', 'reach', 'ignores_normal_resistance', 'value', 'health', 'weight', 'damage')
        values = _unpack(data, '<B3xffIIIfH', 'WEAP DATA')
    elif tag == 'AMMO':
        names = ('speed', 'ignores_normal_resistance', 'value', 'weight', 'damage')
        values = _unpack(data, '<fB3xIfH', 'AMMO DATA')
    else:
        names = ('armor_hundredths', 'value', 'health', 'weight')
        values = _unpack(data, '<HIIf', 'ARMO DATA')
    result = dict(zip(names, values, strict=True))
    if result.get('weapon_type', 0) > 5 or result.get('ignores_normal_resistance', 0) not in (0, 1):
        raise M15AuditError('equipment type/flag outside native domain')
    if any(isinstance(value, float) and (not math.isfinite(value) or value < 0) for value in values):
        raise M15AuditError('equipment has negative/nonfinite physical input')
    if tag == 'ARMO':
        result['biped_flags'], = _unpack(_one(subs, 'BMDT', True), '<I', 'ARMO BMDT')
    enchantment = _one(subs, 'ENAM')
    result['enchantment'] = _reference(record, enchantment, 'ENAM') if enchantment else 'null'
    result['magic_semantics'] = 'M16' if result['enchantment'] != 'null' or result.get('weapon_type') == 4 else None
    result['type'] = tag
    result['editor_id'] = binary._decode_string(_one(subs, 'EDID') or b'')
    return result


def _creature(record: dict) -> dict:
    subs = record['subrecords']
    names = ('type', 'combat_skill', 'magic_skill', 'stealth_skill', 'soul', 'health', 'attack_damage',
             'strength', 'intelligence', 'willpower', 'agility', 'speed', 'endurance', 'personality', 'luck')
    result = dict(zip(names, _unpack(_one(subs, 'DATA', True), '<5BxH2xH8B', 'CREA DATA'), strict=True))
    result['reach'], = _unpack(_one(subs, 'RNAM', True), '<B', 'CREA RNAM')
    if result['type'] > 5 or result['soul'] > 5:
        raise M15AuditError('creature type/soul outside native domain')
    for tag, name in (('BNAM', 'scale'), ('TNAM', 'turning_speed'), ('WNAM', 'foot_weight')):
        payload = _one(subs, tag)
        value = _unpack(payload, '<f', tag)[0] if payload is not None else None
        if value is not None and (not math.isfinite(value) or value < 0):
            raise M15AuditError(f'invalid creature {name}')
        result[name] = value
    inherited = _one(subs, 'CSCR')
    result['sound_base'] = _reference(record, inherited, 'CSCR') if inherited else 'null'
    sounds = []
    sound_type = None
    pending = None
    for sub in subs:
        tag, data = sub['name'], sub['payload']
        if tag == 'CSDT':
            if pending is not None:
                raise M15AuditError('sound missing chance')
            sound_type, = _unpack(data, '<I', tag)
            if sound_type > 9:
                raise M15AuditError('invalid creature sound type')
        elif tag == 'CSDI':
            if sound_type is None or pending is not None:
                raise M15AuditError('unordered creature sound')
            pending = _reference(record, data, tag)
        elif tag == 'CSDC':
            chance, = _unpack(data, '<B', tag)
            if pending is None or chance > 100:
                raise M15AuditError('invalid creature sound chance/order')
            sounds.append({'type': sound_type, 'sound': pending, 'chance': chance})
            pending = None
    if pending is not None:
        raise M15AuditError('sound missing chance')
    result['sounds'] = sounds
    result['model'] = binary._decode_string(_one(subs, 'MODL') or b'')
    for tag, name in (('KFFZ', 'animations'), ('NIFZ', 'meshes')):
        raw = _one(subs, tag) or b''
        if raw and raw[-1] != 0:
            raise M15AuditError(f'unterminated {tag}')
        result[name] = [binary._decode_string(part) for part in raw.split(b'\0') if part]
    return result


def inventory(paths: list[Path], prisons: list[dict] | None = None) -> dict[str, Any]:
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
    styles, actors, settings, factions, equipment = {}, {}, {}, {}, {}
    ownership, references = {}, {}
    skills, spells, effect_definitions = {}, {}, {}
    failures = []
    for key, record in winners.items():
        if record['deleted'] or 'subrecords' not in record:
            continue
        subs = record['subrecords']
        edid = binary._decode_string(_one(subs, 'EDID') or b'')
        try:
            if record['type'] in ('REFR', 'ACHR', 'ACRE', 'CELL'):
                owner = _one(subs, 'XOWN')
                rank = _one(subs, 'XRNK')
                global_ = _one(subs, 'XGLB')
                access = {'owner': _reference(record, owner, 'XOWN') if owner is not None else 'null',
                          'rank': _unpack(rank, '<i', 'XRNK')[0] if rank is not None else None,
                          'global': _reference(record, global_, 'XGLB') if global_ is not None else 'null'}
                if any(value is not None for value in (owner, rank, global_)):
                    ownership[key] = dict(access, type=record['type'], cell=record['cell'])
                if record['type'] != 'CELL':
                    base = _reference(record, _one(subs, 'NAME', True), 'NAME')
                    references[key] = dict(access, type=record['type'], cell=record['cell'], base=base, editor_id=edid)
                    teleport = _one(subs, 'XTEL')
                    if teleport is not None:
                        values = _unpack(teleport, '<I6f', 'XTEL')
                        if any(not math.isfinite(v) for v in values[1:]):
                            raise M15AuditError('nonfinite door destination')
                        references[key]['destination'] = _stable_key(record['plugin'], values[0], record['masters'])
            elif record['type'] == 'MGEF':
                effect_definitions[key] = effect_definition(record)
            elif record['type'] == 'SPEL':
                spells[key] = dict(spell_definition(record), editor_id=edid)
            elif record['type'] == 'SKIL':
                skills[key] = dict(skill_definition(_one(subs, 'INDX', True), _one(subs, 'DATA', True)),
                                   editor_id=edid)
            elif record['type'] == 'CSTY':
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
                style_key = _stable_key(record['plugin'], struct.unpack('<I', style)[0], record['masters']) if style else 'null'
                ai = _one(subs, 'AIDT')
                flags = _one(subs, 'ACBS')
                if ai is None or len(ai) != 12 or flags is None or len(flags) != 16:
                    raise M15AuditError('actor requires TES4 AIDT and ACBS layouts')
                actor_flags = struct.unpack_from('<I', flags)[0]
                actors[key] = {'editor_id': edid, 'type': record['type'], 'style': style_key,
                    'aggression': ai[0], 'confidence': ai[1], 'energy': ai[2], 'responsibility': ai[3],
                    'flags': actor_flags, 'essential': bool(actor_flags & 2), 'respawn': bool(actor_flags & 8)}
                actors[key]['inventory'] = []
                actors[key]['spells'] = []
                for entry in subs:
                    if entry['name'] == 'CNTO':
                        item, count = _unpack(entry['payload'], '<Ii', 'actor CNTO')
                        actors[key]['inventory'].append({
                            'item': _stable_key(record['plugin'], item, record['masters']), 'count': count})
                    elif entry['name'] == 'SPLO':
                        actors[key]['spells'].append(_reference(record, entry['payload'], 'actor SPLO'))
                if record['type'] == 'NPC_':
                    for tag, name in (('RNAM', 'race'), ('CNAM', 'class')):
                        payload = _one(subs, tag)
                        actors[key][name] = _reference(record, payload, tag) if payload is not None else 'null'
                actors[key]['factions'] = []
                for entry in subs:
                    if entry['name'] == 'SNAM':
                        faction, rank = _unpack(entry['payload'], '<Ib3x', 'actor SNAM')
                        actors[key]['factions'].append({'faction': _stable_key(record['plugin'], faction, record['masters']), 'rank': rank})
                if record['type'] == 'CREA':
                    actors[key]['creature'] = _creature(record)
            elif record['type'] in ('WEAP', 'AMMO', 'ARMO'):
                equipment[key] = _equipment(record)
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
                relations = []
                for entry in subs:
                    if entry['name'] == 'XNAM':
                        faction, modifier = _unpack(entry['payload'], '<Ii', 'FACT XNAM')
                        relations.append({'faction': _stable_key(record['plugin'], faction, record['masters']), 'modifier': modifier})
                factions[key] = {'editor_id': edid, 'flags': payload[0], 'crime_multiplier': value, 'relationships': relations}
        except (ValueError, TypeError, struct.error) as error:
            # Never publish a partially decoded actor to semantic link checks.
            actors.pop(key, None)
            failures.append(f'{key}: {error}')
    for key, actor in actors.items():
        if actor['style'] != 'null' and actor['style'] not in styles:
            failures.append(f'{key}: missing/deleted/wrong-type combat style {actor["style"]}')
    def check_link(source, target, types):
        if target != 'null' and (target not in winners or winners[target]['deleted'] or winners[target]['type'] not in types):
            failures.append(f'{source}: missing/deleted/wrong-type {target}; expected {types}')
    for key, access in ownership.items():
        check_link(key, access['owner'], ('FACT', 'NPC_'))
        check_link(key, access['global'], ('GLOB',))
    base_types = tuple(sorted({r['type'] for r in winners.values()} - {'REFR', 'ACHR', 'ACRE', 'CELL'}))
    for key, reference in references.items():
        check_link(key, reference['base'], ('NPC_',) if reference['type'] == 'ACHR' else ('CREA',) if reference['type'] == 'ACRE' else base_types)
        if reference['cell'] is not None:
            check_link(key, reference['cell'], ('CELL',))
        if 'destination' in reference:
            check_link(key, reference['destination'], ('REFR',))
    for key, faction in factions.items():
        for relation in faction['relationships']:
            check_link(key, relation['faction'], ('FACT',))
    for key, item in equipment.items():
        check_link(key, item['enchantment'], ('ENCH',))
    for key, spell in spells.items():
        for effect in spell['effects']:
            if effect['script'] is not None:
                check_link(key, effect['script']['key'], ('SCPT',))
    for key, actor in actors.items():
        for entry in actor['inventory']:
            check_link(key, entry['item'], ('WEAP', 'ARMO', 'AMMO', 'MISC', 'KEYM', 'BOOK',
                'ALCH', 'APPA', 'CLOT', 'INGR', 'SLGM', 'SGST', 'LIGH', 'LVLI'))
        for spell in actor['spells']:
            check_link(key, spell, ('SPEL', 'LVSP'))
        if actor['type'] == 'NPC_':
            check_link(key, actor['race'], ('RACE',))
            check_link(key, actor['class'], ('CLAS',))
        for membership in actor['factions']:
            check_link(key, membership['faction'], ('FACT',))
        if 'creature' in actor:
            check_link(key, actor['creature']['sound_base'], ('CREA',))
            for sound in actor['creature']['sounds']:
                check_link(key, sound['sound'], ('SOUN',))
    prison_reports = []
    for prison in prisons or []:
        report = {'name': prison['name'], 'forms': {}}
        check_link(prison['name'], prison['cell'], ('CELL',))
        if prison['cell'] == 'null':
            failures.append(f"{prison['name']}: missing prison cell")
        for role, types in (('prison_marker', ('DOOR',)), ('release_marker', ('DOOR',)),
                            ('cell_door', ('DOOR',)), ('evidence', ('CONT',)), ('bed_candidate', ('FURN',))):
            key = prison[role]
            reference = references.get(key)
            if reference is None:
                failures.append(f"{prison['name']}: missing {role} {key}")
                continue
            check_link(key, reference['base'], types)
            if role != 'release_marker' and reference['cell'] != prison['cell']:
                failures.append(f"{prison['name']}: {role} is outside prison cell")
            report['forms'][role] = dict(reference, key=key)
        for role, opposite in (('prison_marker', 'release_marker'), ('release_marker', 'prison_marker')):
            if report['forms'].get(role, {}).get('destination') != prison[opposite]:
                failures.append(f"{prison['name']}: {role} lacks reciprocal prison teleport")
        report['guards'] = []
        for key in prison['guards']:
            reference = references.get(key)
            if reference is None or reference['type'] != 'ACHR':
                failures.append(f"{prison['name']}: missing guard {key}")
                continue
            report['guards'].append(dict(reference, key=key))
        prison_reports.append(report)
    skill_counts = collections.Counter(skill['actor_value'] for skill in skills.values())
    if any(count != 1 for count in skill_counts.values()):
        failures.append('ambiguous winning SKIL actor values')
    effect_codes = collections.Counter(effect['code'] for effect in effect_definitions.values())
    if any(count != 1 for count in effect_codes.values()):
        failures.append('ambiguous winning MGEF effect codes')
    unresolved = [key for key, actor in actors.items() if actor['style'] == 'null']
    return {'kind': 'm15-native-data-inventory', 'plugins': plugins, 'styles': styles, 'actors': actors,
        'settings': settings, 'skills': skills, 'spells': spells, 'effect_definitions': effect_definitions, 'factions': factions, 'equipment': equipment,
        'ownership': ownership, 'references': references, 'prisons': prison_reports, 'failures': failures, 'data_passed': not failures,
        'skill_inventory_complete': skill_counts == collections.Counter(range(12, 33)),
        'unresolved_default_actors': unresolved, 'runtime_rules_verified': False,
        'open_gates': ['original-game default policy verification', 'independent physical/crime rule matrix'],
        'summary': {'styles': len(styles), 'skills': len(skills), 'spells': len(spells), 'effect_definitions': len(effect_definitions), 'actors': len(actors), 'settings': len(settings),
                    'factions': len(factions), 'equipment': len(equipment), 'owned_forms': len(ownership), 'references': len(references), 'prisons': len(prison_reports), 'default_actors': len(unresolved),
                    'style_size_distribution': dict(sorted(collections.Counter(str(s['standard_size']) for s in styles.values()).items()))},
        'passed': False} # Data inventory alone never closes the M15 rule/oracle gate.


def check_count_lock(report: dict, lock: dict) -> dict:
    """Compare a reviewed, hash-bound inventory; never rewrite the lock."""
    failures = []
    if lock.get('schema_version') != 1:
        failures.append('unsupported count-lock schema')
    actual = [{'name': p['name'], 'sha256': p['sha256']} for p in report['plugins']]
    if not lock.get('plugins') or lock['plugins'] != actual:
        failures.append('content order, names or hashes differ from reviewed count lock')
    counts = lock.get('counts')
    if not isinstance(counts, dict) or not counts or set(counts) != set(report['summary']):
        failures.append('count lock must cover the complete reported summary')
    else:
        for key, value in counts.items():
            if value != report['summary'][key] or type(value) is not type(report['summary'][key]):
                failures.append(f'count differs from reviewed value: {key}')
    return {'passed': not failures, 'failures': failures}
