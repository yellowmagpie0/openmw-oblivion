"""Strict, load-order-independent TES4 PACK/PGRD audit for M14.

The normal compatibility harness uses ``esmtool`` for broad file audits.  M14
also needs to inspect the native fields that drive package selection and
navigation, so this module has a deliberately small binary reader for the
Oblivion (TES4) record layout.  It retains only the winning normalized records
needed by the audit; it never copies proprietary record payloads into the
repository or generated reports.
"""

from __future__ import annotations

import collections
import hashlib
import json
import struct
import zlib

from pathlib import Path
from typing import Any, Iterable


class M14AuditError(RuntimeError):
    """A malformed TES4 file or M14 record layout."""


RECORD_HEADER_SIZE = 20
GROUP_HEADER_SIZE = 20
COMPRESSED_FLAG = 0x00040000
DELETED_FLAG = 0x00000020
CELL_CHILD_GROUPS = {6, 8, 9, 10}

PACKAGE_TYPES = {
    0: "Find",
    1: "Follow",
    2: "Escort",
    3: "Eat",
    4: "Sleep",
    5: "Wander",
    6: "Travel",
    7: "Accompany",
    8: "UseItemAt",
    9: "Ambush",
    10: "FleeNotCombat",
    11: "CastMagic",
}

LOCATION_TYPES = {
    -1: "None",
    0: "NearReference",
    1: "InCell",
    2: "CurrentLocation",
    3: "EditorLocation",
    4: "ObjectId",
    5: "ObjectType",
}

TARGET_TYPES = {
    -1: "None",
    0: "SpecificReference",
    1: "ObjectId",
    2: "ObjectType",
    3: "LinkedReference",
}

CONDITION_RUN_ON = {
    0: "Subject",
    1: "Target",
    2: "Reference",
    3: "CombatTarget",
    4: "LinkedReference",
}

# TES4 PTDT object-type filters.  These are the Oblivion values, not the
# similarly numbered Fallout/TES5 filters.  Zero means no object filter.
PACKAGE_OBJECT_TYPES = {
    0: "None",
    1: "Activators",
    2: "Apparatus",
    3: "Armor",
    4: "Books",
    5: "Clothing",
    6: "Containers",
    7: "Doors",
    8: "Ingredients",
    9: "Lights",
    10: "Miscellaneous",
    11: "Flora",
    12: "Furniture",
    13: "Weapons: All",
    14: "Ammo",
    15: "NPCs",
    16: "Creatures",
    17: "Soul Gems",
    18: "Keys",
    19: "Alchemy",
    20: "Food",
    21: "All: Combat Wearable",
    22: "All: Wearable",
    23: "Weapons: None",
    24: "Weapons: Melee",
    25: "Weapons: Ranged",
    26: "Spells: Any",
    27: "Spells: Range Target",
    28: "Spells: Range Touch",
    29: "Spells: Range Self",
    30: "Spells: School Alteration",
    31: "Spells: School Conjuration",
    32: "Spells: School Destruction",
    33: "Spells: School Illusion",
    34: "Spells: School Mysticism",
    35: "Spells: School Restoration",
}

# Oblivion.esm contains a small number of old stock packages whose PTDT
# object-type value is outside the TES4 enum.  They are retained in the
# report and deliberately do not match anything in the native resolver.  A
# new value must be investigated here before the audit can pass.
REVIEWED_OBJECT_TYPE_ANOMALIES = {
    38: "stock-Oblivion out-of-range Find target; native resolver intentionally yields no match",
}

# Keep this in sync with the semantic masks in aipackagedata.cpp.  The
# complement is intentionally reported as raw/reserved, never discarded.
KNOWN_PACKAGE_FLAGS = (
    0x00000001 | 0x00000002 | 0x00000004 | 0x00000008 | 0x00000010 | 0x00000020
    | 0x00000040 | 0x00000080 | 0x00000100 | 0x00000200 | 0x00000400 | 0x00001000
    | 0x00002000 | 0x00020000 | 0x00040000 | 0x00080000 | 0x00100000 | 0x00200000
    | 0x00400000 | 0x00800000 | 0x01000000
)

# Reserved PKDT bits are not silently accepted just because the bit pattern
# is present in the stock files. Each observed bit has a written disposition;
# if a future official override introduces another bit, the audit fails until
# its semantics are reviewed and the native boundary is updated deliberately.
REVIEWED_RESERVED_PACKAGE_FLAGS = {
    0x00000800: "stock Oblivion reserved PKDT bit; preserved in runtime state and has no M14 behavior",
    0x00004000: "stock Oblivion reserved PKDT bit; preserved in runtime state and has no M14 behavior",
    0x00008000: "stock Oblivion reserved PKDT bit; preserved in runtime state and has no M14 behavior",
}

# These are the functions backed by the native coordinator.  The audit
# reports every other function reachable from a winning package and makes it
# impossible to hide an unsupported condition behind a permissive default.
SUPPORTED_CONDITION_FUNCTIONS = {
    18,   # GetCurrentTime
    1,    # GetDistance
    27,   # GetLineOfSight
    32,   # GetIsInSameCell
    35,   # GetDisabled
    46,   # GetDead
    64,   # GetIsCreature
    47,   # GetItemCount
    80,   # GetLevel
    81,   # GetArmorRating
    110,  # GetCurrentAIPackage
    143,  # GetCurrentAIProcedure
    161,  # GetIsCurrentPackage
    180,  # GetDetectionLevel
    45,   # GetDetected
    49,   # GetSleeping
    244,  # GetRestrained
    25,   # IsMoving
    286,  # IsSneaking
    287,  # IsRunning
    289,  # IsInCombat
    300,  # IsInInterior
    327,  # IsRidingHorse
    353,  # IsActor
    354,  # IsEssential
    72,   # GetIsID
    67,   # GetInCell
    74,   # GetGlobalValue
    77,   # GetRandomPercent
    5,    # GetLocked
    14,   # GetActorValue
    48,   # GetGold
    50,   # GetTalkedToPC
    53,   # GetScriptVariable
    56,   # GetQuestRunning
    58,   # GetStage
    59,   # GetStageDone
    62,   # IsRaining
    75,   # IsSnowing
    63,   # GetAttacked
    71,   # GetInFaction
    79,   # GetQuestVariable
    84,   # GetDeadCount
    91,   # GetIsAlerted
    132,  # GetPCInFaction
    136,  # GetIsReference
    159,  # GetSitting
    170,  # GetDayofWeek
    171,  # IsPlayerInJail
    182,  # GetEquipped
    193,  # GetPCExpelled
    223,  # IsSpellTarget
    230,  # GetInCellParam
    249,  # GetPCFame
    266,  # IsPleasant
    280,  # IsCellOwner
    310,  # GetInWorldspace
    339,  # IsPlayersLastRiddenHorse
    358,  # IsPlayerMovingIntoNewSpace
    365,  # IsChild
}

