#!/usr/bin/env python3
"""Read, write, mutate, and compare OpenMW's native TES4 save state.

This module intentionally understands only the versioned ``T4ST`` envelope.
All unrelated OpenMW save records are copied byte-for-byte when a state is
rewritten, making it suitable for process-restart acceptance tests and failure
fixtures without depending on proprietary game data.
"""

from __future__ import annotations

import copy
import json
import math
import struct

from pathlib import Path
from typing import Any


MAGIC = b"OMW4STATE"
CURRENT_VERSION = 39
SUPPORTED_VERSIONS = set(range(1, CURRENT_VERSION + 1))
MAX_COLLECTION = 1_000_000
MAX_STRING = 16 * 1024 * 1024
MAX_PAYLOAD = 256 * 1024 * 1024
CHUNK_SIZE = 60 * 1024
DEFAULT_MIGRATION_RACE = "content:oblivion.esm:000907"
DEFAULT_MIGRATION_CLASS = "content:oblivion.esm:0230e6"


class RuntimeStateError(RuntimeError):
    pass


def _global_identity(value: Any) -> tuple[str, str, int]:
    """Parse non-null global keys like ESM::FormKey::deserialize on this host.

    Old envelopes may use unpadded hex or mixed-case content names. Preserve
    their wire text, but compare the identities after native normalization.
    This deliberately differs from the canonical-only native actor ledger.
    """
    parts = value.split(":") if isinstance(value, str) else []
    if len(parts) != 3:
        raise RuntimeStateError("Invalid TES4 runtime-state global FormKey")
    kind, namespace, number = parts
    if (kind not in ("content", "dynamic") or not number
            or any(char not in "0123456789abcdefABCDEF" for char in number)):
        raise RuntimeStateError("Invalid TES4 runtime-state global FormKey")
    serial = int(number, 16)
    if not 0 < serial <= (0xffffff if kind == "content" else 0xffffffffffffffff):
        raise RuntimeStateError("Null or overflowing TES4 runtime-state global FormKey")
    if kind == "content":
        namespace = namespace.rsplit("/", 1)[-1]
        namespace = "".join(chr(ord(char) + 32) if "A" <= char <= "Z" else char for char in namespace)
    if not namespace:
        raise RuntimeStateError("Empty TES4 runtime-state global FormKey namespace")
    return kind, namespace, serial


class _Reader:
    def __init__(self, data: bytes):
        if len(data) > MAX_PAYLOAD:
            raise RuntimeStateError("TES4 runtime-state payload exceeds the size limit")
        self.data = data
        self.offset = 0

    def take(self, size: int) -> bytes:
        if size < 0 or self.offset + size > len(self.data):
            raise RuntimeStateError("Truncated TES4 runtime-state payload")
        result = self.data[self.offset : self.offset + size]
        self.offset += size
        return result

    def unpack(self, fmt: str) -> Any:
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, self.take(size))[0]

    def count(self) -> int:
        value = self.unpack("<I")
        if value > MAX_COLLECTION:
            raise RuntimeStateError("TES4 runtime-state collection exceeds the size limit")
        return value

    def string(self) -> str:
        size = self.unpack("<I")
        if size > MAX_STRING:
            raise RuntimeStateError("TES4 runtime-state string exceeds the size limit")
        try:
            return self.take(size).decode("utf-8")
        except UnicodeDecodeError as error:
            raise RuntimeStateError("TES4 runtime-state string is not UTF-8") from error


class _Writer:
    def __init__(self):
        self.parts: list[bytes] = []

    def add(self, data: bytes) -> None:
        self.parts.append(data)

    def pack(self, fmt: str, value: Any) -> None:
        self.add(struct.pack(fmt, value))

    def string(self, value: str) -> None:
        data = value.encode("utf-8")
        if len(data) > MAX_STRING:
            raise RuntimeStateError("TES4 runtime-state string exceeds the size limit")
        self.pack("<I", len(data))
        self.add(data)

    def finish(self) -> bytes:
        result = b"".join(self.parts)
        if len(result) > MAX_PAYLOAD:
            raise RuntimeStateError("TES4 runtime-state payload exceeds the size limit")
        return result


def _position(reader: _Reader) -> list[float]:
    value = [reader.unpack("<f") for _ in range(6)]
    if not all(math.isfinite(item) for item in value):
        raise RuntimeStateError("TES4 runtime-state position is not finite")
    return value


def _write_position(writer: _Writer, value: list[float]) -> None:
    if len(value) != 6 or not all(math.isfinite(item) for item in value):
        raise RuntimeStateError("Invalid TES4 runtime-state position")
    for item in value:
        writer.pack("<f", item)


def _calendar(reader: _Reader) -> list[int | float]:
    value: list[int | float] = [reader.unpack("<i"), reader.unpack("<i"), reader.unpack("<i"), reader.unpack("<d")]
    if not _valid_calendar(value):
        raise RuntimeStateError("Invalid TES4 runtime-state schedule calendar")
    return value


def _write_calendar(writer: _Writer, value: list[int | float]) -> None:
    if (len(value) != 4 or not _valid_calendar(value)
            or not 0 <= int(value[1]) < 12 or not 1 <= int(value[2]) <= 31
            or not math.isfinite(float(value[3])) or not 0.0 <= float(value[3]) < 24.0):
        raise RuntimeStateError("Invalid TES4 runtime-state schedule calendar")
    writer.pack("<i", int(value[0]))
    writer.pack("<i", int(value[1]))
    writer.pack("<i", int(value[2]))
    writer.pack("<d", float(value[3]))


def _is_leap_year(year: int) -> bool:
    return year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)


def _days_in_month(year: int, month: int) -> int:
    days = (31, 29 if _is_leap_year(year) else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31)
    return days[month] if 0 <= month < len(days) else 0


def _valid_calendar(value: Any) -> bool:
    if not isinstance(value, list) or len(value) != 4:
        return False
    try:
        year, month, day = int(value[0]), int(value[1]), int(value[2])
        hour = float(value[3])
    except (TypeError, ValueError, OverflowError):
        return False
    if not 0 <= month < 12 or not 1 <= day <= _days_in_month(year, month):
        return False
    return math.isfinite(hour) and 0.0 <= hour < 24.0


def _value(reader: _Reader) -> bool | int | float | str:
    kind = reader.unpack("<B")
    if kind == 1:
        value = reader.unpack("<B")
        if value > 1:
            raise RuntimeStateError(
                f"Invalid TES4 runtime-state boolean {value} at payload offset {reader.offset - 1}"
            )
        return bool(value)
    if kind == 2:
        return reader.unpack("<q")
    if kind == 3:
        value = reader.unpack("<d")
        if not math.isfinite(value):
            raise RuntimeStateError("TES4 runtime-state value is not finite")
        return value
    if kind == 4:
        return reader.string()
    raise RuntimeStateError(f"Unknown TES4 runtime-state value type {kind}")


def _write_value(writer: _Writer, value: bool | int | float | str) -> None:
    if isinstance(value, bool):
        writer.pack("<B", 1)
        writer.pack("<B", int(value))
    elif isinstance(value, int):
        writer.pack("<B", 2)
        writer.pack("<q", value)
    elif isinstance(value, float):
        if not math.isfinite(value):
            raise RuntimeStateError("TES4 runtime-state value is not finite")
        writer.pack("<B", 3)
        writer.pack("<d", value)
    elif isinstance(value, str):
        writer.pack("<B", 4)
        writer.string(value)
    else:
        raise RuntimeStateError(f"Unsupported TES4 runtime-state value {value!r}")


def _script_value(reader: _Reader) -> dict[str, Any] | None:
    kind = reader.unpack("<B")
    if kind == 0:
        return None
    if kind == 1:
        return {"type": "number", "value": reader.unpack("<q")}
    if kind == 2:
        value = reader.unpack("<d")
        if not math.isfinite(value):
            raise RuntimeStateError("TES4 runtime-state script value is not finite")
        return {"type": "number", "value": value}
    if kind == 3:
        return {"type": "string", "value": reader.string()}
    if kind == 4:
        return {"type": "reference", "value": reader.string()}
    raise RuntimeStateError(f"Unknown TES4 runtime-state script value type {kind}")


def _write_script_value(writer: _Writer, value: dict[str, Any] | None) -> None:
    if value is None:
        writer.pack("<B", 0)
        return
    kind, item = value.get("type"), value.get("value")
    if kind == "number" and isinstance(item, int):
        writer.pack("<B", 1)
        writer.pack("<q", item)
    elif kind == "number" and isinstance(item, float):
        if not math.isfinite(item):
            raise RuntimeStateError("TES4 runtime-state script value is not finite")
        writer.pack("<B", 2)
        writer.pack("<d", item)
    elif kind == "string":
        writer.pack("<B", 3)
        writer.string(str(item))
    elif kind == "reference":
        writer.pack("<B", 4)
        writer.string(str(item))
    else:
        raise RuntimeStateError(f"Unsupported TES4 runtime-state script value {value!r}")


def _inventory(reader: _Reader, version: int) -> list[dict[str, Any]]:
    result = []
    for _ in range(reader.count()):
        item: dict[str, Any] = {"base": reader.string(), "count": reader.unpack("<i")}
        if version >= 4:
            item.update({
                "condition": reader.unpack("<f" if version >= 24 else "<i"),
                "charge": reader.unpack("<f"),
                "equipped_slots": reader.unpack("<I"),
                "hotkey": reader.unpack("<b"),
                "owner": reader.string(),
                "remaining_usage_time": reader.unpack("<f"),
            })
        result.append(item)
    return result


def _write_inventory(writer: _Writer, value: list[dict[str, Any]], version: int) -> None:
    writer.pack("<I", len(value))
    for item in value:
        writer.string(str(item["base"]))
        writer.pack("<i", int(item["count"]))
        if version >= 4:
            writer.pack("<f" if version >= 24 else "<i",
                        float(item.get("condition", -1)) if version >= 24
                        else int(item.get("condition", -1)))
            writer.pack("<f", float(item.get("charge", -1.0)))
            writer.pack("<I", int(item.get("equipped_slots", 0)))
            writer.pack("<b", int(item.get("hotkey", -1)))
            writer.string(str(item.get("owner", "null")))
            writer.pack("<f", float(item.get("remaining_usage_time", -1.0)))


def _finite_float(reader: _Reader, label: str) -> float:
    value = reader.unpack("<f")
    if not math.isfinite(value) or value < 0.0:
        raise RuntimeStateError(f"Invalid TES4 runtime-state {label}")
    return value


def _read_actor_ai(reader: _Reader, version: int) -> dict[str, Any]:
    actor = {
        "actor": reader.string(),
        "base": reader.string(),
        "package": reader.string(),
        "script_package": reader.string(),
        "target": reader.string(),
        "target_base": reader.string(),
        "cell": reader.string(),
        "pathgrid": reader.string(),
        "door": reader.string(),
        "destination_cell": reader.string(),
        "last_valid_cell": reader.string(),
        "action_item": reader.string(),
        "last_transition_door": reader.string(),
        "companion_group": reader.string(),
        "companion_side_with": reader.string(),
        "mount": reader.string(),
        "rider": reader.string(),
        "schedule_window": None,
        "condition_result": 0,
    }
    has_schedule_window = reader.unpack("<B")
    if has_schedule_window > 1:
        raise RuntimeStateError("Invalid TES4 runtime-state actor schedule-window flag")
    if has_schedule_window:
        actor["schedule_window"] = {
            "start": _calendar(reader),
            "end": _calendar(reader),
            "duration_hours": reader.unpack("<d"),
        }
        if (not math.isfinite(float(actor["schedule_window"]["duration_hours"]))
                or float(actor["schedule_window"]["duration_hours"]) < 0.0):
            raise RuntimeStateError("Invalid TES4 runtime-state actor schedule window")
    actor["condition_result"] = reader.unpack("<B")
    if int(actor["condition_result"]) > 3:
        raise RuntimeStateError("Invalid TES4 runtime-state actor condition result")
    actor["destination_position"] = _position(reader)
    actor["last_valid_position"] = _position(reader)
    actor.update({
        "source": reader.unpack("<B"),
        "package_type": reader.unpack("<B"),
        "procedure": reader.unpack("<H"),
        "phase": reader.unpack("<B"),
        "tier": reader.unpack("<B"),
        "boundary": reader.unpack("<B"),
        "list_index": reader.unpack("<I"),
        "path_node": reader.unpack("<I"),
        "repath_attempts": reader.unpack("<I"),
        "formation_index": reader.unpack("<i"),
        "selection_generation": reader.unpack("<Q"),
        "route_generation": reader.unpack("<Q"),
        "transition_generation": reader.unpack("<Q"),
        "action_timer": _finite_float(reader, "actor action timer"),
        "duration_remaining": _finite_float(reader, "actor duration"),
        "no_progress_seconds": _finite_float(reader, "actor no-progress timer"),
        "door_cooldown": _finite_float(reader, "actor door cooldown"),
        "low_process_timer": _finite_float(reader, "actor low-process timer"),
        "next_low_process_tick": _finite_float(reader, "actor next low-process tick"),
    })
    restrained, reserved, has_destination = reader.unpack("<B"), reader.unpack("<B"), reader.unpack("<B")
    if restrained > 1 or reserved > 1 or has_destination > 1:
        raise RuntimeStateError("Invalid TES4 runtime-state actor AI flags")
    actor["restrained"] = bool(restrained)
    actor["action_reserved"] = bool(reserved)
    actor["has_destination"] = bool(has_destination)
    if version >= 7:
        door_animation_started = reader.unpack("<B")
        if door_animation_started > 1:
            raise RuntimeStateError("Invalid TES4 actor AI door-animation flag")
        actor["door_animation_started"] = bool(door_animation_started)
    actor["interruption_reason"] = reader.string()
    return actor


