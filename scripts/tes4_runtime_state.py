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
CURRENT_VERSION = 8
SUPPORTED_VERSIONS = set(range(1, CURRENT_VERSION + 1))
MAX_COLLECTION = 1_000_000
MAX_STRING = 16 * 1024 * 1024
MAX_PAYLOAD = 256 * 1024 * 1024
CHUNK_SIZE = 60 * 1024
DEFAULT_MIGRATION_RACE = "content:oblivion.esm:000907"
DEFAULT_MIGRATION_CLASS = "content:oblivion.esm:0230e6"


class RuntimeStateError(RuntimeError):
    pass


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
                "condition": reader.unpack("<i"),
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
            writer.pack("<i", int(item.get("condition", -1)))
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
    for key, value in globals_.items():
        if str(key) == "null" or not str(key):
            raise RuntimeStateError("TES4 runtime-state global has a null FormKey")
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
        condition = int(item.get("condition", -1))
        charge = float(item.get("charge", -1.0))
        usage = float(item.get("remaining_usage_time", -1.0))
        slots = int(item.get("equipped_slots", 0))
        hotkey = int(item.get("hotkey", -1))
        if (condition < -1 or not math.isfinite(charge) or charge < -1.0
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


def write_save(source: Path, destination: Path, state: dict[str, Any]) -> None:
    data = source.read_bytes()
    start, end, _, _ = _find_runtime_record(data)
    state = copy.deepcopy(state)
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
    state.setdefault("actor_ai", [])
    state.setdefault("path_points", [])
    state.setdefault("companions", [])
    state.setdefault("mounts", [])
    state.setdefault("detection_vectors", [])
    state.setdefault("pending_package_done", [])
    state.setdefault("physical_actions", {"next": 1, "pending": []})
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
    result["schema_version"] = CURRENT_VERSION
    result.setdefault("script_event_sequence", 0)
    result.setdefault("script_instances", [])
    result.setdefault("quests", [])
    result.setdefault("ai_rng_state", 1)
    result.setdefault("actor_ai", [])
    result.setdefault("path_points", [])
    result.setdefault("companions", [])
    result.setdefault("mounts", [])
    result.setdefault("detection_vectors", [])
    result.setdefault("pending_package_done", [])
    result.setdefault("physical_actions", {"next": 1, "pending": []})
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