# Names are included for the functions that M14 currently evaluates and for
# the common package predicates.  Unknown numeric entries remain explicit.
CONDITION_NAMES = {
    1: "GetDistance",
    18: "GetCurrentTime",
    25: "IsMoving",
    27: "GetLineOfSight",
    32: "GetIsInSameCell",
    35: "GetDisabled",
    45: "GetDetected",
    46: "GetDead",
    47: "GetItemCount",
    49: "GetSleeping",
    64: "GetIsCreature",
    67: "GetInCell",
    72: "GetIsID",
    74: "GetGlobalValue",
    77: "GetRandomPercent",
    80: "GetLevel",
    81: "GetArmorRating",
    110: "GetCurrentAIPackage",
    143: "GetCurrentAIProcedure",
    161: "GetIsCurrentPackage",
    180: "GetDetectionLevel",
    244: "GetRestrained",
    286: "IsSneaking",
    287: "IsRunning",
    289: "IsInCombat",
    300: "IsInInterior",
    327: "IsRidingHorse",
    353: "IsActor",
    354: "IsEssential",
    5: "GetLocked",
    14: "GetActorValue",
    48: "GetGold",
    50: "GetTalkedToPC",
    53: "GetScriptVariable",
    56: "GetQuestRunning",
    58: "GetStage",
    59: "GetStageDone",
    62: "IsRaining",
    75: "IsSnowing",
    63: "GetAttacked",
    71: "GetInFaction",
    79: "GetQuestVariable",
    84: "GetDeadCount",
    91: "GetIsAlerted",
    132: "GetPCInFaction",
    136: "GetIsReference",
    159: "GetSitting",
    170: "GetDayofWeek",
    171: "IsPlayerInJail",
    182: "GetEquipped",
    193: "GetPCExpelled",
    223: "IsSpellTarget",
    230: "GetInCellParam",
    249: "GetPCFame",
    266: "IsPleasant",
    280: "IsCellOwner",
    310: "GetInWorldspace",
    339: "IsPlayersLastRiddenHorse",
    358: "IsPlayerMovingIntoNewSpace",
    365: "IsChild",
}


def _tag(value: bytes) -> str:
    return value.decode("ascii", errors="replace")


def _u8(data: bytes, offset: int) -> int:
    return data[offset]


def _i8(data: bytes, offset: int) -> int:
    return struct.unpack_from("<b", data, offset)[0]


def _u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def _i16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def _u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def _i32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<i", data, offset)[0]


def _f32(data: bytes, offset: int) -> float:
    return struct.unpack_from("<f", data, offset)[0]


def _stable_key(plugin: str, raw: int, masters: list[str]) -> str:
    if raw == 0:
        return "null"
    owner_index, local_id = raw >> 24, raw & 0x00FFFFFF
    owner = masters[owner_index] if owner_index < len(masters) else plugin
    return f"content:{owner.casefold()}:{local_id:06x}"


def _fingerprint(value: Any) -> str:
    encoded = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode()
    return "sha256:" + hashlib.sha256(encoded).hexdigest()


def _error(plugin: str, record: str, offset: int, message: str) -> M14AuditError:
    return M14AuditError(f"{plugin}:{record}:0x{offset:x}: {message}")


def _subrecords(payload: bytes, plugin: str, record: str) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    offset = 0
    while offset < len(payload):
        header_offset = offset
        if len(payload) - offset < 6:
            raise _error(plugin, record, offset, "trailing bytes cannot form a subrecord header")
        name_bytes = payload[offset : offset + 4]
        size = _u16(payload, offset + 4)
        offset += 6
        if name_bytes == b"XXXX":
            if size != 4 or len(payload) - offset < 4:
                raise _error(plugin, record, header_offset, "invalid XXXX subrecord")
            extended_size = _u32(payload, offset)
            offset += 4
            if len(payload) - offset < 6:
                raise _error(plugin, record, header_offset, "XXXX is not followed by a subrecord")
            name_bytes = payload[offset : offset + 4]
            size = _u16(payload, offset + 4)
            offset += 6
            size = extended_size
        if size > len(payload) - offset:
            raise _error(plugin, record, header_offset, "subrecord payload extends past the record")
        result.append({
            "name": _tag(name_bytes),
            "payload": payload[offset : offset + size],
            "offset": header_offset,
            "declared_size": size,
        })
        offset += size
    return result


def _payload(data: bytes, start: int, size: int, flags: int, plugin: str, record: str) -> bytes:
    value = data[start : start + size]
    if len(value) != size:
        raise _error(plugin, record, start, "record payload is truncated")
    if not flags & COMPRESSED_FLAG:
        return value
    if size < 4:
        raise _error(plugin, record, start, "compressed record has no size prefix")
    expected = _u32(value, 0)
    try:
        inflated = zlib.decompress(value[4:])
    except zlib.error as error:
        raise _error(plugin, record, start, f"compressed payload is invalid: {error}") from error
    if len(inflated) != expected:
        raise _error(plugin, record, start, "compressed payload size does not match its prefix")
    return inflated