def _validate_ai(state: dict[str, Any]) -> None:
    actors = state.get("actor_ai", [])
    seen: set[str] = set()
    actor_ai_by_key: dict[str, dict[str, Any]] = {}
    procedure_for_type = {index: index + 1 for index in range(13)}
    for actor in actors:
        required = ("actor", "base", "cell")
        if any(str(actor.get(key, "null")) == "null" for key in required):
            raise RuntimeStateError("Invalid TES4 runtime-state actor AI identity")
        actor_key = str(actor["actor"])
        if actor_key in seen:
            raise RuntimeStateError(f"Duplicate TES4 runtime-state actor AI identity {actor_key}")
        seen.add(actor_key)
        actor_ai_by_key[actor_key] = actor
        if (str(actor.get("companion_side_with", "null")) == actor_key):
            raise RuntimeStateError("TES4 actor companion side-with points to itself")
        source = int(actor.get("source", 0))
        package_type = int(actor.get("package_type", 255))
        procedure = int(actor.get("procedure", 0))
        if source == 0:
            if (str(actor.get("package", "null")) != "null"
                    or package_type != 255 or procedure != 0):
                raise RuntimeStateError("TES4 idle actor AI state contains a package")
        elif source in (1, 2):
            if str(actor.get("package", "null")) == "null" or package_type not in procedure_for_type:
                raise RuntimeStateError("TES4 active actor AI state has an invalid package identity")
            if procedure != procedure_for_type[package_type]:
                raise RuntimeStateError("TES4 active actor AI state has an invalid package procedure")
        if source == 2 and str(actor.get("script_package", "null")) == "null":
            raise RuntimeStateError("TES4 script-owned actor AI state has no script package")
        if source not in (0, 1, 2):
            raise RuntimeStateError("Invalid TES4 runtime-state actor AI source")
        if str(actor.get("pathgrid", "null")) == "null" and int(actor.get("path_node", 0)) != 0:
            raise RuntimeStateError("TES4 actor AI state has a node without a pathgrid")
        if (int(actor.get("repath_attempts", 0)) > 8
                or int(actor.get("formation_index", -1)) < -1
                or len(str(actor.get("interruption_reason", ""))) > 1024):
            raise RuntimeStateError("Invalid TES4 actor AI counters")
        if not 0 <= int(actor.get("condition_result", 0)) <= 3:
            raise RuntimeStateError("Invalid TES4 runtime-state actor condition result")
        schedule_window = actor.get("schedule_window")
        if schedule_window is not None:
            if not isinstance(schedule_window, dict):
                raise RuntimeStateError("Invalid TES4 runtime-state actor schedule window")
            for key in ("start", "end"):
                calendar = schedule_window.get(key)
                if not _valid_calendar(calendar):
                    raise RuntimeStateError("Invalid TES4 runtime-state actor schedule window")
            duration = float(schedule_window.get("duration_hours", 0.0))
            if not math.isfinite(duration) or duration < 0.0:
                raise RuntimeStateError("Invalid TES4 runtime-state actor schedule window")
        for key in (
            "action_timer", "duration_remaining", "no_progress_seconds", "door_cooldown", "low_process_timer",
            "next_low_process_tick",
        ):
            value = float(actor.get(key, 0.0))
            if not math.isfinite(value) or value < 0.0:
                raise RuntimeStateError("Invalid TES4 actor AI timer")
        if bool(actor.get("has_destination", False)) and str(actor.get("destination_cell", "null")) == "null":
            raise RuntimeStateError("TES4 actor AI destination has no destination cell")
        if bool(actor.get("door_animation_started", False)) and (
            int(state["schema_version"]) < 7
            or int(actor.get("phase", 0)) != 3
            or str(actor.get("door", "null")) == "null"
        ):
            raise RuntimeStateError("Invalid TES4 actor AI door-animation state")
        if not 0 <= int(actor.get("source", 0)) <= 2 or not 0 <= int(actor.get("package_type", 255)) <= 255:
            raise RuntimeStateError("Invalid TES4 actor AI enum")
        if not 0 <= int(actor.get("procedure", 0)) <= 13 or not 0 <= int(actor.get("phase", 0)) <= 12:
            raise RuntimeStateError("Invalid TES4 actor AI phase enum")
        if not 0 <= int(actor.get("tier", 0)) <= 1 or not 0 <= int(actor.get("boundary", 0)) <= 3:
            raise RuntimeStateError("Invalid TES4 actor AI process enum")

    points = state.get("path_points", [])
    point_keys: set[tuple[str, int]] = set()
    for point in points:
        key = (str(point.get("pathgrid", "null")), int(point.get("node", 0)))
        if key[0] == "null" or key in point_keys:
            raise RuntimeStateError("Invalid or duplicate TES4 path-point overlay")
        point_keys.add(key)

    companions = state.get("companions", [])
    companion_pairs: set[tuple[str, str]] = set()
    edges: dict[str, list[str]] = {}
    for relation in companions:
        leader, member = str(relation.get("leader", "null")), str(relation.get("member", "null"))
        group = str(relation.get("group", leader))
        side_with = str(relation.get("side_with", "null"))
        if (leader == "null" or member == "null" or group == "null" or leader == member
                or (leader, member) in companion_pairs
                or (side_with != "null" and side_with == leader)
                or int(relation.get("formation_index", -1)) < -1):
            raise RuntimeStateError("Invalid or duplicate TES4 companion relation")
        companion_pairs.add((leader, member))
        edges.setdefault(leader, []).append(member)
        member_state = actor_ai_by_key.get(member)
        if member_state is not None:
            if ((str(member_state.get("target", "null")) != "null"
                 and str(member_state.get("target")) != leader)
                    or (str(member_state.get("companion_group", "null")) != "null"
                        and str(member_state.get("companion_group")) != group)
                    or str(member_state.get("companion_side_with", "null")) != side_with):
                raise RuntimeStateError("TES4 companion relation is not reciprocal in actor AI state")
    visiting: set[str] = set()
    visited: set[str] = set()

    def visit(key: str) -> None:
        if key in visiting:
            raise RuntimeStateError("TES4 companion relations contain a cycle")
        if key in visited:
            return
        visiting.add(key)
        for member in edges.get(key, []):
            visit(member)
        visiting.remove(key)
        visited.add(key)

    for leader in sorted(edges):
        visit(leader)

    horses: set[str] = set()
    riders: set[str] = set()
    mount_pairs: set[tuple[str, str]] = set()
    for relation in state.get("mounts", []):
        horse, rider = str(relation.get("horse", "null")), str(relation.get("rider", "null"))
        if (horse == "null" or rider == "null" or horse == rider
                or (horse, rider) in mount_pairs or horse in horses or rider in riders):
            raise RuntimeStateError("Invalid or duplicate TES4 mount relation")
        mount_pairs.add((horse, rider))
        horses.add(horse)
        riders.add(rider)
        horse_state = actor_ai_by_key.get(horse)
        rider_state = actor_ai_by_key.get(rider)
        if bool(relation.get("mounted", False)):
            if ((horse_state is not None and str(horse_state.get("rider", "null")) != rider)
                    or (rider_state is not None and str(rider_state.get("mount", "null")) != horse)):
                raise RuntimeStateError("TES4 mounted relation is not reciprocal in actor AI state")
        elif ((horse_state is not None and str(horse_state.get("rider", "null")) == rider)
              or (rider_state is not None and str(rider_state.get("mount", "null")) == horse)):
            raise RuntimeStateError("TES4 inactive mount relation has active actor AI state")

    for key, actor in actor_ai_by_key.items():
        mount = str(actor.get("mount", "null"))
        rider = str(actor.get("rider", "null"))
        horse_state = actor_ai_by_key.get(mount)
        rider_state = actor_ai_by_key.get(rider)
        if horse_state is not None and str(horse_state.get("rider", "null")) != key:
            raise RuntimeStateError("TES4 actor mount state is not reciprocal")
        if rider_state is not None and str(rider_state.get("mount", "null")) != key:
            raise RuntimeStateError("TES4 actor rider state is not reciprocal")

    detection_pairs: set[tuple[str, str]] = set()
    for vector in state.get("detection_vectors", []):
        observer = str(vector.get("observer", "null"))
        target = str(vector.get("target", "null"))
        score = float(vector.get("score", 0.0))
        line_of_sight = bool(vector.get("line_of_sight", False))
        if (observer == "null" or target == "null" or observer == target
                or (observer, target) in detection_pairs
                or not math.isfinite(score) or not 0.0 <= score <= 100.0
                or (bool(vector.get("detected", False)) and not line_of_sight)):
            raise RuntimeStateError("Invalid or duplicate TES4 detection vector")
        detection_pairs.add((observer, target))


