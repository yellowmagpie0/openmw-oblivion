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
CURRENT_VERSION = 3
SUPPORTED_VERSIONS = {1, 2, CURRENT_VERSION}
MAX_COLLECTION = 1_000_000
MAX_STRING = 16 * 1024 * 1024
MAX_PAYLOAD = 256 * 1024 * 1024
CHUNK_SIZE = 60 * 1024


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
    return [reader.unpack("<f") for _ in range(6)]


def _write_position(writer: _Writer, value: list[float]) -> None:
    if len(value) != 6 or not all(math.isfinite(item) for item in value):
        raise RuntimeStateError("Invalid TES4 runtime-state position")
    for item in value:
        writer.pack("<f", item)


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


def _inventory(reader: _Reader) -> list[dict[str, Any]]:
    return [{"base": reader.string(), "count": reader.unpack("<i")} for _ in range(reader.count())]


def _write_inventory(writer: _Writer, value: list[dict[str, Any]]) -> None:
    writer.pack("<I", len(value))
    for item in value:
        writer.string(str(item["base"]))
        writer.pack("<i", int(item["count"]))


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
    player["inventory"] = _inventory(reader)
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
        reference["inventory"] = _inventory(reader)
        custom: dict[str, Any] = {}
        for _ in range(reader.count()):
            name = reader.string()
            if name in custom:
                raise RuntimeStateError(f"Duplicate TES4 custom value {name}")
            custom[name] = _value(reader)
        reference["custom_state"] = custom
        references.append(reference)
    result["references"] = references
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
    if reader.offset != len(payload):
        raise RuntimeStateError("TES4 runtime-state payload has trailing data")
    return result


def encode_payload(state: dict[str, Any]) -> bytes:
    version = state.get("schema_version")
    if version not in SUPPORTED_VERSIONS or state.get("profile") != "oblivion":
        raise RuntimeStateError("Unsupported TES4 runtime-state schema or profile")
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
    writer.string(str(player["reference"]))
    writer.string(str(player["cell"]))
    _write_position(writer, player["position"])
    actor_values = player["actor_values"]
    writer.pack("<I", len(actor_values))
    for name in sorted(actor_values):
        writer.string(name)
        writer.pack("<d", float(actor_values[name]))
    _write_inventory(writer, player["inventory"])
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
        _write_inventory(writer, reference["inventory"])
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
    state["schema_version"] = CURRENT_VERSION
    state.setdefault("script_event_sequence", 0)
    state.setdefault("script_instances", [])
    state.setdefault("quests", [])
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