def _first(subrecords: list[dict[str, Any]], name: str) -> dict[str, Any] | None:
    return next((item for item in subrecords if item["name"] == name), None)


def _all(subrecords: list[dict[str, Any]], name: str) -> list[dict[str, Any]]:
    return [item for item in subrecords if item["name"] == name]


def _decode_string(payload: bytes) -> str:
    return payload.split(b"\0", 1)[0].decode("utf-8", errors="replace")


def _parse_pack(record: dict[str, Any], subrecords: list[dict[str, Any]], masters: list[str]) -> dict[str, Any]:
    plugin, key = record["plugin"], record["key"]
    result: dict[str, Any] = {"key": key, "plugin": plugin, "conditions": []}
    pkdt = _first(subrecords, "PKDT")
    if pkdt is None or len(pkdt["payload"]) not in (4, 8):
        raise _error(plugin, "PACK", 0, "PKDT is missing or has an unsupported size")
    raw = pkdt["payload"]
    flags = _u32(raw, 0) if len(raw) == 8 else _u16(raw, 0)
    package_type = raw[4] if len(raw) == 8 else raw[2]
    result.update({
        "flags": flags,
        "flags_hex": f"0x{flags:08x}",
        "reserved_flags": flags & ~KNOWN_PACKAGE_FLAGS,
        "package_type_raw": package_type,
        "package_type": PACKAGE_TYPES.get(package_type, "Unknown"),
        "short_pkdt": len(raw) == 4,
    })

    psdt = _first(subrecords, "PSDT")
    if psdt is None or len(psdt["payload"]) != 8:
        raise _error(plugin, "PACK", psdt["offset"] if psdt else 0, "PSDT is missing or has the wrong size")
    schedule = psdt["payload"]
    schedule_value = {
        "month": _i8(schedule, 0),
        "day_of_week": _i8(schedule, 1),
        "date": _u8(schedule, 2),
        "start_hour": _i8(schedule, 3),
        "duration": _i32(schedule, 4),
    }
    result["schedule"] = schedule_value
    if not (-1 <= schedule_value["month"] <= 11
            and -1 <= schedule_value["day_of_week"] <= 10
            and 0 <= schedule_value["date"] <= 31
            and -1 <= schedule_value["start_hour"] <= 23
            and schedule_value["duration"] >= 0):
        raise _error(plugin, "PACK", psdt["offset"], "PSDT contains an invalid calendar value")

    pldt = _first(subrecords, "PLDT")
    location = {"kind": "None", "raw_kind": -1, "radius": 0, "reference": "null", "object_type": 0}
    if pldt is not None:
        if len(pldt["payload"]) != 12:
            raise _error(plugin, "PACK", pldt["offset"], "PLDT has the wrong size")
        raw_location = pldt["payload"]
        raw_kind = _i32(raw_location, 0)
        location.update({
            "kind": LOCATION_TYPES.get(raw_kind, "Unknown"),
            "raw_kind": raw_kind,
            "radius": _i32(raw_location, 8),
        })
        if raw_kind in LOCATION_TYPES and raw_kind >= 0:
            if raw_kind in (0, 1, 4):
                location["reference"] = _stable_key(plugin, _u32(raw_location, 4), masters)
            elif raw_kind == 5:
                location["object_type"] = _u32(raw_location, 4)
        elif raw_kind not in LOCATION_TYPES:
            raise _error(plugin, "PACK", pldt["offset"], f"unknown PLDT discriminant {raw_kind}")
    result["location"] = location

    ptdt = _first(subrecords, "PTDT")
    target = {"kind": "None", "raw_kind": -1, "distance": 0, "reference": "null", "object_type": 0}
    if ptdt is not None:
        if len(ptdt["payload"]) != 12:
            raise _error(plugin, "PACK", ptdt["offset"], "PTDT has the wrong size")
        raw_target = ptdt["payload"]
        raw_kind = _i32(raw_target, 0)
        target.update({
            "kind": TARGET_TYPES.get(raw_kind, "Unknown"),
            "raw_kind": raw_kind,
            "distance": _i32(raw_target, 8),
        })
        if raw_kind in TARGET_TYPES and raw_kind >= 0:
            if raw_kind in (0, 1, 3):
                target["reference"] = _stable_key(plugin, _u32(raw_target, 4), masters)
            elif raw_kind == 2:
                target["object_type"] = _u32(raw_target, 4)
        elif raw_kind not in TARGET_TYPES:
            raise _error(plugin, "PACK", ptdt["offset"], f"unknown PTDT discriminant {raw_kind}")
    result["target"] = target

    for condition in _all(subrecords, "CTDA") + _all(subrecords, "CTDT"):
        expected = 24 if condition["name"] == "CTDA" else 20
        if len(condition["payload"]) != expected:
            raise _error(plugin, "PACK", condition["offset"], f"{condition['name']} has the wrong size")
        payload = condition["payload"]
        function = _i32(payload, 8)
        result["conditions"].append({
            "layout": condition["name"],
            "function": function,
            "name": CONDITION_NAMES.get(function, f"Function{function}"),
            "flags": _u8(payload, 0),
            "run_on_raw": _u32(payload, 20) if condition["name"] == "CTDA" else 0,
            "run_on": CONDITION_RUN_ON.get(
                _u32(payload, 20) if condition["name"] == "CTDA" else 0, "Unknown"
            ),
            "parameter1": _u32(payload, 12),
            "parameter2": _u32(payload, 16),
            "parameter1_key": _stable_key(plugin, _u32(payload, 12), masters),
            "parameter2_key": _stable_key(plugin, _u32(payload, 16), masters),
        })

    known_skipped = {
        "TNAM", "INAM", "CNAM", "SCHR", "POBA", "POCA", "POEA", "SCTX", "SCDA", "SCRO",
        "IDLA", "IDLC", "IDLF", "IDLT", "PKDD", "PKD2", "PKPT", "PKED", "PKE2", "PKAM", "PUID",
        "PKW3", "PTD2", "PLD2", "PKFD", "SLSD", "SCVR", "SCRV", "IDLB", "ANAM", "BNAM", "FNAM",
        "PNAM", "QNAM", "UNAM", "XNAM", "PDTO", "PTDA", "PFOR", "PFO2", "PRCB", "PKCU", "PKC2",
        "CITC", "CIS1", "CIS2", "VMAD", "TPIC",
    }
    result["skipped_subrecords"] = sorted({item["name"] for item in subrecords if item["name"] in known_skipped})
    recognized = {"EDID", "PKDT", "PSDT", "PLDT", "PTDT", "CTDA", "CTDT"} | known_skipped
    unknown = [item["name"] for item in subrecords if item["name"] not in recognized]
    if unknown:
        raise _error(plugin, "PACK", 0, "unknown subrecords: " + ", ".join(sorted(set(unknown))))
    return result