def _validate_basic_state(state: dict[str, Any]) -> None:
    """Validate the common envelope and the pre-M14 state shared by C++.

    The Python tool is also used to inspect saves produced by the engine, so
    this deliberately mirrors the C++ RuntimeState::validate checks instead
    of relying on a successful struct.pack as validation.
    """

    version = state.get("schema_version")
    if version not in SUPPORTED_VERSIONS or state.get("profile") != "oblivion":
        raise RuntimeStateError("Unsupported TES4 runtime-state schema or profile")
    try:
        if int(state.get("next_dynamic_serial", 0)) == 0:
            raise RuntimeStateError("TES4 runtime-state dynamic serial must be non-zero")
    except (TypeError, ValueError, OverflowError) as error:
        raise RuntimeStateError("Invalid TES4 runtime-state dynamic serial") from error

    combat_rng = state.get("combat_rng_state", 1)
    if type(combat_rng) is not int or not 0 <= combat_rng <= 0xFFFFFFFF:
        raise RuntimeStateError("Invalid TES4 combat random state")
    if version < 27 and combat_rng != 1:
        raise RuntimeStateError("Native combat random state requires runtime schema27")

    clock = state.get("clock")
    if not isinstance(clock, dict):
        raise RuntimeStateError("Invalid TES4 runtime-state clock")
    try:
        clock_value = [int(clock["year"]), int(clock["month"]), int(clock["day"]), float(clock["hour"])]
        time_scale = float(clock["time_scale"])
    except (KeyError, TypeError, ValueError, OverflowError) as error:
        raise RuntimeStateError("Invalid TES4 runtime-state clock") from error
    if not _valid_calendar(clock_value) or not math.isfinite(time_scale):
        raise RuntimeStateError("TES4 runtime-state clock is not finite")

    def check_collection(value: Any, label: str) -> list[Any]:
        if not isinstance(value, list) or len(value) > MAX_COLLECTION:
            raise RuntimeStateError(f"TES4 runtime-state {label} exceeds the size limit")
        return value

    pending = check_collection(state.get("pending_package_done", []), "pending package completion list")
    actions = state.get("physical_actions", {"next": 1, "pending": []})
    if not isinstance(actions, dict):
        raise RuntimeStateError("Invalid TES4 physical action ledger")
    next_action = actions.get("next")
    if type(next_action) is not int or not 1 <= next_action <= 0xFFFFFFFFFFFFFFFF:
        raise RuntimeStateError("Invalid TES4 next physical action identity")
    action_ids = check_collection(actions.get("pending"), "pending physical action list")
    seen_actions: set[int] = set()
    for action in action_ids:
        if type(action) is not int or not 0 < action < next_action or action in seen_actions:
            raise RuntimeStateError("Invalid or duplicate TES4 pending physical action identity")
        seen_actions.add(action)
    if version < 8 and (next_action != 1 or action_ids):
        raise RuntimeStateError("TES4 runtime-state versions before 8 cannot contain physical actions")
    if version < 6 and pending:
        raise RuntimeStateError("TES4 runtime-state versions before 6 cannot contain pending package events")
    for event in pending:
        if not isinstance(event, dict) or any(
            not isinstance(event.get(key), str) or event[key] in ("", "null") for key in ("actor", "package")
        ):
            raise RuntimeStateError("Invalid TES4 pending package completion identity")

    content = check_collection(state.get("content", []), "content list")
    plugins: set[str] = set()
    for item in content:
        if not isinstance(item, dict):
            raise RuntimeStateError("Invalid TES4 runtime-state content identity")
        plugin = str(item.get("plugin", "")).casefold()
        fingerprint = str(item.get("fingerprint", ""))
        if not plugin or plugin in plugins:
            raise RuntimeStateError(f"Duplicate TES4 runtime-state content identity: {plugin}")
        if not fingerprint:
            raise RuntimeStateError("TES4 runtime-state content fingerprint is empty")
        plugins.add(plugin)

    player = state.get("player")
    if not isinstance(player, dict) or str(player.get("reference", "null")) == "null" \
            or str(player.get("cell", "null")) == "null":
        raise RuntimeStateError("TES4 runtime-state player has a null required FormKey")
    try:
        _write_position(_Writer(), player["position"])
    except (KeyError, TypeError, ValueError, OverflowError) as error:
        raise RuntimeStateError("Invalid TES4 runtime-state player position") from error
    actor_values = player.get("actor_values", {})
    if not isinstance(actor_values, dict) or len(actor_values) > MAX_COLLECTION:
        raise RuntimeStateError("TES4 runtime-state player actor-value list exceeds the size limit")
    for name, value in actor_values.items():
        if not str(name) or not math.isfinite(float(value)):
            raise RuntimeStateError("Invalid TES4 runtime-state player actor value")
    inventory = player.get("inventory")
    if not isinstance(inventory, list):
        raise RuntimeStateError("Invalid TES4 runtime-state player inventory")
    _validate_inventory(inventory, version, True)
    if version < 3:
        if any(key in player for key in ("name", "race", "class", "birthsign", "female", "character_generation_flags")):
            raise RuntimeStateError("TES4 runtime-state version 1/2 cannot contain character-generation state")
    else:
        if (not isinstance(player.get("name"), str) or len(player["name"]) > 1024
                or str(player.get("race", "null")) == "null" or str(player.get("class", "null")) == "null"
                or int(player.get("character_generation_flags", 0)) > 0x1f):
            raise RuntimeStateError("Invalid TES4 runtime-state character-generation state")
        if not isinstance(player.get("female", False), bool):
            raise RuntimeStateError("Invalid TES4 runtime-state player sex")

    globals_ = state.get("globals", {})
    if not isinstance(globals_, dict) or len(globals_) > MAX_COLLECTION:
        raise RuntimeStateError("TES4 runtime-state global list exceeds the size limit")
    global_keys: set[tuple[str, str, int]] = set()
    for key, value in globals_.items():
        identity = _global_identity(key)
        if identity in global_keys:
            raise RuntimeStateError("Duplicate TES4 runtime-state global FormKey")
        global_keys.add(identity)
        _write_value(_Writer(), value)

    references = check_collection(state.get("references", []), "reference list")
    reference_keys: set[str] = set()
    for reference in references:
        if not isinstance(reference, dict):
            raise RuntimeStateError("Invalid TES4 runtime-state reference")
        key = str(reference.get("key", "null"))
        if (key == "null" or str(reference.get("base", "null")) == "null"
                or str(reference.get("cell", "null")) == "null" or key in reference_keys):
            raise RuntimeStateError("Invalid or duplicate TES4 runtime-state reference")
        reference_keys.add(key)
        owner = reference.get("owner")
        if owner is not None and str(owner) == "null":
            raise RuntimeStateError("TES4 runtime-state reference has a null owner")
        _write_position(_Writer(), reference["position"])
        reference_inventory = reference.get("inventory")
        if not isinstance(reference_inventory, list):
            raise RuntimeStateError("Invalid TES4 runtime-state reference inventory")
        _validate_inventory(reference_inventory, version, False)
        custom = reference.get("custom_state", {})
        if not isinstance(custom, dict) or len(custom) > MAX_COLLECTION:
            raise RuntimeStateError("TES4 runtime-state reference custom state exceeds the size limit")
        for name, value in custom.items():
            if not str(name):
                raise RuntimeStateError("TES4 runtime-state custom-state key is empty")
            _write_value(_Writer(), value)
            if name == "obscript.look_target":
                parts = value.split(":") if isinstance(value, str) else []
                valid = len(parts) == 3 and parts[0] in ("content", "dynamic") and bool(parts[1])
                if valid:
                    kind, namespace, number = parts
                    valid = len(number) == (6 if kind == "content" else 16) and all(
                        char in "0123456789abcdef" for char in number
                    ) and int(number, 16) != 0
                    if kind == "content":
                        valid = valid and "/" not in namespace and not any("A" <= c <= "Z" for c in namespace)
                if not valid:
                    raise RuntimeStateError("Invalid TES4 scripted Look target")

    native_actors = check_collection(state.get("native_actor_values", []), "native actor-value list")
    if version < 9 and native_actors:
        raise RuntimeStateError("TES4 runtime-state versions before 9 cannot contain native actor values")
    native_keys: set[str] = set()
    bases = {reference["key"]: reference["base"] for reference in references}

    def native_float(value: Any) -> float:
        if type(value) not in (int, float):
            raise RuntimeStateError("Invalid TES4 native actor-value number")
        try:
            result = struct.unpack("<f", struct.pack("<f", value))[0]
        except (OverflowError, struct.error) as error:
            raise RuntimeStateError("TES4 native actor-value overflow") from error
        if not math.isfinite(result):
            raise RuntimeStateError("Nonfinite TES4 native actor value")
        return result

    def native_key(identity: Any) -> None:
        parts = identity.split(":") if isinstance(identity, str) else []
        valid = len(parts) == 3 and parts[0] in ("content", "dynamic") and bool(parts[1])
        if valid:
            kind, namespace, number = parts
            valid = len(number) == (6 if kind == "content" else 16) and all(
                char in "0123456789abcdef" for char in number
            ) and int(number, 16) != 0
            if kind == "content":
                valid = valid and "/" not in namespace and not any("A" <= c <= "Z" for c in namespace)
        if not valid:
            raise RuntimeStateError("Invalid TES4 native actor-value identity")

    native_bases = check_collection(state.get("native_actor_bases", []), "native actor-base list")
    if version < 11 and native_bases:
        raise RuntimeStateError("TES4 native actor base overrides require version 11")
    seen_bases: set[str] = set()
    for base_override in native_bases:
        if not isinstance(base_override, dict):
            raise RuntimeStateError("Invalid TES4 native actor base override")
        key, kind = base_override.get("base"), base_override.get("kind")
        native_key(key)
        if key in seen_bases or type(kind) is not int or kind not in (0, 1):
            raise RuntimeStateError("Invalid or duplicate TES4 native actor base override")
        seen_bases.add(key)
        values = check_collection(base_override.get("values"), "native actor base values")
        if not 1 <= len(values) <= 72:
            raise RuntimeStateError("Invalid TES4 native actor base value count")
        seen_values: set[int] = set()
        for entry in values:
            if not isinstance(entry, list) or len(entry) != 3:
                raise RuntimeStateError("Invalid TES4 native actor base value")
            av, storage, value = entry
            if type(av) is not int or not 0 <= av < 72 or av == 11 or 37 <= av <= 39 or av in seen_values:
                raise RuntimeStateError("Invalid or duplicate TES4 native actor base value")
            seen_values.add(av)
            if kind == 1 and 12 <= av <= 32 and av not in (12, 19, 26):
                raise RuntimeStateError("Noncanonical TES4 creature base skill")
            if type(storage) is not int or storage != int(av >= 40):
                raise RuntimeStateError("Invalid TES4 native actor base storage type")
            if storage == 0:
                maximum = (1 << 31) - 1 if av == 8 else 65535 if av in (9, 10) else 255
                minimum = -(1 << 31) if av == 8 else 0
                if type(value) is not int or not minimum <= value <= maximum:
                    raise RuntimeStateError("TES4 native actor base exceeds storage width")
            else:
                native_float(value)

    for actor in native_actors:
        if not isinstance(actor, dict):
            raise RuntimeStateError("Invalid TES4 native actor values")
        key, base = actor.get("actor"), actor.get("base")
        native_key(key)
        native_key(base)
        if key in native_keys:
            raise RuntimeStateError("Duplicate TES4 native actor-value identity")
        native_keys.add(key)
        owner, process = actor.get("owner"), actor.get("process")
        if any(type(v) is not int or v not in (0, 1) for v in (owner, process)):
            raise RuntimeStateError("Invalid TES4 native actor-value owner/process")
        is_player = key == player["reference"]
        if (owner == 0) != is_player:
            raise RuntimeStateError("TES4 native actor-value player identity mismatch")
        if not is_player and bases.get(key) != base:
            raise RuntimeStateError("Dangling or mismatched TES4 native actor-value reference")
        action = actor.get("process_action")
        if action is not None and (version < 26 or process != 1 or type(action) is not int
                                  or not -32768 <= action <= 32767):
            raise RuntimeStateError("TES4 native action code requires Active process, signed int16 and version 26")
        knocked = actor.get("process_knocked_state")
        if knocked is not None and (version < 25 or process != 1 or type(knocked) is not int
                                   or not -128 <= knocked <= 127):
            raise RuntimeStateError("TES4 native knocked byte requires Active process, signed int8 and version 25")
        form_values = actor.get("player_form_values")
        if form_values is not None:
            if version < 10 or owner != 0:
                raise RuntimeStateError("TES4 player form values require player ownership and version 10")
            if not isinstance(form_values, list) or len(form_values) != 4 or any(
                type(value) is not int or not -(1 << 31) <= value < (1 << 31) for value in form_values
            ):
                raise RuntimeStateError("Invalid TES4 player form values")
        values = actor.get("values")
        if not isinstance(values, list) or len(values) != 72:
            raise RuntimeStateError("TES4 native actor values require 72 entries")
        form_health = actor.get("nonplayer_form_health")
        if form_health is not None:
            if version < 17 or owner != 1 or type(form_health) is not int or not -(1 << 31) <= form_health < (1 << 31):
                raise RuntimeStateError("TES4 nonplayer form Health requires nonplayer ownership, int32 and version 17")
            expected_base = struct.unpack("<f", struct.pack("<f", form_health))[0]
            if not isinstance(values[8], list) or len(values[8]) != 4 or native_float(values[8][0]) != expected_base:
                raise RuntimeStateError("TES4 nonplayer form Health conflicts with resolved float base")
        abilities = actor.get("passive_abilities")
        if abilities is not None:
            if version < 18:
                raise RuntimeStateError("TES4 passive ability ownership requires version 18")
            seen_spells = set()
            codes = {int.from_bytes(code.encode("ascii"), "little") for code in
                ("WABR", "WKFI", "WKFR", "WKSH", "WKMA", "FOSP", "SABS", "STMA",
                 "FOAT", "RSFI", "RSPO", "RSDI", "RSMA", "RSFR")}
            for ability in check_collection(abilities, "passive ability ownership list"):
                if not isinstance(ability, dict):
                    raise RuntimeStateError("Invalid TES4 passive ability")
                spell = ability.get("spell")
                native_key(spell)
                if spell in seen_spells:
                    raise RuntimeStateError("Duplicate TES4 passive ability ownership")
                seen_spells.add(spell)
                effects = check_collection(ability.get("effects"), "passive ability effect list")
                if not effects:
                    raise RuntimeStateError("Empty TES4 passive ability effects")
                seen_indices = set()
                for effect in effects:
                    if not isinstance(effect, list) or len(effect) != (5 if version >= 19 else 4):
                        raise RuntimeStateError("Invalid TES4 passive value-modifier ownership")
                    index, code, av, magnitude = effect[:4]
                    if (type(index) is not int or not 0 <= index < (1 << 32) or index in seen_indices
                        or type(code) is not int or code not in codes
                        or type(av) is not int or not 0 <= av < 72):
                        raise RuntimeStateError("Invalid TES4 passive value-modifier ownership")
                    native_float(magnitude)
                    if version >= 19 and effect[4] is not None:
                        native_float(effect[4])
                    seen_indices.add(index)
        for value in values:
            if not isinstance(value, list) or len(value) != 4:
                raise RuntimeStateError("Invalid TES4 native actor-value categories")
            base_value = native_float(value[0])
            maximum, script, damage = [0.0 if v is None else native_float(v) for v in value[1:]]
            if owner == 0:
                native_float(base_value + maximum + script + damage)
            else:
                low = native_float(base_value + script + damage)
                if process == 1:
                    native_float(low + maximum)

    manager_time = native_float(state.get("native_actor_manager_time", 0))
    update_times = check_collection(state.get("native_actor_update_times", []), "native actor update time list")
    if version < 16 and (manager_time != 0 or update_times):
        raise RuntimeStateError("TES4 native actor clocks require version 16")
    if manager_time > 100000:
        raise RuntimeStateError("Invalid TES4 native actor manager time")
    clock_keys = set()
    for entry in update_times:
        if not isinstance(entry, dict) or set(entry) != {"actor", "time"}:
            raise RuntimeStateError("Invalid TES4 native actor update time")
        native_key(entry["actor"])
        if entry["actor"] not in native_keys or entry["actor"] in clock_keys:
            raise RuntimeStateError("Duplicate or dangling TES4 native actor update time")
        if native_float(entry["time"]) > 100000:
            raise RuntimeStateError("Invalid TES4 native actor update time")
        clock_keys.add(entry["actor"])

    breath = check_collection(state.get("native_actor_breath", []), "native actor breath list")
    if version < 14 and breath:
        raise RuntimeStateError("TES4 native actor breath requires version 14")
    breath_keys = set()
    for entry in breath:
        if not isinstance(entry, dict) or set(entry) != {"actor", "remaining"}:
            raise RuntimeStateError("Invalid TES4 native actor breath")
        native_key(entry["actor"])
        if entry["actor"] not in native_keys or entry["actor"] in breath_keys:
            raise RuntimeStateError("Duplicate or dangling TES4 native actor breath")
        native_float(entry["remaining"])
        breath_keys.add(entry["actor"])

    death_counts = check_collection(state.get("native_death_counts", []), "native death count list")
    if version < 13 and death_counts:
        raise RuntimeStateError("TES4 native death counts require version 13")
    counted_bases = set()
    for entry in death_counts:
        if not isinstance(entry, dict) or set(entry) != {"base", "count"}:
            raise RuntimeStateError("Invalid TES4 native death count")
        native_key(entry["base"])
        if entry["base"] in counted_bases or type(entry["count"]) is not int or not 0 <= entry["count"] <= 65535:
            raise RuntimeStateError("Invalid or duplicate TES4 native death count")
        counted_bases.add(entry["base"])

    lives = check_collection(state.get("native_actor_life", []), "native actor life list")
    death_events = check_collection(state.get("pending_death_events", []), "pending death event list")
    next_death = state.get("next_death_event", 1)
    if version < 12 and (lives or death_events or next_death != 1):
        raise RuntimeStateError("TES4 native actor lifecycle requires version 12")
    if type(next_death) is not int or not 1 <= next_death < 1 << 64:
        raise RuntimeStateError("Invalid TES4 next death event identity")
    value_bases = {actor["actor"]: actor["base"] for actor in native_actors}
    legacy_refs = {reference["key"]: reference for reference in references}
    life_keys: set[str] = set()

    def life_source(key: Any) -> None:
        if key == "null":
            return
        native_key(key)
        if key != player["reference"] and key not in bases:
            raise RuntimeStateError("Dangling TES4 native life source")

    for life in lives:
        if not isinstance(life, dict):
            raise RuntimeStateError("Invalid TES4 native actor life state")
        key, base = life.get("actor"), life.get("base")
        native_key(key)
        native_key(base)
        if key in life_keys:
            raise RuntimeStateError("Duplicate TES4 native actor life identity")
        life_keys.add(key)
        if (key != player["reference"] and bases.get(key) != base) or value_bases.get(key, base) != base:
            raise RuntimeStateError("Dangling or mismatched TES4 native actor life reference")
        phase, remaining, killer = life.get("phase"), life.get("recovery_remaining"), life.get("killer")
        life_source(killer)
        if type(phase) is not int or phase not in (0, 1, 2):
            raise RuntimeStateError("Invalid TES4 native actor life phase")
        remaining = native_float(remaining)
        if remaining < 0 or (phase != 2 and remaining != 0) or (phase == 0 and killer != "null"):
            raise RuntimeStateError("Invalid TES4 native actor life timer or killer")
        custom = legacy_refs.get(key, {}).get("custom_state", {})
        if "obscript.dead" in custom:
            old = custom["obscript.dead"]
            if type(old) is not bool or old != (phase == 1):
                raise RuntimeStateError("TES4 native actor life conflicts with legacy obscript.dead")
    for reference in references:
        draw = reference.get("actor_draw_state")
        if draw is None:
            continue
        if version < 29:
            raise RuntimeStateError("TES4 native actor draw state requires version 29")
        if type(draw) is not int or draw not in (0, 1, 2):
            raise RuntimeStateError("Invalid TES4 native actor draw state")
        key, base = reference["key"], reference["base"]
        life = next((item for item in lives if item["actor"] == key), None)
        if (key == player["reference"] or value_bases.get(key) != base or
                life is None or life["base"] != base):
            raise RuntimeStateError("Dangling or mismatched TES4 native actor draw state owner")

    engagements = check_collection(state.get("native_combat_engagements", []), "native combat engagement list")
    if version < 15 and engagements:
        raise RuntimeStateError("TES4 native combat engagements require version 15")
    phases = {life["actor"]: life["phase"] for life in lives}
    seen_engagements: set[tuple[str, str]] = set()
    for engagement in engagements:
        if not isinstance(engagement, dict) or set(engagement) != {"first", "second"}:
            raise RuntimeStateError("Invalid TES4 combat engagement")
        first, second = engagement["first"], engagement["second"]
        native_key(first)
        native_key(second)
        pair = (first, second)
        if (first >= second or pair in seen_engagements or
                any(actor not in native_keys or actor not in phases or phases[actor] == 1 for actor in pair)):
            raise RuntimeStateError("Invalid, duplicate, dangling or terminal TES4 combat engagement")
        seen_engagements.add(pair)
    action_owners = check_collection(state.get("physical_action_owners", []), "physical action owner list")
    if version < 20 and action_owners:
        raise RuntimeStateError("TES4 physical action owners require version 20")
    owned_ids: set[int] = set()
    for entry in action_owners:
        if not isinstance(entry, dict) or set(entry) != {"id", "actor"}:
            raise RuntimeStateError("Invalid TES4 physical action owner")
        identity, actor = entry["id"], entry["actor"]
        native_key(actor)
        if (type(identity) is not int or identity not in seen_actions or identity in owned_ids
                or actor not in native_keys or phases.get(actor) != 0):
            raise RuntimeStateError("Invalid, duplicate, dangling or incapacitated TES4 physical action owner")
        owned_ids.add(identity)
    clocks = check_collection(state.get("native_animation_clocks", []), "native animation clocks")
    if version < 23 and clocks:
        raise RuntimeStateError("TES4 animation clocks require version23")
    clock_actors: set[str] = set()
    for entry in clocks:
        if not isinstance(entry, dict) or set(entry) != {"actor", "clock"}:
            raise RuntimeStateError("Invalid TES4 native animation clock")
        actor = entry["actor"]
        native_key(actor)
        if actor in clock_actors or actor not in native_keys or actor not in phases or native_float(entry["clock"]) < 0:
            raise RuntimeStateError("Duplicate, dangling or invalid TES4 animation clock")
        clock_actors.add(actor)
    pulses = check_collection(state.get("native_actor_knockback", []), "native actor knockback list")
    if version < 30 and pulses:
        raise RuntimeStateError("TES4 actor knockback requires version30")
    pulse_actors: set[str] = set()
    for entry in pulses:
        if not isinstance(entry, dict) or set(entry) != {"actor", "acceleration", "remaining"}:
            raise RuntimeStateError("Invalid TES4 native actor knockback")
        actor = entry["actor"]
        native_key(actor)
        if actor in pulse_actors or actor not in native_keys or actor not in phases:
            raise RuntimeStateError("Duplicate or dangling TES4 native actor knockback")
        vector = entry["acceleration"]
        if not isinstance(vector, list) or len(vector) != 3:
            raise RuntimeStateError("Invalid TES4 native actor knockback vector")
        for component in vector:
            native_float(component)
        if native_float(entry["remaining"]) < 0:
            raise RuntimeStateError("Negative TES4 native actor knockback timer")
        pulse_actors.add(actor)
    if "native_player_bow_timer" in state:
        if version < 37:
            raise RuntimeStateError("TES4 Player bow timer requires version37")
        native_float(state["native_player_bow_timer"])
    if "native_physical_blend_time_cache" in state:
        cache = state["native_physical_blend_time_cache"]
        if version < 36 or not isinstance(cache, dict) or set(cache) != {"cycle", "stop_key", "start_key", "key_time", "result"}:
            raise RuntimeStateError("Invalid TES4 native physical cache version or shape")
        if type(cache["cycle"]) is not int or not 0 <= cache["cycle"] <= 0xffffffff:
            raise RuntimeStateError("Invalid TES4 native physical cache cycle")
        for field in ("stop_key", "start_key", "key_time", "result"):
            native_float(cache[field])

    ragdolls = check_collection(state.get("native_actor_ragdolls", []), "native actor ragdoll list")
    if version < 31 and ragdolls:
        raise RuntimeStateError("TES4 actor ragdolls require version31")
    ragdoll_actors: set[str] = set()
    for entry in ragdolls:
        fields = {"actor", "base", "model", "asset_hash", "bodies"}
        if (not isinstance(entry, dict) or not fields <= set(entry)
                or not set(entry) <= fields | {"native_blends", "native_controllers"}):
            raise RuntimeStateError("Invalid TES4 ragdoll snapshot")
        actor, base = entry["actor"], entry["base"]
        native_key(actor)
        native_key(base)
        if actor in ragdoll_actors or actor not in native_keys or actor not in phases or value_bases.get(actor) != base:
            raise RuntimeStateError("Duplicate, dangling or mismatched TES4 ragdoll owner")
        ragdoll_actors.add(actor)
        model = entry["model"]
        if (not isinstance(model, str) or not model or len(model) > 4096
                or any(ord(c) < 32 or ord(c) >= 127 or c in "\\:" or "A" <= c <= "Z" for c in model)
                or any(component in ("", ".", "..") for component in model.split("/"))):
            raise RuntimeStateError("Noncanonical TES4 ragdoll model path")
        digest = entry["asset_hash"]
        if not isinstance(digest, str) or len(digest) != 32 or any(c not in "0123456789abcdef" for c in digest):
            raise RuntimeStateError("Invalid TES4 ragdoll asset hash")
        bodies = check_collection(entry["bodies"], "native ragdoll bodies")
        if not bodies:
            raise RuntimeStateError("Empty TES4 ragdoll snapshot")
        previous = -1
        nodes: set[int] = set()
        for body in bodies:
            fields = {"record", "node_record", "rotation", "position", "linear_velocity", "angular_velocity"}
            if (not isinstance(body, dict) or not fields <= set(body)
                    or not set(body) <= fields | {"native_packed_velocity", "native_motion"}):
                raise RuntimeStateError("Invalid TES4 ragdoll body")
            packed = body.get("native_packed_velocity")
            if "native_packed_velocity" in body:
                if version < 32 or not isinstance(packed, dict) or set(packed) != {"linear", "angular"}:
                    raise RuntimeStateError("Invalid TES4 packed ragdoll velocity or version")
                for vector in packed.values():
                    if not isinstance(vector, list) or len(vector) != 4:
                        raise RuntimeStateError("Invalid TES4 packed ragdoll lane count")
                    for value in vector:
                        native_float(value)
            if ("native_packed_velocity" in body) != ("native_packed_velocity" in bodies[0]):
                raise RuntimeStateError("Incomplete TES4 packed ragdoll snapshot")
            if "native_motion" in body:
                motion = body["native_motion"]
                if version < 33 or type(motion) is not int or motion not in (1, 6) or "native_packed_velocity" not in body:
                    raise RuntimeStateError("Invalid TES4 logical ragdoll motion or version")
            if ("native_motion" in body) != ("native_motion" in bodies[0]):
                raise RuntimeStateError("Incomplete TES4 logical ragdoll motion snapshot")
            record, node = body["record"], body["node_record"]
            if (type(record) is not int or type(node) is not int or not previous < record <= 0x7fffffff
                    or not 0 <= node <= 0x7fffffff or node in nodes):
                raise RuntimeStateError("Invalid TES4 ragdoll body identity or order")
            previous = record
            nodes.add(node)
            for field, size in [("rotation", 9), ("position", 3), ("linear_velocity", 3), ("angular_velocity", 3)]:
                vector = body[field]
                if not isinstance(vector, list) or len(vector) != size:
                    raise RuntimeStateError("Invalid TES4 ragdoll pose or velocity")
                for value in vector:
                    native_float(value)
            rotation = [native_float(value) for value in body["rotation"]]
            for row in range(3):
                for other in range(row, 3):
                    dot = sum(rotation[row*3+c] * rotation[other*3+c] for c in range(3))
                    if abs(dot - (1. if row == other else 0.)) > 1e-4:
                        raise RuntimeStateError("Nonrigid TES4 ragdoll rotation")
            r = rotation
            determinant = r[0]*(r[4]*r[8]-r[5]*r[7])-r[1]*(r[3]*r[8]-r[5]*r[6])+r[2]*(r[3]*r[7]-r[4]*r[6])
            if abs(determinant - 1.) > 1e-4:
                raise RuntimeStateError("Improper TES4 ragdoll rotation")
        if "native_blends" in entry:
            blends = check_collection(entry["native_blends"], "native ragdoll blends")
            if (version < 34 or len(blends) > len(bodies)
                    or "native_packed_velocity" not in bodies[0] or "native_motion" not in bodies[0]):
                raise RuntimeStateError("Invalid TES4 native blend version or incomplete body snapshot")
            body_records = {body["record"] for body in bodies}
            previous_blend = -1
            for blend in blends:
                if not isinstance(blend, dict) or set(blend) != {
                        "body_record", "collision_flags", "requested_motion", "hierarchy_gain", "velocity_gain"}:
                    raise RuntimeStateError("Invalid TES4 native blend snapshot")
                record, flags, request = blend["body_record"], blend["collision_flags"], blend["requested_motion"]
                if (type(record) is not int or record not in body_records or record <= previous_blend
                        or type(flags) is not int or not 0 <= flags <= 0xffff
                        or type(request) is not int or not 0 <= request <= 0xffffffff):
                    raise RuntimeStateError("Invalid TES4 native blend identity, order or flags/request")
                previous_blend = record
                native_float(blend["hierarchy_gain"])
                native_float(blend["velocity_gain"])
        if "native_controllers" in entry:
            controls = entry["native_controllers"]
            if (version < 35 or "native_blends" not in entry or "native_motion" not in bodies[0]
                    or "native_packed_velocity" not in bodies[0] or not isinstance(controls, dict)
                    or set(controls) != {"blends", "velocities"}):
                raise RuntimeStateError("Invalid TES4 controller version, shape or incomplete body snapshot")
            def common_controller(controller, fields, state_fields):
                if not isinstance(controller, dict) or set(controller) != fields:
                    raise RuntimeStateError("Invalid TES4 controller fields")
                attached, target = controller["attached_node"], controller["target_node"]
                if (type(attached) is not int or attached not in nodes
                        or (target is not None and (type(target) is not int or target not in nodes))):
                    raise RuntimeStateError("Invalid TES4 controller attachment or target")
                value = controller["state"]
                if not isinstance(value, dict) or set(value) != state_fields:
                    raise RuntimeStateError("Invalid TES4 controller state fields")
                timing, clock = value["timing"], value["clock"]
                if (not isinstance(timing, dict) or set(timing) != {"flags", "frequency", "phase", "start_key", "stop_key"}
                        or type(timing["flags"]) is not int or not 0 <= timing["flags"] <= 0xffff
                        or not isinstance(clock, dict) or set(clock) != {"start_time", "previous_time", "elapsed"}):
                    raise RuntimeStateError("Invalid TES4 controller timing or clock fields")
                for field in ("frequency", "phase", "start_key", "stop_key"): native_float(timing[field])
                for field in ("start_time", "previous_time", "elapsed"): native_float(clock[field])
                return value
            authored = check_collection(controls["blends"], "native authored controllers")
            generated = check_collection(controls["velocities"], "native generated controllers")
            if len(authored) > len(bodies) or len(generated) > len(bodies):
                raise RuntimeStateError("Excessive TES4 owned controller count")
            previous_controller = -1
            attachments = set()
            for controller in authored:
                value = common_controller(controller, {"record", "attached_node", "target_node", "state"},
                    {"timing", "clock", "keys", "cursor", "cached_gains", "setup_state"})
                record = controller["record"]
                if (type(record) is not int or not previous_controller < record <= 0x7fffffff
                        or controller["attached_node"] in attachments):
                    raise RuntimeStateError("Invalid TES4 authored controller identity or order")
                previous_controller = record; attachments.add(controller["attached_node"])
                for field in ("cursor", "setup_state"):
                    if type(value[field]) is not int or not 0 <= value[field] <= 0xffffffff:
                        raise RuntimeStateError("Invalid TES4 authored controller cursor or setup")
                cached = value["cached_gains"]
                if not isinstance(cached, dict) or set(cached) != {"hierarchy", "velocity"}:
                    raise RuntimeStateError("Invalid TES4 authored controller cached gains")
                native_float(cached["hierarchy"]); native_float(cached["velocity"])
                keys = check_collection(value["keys"], "native authored controller keys")
                if len(keys) >= 2 and value["cursor"] >= len(keys) - 1:
                    raise RuntimeStateError("Invalid TES4 authored controller cursor")
                previous_key = None
                for key in keys:
                    if not isinstance(key, dict) or set(key) != {"time", "hierarchy_gain", "velocity_gain"}:
                        raise RuntimeStateError("Invalid TES4 authored controller key fields")
                    time = native_float(key["time"]); native_float(key["hierarchy_gain"]); native_float(key["velocity_gain"])
                    if previous_key is not None and previous_key > time:
                        raise RuntimeStateError("Invalid TES4 authored controller key order")
                    previous_key = time
            previous_controller = -1
            for controller in generated:
                value = common_controller(controller, {"attached_node", "target_node", "precedes_blend", "state"},
                    {"timing", "clock", "force_vector", "frame_delta"})
                if type(controller["precedes_blend"]) is not bool or controller["attached_node"] <= previous_controller:
                    raise RuntimeStateError("Invalid TES4 generated controller order or position")
                previous_controller = controller["attached_node"]
                if (native_float(value["timing"]["start_key"]) > native_float(value["timing"]["stop_key"])
                        or native_float(value["frame_delta"]) < 0):
                    raise RuntimeStateError("Invalid TES4 generated controller timing or delta")
                force = value["force_vector"]
                if not isinstance(force, list) or len(force) != 4:
                    raise RuntimeStateError("Invalid TES4 generated controller force lanes")
                for component in force: native_float(component)
    melee_states = check_collection(state.get("native_melee_states", []), "native melee state list")
    if version < 21 and melee_states:
        raise RuntimeStateError("TES4 melee state requires version 21")
    melee_actors: set[str] = set()
    melee_ids: set[int] = set()
    owner_map = {entry["id"]: entry["actor"] for entry in action_owners}
    for entry in melee_states:
        fields = {"actor", "input", "strike"}
        if version >= 28:
            fields.add("ai_intent")
        if not isinstance(entry, dict) or set(entry) != fields:
            raise RuntimeStateError("Invalid TES4 melee state")
        actor = entry["actor"]
        native_key(actor)
        if actor in melee_actors or actor not in native_keys or phases.get(actor) != 0:
            raise RuntimeStateError("Duplicate, dangling or incapacitated TES4 melee owner")
        melee_actors.add(actor)
        intent = entry.get("ai_intent")
        if intent is not None:
            if not isinstance(intent, dict) or set(intent) != {"target", "style"}:
                raise RuntimeStateError("Invalid TES4 melee AI intent")
            target, style = intent["target"], intent["style"]
            native_key(target)
            native_key(style)
            if (actor == state["player"]["reference"] or target == actor
                    or target not in native_keys or phases.get(target) != 0
                    or tuple(sorted((actor, target))) not in seen_engagements):
                raise RuntimeStateError("Dangling or invalid TES4 melee AI target")
        control = entry["input"]
        if not isinstance(control, dict) or set(control) != {"held_seconds", "input_held", "prefer_left", "queued"}:
            raise RuntimeStateError("Invalid TES4 melee input")
        if (native_float(control["held_seconds"]) < 0 or type(control["input_held"]) is not bool
                or type(control["prefer_left"]) is not bool or type(control["queued"]) is not int
                or not 0 <= control["queued"] <= 2):
            raise RuntimeStateError("Invalid TES4 melee input state")
        strike = entry["strike"]
        if strike is None:
            continue
        strike_fields = {"id", "kind", "weapon_base", "animation_group", "playback_speed", "animation_time", "contact_committed"}
        if version >= 22:
            strike_fields.add("ordinary_phase")
        if version >= 23:
            strike_fields.add("sequence_timing")
        if not isinstance(strike, dict) or set(strike) != strike_fields:
            raise RuntimeStateError("Invalid TES4 melee strike")
        if version >= 22 and (type(strike["ordinary_phase"]) is not int or not 0 <= strike["ordinary_phase"] <= 3):
            raise RuntimeStateError("Invalid TES4 ordinary melee phase")
        if version >= 23 and strike["sequence_timing"] is not None:
            timing = strike["sequence_timing"]
            if (actor not in clock_actors or not isinstance(timing, dict)
                    or set(timing) != {"easing", "offset", "ease_start", "last_input", "ease_end", "weighted_time", "output_time"}
                    or type(timing["easing"]) is not bool):
                raise RuntimeStateError("Invalid TES4 melee sequence timing or missing actor clock")
            optional = [timing[k] for k in ("offset", "ease_start", "last_input")]
            if any(v is None for v in optional) and not all(v is None for v in optional):
                raise RuntimeStateError("Partial TES4 melee sequence initialization")
            for value in optional:
                if value is not None:
                    native_float(value)
            for key in ("ease_end", "weighted_time", "output_time"):
                native_float(timing[key])
        identity, kind, committed = strike["id"], strike["kind"], strike["contact_committed"]
        if (type(identity) is not int or not 0 < identity < next_action or identity in melee_ids
                or type(kind) is not int or not 0 <= kind <= 6 or type(committed) is not bool
                or native_float(strike["animation_time"]) < 0 or native_float(strike["playback_speed"]) <= 0):
            raise RuntimeStateError("Invalid or duplicate TES4 melee strike state")
        group = strike["animation_group"]
        if not isinstance(group, str) or not group or "\0" in group or len(group.encode("utf-8")) > MAX_STRING:
            raise RuntimeStateError("Invalid TES4 melee animation group")
        if strike["weapon_base"] != "null":
            native_key(strike["weapon_base"])
        if (committed and identity in seen_actions) or (not committed and owner_map.get(identity) != actor):
            raise RuntimeStateError("Replaying or unowned TES4 melee action")
        melee_ids.add(identity)
    bows = check_collection(state.get("native_bow_states", []), "native bow state list")
    if version < 38 and bows:
        raise RuntimeStateError("TES4 owned bow playback requires version38")
    bow_actors = set()
    strike_actors = {entry["actor"] for entry in melee_states if entry["strike"] is not None}
    for bow in bows:
        fields = {"actor", "id", "bow_base", "ammo_base", "animation_group",
                  "playback_rate", "phase", "sequence_offset", "key_times", "action", "release_committed"}
        if version >= 39:
            fields.add("player_hold_latched")
        if not isinstance(bow, dict) or set(bow) != fields:
            raise RuntimeStateError("Invalid TES4 bow state")
        actor, identity = bow["actor"], bow["id"]
        if version >= 39 and (type(bow["player_hold_latched"]) is not bool
                or (bow["player_hold_latched"] and actor != state["player"]["reference"])):
            raise RuntimeStateError("Invalid native Player bow hold latch ownership")
        for field in ("actor", "bow_base", "ammo_base"):
            native_key(bow[field])
        phase, action, committed = bow["phase"], bow["action"], bow["release_committed"]
        if (actor in bow_actors or actor not in native_keys or phases.get(actor) != 0
                or actor not in clock_actors or actor in strike_actors
                or type(identity) is not int or not 0 < identity < next_action or identity in melee_ids
                or type(phase) is not int or not 0 <= phase <= 4
                or type(action) is not int or type(committed) is not bool):
            raise RuntimeStateError("Invalid TES4 bow owner, identity or phase")
        if (committed and (action != 3 or phase < 3 or identity in seen_actions)
                or not committed and (owner_map.get(identity) != actor
                    or (action == 4 and phase > 1)
                    or (action != 4 and (action != 5 or not 1 <= phase <= 3)))):
            raise RuntimeStateError("Incoherent or replaying TES4 bow action")
        group = bow["animation_group"]
        if not isinstance(group, str) or not group or "\0" in group or len(group.encode("utf-8")) > MAX_STRING:
            raise RuntimeStateError("Invalid TES4 bow animation group")
        native_float(bow["playback_rate"]); native_float(bow["sequence_offset"])
        times = bow["key_times"]
        if not isinstance(times, list) or len(times) != 5:
            raise RuntimeStateError("Invalid TES4 bow key sequence")
        times = [native_float(time) for time in times]
        if times[0] < 0 or any(a > b for a, b in zip(times, times[1:])) or times[-1] <= times[0]:
            raise RuntimeStateError("Invalid TES4 supported bow key order")
        bow_actors.add(actor); melee_ids.add(identity)
    previous = 0
    for event in death_events:
        if not isinstance(event, dict):
            raise RuntimeStateError("Invalid TES4 pending death event")
        identity, actor = event.get("id"), event.get("actor")
        if type(identity) is not int or not previous < identity < next_death or actor not in life_keys:
            raise RuntimeStateError("Invalid TES4 pending death event identity, order or reference")
        life_source(event.get("killer"))
        previous = identity

    scripts = check_collection(state.get("script_instances", []), "script instance list")
    quests = check_collection(state.get("quests", []), "quest list")
    if version < 2 and (state.get("script_event_sequence", 0) != 0 or scripts or quests):
        raise RuntimeStateError("TES4 runtime-state version 1 cannot contain ObScript state")
    script_keys: set[tuple[str, str]] = set()
    for script in scripts:
        identity = (str(script.get("unit", "")), str(script.get("context", "null")))
        if not identity[0] or identity[1] == "null" or identity in script_keys:
            raise RuntimeStateError("Invalid or duplicate TES4 runtime-state script instance")
        script_keys.add(identity)
        locals_ = script.get("locals", [])
        if not isinstance(locals_, list) or len(locals_) > MAX_COLLECTION:
            raise RuntimeStateError("TES4 runtime-state script local list exceeds the size limit")
        for value in locals_:
            _write_script_value(_Writer(), value)
    quest_keys: set[str] = set()
    for quest in quests:
        key = str(quest.get("quest", "null"))
        if key == "null" or key in quest_keys:
            raise RuntimeStateError("Invalid or duplicate TES4 runtime-state quest")
        quest_keys.add(key)
        completed = quest.get("completed_stages", [])
        try:
            if not isinstance(completed, list):
                raise ValueError
            # encode_payload canonicalizes this list, while decode_payload
            # rejects a non-canonical wire representation just like C++.
            [int(value) for value in completed]
        except (TypeError, ValueError, OverflowError) as error:
            raise RuntimeStateError("Invalid TES4 runtime-state quest stages") from error

    if version >= 5:
        try:
            if int(state.get("ai_rng_state", 0)) == 0:
                raise RuntimeStateError("TES4 runtime-state AI RNG state must be non-zero")
        except (TypeError, ValueError, OverflowError) as error:
            raise RuntimeStateError("Invalid TES4 runtime-state AI RNG state") from error
        _validate_ai(state)