def _parse_pgrd(record: dict[str, Any], subrecords: list[dict[str, Any]], masters: list[str]) -> dict[str, Any]:
    plugin = record["plugin"]
    data_records = _all(subrecords, "DATA")
    data_record = data_records[0] if data_records else None
    if len(data_records) != 1 or len(data_record["payload"]) != 2:
        raise _error(plugin, "PGRD", data_record["offset"] if data_record else 0,
                     "DATA is missing, duplicated, or malformed")
    pgrp_records = _all(subrecords, "PGRP")
    pgrp = pgrp_records[0] if pgrp_records else None
    pgrp_payload = b"".join(item["payload"] for item in pgrp_records)
    if pgrp is None or len(pgrp_payload) % 16:
        raise _error(plugin, "PGRD", pgrp["offset"] if pgrp else 0, "PGRP is missing or unaligned")
    node_count = _i16(data_record["payload"], 0)
    if node_count < 0 or node_count != len(pgrp_payload) // 16:
        raise _error(plugin, "PGRD", data_record["offset"], "DATA/PGRP node count mismatch")

    points: list[dict[str, Any]] = []
    expected_links = 0
    for offset in range(0, len(pgrp_payload), 16):
        point = pgrp_payload[offset : offset + 16]
        num_links = _u8(point, 12)
        expected_links += num_links
        points.append({
            "x": _f32(point, 0),
            "y": _f32(point, 4),
            "z": _f32(point, 8),
            "num_links": num_links,
            "priority": _u8(point, 13),
            "unknown": _u16(point, 14),
        })

    pgrr_records = _all(subrecords, "PGRR")
    pgrr = pgrr_records[0] if pgrr_records else None
    endpoints: list[int] = []
    if pgrr is not None:
        pgrr_payload = b"".join(item["payload"] for item in pgrr_records)
        if len(pgrr_payload) % 2:
            raise _error(plugin, "PGRD", pgrr["offset"], "PGRR is not aligned")
        endpoints = [_i16(pgrr_payload, offset) for offset in range(0, len(pgrr_payload), 2)]
        if len(endpoints) != expected_links:
            raise _error(plugin, "PGRD", pgrr["offset"], "PGRR endpoint count differs from PGRP")
        for endpoint in endpoints:
            if endpoint != -1 and not 0 <= endpoint < node_count:
                raise _error(plugin, "PGRD", pgrr["offset"], "PGRR endpoint is outside the node array")
    elif expected_links:
        raise _error(plugin, "PGRD", data_record["offset"], "PGRR is missing for linked nodes")

    foreign: list[dict[str, Any]] = []
    for item in _all(subrecords, "PGRI"):
        if len(item["payload"]) % 16:
            raise _error(plugin, "PGRD", item["offset"], "PGRI is not aligned")
        for offset in range(0, len(item["payload"]), 16):
            value = item["payload"][offset : offset + 16]
            local_node = _u16(value, 0)
            if local_node >= node_count:
                raise _error(plugin, "PGRD", item["offset"] + offset, "PGRI local node is out of range")
            foreign.append({
                "local_node": local_node,
                "x": _f32(value, 4),
                "y": _f32(value, 8),
                "z": _f32(value, 12),
            })

    objects: list[dict[str, Any]] = []
    for item in _all(subrecords, "PGRL"):
        if len(item["payload"]) < 4 or (len(item["payload"]) - 4) % 4:
            raise _error(plugin, "PGRD", item["offset"], "PGRL has an invalid size")
        raw_object = _u32(item["payload"], 0)
        linked_nodes = [_i32(item["payload"], offset) for offset in range(4, len(item["payload"]), 4)]
        for node in linked_nodes:
            if node != -1 and not 0 <= node < node_count:
                raise _error(plugin, "PGRD", item["offset"], "PGRL node is outside the node array")
        objects.append({
            "object": _stable_key(plugin, raw_object, masters),
            "linked_nodes": linked_nodes,
        })

    pgag_sizes = [len(item["payload"]) for item in _all(subrecords, "PGAG")]
    recognized = {"DATA", "PGRP", "PGRR", "PGRI", "PGRL", "PGAG"}
    unknown = [item["name"] for item in subrecords if item["name"] not in recognized]
    if unknown:
        raise _error(plugin, "PGRD", 0, "unknown subrecords: " + ", ".join(sorted(set(unknown))))
    return {
        "key": record["key"],
        "plugin": plugin,
        "cell": record["parent_cell"],
        "nodes": points,
        "node_count": node_count,
        "local_edges": expected_links,
        "valid_local_edges": sum(endpoint != -1 for endpoint in endpoints),
        "foreign": foreign,
        "foreign_edges": len(foreign),
        "objects": objects,
        "object_links": len(objects),
        "object_link_nodes": sum(len(item["linked_nodes"]) for item in objects),
        "pgag_sizes": pgag_sizes,
        "pgag_bytes": sum(pgag_sizes),
    }