def _write_actor_ai(writer: _Writer, actor: dict[str, Any], version: int) -> None:
    for key in (
        "actor", "base", "package", "script_package", "target", "target_base", "cell", "pathgrid", "door",
        "destination_cell", "last_valid_cell", "action_item", "last_transition_door", "companion_group",
        "companion_side_with", "mount", "rider",
    ):
        writer.string(str(actor.get(key, "null")))
    schedule_window = actor.get("schedule_window")
    if schedule_window is None:
        writer.pack("<B", 0)
    else:
        writer.pack("<B", 1)
        _write_calendar(writer, schedule_window["start"])
        _write_calendar(writer, schedule_window["end"])
        duration = float(schedule_window.get("duration_hours", 0.0))
        if not math.isfinite(duration) or duration < 0.0:
            raise RuntimeStateError("Invalid TES4 runtime-state actor schedule window")
        writer.pack("<d", duration)
    condition_result = int(actor.get("condition_result", 0))
    if not 0 <= condition_result <= 3:
        raise RuntimeStateError("Invalid TES4 runtime-state actor condition result")
    writer.pack("<B", condition_result)
    _write_position(writer, actor.get("destination_position", [0.0] * 6))
    _write_position(writer, actor.get("last_valid_position", [0.0] * 6))
    writer.pack("<B", int(actor.get("source", 0)))
    writer.pack("<B", int(actor.get("package_type", 255)))
    writer.pack("<H", int(actor.get("procedure", 0)))
    writer.pack("<B", int(actor.get("phase", 0)))
    writer.pack("<B", int(actor.get("tier", 0)))
    writer.pack("<B", int(actor.get("boundary", 0)))
    writer.pack("<I", int(actor.get("list_index", 0)))
    writer.pack("<I", int(actor.get("path_node", 0)))
    writer.pack("<I", int(actor.get("repath_attempts", 0)))
    writer.pack("<i", int(actor.get("formation_index", -1)))
    writer.pack("<Q", int(actor.get("selection_generation", 0)))
    writer.pack("<Q", int(actor.get("route_generation", 0)))
    writer.pack("<Q", int(actor.get("transition_generation", 0)))
    for key in (
        "action_timer", "duration_remaining", "no_progress_seconds", "door_cooldown", "low_process_timer",
        "next_low_process_tick",
    ):
        value = float(actor.get(key, 0.0))
        if not math.isfinite(value) or value < 0.0:
            raise RuntimeStateError("Invalid TES4 actor AI timer")
        writer.pack("<f", value)
    writer.pack("<B", int(bool(actor.get("restrained", False))))
    writer.pack("<B", int(bool(actor.get("action_reserved", False))))
    writer.pack("<B", int(bool(actor.get("has_destination", False))))
    if version >= 7:
        writer.pack("<B", int(bool(actor.get("door_animation_started", False))))
    writer.string(str(actor.get("interruption_reason", "")))