def _record_summary(
    data: bytes, start: int, end: int, plugin: str, masters: list[str], parent_cell: str
) -> dict[str, Any]:
    if end - start < RECORD_HEADER_SIZE:
        raise _error(plugin, "?", start, "record header is truncated")
    record_type = _tag(data[start : start + 4])
    size = _u32(data, start + 4)
    flags = _u32(data, start + 8)
    raw_id = _u32(data, start + 12)
    payload_start = start + RECORD_HEADER_SIZE
    payload_end = payload_start + size
    if payload_end != end or payload_end > len(data):
        raise _error(plugin, record_type, start, "record size is outside its containing group")
    key = _stable_key(plugin, raw_id, masters)
    summary: dict[str, Any] = {
        "type": record_type,
        "plugin": plugin,
        "key": key,
        "raw_id": raw_id,
        "flags": flags,
        "deleted": bool(flags & DELETED_FLAG),
        "parent_cell": parent_cell,
    }
    # Deleted overrides conventionally carry no payload.  They still
    # participate in the stable-key winner calculation, but there is no
    # native PACK/PGRD body to decode.
    if not summary["deleted"] and record_type in {
        "PACK", "PGRD", "NPC_", "CREA", "ACHR", "ACRE", "REFR", "CELL", "DOOR", "FURN"
    }:
        payload = _payload(data, payload_start, size, flags, plugin, record_type)
        summary["subrecords"] = _subrecords(payload, plugin, record_type)
        if record_type == "PACK":
            summary["pack"] = _parse_pack(summary, summary["subrecords"], masters)
        elif record_type == "PGRD":
            summary["pgrd"] = _parse_pgrd(summary, summary["subrecords"], masters)
        elif record_type in {"NPC_", "CREA"}:
            package_ids = []
            for item in _all(summary["subrecords"], "PKID"):
                if len(item["payload"]) != 4:
                    raise _error(plugin, record_type, item["offset"], "PKID has the wrong size")
                package_ids.append(_stable_key(plugin, _u32(item["payload"], 0), masters))
            summary["package_ids"] = package_ids
        elif record_type in {"REFR", "ACHR", "ACRE"}:
            # PGRL points to a placed reference, whose NAME subrecord is the
            # stable base-object identity used for door/furniture
            # classification.  Keep a null value for incomplete references so
            # the PGRL audit can report the exact link rather than silently
            # treating it as an arbitrary object.
            name = _first(summary["subrecords"], "NAME")
            if name is None:
                summary["base_key"] = "null"
            else:
                if len(name["payload"]) != 4:
                    raise _error(plugin, record_type, name["offset"], "NAME has the wrong size")
                summary["base_key"] = _stable_key(plugin, _u32(name["payload"], 0), masters)
            horse = _first(summary["subrecords"], "XHRS")
            if horse is None:
                summary["horse_key"] = "null"
            else:
                if len(horse["payload"]) != 4:
                    raise _error(plugin, record_type, horse["offset"], "XHRS has the wrong size")
                summary["horse_key"] = _stable_key(plugin, _u32(horse["payload"], 0), masters)
            data_record = _first(summary["subrecords"], "DATA")
            if data_record is not None and len(data_record["payload"]) == 24:
                summary["position"] = [
                    _f32(data_record["payload"], offset) for offset in range(0, 24, 4)
                ]
        summary.pop("subrecords", None)
    return summary