def _validate_inventory(value: list[dict[str, Any]], version: int, actor: bool) -> None:
    occupied_slots = 0
    occupied_hotkeys = 0
    metadata = {
        "condition", "charge", "equipped_slots", "hotkey", "owner", "remaining_usage_time"
    }
    for item in value:
        count = int(item.get("count", 0))
        if str(item.get("base", "null")) == "null" or count == 0 or (version >= 4 and count < 0):
            raise RuntimeStateError("Invalid TES4 runtime-state inventory entry")
        if version < 4:
            if metadata.intersection(item):
                raise RuntimeStateError("TES4 runtime-state version 1/2/3 cannot contain M13 item state")
            continue
        condition = float(item.get("condition", -1))
        charge = float(item.get("charge", -1.0))
        usage = float(item.get("remaining_usage_time", -1.0))
        slots = int(item.get("equipped_slots", 0))
        hotkey = int(item.get("hotkey", -1))
        if (not math.isfinite(condition) or condition > 3.4028234663852886e38
                or isinstance(item.get("condition"), bool) or (condition < 0 and condition != -1)
                or (version < 24 and (condition != math.trunc(condition) or condition > 2147483647
                                    or (condition == 0 and math.copysign(1., condition) < 0)))
                or not math.isfinite(charge) or charge < -1.0
                or not math.isfinite(usage) or usage < -1.0 or slots & ~0x7FFFF
                or hotkey < -1 or hotkey > 7):
            raise RuntimeStateError("Invalid TES4 runtime-state inventory metadata")
        if not actor and hotkey != -1:
            raise RuntimeStateError("TES4 reference inventory cannot contain player hotkeys")
        if occupied_slots & slots:
            raise RuntimeStateError("Conflicting TES4 runtime-state equipped slots")
        occupied_slots |= slots
        if hotkey >= 0:
            bit = 1 << hotkey
            if occupied_hotkeys & bit:
                raise RuntimeStateError("Duplicate TES4 runtime-state inventory hotkey")
            occupied_hotkeys |= bit


def _upgrade_inventory(value: list[dict[str, Any]]) -> None:
    """Materialize the v4 item fields when promoting an older save state."""

    for item in value:
        item.setdefault("condition", -1)
        item.setdefault("charge", -1.0)
        item.setdefault("equipped_slots", 0)
        item.setdefault("hotkey", -1)
        item.setdefault("owner", "null")
        item.setdefault("remaining_usage_time", -1.0)


def decode_payload(payload: bytes) -> dict[str, Any]:
    reader = _Reader(payload)
    if reader.take(len(MAGIC)) != MAGIC:
        raise RuntimeStateError("Invalid TES4 runtime-state magic")
    version = reader.unpack("<I")
    profile = reader.unpack("<B")
    if version not in SUPPORTED_VERSIONS:
        raise RuntimeStateError(f"Unsupported TES4 runtime-state version {version}")
    if profile != 2:
        raise RuntimeStateError("TES4 runtime state requires the Oblivion game profile")
    result: dict[str, Any] = {
        "schema_version": version,
        "profile": "oblivion",
        "next_dynamic_serial": reader.unpack("<Q"),
        "content": [],
    }
    for _ in range(reader.count()):
        result["content"].append({"plugin": reader.string(), "fingerprint": reader.string()})
    result["clock"] = {
        "year": reader.unpack("<i"),
        "month": reader.unpack("<i"),
        "day": reader.unpack("<i"),
        "hour": reader.unpack("<d"),
        "time_scale": reader.unpack("<d"),
    }
    player: dict[str, Any] = {
        "reference": reader.string(),
        "cell": reader.string(),
        "position": _position(reader),
        "actor_values": {},
    }
    for _ in range(reader.count()):
        name = reader.string()
        if name in player["actor_values"]:
            raise RuntimeStateError(f"Duplicate TES4 actor value {name}")
        player["actor_values"][name] = reader.unpack("<d")
    player["inventory"] = _inventory(reader, version)
    if version >= 3:
        player["name"] = reader.string()
        player["race"] = reader.string()
        player["class"] = reader.string()
        player["birthsign"] = reader.string()
        female = reader.unpack("<B")
        if female > 1:
            raise RuntimeStateError("Invalid TES4 runtime-state player sex")
        player["female"] = bool(female)
        player["character_generation_flags"] = reader.unpack("<B")
    result["player"] = player

    globals_: dict[str, Any] = {}
    for _ in range(reader.count()):
        key = reader.string()
        if key in globals_:
            raise RuntimeStateError(f"Duplicate TES4 global {key}")
        globals_[key] = _value(reader)
    result["globals"] = globals_

    references: list[dict[str, Any]] = []
    seen: set[str] = set()
    for _ in range(reader.count()):
        reference: dict[str, Any] = {
            "key": reader.string(),
            "base": reader.string(),
            "cell": reader.string(),
        }
        if reference["key"] in seen:
            raise RuntimeStateError(f"Duplicate TES4 reference {reference['key']}")
        seen.add(reference["key"])
        enabled, deleted = reader.unpack("<B"), reader.unpack("<B")
        if enabled > 1 or deleted > 1:
            raise RuntimeStateError("Invalid TES4 runtime-state reference flags")
        reference.update({"enabled": bool(enabled), "deleted": bool(deleted), "position": _position(reader)})
        has_owner = reader.unpack("<B")
        if has_owner > 1:
            raise RuntimeStateError("Invalid TES4 runtime-state owner flag")
        reference["owner"] = reader.string() if has_owner else None
        reference["lock_level"] = reader.unpack("<i")
        reference["inventory"] = _inventory(reader, version)
        custom: dict[str, Any] = {}
        for _ in range(reader.count()):
            name = reader.string()
            if name in custom:
                raise RuntimeStateError(f"Duplicate TES4 custom value {name}")
            custom[name] = _value(reader)
        reference["custom_state"] = custom
        if version >= 29:
            has_draw = reader.unpack("<B")
            if has_draw > 1:
                raise RuntimeStateError("Invalid TES4 native actor draw state flag")
            reference["actor_draw_state"] = reader.unpack("<B") if has_draw else None
        references.append(reference)
    result["references"] = references
    _validate_inventory(result["player"]["inventory"], version, True)
    for reference in references:
        _validate_inventory(reference["inventory"], version, False)
    if version >= 2:
        result["script_event_sequence"] = reader.unpack("<Q")
        result["script_instances"] = []
        result["quests"] = []
        seen_scripts: set[tuple[str, str]] = set()
        for _ in range(reader.count()):
            unit, context = reader.string(), reader.string()
            identity = (unit, context)
            if identity in seen_scripts:
                raise RuntimeStateError(f"Duplicate TES4 script instance {unit} at {context}")
            seen_scripts.add(identity)
            on_load = reader.unpack("<B")
            if on_load > 1:
                raise RuntimeStateError("Invalid TES4 runtime-state OnLoad flag")
            result["script_instances"].append({
                "unit": unit,
                "context": context,
                "on_load_fired": bool(on_load),
                "locals": [_script_value(reader) for _ in range(reader.count())],
            })
        seen_quests: set[str] = set()
        for _ in range(reader.count()):
            quest = reader.string()
            if quest in seen_quests:
                raise RuntimeStateError(f"Duplicate TES4 quest {quest}")
            seen_quests.add(quest)
            stage = reader.unpack("<i")
            running = reader.unpack("<B")
            if running > 1:
                raise RuntimeStateError("Invalid TES4 runtime-state quest running flag")
            completed = [reader.unpack("<i") for _ in range(reader.count())]
            if completed != sorted(set(completed)):
                raise RuntimeStateError("TES4 completed quest stages are not sorted and unique")
            result["quests"].append({
                "quest": quest, "stage": stage, "running": bool(running), "completed_stages": completed,
            })
    if version >= 5:
        result["ai_rng_state"] = reader.unpack("<Q")
        result["actor_ai"] = [_read_actor_ai(reader, version) for _ in range(reader.count())]
        result["path_points"] = []
        for _ in range(reader.count()):
            pathgrid, node, enabled = reader.string(), reader.unpack("<I"), reader.unpack("<B")
            if enabled > 1:
                raise RuntimeStateError("Invalid TES4 runtime-state path-point overlay flag")
            result["path_points"].append({"pathgrid": pathgrid, "node": node, "enabled": bool(enabled)})
        result["companions"] = []
        for _ in range(reader.count()):
            result["companions"].append({
                "leader": reader.string(),
                "member": reader.string(),
                "group": reader.string(),
                "side_with": reader.string(),
                "formation_index": reader.unpack("<i"),
            })
        result["mounts"] = []
        for _ in range(reader.count()):
            horse, rider, owner, last_ridden = (reader.string() for _ in range(4))
            mounted = reader.unpack("<B")
            if mounted > 1:
                raise RuntimeStateError("Invalid TES4 runtime-state mount flag")
            result["mounts"].append({
                "horse": horse,
                "rider": rider,
                "owner": owner,
                "last_ridden": last_ridden,
                "mounted": bool(mounted),
            })
        result["detection_vectors"] = []
        for _ in range(reader.count()):
            observer, target = reader.string(), reader.string()
            score = reader.unpack("<d")
            if not math.isfinite(score) or not 0.0 <= score <= 100.0:
                raise RuntimeStateError("Invalid TES4 runtime-state detection vector score")
            detected, line_of_sight = reader.unpack("<B"), reader.unpack("<B")
            if detected > 1 or line_of_sight > 1:
                raise RuntimeStateError("Invalid TES4 runtime-state detection vector flags")
            result["detection_vectors"].append({
                "observer": observer,
                "target": target,
                "score": score,
                "detected": bool(detected),
                "line_of_sight": bool(line_of_sight),
            })
    if version >= 6:
        result["pending_package_done"] = [
            {"actor": reader.string(), "package": reader.string()} for _ in range(reader.count())
        ]
    if version >= 8:
        result["physical_actions"] = {
            "next": reader.unpack("<Q"),
            "pending": [reader.unpack("<Q") for _ in range(reader.count())],
        }
    if version >= 9:
        result["native_actor_values"] = []
        for _ in range(reader.count()):
            actor = {"actor": reader.string(), "base": reader.string(),
                     "owner": reader.unpack("<B"), "process": reader.unpack("<B"), "values": []}
            for _ in range(72):
                base = reader.unpack("<f")
                mask = reader.unpack("<B")
                if mask > 7:
                    raise RuntimeStateError("Invalid TES4 native actor-value modifier mask")
                actor["values"].append([base] + [reader.unpack("<f") if mask & (1 << i) else None for i in range(3)])
            if version >= 25:
                present = reader.unpack("<B")
                if present > 1:
                    raise RuntimeStateError("Invalid TES4 native knocked byte presence")
                actor["process_knocked_state"] = reader.unpack("<b") if present else None
            if version >= 26:
                present = reader.unpack("<B")
                if present > 1:
                    raise RuntimeStateError("Invalid TES4 native action-code presence")
                actor["process_action"] = reader.unpack("<h") if present else None
            if version >= 10:
                present = reader.unpack("<B")
                if present > 1:
                    raise RuntimeStateError("Invalid TES4 player form-value presence")
                actor["player_form_values"] = [reader.unpack("<i") for _ in range(4)] if present else None
            if version >= 17:
                present = reader.unpack("<B")
                if present > 1:
                    raise RuntimeStateError("Invalid TES4 nonplayer form Health presence")
                actor["nonplayer_form_health"] = reader.unpack("<i") if present else None
            if version >= 18:
                present = reader.unpack("<B")
                if present > 1:
                    raise RuntimeStateError("Invalid TES4 passive ability ownership presence")
                actor["passive_abilities"] = None
                if present:
                    actor["passive_abilities"] = []
                    for _ in range(reader.count()):
                        ability = {"spell": reader.string(), "effects": []}
                        for _ in range(reader.count()):
                            effect = [reader.unpack("<I"), reader.unpack("<I"), reader.unpack("<I"), reader.unpack("<f")]
                            if version >= 19:
                                initial_present = reader.unpack("<B")
                                if initial_present > 1:
                                    raise RuntimeStateError("Invalid TES4 passive initial magnitude presence")
                                effect.append(reader.unpack("<f") if initial_present else None)
                            ability["effects"].append(effect)
                        actor["passive_abilities"].append(ability)
            result["native_actor_values"].append(actor)
    if version >= 11:
        result["native_actor_bases"] = []
        for _ in range(reader.count()):
            base = {"base": reader.string(), "kind": reader.unpack("<B"), "values": []}
            count = reader.count()
            if not 1 <= count <= 72:
                raise RuntimeStateError("Invalid TES4 native actor base value count")
            for _ in range(count):
                av, storage = reader.unpack("<B"), reader.unpack("<B")
                if storage not in (0, 1):
                    raise RuntimeStateError("Invalid TES4 native actor base storage type")
                base["values"].append([av, storage, reader.unpack("<i" if storage == 0 else "<f")])
            result["native_actor_bases"].append(base)
    if version >= 12:
        result["native_actor_life"] = [
            {"actor": reader.string(), "base": reader.string(), "phase": reader.unpack("<B"),
             "recovery_remaining": reader.unpack("<f"), "killer": reader.string()} for _ in range(reader.count())
        ]
        result["next_death_event"] = reader.unpack("<Q")
        result["pending_death_events"] = [
            {"id": reader.unpack("<Q"), "actor": reader.string(), "killer": reader.string()}
            for _ in range(reader.count())
        ]
    if version >= 13:
        result["native_death_counts"] = [
            {"base": reader.string(), "count": reader.unpack("<H")} for _ in range(reader.count())
        ]
    if version >= 14:
        result["native_actor_breath"] = [
            {"actor": reader.string(), "remaining": reader.unpack("<f")} for _ in range(reader.count())
        ]
    if version >= 15:
        result["native_combat_engagements"] = [
            {"first": reader.string(), "second": reader.string()} for _ in range(reader.count())
        ]
    if version >= 16:
        result["native_actor_manager_time"] = reader.unpack("<f")
        result["native_actor_update_times"] = [
            {"actor": reader.string(), "time": reader.unpack("<f")} for _ in range(reader.count())
        ]
    if version >= 20:
        result["physical_action_owners"] = [
            {"id": reader.unpack("<Q"), "actor": reader.string()} for _ in range(reader.count())
        ]
    if version >= 21:
        result["native_melee_states"] = []
        def melee_boolean() -> bool:
            value = reader.unpack("<B")
            if value > 1:
                raise RuntimeStateError("Invalid TES4 melee boolean")
            return bool(value)
        for _ in range(reader.count()):
            entry = {"actor": reader.string(), "input": {
                "held_seconds": reader.unpack("<f"), "input_held": melee_boolean(),
                "prefer_left": melee_boolean(), "queued": reader.unpack("<B")}, "strike": None}
            if melee_boolean():
                entry["strike"] = {"id": reader.unpack("<Q"), "kind": reader.unpack("<B"),
                    "weapon_base": reader.string(), "animation_group": reader.string(),
                    "playback_speed": reader.unpack("<f"), "animation_time": reader.unpack("<f"),
                    "contact_committed": melee_boolean()}
                if version >= 22:
                    entry["strike"]["ordinary_phase"] = reader.unpack("<B")
                if version >= 23:
                    timing = None
                    if melee_boolean():
                        timing = {"easing": melee_boolean()}
                        for key in ("offset", "ease_start", "last_input"):
                            timing[key] = reader.unpack("<f") if melee_boolean() else None
                        for key in ("ease_end", "weighted_time", "output_time"):
                            timing[key] = reader.unpack("<f")
                    entry["strike"]["sequence_timing"] = timing
            if version >= 28:
                entry["ai_intent"] = {"target": reader.string(), "style": reader.string()} if melee_boolean() else None
            result["native_melee_states"].append(entry)
    if version >= 23:
        result["native_animation_clocks"] = [{"actor": reader.string(), "clock": reader.unpack("<f")}
                                              for _ in range(reader.count())]
    if version >= 27:
        result["combat_rng_state"] = reader.unpack("<I")
    if version >= 30:
        result["native_actor_knockback"] = [{"actor": reader.string(),
            "acceleration": [reader.unpack("<f") for _ in range(3)], "remaining": reader.unpack("<f")}
            for _ in range(reader.count())]
    if version >= 31:
        result["native_actor_ragdolls"] = []
        for _ in range(reader.count()):
            entry = {"actor": reader.string(), "base": reader.string(), "model": reader.string(),
                     "asset_hash": reader.string(), "bodies": []}
            for _ in range(reader.count()):
                body = {"record": reader.unpack("<I"), "node_record": reader.unpack("<I")}
                for field, size in [("rotation", 9), ("position", 3), ("linear_velocity", 3), ("angular_velocity", 3)]:
                    body[field] = [reader.unpack("<f") for _ in range(size)]
                if version >= 32:
                    present = reader.unpack("<B")
                    if present > 1:
                        raise RuntimeStateError("Invalid TES4 packed ragdoll presence flag")
                    if present:
                        body["native_packed_velocity"] = {field: [reader.unpack("<f") for _ in range(4)]
                                                          for field in ("linear", "angular")}
                if version >= 33:
                    present = reader.unpack("<B")
                    if present > 1:
                        raise RuntimeStateError("Invalid TES4 ragdoll motion presence flag")
                    if present:
                        body["native_motion"] = reader.unpack("<B")
                entry["bodies"].append(body)
            if version >= 34:
                present = reader.unpack("<B")
                if present > 1:
                    raise RuntimeStateError("Invalid TES4 native blend presence flag")
                if present:
                    count = reader.count()
                    if count > len(entry["bodies"]):
                        raise RuntimeStateError("Excessive TES4 native blend count")
                    entry["native_blends"] = [{"body_record": reader.unpack("<I"),
                        "collision_flags": reader.unpack("<H"), "requested_motion": reader.unpack("<I"),
                        "hierarchy_gain": reader.unpack("<f"), "velocity_gain": reader.unpack("<f")}
                        for _ in range(count)]
            if version >= 35:
                def boolean():
                    value = reader.unpack("<B")
                    if value > 1: raise RuntimeStateError("Invalid TES4 controller presence or boolean")
                    return bool(value)
                def target_node():
                    return reader.unpack("<I") if boolean() else None
                def common_state():
                    timing = {"flags": reader.unpack("<H")}
                    timing.update({field: reader.unpack("<f") for field in ("frequency", "phase", "start_key", "stop_key")})
                    return {"timing": timing, "clock": {field: reader.unpack("<f")
                        for field in ("start_time", "previous_time", "elapsed")}}
                if boolean():
                    controls = {"blends": [], "velocities": []}; entry["native_controllers"] = controls
                    count = reader.count()
                    if count > len(entry["bodies"]): raise RuntimeStateError("Excessive TES4 authored controller count")
                    for _ in range(count):
                        controller = {"record": reader.unpack("<I"), "attached_node": reader.unpack("<I"), "target_node": target_node()}
                        value = common_state(); controller["state"] = value
                        value["cursor"] = reader.unpack("<I")
                        value["cached_gains"] = {"hierarchy": reader.unpack("<f"), "velocity": reader.unpack("<f")}
                        value["setup_state"] = reader.unpack("<I")
                        value["keys"] = [{"time": reader.unpack("<f"), "hierarchy_gain": reader.unpack("<f"), "velocity_gain": reader.unpack("<f")}
                            for _ in range(reader.count())]
                        controls["blends"].append(controller)
                    count = reader.count()
                    if count > len(entry["bodies"]): raise RuntimeStateError("Excessive TES4 generated controller count")
                    for _ in range(count):
                        controller = {"attached_node": reader.unpack("<I"), "target_node": target_node(), "precedes_blend": boolean()}
                        value = common_state(); controller["state"] = value
                        value["force_vector"] = [reader.unpack("<f") for _ in range(4)]; value["frame_delta"] = reader.unpack("<f")
                        controls["velocities"].append(controller)
            result["native_actor_ragdolls"].append(entry)
    if version >= 36:
        present = reader.unpack("<B")
        if present not in (0, 1):
            raise RuntimeStateError("Invalid TES4 native physical cache presence marker")
        if present:
            result["native_physical_blend_time_cache"] = {"cycle": reader.unpack("<I"),
                **{field: reader.unpack("<f") for field in ("stop_key", "start_key", "key_time", "result")}}
    if version >= 37:
        present = reader.unpack("<B")
        if present not in (0, 1):
            raise RuntimeStateError("Invalid TES4 Player bow timer presence marker")
        if present:
            result["native_player_bow_timer"] = reader.unpack("<f")
    if version >= 38:
        result["native_bow_states"] = []
        for _ in range(reader.count()):
            bow = {"actor": reader.string(), "id": reader.unpack("<Q"),
                "bow_base": reader.string(), "ammo_base": reader.string(),
                "animation_group": reader.string(), "playback_rate": reader.unpack("<f"),
                "phase": reader.unpack("<B"), "sequence_offset": reader.unpack("<f"),
                "key_times": [reader.unpack("<f") for _ in range(5)], "action": reader.unpack("<h")}
            marker = reader.unpack("<B")
            if marker not in (0, 1):
                raise RuntimeStateError("Invalid TES4 bow release marker")
            bow["release_committed"] = bool(marker)
            if version >= 39:
                latch = reader.unpack("<B")
                if latch not in (0, 1):
                    raise RuntimeStateError("Invalid native Player bow hold latch")
                bow["player_hold_latched"] = bool(latch)
            result["native_bow_states"].append(bow)
    _validate_basic_state(result)
    if reader.offset != len(payload):
        raise RuntimeStateError("TES4 runtime-state payload has trailing data")
    return result