def _parse_plugin(path: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    plugin = path.name
    data = path.read_bytes()
    if len(data) < RECORD_HEADER_SIZE or data[:4] != b"TES4":
        raise M14AuditError(f"{plugin}: missing TES4 header")
    header_size = _u32(data, 4)
    header_end = RECORD_HEADER_SIZE + header_size
    if header_end > len(data):
        raise _error(plugin, "TES4", 0, "header payload is truncated")
    header_payload = _payload(data, RECORD_HEADER_SIZE, header_size, _u32(data, 8), plugin, "TES4")
    masters = [_decode_string(item["payload"]) for item in _all(_subrecords(header_payload, plugin, "TES4"), "MAST")]
    records: list[dict[str, Any]] = []

    def walk(start: int, end: int, parent_cell: str) -> None:
        offset = start
        while offset < end:
            if end - offset < RECORD_HEADER_SIZE:
                raise _error(plugin, "GRUP", offset, "group contains a truncated header")
            name = data[offset : offset + 4]
            size = _u32(data, offset + 4)
            item_end = offset + size if name == b"GRUP" else offset + RECORD_HEADER_SIZE + size
            if item_end > end or item_end < offset + RECORD_HEADER_SIZE:
                raise _error(plugin, _tag(name), offset, "item size is outside its group")
            if name == b"GRUP":
                if size < GROUP_HEADER_SIZE:
                    raise _error(plugin, "GRUP", offset, "group header size is invalid")
                group_type = _i32(data, offset + 12)
                child_cell = parent_cell
                if group_type in CELL_CHILD_GROUPS:
                    child_cell = _stable_key(plugin, _u32(data, offset + 8), masters)
                walk(offset + GROUP_HEADER_SIZE, item_end, child_cell)
            else:
                records.append(_record_summary(data, offset, item_end, plugin, masters, parent_cell))
            offset = item_end
        if offset != end:
            raise _error(plugin, "GRUP", offset, "group walk did not end on its boundary")

    walk(header_end, len(data), "null")
    return {
        "name": plugin,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "masters": [master.casefold() for master in masters],
        "record_count": len(records),
    }, records


def _counter(values: Iterable[Any]) -> dict[str, int]:
    return dict(sorted(collections.Counter(str(value) for value in values).items()))


def count_lock_snapshot(report: dict[str, Any]) -> dict[str, Any]:
    summary = report["summary"]
    return {
        "schema_version": 1,
        "official_content": report["official_content"],
        "expected": {
            "winning_pack_count": summary["winning_pack_count"],
            "winning_pgrd_count": summary["winning_pgrd_count"],
            "winning_cell_count": summary["winning_cell_count"],
            "package_type_distribution": summary["package_type_distribution"],
            "package_flag_distribution": summary["package_flag_distribution"],
            "reserved_package_flag_distribution": summary["reserved_package_flag_distribution"],
            "reserved_package_flag_bit_distribution": summary["reserved_package_flag_bit_distribution"],
            "reviewed_reserved_package_flags": summary["reviewed_reserved_package_flags"],
            "unsupported_reserved_package_flags": summary["unsupported_reserved_package_flags"],
            "schedule_distribution": summary["schedule_distribution"],
            "location_distribution": summary["location_distribution"],
            "target_distribution": summary["target_distribution"],
            "location_object_type_distribution": summary["location_object_type_distribution"],
            "target_object_type_distribution": summary["target_object_type_distribution"],
            "unsupported_package_types": summary["unsupported_package_types"],
            "reviewed_object_type_anomaly_count": summary["reviewed_object_type_anomaly_count"],
            "unsupported_object_type_anomaly_count": summary["unsupported_object_type_anomaly_count"],
            "condition_function_distribution": summary["condition_function_distribution"],
            "unsupported_condition_functions": summary["unsupported_condition_functions"],
            "condition_run_on_distribution": summary["condition_run_on_distribution"],
            "unsupported_condition_run_on": summary["unsupported_condition_run_on"],
            "pathgrid_node_count": summary["pathgrid_node_count"],
            "pathgrid_local_edge_count": summary["pathgrid_local_edge_count"],
            "pathgrid_foreign_edge_count": summary["pathgrid_foreign_edge_count"],
            "pathgrid_object_link_count": summary["pathgrid_object_link_count"],
            "pathgrid_object_link_node_count": summary["pathgrid_object_link_node_count"],
            "pathgrid_object_link_kind_distribution": summary["pathgrid_object_link_kind_distribution"],
            "pathgrid_object_link_reference_type_distribution": summary[
                "pathgrid_object_link_reference_type_distribution"
            ],
            "pathgrid_object_link_base_type_distribution": summary[
                "pathgrid_object_link_base_type_distribution"
            ],
            "pathgrid_pgag_bytes": summary["pathgrid_pgag_bytes"],
            "pathgrid_pgag_size_distribution": summary["pathgrid_pgag_size_distribution"],
            "package_fingerprint": summary["package_fingerprint"],
            "pathgrid_fingerprint": summary["pathgrid_fingerprint"],
        },
    }


def validate_count_lock(report: dict[str, Any], count_lock: dict[str, Any]) -> dict[str, Any]:
    actual = count_lock_snapshot(report)
    failures: list[str] = []
    if count_lock.get("schema_version") != actual["schema_version"]:
        failures.append("count-lock schema version differs")
    if count_lock.get("official_content") != actual["official_content"]:
        failures.append("official content fingerprints differ")
    expected = count_lock.get("expected", {})
    for name, value in actual["expected"].items():
        if expected.get(name) != value:
            failures.append(f"{name}: expected {expected.get(name)!r}, got {value!r}")
    return {"passed": not failures, "failures": failures}


def audit(
    data_dir: Path,
    plugin_names: Iterable[str],
    count_lock_path: Path | None = None,
    write_count_lock: bool = False,
) -> dict[str, Any]:
    paths = [data_dir / name for name in plugin_names]
    missing = [str(path) for path in paths if not path.is_file()]
    if missing:
        raise M14AuditError("missing official plugins: " + ", ".join(missing))

    plugin_reports: list[dict[str, Any]] = []
    histories: dict[str, list[dict[str, Any]]] = collections.defaultdict(list)
    for path in paths:
        plugin_report, records = _parse_plugin(path)
        plugin_reports.append(plugin_report)
        for record in records:
            if record["key"] != "null":
                histories[record["key"]].append(record)

    winners = {key: records[-1] for key, records in histories.items()}
    winning = [record for record in winners.values() if not record["deleted"]]
    packs = sorted((record["pack"] for record in winning if record["type"] == "PACK"), key=lambda item: item["key"])
    pgrds = sorted((record["pgrd"] for record in winning if record["type"] == "PGRD"), key=lambda item: item["key"])
    cells = sorted((record for record in winning if record["type"] == "CELL"), key=lambda item: item["key"])

    package_types = _counter(item["package_type"] for item in packs)
    package_flags = _counter(item["flags_hex"] for item in packs)
    reserved_flags = _counter(f"0x{item['reserved_flags']:08x}" for item in packs if item["reserved_flags"])
    reserved_flag_bits = _counter(
        f"0x{bit:08x}"
        for item in packs
        for bit in (1 << index for index in range(32))
        if item["reserved_flags"] & bit
    )
    unreviewed_reserved_flags = [
        {
            "package": item["key"],
            "flags": item["flags_hex"],
            "bit": f"0x{bit:08x}",
        }
        for item in packs
        for bit in (1 << index for index in range(32))
        if item["reserved_flags"] & bit and bit not in REVIEWED_RESERVED_PACKAGE_FLAGS
    ]
    schedules = _counter(
        json.dumps(item["schedule"], sort_keys=True, separators=(",", ":")) for item in packs
    )
    locations = _counter(item["location"]["kind"] for item in packs)
    targets = _counter(item["target"]["kind"] for item in packs)
    location_object_types = _counter(
        f"{item['location']['object_type']}:{PACKAGE_OBJECT_TYPES.get(item['location']['object_type'], 'Unknown')}"
        for item in packs
        if item["location"]["kind"] == "ObjectType"
    )
    target_object_types = _counter(
        f"{item['target']['object_type']}:{PACKAGE_OBJECT_TYPES.get(item['target']['object_type'], 'Unknown')}"
        for item in packs
        if item["target"]["kind"] == "ObjectType"
    )
    package_object_type_anomalies = []
    for item in packs:
        for context in ("location", "target"):
            value = item[context]["object_type"]
            if item[context]["kind"] != "ObjectType" or value in PACKAGE_OBJECT_TYPES:
                continue
            package_object_type_anomalies.append({
                "package": item["key"],
                "context": context,
                "value": value,
                "disposition": REVIEWED_OBJECT_TYPE_ANOMALIES.get(value),
            })
    unresolved_object_type_anomalies = [
        item for item in package_object_type_anomalies if item["disposition"] is None
    ]
    reviewed_object_type_anomalies = [
        item for item in package_object_type_anomalies if item["disposition"] is not None
    ]
    unsupported_package_types = _counter(
        f"{item['package_type_raw']}:{item['package_type']}"
        for item in packs
        if item["package_type_raw"] not in PACKAGE_TYPES
    )
    conditions = [condition for item in packs for condition in item["conditions"]]
    condition_functions = _counter(
        f"{item['function']}:{item['name']}" for item in conditions
    )
    unsupported_conditions = _counter(
        f"{item['function']}:{item['name']}"
        for item in conditions
        if item["function"] not in SUPPORTED_CONDITION_FUNCTIONS
    )
    condition_run_on = _counter(
        f"{item['run_on_raw']}:{item['run_on']}" for item in conditions
    )
    unsupported_condition_run_on = _counter(
        f"{item['run_on_raw']}:{item['run_on']}"
        for item in conditions
        if item["run_on"] == "Unknown"
    )

    actor_records = [record for record in winning if record["type"] in {"NPC_", "CREA"}]
    pack_keys = {item["key"] for item in packs}
    actor_no_packages = []
    actor_unresolved_packages = []
    for actor in actor_records:
        package_ids = actor.get("package_ids", [])
        if not package_ids:
            actor_no_packages.append(actor["key"])
        unresolved = sorted(package for package in package_ids if package not in pack_keys)
        if unresolved:
            actor_unresolved_packages.append({"actor": actor["key"], "packages": unresolved})

    pgrd_by_cell: dict[str, list[str]] = collections.defaultdict(list)
    for pgrd in pgrds:
        pgrd_by_cell[pgrd["cell"]].append(pgrd["key"])
    ambiguous_cells = sorted(cell for cell, graph_keys in pgrd_by_cell.items() if cell != "null" and len(graph_keys) > 1)
    usable_cells = {cell for cell, graph_keys in pgrd_by_cell.items() if cell != "null" and len(graph_keys) == 1}
    cells_without_graph = sorted(cell["key"] for cell in cells if cell["key"] not in usable_cells)
    unowned_pgrds = sorted(item["key"] for item in pgrds if item["cell"] == "null")

    package_fingerprint = _fingerprint([
        {
            "key": item["key"],
            "type": item["package_type_raw"],
            "flags": item["flags"],
            "schedule": item["schedule"],
            "location": item["location"],
            "target": item["target"],
            "conditions": item["conditions"],
        }
        for item in packs
    ])
    pathgrid_fingerprint = _fingerprint([
        {
            "key": item["key"],
            "cell": item["cell"],
            "nodes": item["nodes"],
            "foreign": item["foreign"],
            "objects": item["objects"],
            "pgag_sizes": item["pgag_sizes"],
        }
        for item in pgrds
    ])

    invalid_references = []
    if unowned_pgrds:
        invalid_references.extend({"kind": "pgrd-parent-cell", "pathgrid": key} for key in unowned_pgrds)
    invalid_references.extend(
        {"kind": "ambiguous-cell-pathgrid", "cell": cell, "pathgrids": sorted(pgrd_by_cell[cell])}
        for cell in ambiguous_cells
    )

    # PGRL is an object link, not a free-standing FormID.  In the shipped
    # Oblivion data it normally names a placed REFR whose NAME subrecord names
    # the actual DOOR/FURN (or another base object).  Keep the two identities
    # separate so a missing placed reference, a deleted override, and a
    # missing/deleted base are distinct audit failures.  A direct DOOR/FURN
    # winner is also accepted for compatibility with older writers that emit
    # the base object in PGRL.
    pathgrid_object_link_kinds: list[str] = []
    pathgrid_object_link_reference_types: list[str] = []
    pathgrid_object_link_base_types: list[str] = []
    for item in pgrds:
        for object_link in item["objects"]:
            object_key = object_link["object"]
            linked = winners.get(object_key)
            if linked is None:
                pathgrid_object_link_kinds.append("Missing")
                pathgrid_object_link_reference_types.append("Missing")
                pathgrid_object_link_base_types.append("Missing")
                invalid_references.append({
                    "kind": "pgrl-object-missing",
                    "pathgrid": item["key"],
                    "object": object_key,
                })
                continue
            if linked["deleted"]:
                pathgrid_object_link_kinds.append("Deleted")
                pathgrid_object_link_reference_types.append("Deleted")
                pathgrid_object_link_base_types.append("Deleted")
                invalid_references.append({
                    "kind": "pgrl-object-deleted",
                    "pathgrid": item["key"],
                    "object": object_key,
                })
                continue

            linked_type = linked["type"]
            pathgrid_object_link_reference_types.append(linked_type)
            if linked_type not in {"REFR", "ACHR", "ACRE"}:
                # Direct base-object links are accepted only for the two
                # object classes that can open or reserve a pathgrid edge.
                if linked_type in {"DOOR", "FURN"}:
                    pathgrid_object_link_kinds.append(
                        "Door" if linked_type == "DOOR" else "Furniture"
                    )
                    pathgrid_object_link_base_types.append(linked_type)
                    continue
                pathgrid_object_link_kinds.append("Other")
                pathgrid_object_link_base_types.append(linked_type)
                invalid_references.append({
                    "kind": "pgrl-object-not-reference",
                    "pathgrid": item["key"],
                    "object": object_key,
                    "record_type": linked_type,
                })
                continue

            base_key = linked.get("base_key", "null")
            if base_key == "null":
                pathgrid_object_link_kinds.append("Missing")
                pathgrid_object_link_base_types.append("Missing")
                invalid_references.append({
                    "kind": "pgrl-base-missing",
                    "pathgrid": item["key"],
                    "object": object_key,
                    "reference_type": linked_type,
                })
                continue
            base = winners.get(base_key)
            if base is None:
                pathgrid_object_link_kinds.append("Missing")
                pathgrid_object_link_base_types.append("Missing")
                invalid_references.append({
                    "kind": "pgrl-base-missing",
                    "pathgrid": item["key"],
                    "object": object_key,
                    "base": base_key,
                    "reference_type": linked_type,
                })
                continue
            if base["deleted"]:
                pathgrid_object_link_kinds.append("Deleted")
                pathgrid_object_link_base_types.append("Deleted")
                invalid_references.append({
                    "kind": "pgrl-base-deleted",
                    "pathgrid": item["key"],
                    "object": object_key,
                    "base": base_key,
                    "reference_type": linked_type,
                })
                continue

            base_type = base["type"]
            pathgrid_object_link_base_types.append(base_type)
            if base_type == "DOOR":
                pathgrid_object_link_kinds.append("Door")
            elif base_type == "FURN":
                pathgrid_object_link_kinds.append("Furniture")
            else:
                pathgrid_object_link_kinds.append("Other")

    summary = {
        "winning_record_count": len(winning),
        "winning_pack_count": len(packs),
        "winning_pgrd_count": len(pgrds),
        "winning_cell_count": len(cells),
        "package_type_distribution": package_types,
        "package_flag_distribution": package_flags,
        "reserved_package_flag_distribution": reserved_flags,
        "reserved_package_flag_bit_distribution": reserved_flag_bits,
        "reviewed_reserved_package_flags": {
            f"0x{bit:08x}": explanation for bit, explanation in sorted(REVIEWED_RESERVED_PACKAGE_FLAGS.items())
            if any(item["reserved_flags"] & bit for item in packs)
        },
        "unsupported_reserved_package_flags": unreviewed_reserved_flags,
        "schedule_distribution": schedules,
        "location_distribution": locations,
        "target_distribution": targets,
        "location_object_type_distribution": location_object_types,
        "target_object_type_distribution": target_object_types,
        "unsupported_package_types": unsupported_package_types,
        "reviewed_object_type_anomaly_count": len(reviewed_object_type_anomalies),
        "unsupported_object_type_anomaly_count": len(unresolved_object_type_anomalies),
        "condition_count": len(conditions),
        "condition_function_distribution": condition_functions,
        "unsupported_condition_functions": unsupported_conditions,
        "condition_run_on_distribution": condition_run_on,
        "unsupported_condition_run_on": unsupported_condition_run_on,
        "pathgrid_node_count": sum(item["node_count"] for item in pgrds),
        "pathgrid_local_edge_count": sum(item["local_edges"] for item in pgrds),
        "pathgrid_foreign_edge_count": sum(item["foreign_edges"] for item in pgrds),
        "pathgrid_object_link_count": sum(item["object_links"] for item in pgrds),
        "pathgrid_object_link_node_count": sum(item["object_link_nodes"] for item in pgrds),
        "pathgrid_object_link_kind_distribution": _counter(pathgrid_object_link_kinds),
        "pathgrid_object_link_reference_type_distribution": _counter(
            pathgrid_object_link_reference_types
        ),
        "pathgrid_object_link_base_type_distribution": _counter(pathgrid_object_link_base_types),
        "pathgrid_pgag_bytes": sum(item["pgag_bytes"] for item in pgrds),
        "pathgrid_pgag_size_distribution": _counter(
            size for item in pgrds for size in item["pgag_sizes"]
        ),
        "actor_count": len(actor_records),
        "actors_without_packages": len(actor_no_packages),
        "actors_with_unresolved_packages": len(actor_unresolved_packages),
        "cells_without_usable_graph": len(cells_without_graph),
        "ambiguous_pathgrid_cells": len(ambiguous_cells),
        "invalid_reference_count": len(invalid_references),
        "package_fingerprint": package_fingerprint,
        "pathgrid_fingerprint": pathgrid_fingerprint,
    }
    report: dict[str, Any] = {
        "schema_version": 1,
        "official_content": [
            {key: item[key] for key in ("name", "size", "sha256")} for item in plugin_reports
        ],
        "plugins": plugin_reports,
        "summary": summary,
        "unsupported": {
            "condition_functions": dict(sorted(unsupported_conditions.items())),
            "condition_run_on": dict(sorted(unsupported_condition_run_on.items())),
            "package_types": unsupported_package_types,
            "reserved_package_flags": unreviewed_reserved_flags,
            "object_types": unresolved_object_type_anomalies,
            "actor_packages": actor_unresolved_packages,
            "invalid_references": invalid_references,
            "reviewed_object_type_anomalies": reviewed_object_type_anomalies,
        },
        "actors_without_packages": sorted(actor_no_packages),
        "cells_without_usable_graph": cells_without_graph,
        "ambiguous_pathgrid_cells": ambiguous_cells,
        "package_fingerprint": package_fingerprint,
        "pathgrid_fingerprint": pathgrid_fingerprint,
    }

    if count_lock_path is not None:
        if write_count_lock:
            count_lock_path.parent.mkdir(parents=True, exist_ok=True)
            count_lock_path.write_text(
                json.dumps(count_lock_snapshot(report), indent=2, sort_keys=True) + "\n", encoding="utf-8"
            )
            report["count_lock"] = {"passed": True, "written": str(count_lock_path), "failures": []}
        elif count_lock_path.is_file():
            report["count_lock"] = validate_count_lock(
                report, json.loads(count_lock_path.read_text(encoding="utf-8"))
            )
            report["count_lock"]["path"] = str(count_lock_path)
        else:
            report["count_lock"] = {
                "passed": False,
                "failures": [f"missing count lock: {count_lock_path}"],
                "path": str(count_lock_path),
            }
    else:
        report["count_lock"] = {"passed": True, "failures": []}

    report["passed"] = (
        report["count_lock"]["passed"]
        and not report["unsupported"]["condition_functions"]
        and not report["unsupported"]["condition_run_on"]
        and not report["unsupported"]["package_types"]
        and not report["unsupported"]["reserved_package_flags"]
        and not report["unsupported"]["object_types"]
        and not report["unsupported"]["actor_packages"]
        and not report["unsupported"]["invalid_references"]
        and not report["ambiguous_pathgrid_cells"]
    )
    return report