def encode_payload(state: dict[str, Any]) -> bytes:
    version = state.get("schema_version")
    if version not in SUPPORTED_VERSIONS or state.get("profile") != "oblivion":
        raise RuntimeStateError("Unsupported TES4 runtime-state schema or profile")
    if version < 5 and state.get("detection_vectors"):
        raise RuntimeStateError("TES4 runtime-state version 1/2/3/4 cannot contain detection vectors")
    _validate_basic_state(state)
    writer = _Writer()
    writer.add(MAGIC)
    writer.pack("<I", version)
    writer.pack("<B", 2)
    writer.pack("<Q", int(state["next_dynamic_serial"]))
    writer.pack("<I", len(state["content"]))
    for item in state["content"]:
        writer.string(str(item["plugin"]).casefold())
        writer.string(str(item["fingerprint"]))
    clock = state["clock"]
    writer.pack("<i", int(clock["year"]))
    writer.pack("<i", int(clock["month"]))
    writer.pack("<i", int(clock["day"]))
    writer.pack("<d", float(clock["hour"]))
    writer.pack("<d", float(clock["time_scale"]))
    player = state["player"]
    _validate_inventory(player["inventory"], version, True)
    writer.string(str(player["reference"]))
    writer.string(str(player["cell"]))
    _write_position(writer, player["position"])
    actor_values = player["actor_values"]
    writer.pack("<I", len(actor_values))
    for name in sorted(actor_values):
        writer.string(name)
        writer.pack("<d", float(actor_values[name]))
    _write_inventory(writer, player["inventory"], version)
    if version >= 3:
        writer.string(str(player["name"]))
        writer.string(str(player["race"]))
        writer.string(str(player["class"]))
        writer.string(str(player.get("birthsign", "null")))
        writer.pack("<B", int(bool(player.get("female", False))))
        writer.pack("<B", int(player.get("character_generation_flags", 0)))
    globals_ = state["globals"]
    writer.pack("<I", len(globals_))
    for key in sorted(globals_):
        writer.string(key)
        _write_value(writer, globals_[key])
    references = sorted(state["references"], key=lambda item: item["key"])
    for reference in references:
        _validate_inventory(reference["inventory"], version, False)
    writer.pack("<I", len(references))
    for reference in references:
        writer.string(str(reference["key"]))
        writer.string(str(reference["base"]))
        writer.string(str(reference["cell"]))
        writer.pack("<B", int(bool(reference["enabled"])))
        writer.pack("<B", int(bool(reference["deleted"])))
        _write_position(writer, reference["position"])
        writer.pack("<B", int(reference["owner"] is not None))
        if reference["owner"] is not None:
            writer.string(str(reference["owner"]))
        writer.pack("<i", int(reference["lock_level"]))
        _write_inventory(writer, reference["inventory"], version)
        custom = reference["custom_state"]
        writer.pack("<I", len(custom))
        for name in sorted(custom):
            writer.string(name)
            _write_value(writer, custom[name])
        if version >= 29:
            draw = reference.get("actor_draw_state")
            writer.pack("<B", int(draw is not None))
            if draw is not None:
                writer.pack("<B", draw)
    if version >= 2:
        writer.pack("<Q", int(state.get("script_event_sequence", 0)))
        scripts = sorted(state.get("script_instances", []), key=lambda item: (item["unit"], item["context"]))
        writer.pack("<I", len(scripts))
        for script in scripts:
            writer.string(str(script["unit"]))
            writer.string(str(script["context"]))
            writer.pack("<B", int(bool(script.get("on_load_fired", False))))
            writer.pack("<I", len(script["locals"]))
            for value in script["locals"]:
                _write_script_value(writer, value)
        quests = sorted(state.get("quests", []), key=lambda item: item["quest"])
        writer.pack("<I", len(quests))
        for quest in quests:
            writer.string(str(quest["quest"]))
            writer.pack("<i", int(quest["stage"]))
            writer.pack("<B", int(bool(quest["running"])))
            completed = sorted(set(int(value) for value in quest["completed_stages"]))
            writer.pack("<I", len(completed))
            for stage in completed:
                writer.pack("<i", stage)
    if version >= 5:
        _validate_ai(state)
        rng_state = int(state.get("ai_rng_state", 1))
        if rng_state == 0:
            raise RuntimeStateError("TES4 runtime-state AI RNG state must be non-zero")
        writer.pack("<Q", rng_state)

        actors = sorted(state.get("actor_ai", []), key=lambda item: item["actor"])
        writer.pack("<I", len(actors))
        for actor in actors:
            _write_actor_ai(writer, actor, version)

        points = sorted(state.get("path_points", []), key=lambda item: (item["pathgrid"], int(item["node"])))
        writer.pack("<I", len(points))
        for point in points:
            writer.string(str(point["pathgrid"]))
            writer.pack("<I", int(point["node"]))
            writer.pack("<B", int(bool(point.get("enabled", True))))

        companions = sorted(
            state.get("companions", []), key=lambda item: (item["leader"], item["member"])
        )
        writer.pack("<I", len(companions))
        for relation in companions:
            writer.string(str(relation["leader"]))
            writer.string(str(relation["member"]))
            writer.string(str(relation.get("group", relation["leader"])))
            writer.string(str(relation.get("side_with", "null")))
            writer.pack("<i", int(relation.get("formation_index", -1)))

        mounts = sorted(state.get("mounts", []), key=lambda item: (item["horse"], item["rider"]))
        writer.pack("<I", len(mounts))
        for relation in mounts:
            writer.string(str(relation["horse"]))
            writer.string(str(relation["rider"]))
            writer.string(str(relation.get("owner", "null")))
            writer.string(str(relation.get("last_ridden", "null")))
            writer.pack("<B", int(bool(relation.get("mounted", False))))

        vectors = sorted(
            state.get("detection_vectors", []),
            key=lambda item: (item["observer"], item["target"]),
        )
        writer.pack("<I", len(vectors))
        for vector in vectors:
            writer.string(str(vector["observer"]))
            writer.string(str(vector["target"]))
            score = float(vector.get("score", 0.0))
            if not math.isfinite(score) or not 0.0 <= score <= 100.0:
                raise RuntimeStateError("Invalid TES4 runtime-state detection vector score")
            writer.pack("<d", score)
            writer.pack("<B", int(bool(vector.get("detected", False))))
            writer.pack("<B", int(bool(vector.get("line_of_sight", False))))
    if version >= 6:
        pending = state.get("pending_package_done", [])
        writer.pack("<I", len(pending))
        for event in pending:
            writer.string(event["actor"])
            writer.string(event["package"])
    if version >= 8:
        actions = state.get("physical_actions", {"next": 1, "pending": []})
        writer.pack("<Q", actions["next"])
        writer.pack("<I", len(actions["pending"]))
        for action in sorted(actions["pending"]):
            writer.pack("<Q", action)
    if version >= 9:
        actors = sorted(state.get("native_actor_values", []), key=lambda actor: actor["actor"])
        writer.pack("<I", len(actors))
        for actor in actors:
            writer.string(actor["actor"])
            writer.string(actor["base"])
            writer.pack("<B", actor["owner"])
            writer.pack("<B", actor["process"])
            for value in actor["values"]:
                writer.pack("<f", value[0])
                writer.pack("<B", sum(1 << i for i, modifier in enumerate(value[1:]) if modifier is not None))
                for modifier in value[1:]:
                    if modifier is not None:
                        writer.pack("<f", modifier)
            if version >= 25:
                knocked = actor.get("process_knocked_state")
                writer.pack("<B", knocked is not None)
                if knocked is not None:
                    writer.pack("<b", knocked)
            if version >= 26:
                action = actor.get("process_action")
                writer.pack("<B", action is not None)
                if action is not None:
                    writer.pack("<h", action)
            if version >= 10:
                form_values = actor.get("player_form_values")
                writer.pack("<B", form_values is not None)
                if form_values is not None:
                    for value in form_values:
                        writer.pack("<i", value)
            if version >= 17:
                form_health = actor.get("nonplayer_form_health")
                writer.pack("<B", form_health is not None)
                if form_health is not None:
                    writer.pack("<i", form_health)
            if version >= 18:
                abilities = actor.get("passive_abilities")
                writer.pack("<B", abilities is not None)
                if abilities is not None:
                    writer.pack("<I", len(abilities))
                    for ability in abilities:
                        writer.string(ability["spell"])
                        writer.pack("<I", len(ability["effects"]))
                        for effect in ability["effects"]:
                            index, code, av, magnitude = effect[:4]
                            writer.add(struct.pack("<IIIf", index, code, av, magnitude))
                            if version >= 19:
                                initial_magnitude = effect[4]
                                writer.pack("<B", initial_magnitude is not None)
                                if initial_magnitude is not None:
                                    writer.pack("<f", initial_magnitude)
    if version >= 11:
        bases = sorted(state.get("native_actor_bases", []), key=lambda base: base["base"])
        writer.pack("<I", len(bases))
        for base in bases:
            writer.string(base["base"])
            writer.pack("<B", base["kind"])
            writer.pack("<I", len(base["values"]))
            for av, storage, value in sorted(base["values"], key=lambda entry: entry[0]):
                writer.pack("<B", av)
                writer.pack("<B", storage)
                writer.pack("<i" if storage == 0 else "<f", value)
    if version >= 12:
        lives = sorted(state.get("native_actor_life", []), key=lambda life: life["actor"])
        writer.pack("<I", len(lives))
        for life in lives:
            writer.string(life["actor"])
            writer.string(life["base"])
            writer.pack("<B", life["phase"])
            writer.pack("<f", life["recovery_remaining"])
            writer.string(life["killer"])
        writer.pack("<Q", state.get("next_death_event", 1))
        events = state.get("pending_death_events", [])
        writer.pack("<I", len(events))
        for event in events:
            writer.pack("<Q", event["id"])
            writer.string(event["actor"])
            writer.string(event["killer"])
    if version >= 13:
        counts = sorted(state.get("native_death_counts", []), key=lambda item: item["base"])
        writer.pack("<I", len(counts))
        for entry in counts:
            writer.string(entry["base"])
            writer.pack("<H", entry["count"])
    if version >= 14:
        breath = sorted(state.get("native_actor_breath", []), key=lambda item: item["actor"])
        writer.pack("<I", len(breath))
        for entry in breath:
            writer.string(entry["actor"])
            writer.pack("<f", entry["remaining"])
    if version >= 15:
        engagements = sorted(state.get("native_combat_engagements", []), key=lambda item: (item["first"], item["second"]))
        writer.pack("<I", len(engagements))
        for entry in engagements:
            writer.string(entry["first"])
            writer.string(entry["second"])
    if version >= 16:
        writer.pack("<f", state.get("native_actor_manager_time", 0))
        times = sorted(state.get("native_actor_update_times", []), key=lambda item: item["actor"])
        writer.pack("<I", len(times))
        for entry in times:
            writer.string(entry["actor"])
            writer.pack("<f", entry["time"])
    if version >= 20:
        owners = sorted(state.get("physical_action_owners", []), key=lambda item: item["id"])
        writer.pack("<I", len(owners))
        for entry in owners:
            writer.pack("<Q", entry["id"])
            writer.string(entry["actor"])
    if version >= 21:
        melee_states = sorted(state.get("native_melee_states", []), key=lambda item: item["actor"])
        writer.pack("<I", len(melee_states))
        for entry in melee_states:
            writer.string(entry["actor"])
            control, strike = entry["input"], entry["strike"]
            writer.pack("<f", control["held_seconds"])
            for value in (int(control["input_held"]), int(control["prefer_left"]), control["queued"], int(strike is not None)):
                writer.pack("<B", value)
            if strike is not None:
                writer.pack("<Q", strike["id"])
                writer.pack("<B", strike["kind"])
                writer.string(strike["weapon_base"])
                writer.string(strike["animation_group"])
                writer.pack("<f", strike["playback_speed"])
                writer.pack("<f", strike["animation_time"])
                writer.pack("<B", int(strike["contact_committed"]))
                if version >= 22:
                    writer.pack("<B", strike["ordinary_phase"])
                if version >= 23:
                    timing = strike["sequence_timing"]
                    writer.pack("<B", int(timing is not None))
                    if timing is not None:
                        writer.pack("<B", int(timing["easing"]))
                        for key in ("offset", "ease_start", "last_input"):
                            value = timing[key]
                            writer.pack("<B", int(value is not None))
                            if value is not None:
                                writer.pack("<f", value)
                        for key in ("ease_end", "weighted_time", "output_time"):
                            writer.pack("<f", timing[key])
            if version >= 28:
                intent = entry["ai_intent"]
                writer.pack("<B", int(intent is not None))
                if intent is not None:
                    writer.string(intent["target"])
                    writer.string(intent["style"])
    if version >= 23:
        clocks = sorted(state.get("native_animation_clocks", []), key=lambda item: item["actor"])
        writer.pack("<I", len(clocks))
        for entry in clocks:
            writer.string(entry["actor"])
            writer.pack("<f", entry["clock"])
    if version >= 27:
        writer.pack("<I", state.get("combat_rng_state", 1))
    if version >= 30:
        pulses = sorted(state.get("native_actor_knockback", []), key=lambda item: item["actor"])
        writer.pack("<I", len(pulses))
        for entry in pulses:
            writer.string(entry["actor"])
            for component in entry["acceleration"]:
                writer.pack("<f", component)
            writer.pack("<f", entry["remaining"])
    if version >= 31:
        ragdolls = sorted(state.get("native_actor_ragdolls", []), key=lambda item: item["actor"])
        writer.pack("<I", len(ragdolls))
        for entry in ragdolls:
            for field in ["actor", "base", "model", "asset_hash"]:
                writer.string(entry[field])
            writer.pack("<I", len(entry["bodies"]))
            for body in entry["bodies"]:
                writer.pack("<I", body["record"])
                writer.pack("<I", body["node_record"])
                for field in ["rotation", "position", "linear_velocity", "angular_velocity"]:
                    for value in body[field]:
                        writer.pack("<f", value)
                if version >= 32:
                    writer.pack("<B", int("native_packed_velocity" in body))
                    if "native_packed_velocity" in body:
                        for field in ("linear", "angular"):
                            for value in body["native_packed_velocity"][field]:
                                writer.pack("<f", value)
                if version >= 33:
                    writer.pack("<B", int("native_motion" in body))
                    if "native_motion" in body:
                        writer.pack("<B", body["native_motion"])
            if version >= 34:
                writer.pack("<B", int("native_blends" in entry))
                if "native_blends" in entry:
                    writer.pack("<I", len(entry["native_blends"]))
                    for blend in entry["native_blends"]:
                        writer.pack("<I", blend["body_record"])
                        writer.pack("<H", blend["collision_flags"])
                        writer.pack("<I", blend["requested_motion"])
                        writer.pack("<f", blend["hierarchy_gain"])
                        writer.pack("<f", blend["velocity_gain"])
            if version >= 35:
                writer.pack("<B", int("native_controllers" in entry))
                if "native_controllers" in entry:
                    def target_node(node):
                        writer.pack("<B", int(node is not None))
                        if node is not None: writer.pack("<I", node)
                    def common_state(value):
                        writer.pack("<H", value["timing"]["flags"])
                        for field in ("frequency", "phase", "start_key", "stop_key"): writer.pack("<f", value["timing"][field])
                        for field in ("start_time", "previous_time", "elapsed"): writer.pack("<f", value["clock"][field])
                    controls = entry["native_controllers"]
                    writer.pack("<I", len(controls["blends"]))
                    for controller in controls["blends"]:
                        writer.pack("<I", controller["record"]); writer.pack("<I", controller["attached_node"])
                        target_node(controller["target_node"]); value = controller["state"]; common_state(value)
                        writer.pack("<I", value["cursor"])
                        writer.pack("<f", value["cached_gains"]["hierarchy"]); writer.pack("<f", value["cached_gains"]["velocity"])
                        writer.pack("<I", value["setup_state"]); writer.pack("<I", len(value["keys"]))
                        for key in value["keys"]:
                            for field in ("time", "hierarchy_gain", "velocity_gain"): writer.pack("<f", key[field])
                    writer.pack("<I", len(controls["velocities"]))
                    for controller in controls["velocities"]:
                        writer.pack("<I", controller["attached_node"]); target_node(controller["target_node"])
                        writer.pack("<B", int(controller["precedes_blend"])); value = controller["state"]; common_state(value)
                        for component in value["force_vector"]: writer.pack("<f", component)
                        writer.pack("<f", value["frame_delta"])
    if version >= 36:
        writer.pack("<B", int("native_physical_blend_time_cache" in state))
        if "native_physical_blend_time_cache" in state:
            cache = state["native_physical_blend_time_cache"]
            writer.pack("<I", cache["cycle"])
            for field in ("stop_key", "start_key", "key_time", "result"):
                writer.pack("<f", cache[field])
    if version >= 37:
        writer.pack("<B", int("native_player_bow_timer" in state))
        if "native_player_bow_timer" in state:
            writer.pack("<f", state["native_player_bow_timer"])
    if version >= 38:
        bows = sorted(state.get("native_bow_states", []), key=lambda bow: bow["actor"])
        writer.pack("<I", len(bows))
        for bow in bows:
            writer.string(bow["actor"]); writer.pack("<Q", bow["id"])
            for field in ("bow_base", "ammo_base", "animation_group"): writer.string(bow[field])
            writer.pack("<f", bow["playback_rate"]); writer.pack("<B", bow["phase"])
            writer.pack("<f", bow["sequence_offset"])
            for time in bow["key_times"]: writer.pack("<f", time)
            writer.pack("<h", bow["action"]); writer.pack("<B", int(bow["release_committed"]))
            if version >= 39: writer.pack("<B", int(bow["player_hold_latched"]))
    return writer.finish()


def _find_runtime_record(data: bytes) -> tuple[int, int, int, bytes]:
    offset = 0
    while offset < len(data):
        if offset + 16 > len(data):
            raise RuntimeStateError("Truncated OpenMW save record header")
        name = data[offset : offset + 4]
        size = struct.unpack_from("<I", data, offset + 4)[0]
        end = offset + 16 + size
        if end > len(data):
            raise RuntimeStateError(f"Truncated OpenMW save record {name!r}")
        if name == b"T4ST":
            payload = bytearray()
            sub = offset + 16
            version: int | None = None
            while sub < end:
                if sub + 8 > end:
                    raise RuntimeStateError("Truncated T4ST subrecord header")
                sub_name = data[sub : sub + 4]
                sub_size = struct.unpack_from("<I", data, sub + 4)[0]
                sub_end = sub + 8 + sub_size
                if sub_end > end:
                    raise RuntimeStateError("Truncated T4ST subrecord")
                value = data[sub + 8 : sub_end]
                if sub_name == b"VERS":
                    if sub_size != 4:
                        raise RuntimeStateError("Invalid T4ST VERS subrecord")
                    version = struct.unpack("<I", value)[0]
                elif sub_name == b"DATA":
                    payload.extend(value)
                else:
                    raise RuntimeStateError(f"Unknown T4ST subrecord {sub_name!r}")
                sub = sub_end
            if version not in SUPPORTED_VERSIONS:
                raise RuntimeStateError(f"Unsupported T4ST record version {version}")
            return offset, end, size, bytes(payload)
        offset = end
    raise RuntimeStateError("OpenMW save has no T4ST record")


def load_save(path: Path) -> dict[str, Any]:
    return decode_payload(_find_runtime_record(path.read_bytes())[3])


def _upgrade_actor_draw(state: dict[str, Any]) -> None:
    for reference in state.get("references", []):
        if state.get("schema_version", 1) < 29 and reference.get("actor_draw_state") is not None:
            raise RuntimeStateError("Legacy TES4 save cannot carry native actor draw state")
        reference.setdefault("actor_draw_state", None)


def _upgrade_actor_knockback(state: dict[str, Any]) -> None:
    if state.get("schema_version", 1) < 30 and state.get("native_actor_knockback"):
        raise RuntimeStateError("Legacy TES4 save cannot carry native actor knockback")
    state.setdefault("native_actor_knockback", [])
    if state.get("schema_version", 1) < 31 and state.get("native_actor_ragdolls"):
        raise RuntimeStateError("Legacy TES4 save cannot carry native actor ragdolls")
    state.setdefault("native_actor_ragdolls", [])


def _upgrade_bow_states(state: dict[str, Any]) -> None:
    if state.get("schema_version", 1) < 38 and state.get("native_bow_states"):
        raise RuntimeStateError("Legacy TES4 save cannot carry owned bow playback")
    state.setdefault("native_bow_states", [])
    if state.get("schema_version", 1) < 39:
        for bow in state["native_bow_states"]:
            if bow.get("player_hold_latched", False):
                raise RuntimeStateError("Legacy TES4 save cannot carry a Player bow hold latch")
            bow["player_hold_latched"] = False


def _upgrade_melee_ai(state: dict[str, Any]) -> None:
    if state.get("schema_version", 1) < 28:
        for entry in state.get("native_melee_states", []):
            if entry.get("ai_intent") is not None:
                raise RuntimeStateError("Legacy TES4 save cannot carry melee AI intent")
            entry["ai_intent"] = None


def _upgrade_melee_timing(state: dict[str, Any]) -> None:
    if state.get("schema_version", 1) < 23:
        for entry in state.get("native_melee_states", []):
            if entry["strike"] is not None:
                entry["strike"]["sequence_timing"] = None
        state.setdefault("native_animation_clocks", [])


def _upgrade_melee_phases(state: dict[str, Any]) -> None:
    if state.get("schema_version", 1) < 22:
        for entry in state.get("native_melee_states", []):
            if entry["strike"] is not None:
                entry["strike"]["ordinary_phase"] = 0


def write_save(source: Path, destination: Path, state: dict[str, Any]) -> None:
    data = source.read_bytes()
    start, end, _, _ = _find_runtime_record(data)
    state = copy.deepcopy(state)
    _upgrade_melee_phases(state)
    _upgrade_melee_timing(state)
    _upgrade_melee_ai(state)
    _upgrade_actor_draw(state)
    _upgrade_actor_knockback(state)
    _upgrade_bow_states(state)
    # v1/v2 did not carry character-generation fields.  Promote them with
    # stable Oblivion defaults before encoding v5; without this step a real
    # legacy save could be decoded but not rewritten by the migration tool.
    player = state.setdefault("player", {})
    player.setdefault("name", "")
    player.setdefault("race", DEFAULT_MIGRATION_RACE)
    player.setdefault("class", DEFAULT_MIGRATION_CLASS)
    player.setdefault("birthsign", "null")
    player.setdefault("female", False)
    player.setdefault("character_generation_flags", 0)
    state["schema_version"] = CURRENT_VERSION
    state.setdefault("script_event_sequence", 0)
    state.setdefault("script_instances", [])
    state.setdefault("quests", [])
    state.setdefault("ai_rng_state", 1)
    state.setdefault("combat_rng_state", 1)
    state.setdefault("actor_ai", [])
    state.setdefault("path_points", [])
    state.setdefault("companions", [])
    state.setdefault("mounts", [])
    state.setdefault("detection_vectors", [])
    state.setdefault("pending_package_done", [])
    state.setdefault("physical_actions", {"next": 1, "pending": []})
    state.setdefault("physical_action_owners", [])
    state.setdefault("native_melee_states", [])
    state.setdefault("native_actor_values", [])
    state.setdefault("native_actor_bases", [])
    state.setdefault("native_actor_life", [])
    state.setdefault("native_death_counts", [])
    state.setdefault("native_actor_breath", [])
    state.setdefault("native_combat_engagements", [])
    state.setdefault("native_actor_manager_time", 0)
    state.setdefault("native_actor_update_times", [])
    state.setdefault("next_death_event", 1)
    state.setdefault("pending_death_events", [])
    _upgrade_inventory(state["player"]["inventory"])
    for reference in state["references"]:
        _upgrade_inventory(reference["inventory"])
    payload = encode_payload(state)
    record_body = struct.pack("<4sI", b"VERS", 4) + struct.pack("<I", CURRENT_VERSION)
    for offset in range(0, len(payload), CHUNK_SIZE):
        chunk = payload[offset : offset + CHUNK_SIZE]
        record_body += struct.pack("<4sI", b"DATA", len(chunk)) + chunk
    header = data[start : start + 4] + struct.pack("<I", len(record_body)) + data[start + 8 : start + 16]
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data[:start] + header + record_body + data[end:])


def mutate_for_acceptance(state: dict[str, Any], label: str) -> dict[str, Any]:
    """Apply deterministic changes spanning every M4 state family."""

    result = copy.deepcopy(state)
    _upgrade_melee_phases(result)
    _upgrade_melee_timing(result)
    _upgrade_melee_ai(result)
    _upgrade_actor_draw(result)
    _upgrade_actor_knockback(result)
    _upgrade_bow_states(result)
    result["schema_version"] = CURRENT_VERSION
    result.setdefault("script_event_sequence", 0)
    result.setdefault("script_instances", [])
    result.setdefault("quests", [])
    result.setdefault("ai_rng_state", 1)
    result.setdefault("combat_rng_state", 1)
    result.setdefault("actor_ai", [])
    result.setdefault("path_points", [])
    result.setdefault("companions", [])
    result.setdefault("mounts", [])
    result.setdefault("detection_vectors", [])
    result.setdefault("pending_package_done", [])
    result.setdefault("physical_actions", {"next": 1, "pending": []})
    result.setdefault("physical_action_owners", [])
    result.setdefault("native_melee_states", [])
    result.setdefault("native_actor_values", [])
    result.setdefault("native_actor_bases", [])
    result.setdefault("native_actor_life", [])
    result.setdefault("native_death_counts", [])
    result.setdefault("native_actor_breath", [])
    result.setdefault("native_combat_engagements", [])
    result.setdefault("native_actor_manager_time", 0)
    result.setdefault("native_actor_update_times", [])
    result.setdefault("next_death_event", 1)
    result.setdefault("pending_death_events", [])
    _upgrade_inventory(result["player"]["inventory"])
    for reference in result["references"]:
        _upgrade_inventory(reference["inventory"])
    player = result["player"]
    player.setdefault("name", "Bendu Olo")
    player.setdefault("race", "content:oblivion.esm:000907")
    player.setdefault("class", "content:oblivion.esm:0230e6")
    player.setdefault("birthsign", "null")
    player.setdefault("female", False)
    player.setdefault("character_generation_flags", 0)
    result["next_dynamic_serial"] += 41
    # Oblivion.esm declares GameHour as an integer GLOB even though OpenMW's time facade accepts a float, so use an
    # exactly representable value until the profile owns a fractional-hour adapter.
    result["clock"].update({"year": 434, "month": 5, "day": 12, "hour": 12.0, "time_scale": 0.0})
    if label != "exterior":
        player["position"][0] += 32.0
    actor = player["actor_values"]
    actor["health.current"] = min(actor.get("health.base", 50.0) + actor.get("health.modifier", 0.0), 37.0)
    actor["magicka.current"] = 19.0
    actor["level"] = 2.0

    references = result["references"]
    if len(references) < 2:
        raise RuntimeStateError("M4 acceptance mutation requires at least two native references")
    primary = references[0]
    primary["enabled"] = not primary["enabled"]
    primary["position"][1] += 64.0
    primary["owner"] = primary["base"]
    primary["inventory"] = [{"base": primary["base"], "count": 3}]
    primary["custom_state"].update({"count": 2, "scale": 1.25, "m4_probe": label})
    primary["deleted"] = False

    dynamic = copy.deepcopy(primary)
    dynamic["key"] = f"dynamic:openmw:{result['next_dynamic_serial'] - 1:016x}"
    dynamic["enabled"] = True
    dynamic["deleted"] = False
    dynamic["owner"] = None
    dynamic["lock_level"] = 0
    dynamic["custom_state"].update({"count": 1, "m4_probe": f"{label}-dynamic"})
    references.append(dynamic)

    deleted = references[1]
    deleted["deleted"] = True
    deleted["custom_state"]["count"] = 0

    actor_types = {
        int.from_bytes(b"NPC_", "little") | 0x00800000,
        int.from_bytes(b"CREA", "little") | 0x00800000,
    }
    lockable = next(
        (
            item
            for item in references
            if item["custom_state"].get("record_type") not in actor_types
            and item["custom_state"].get("locked") is False
            and not item["deleted"]
        ),
        None,
    )
    if lockable is None:
        raise RuntimeStateError("M4 acceptance mutation found no unlocked non-actor reference")
    lockable["lock_level"] = 37
    lockable["custom_state"]["locked"] = True

    unlockable = next(
        (
            item
            for item in references
            if item is not lockable
            and item["custom_state"].get("record_type") not in actor_types
            and item["custom_state"].get("locked") is True
            and not item["deleted"]
        ),
        None,
    )
    if unlockable is None and label != "exterior":
        raise RuntimeStateError("M4 acceptance mutation found no locked non-actor reference")
    if unlockable is not None:
        unlockable["custom_state"]["locked"] = False

    if not player["inventory"]:
        player["inventory"] = [{"base": primary["base"], "count": 2}]
    else:
        player["inventory"][0]["count"] = 2

    calendar_ids = {0x35, 0x36, 0x37, 0x38, 0x39, 0x3A}
    mutable_global = next(
        (
            (key, value)
            for key, value in sorted(result["globals"].items())
            if not isinstance(value, str) and int(key.rsplit(":", 1)[-1], 16) not in calendar_ids
        ),
        None,
    )
    if mutable_global is None:
        raise RuntimeStateError("M4 acceptance mutation found no numeric global")
    key, value = mutable_global
    result["globals"][key] = (not value) if isinstance(value, bool) else value + (0.5 if isinstance(value, float) else 7)
    calendar_values = {0x35: 434, 0x36: 5, 0x37: 12, 0x38: 12, 0x3A: 0}
    for global_key in result["globals"]:
        local_id = int(global_key.rsplit(":", 1)[-1], 16)
        if local_id in calendar_values:
            result["globals"][global_key] = calendar_values[local_id]
    _upgrade_inventory(player["inventory"])
    for reference in references:
        _upgrade_inventory(reference["inventory"])
    return result


def canonical(state: dict[str, Any]) -> str:
    return json.dumps(state, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def compare(expected: dict[str, Any], actual: dict[str, Any]) -> dict[str, Any]:
    expected_text, actual_text = canonical(expected), canonical(actual)
    return {
        "passed": expected_text == actual_text,
        "expected_sha256": __import__("hashlib").sha256(expected_text.encode()).hexdigest(),
        "actual_sha256": __import__("hashlib").sha256(actual_text.encode()).hexdigest(),
        "expected": expected,
        "actual": actual,
    }
