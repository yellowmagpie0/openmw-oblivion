#!/usr/bin/env python3
"""Reproducible compatibility evidence for the OpenMW Oblivion project.

The tool deliberately keeps proprietary game data outside the source tree.  It
accepts installation paths at runtime and writes all generated evidence below a
caller-selected output directory (normally below ``build/``).
"""

from __future__ import annotations

import argparse
import collections
import datetime as dt
import hashlib
import html
import json
import math
import os
import platform
import re
import shutil
import signal
import statistics
import struct
import subprocess
import sys
import time

from pathlib import Path
from typing import Any, Iterable

try:
    import fcntl
except ImportError:  # pragma: no cover - virtual playback is Linux-only
    fcntl = None  # type: ignore[assignment]

sys.path.insert(0, str(Path(__file__).resolve().parent))
import tes4_runtime_state as tes4_state  # noqa: E402
import tes4_m14_audit as tes4_m14  # noqa: E402


SCHEMA_VERSION = 1

SCENARIO_ACTION_TYPES = {
    "sleep", "gamepad_button", "gamepad_axis", "screenshot", "key", "key_down", "key_up", "key_held",
    "key_hold", "type_held", "type", "mouse_move", "mouse_move_absolute", "mouse_click", "mouse_down",
    "mouse_up", "focus_window", "command", "assert_file", "m14_checkpoint", "m14_assert_events",
    "m14_observe", "m14_advance_clock", "m14_obstruction", "m14_debug_navigation",
}
FORBIDDEN_M14_ACTION_TYPES = {
    "select_actor", "select_package", "advance_phase", "set_phase", "move_actor", "teleport_actor",
    "set_position", "consume_item", "open_door", "mount_horse", "force_success",
}


def _ioc(direction: int, kind: str, number: int, size: int = 0) -> int:
    return (direction << 30) | (ord(kind) << 8) | number | (size << 16)


class VirtualGamepad:
    """Small Linux uinput Xbox-compatible pad used by runtime acceptance playback."""

    EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
    SYN_REPORT = 0
    BUTTONS = {
        "a": 304, "b": 305, "x": 307, "y": 308, "leftshoulder": 310, "rightshoulder": 311,
        "back": 314, "start": 315, "guide": 316, "leftstick": 317, "rightstick": 318,
        "dpad_up": 544, "dpad_down": 545, "dpad_left": 546, "dpad_right": 547,
    }
    AXES = {"left_x": 0, "left_y": 1, "left_trigger": 2, "right_x": 3, "right_y": 4,
            "right_trigger": 5, "dpad_x": 16, "dpad_y": 17}

    def __init__(self) -> None:
        if fcntl is None or not Path("/dev/uinput").exists():
            raise RuntimeError("virtual gamepad playback requires Linux uinput")
        self.fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
        set_bit = lambda number, value: fcntl.ioctl(self.fd, _ioc(1, "U", number, 4), value)
        set_bit(100, self.EV_KEY)
        set_bit(100, self.EV_ABS)
        for code in self.BUTTONS.values():
            set_bit(101, code)
        for code in self.AXES.values():
            set_bit(103, code)
        maximum = [0] * 64
        minimum = [0] * 64
        for code in (0, 1, 3, 4):
            maximum[code], minimum[code] = 32767, -32768
        for code in (2, 5):
            maximum[code], minimum[code] = 32767, 0
        for code in (16, 17):
            maximum[code], minimum[code] = 1, -1
        descriptor = struct.pack("80sHHHHI", b"OpenMW M12 Virtual Gamepad", 0x03, 0x045E, 0x028E, 0x0110, 0)
        descriptor += struct.pack("256i", *(maximum + minimum + [0] * 128))
        os.write(self.fd, descriptor)
        fcntl.ioctl(self.fd, _ioc(0, "U", 1))
        time.sleep(0.5)

    def event(self, event_type: int, code: int, value: int) -> None:
        os.write(self.fd, struct.pack("llHHi", 0, 0, event_type, code, value))
        os.write(self.fd, struct.pack("llHHi", 0, 0, self.EV_SYN, self.SYN_REPORT, 0))

    def close(self) -> None:
        if self.fd >= 0:
            fcntl.ioctl(self.fd, _ioc(0, "U", 2))
            os.close(self.fd)
            self.fd = -1


_VIRTUAL_GAMEPAD: VirtualGamepad | None = None
OFFICIAL_PLUGIN_ORDER = (
    "Oblivion.esm",
    "DLCShiveringIsles.esp",
    "DLCBattlehornCastle.esp",
    "DLCFrostcrag.esp",
    "DLCHorseArmor.esp",
    "DLCMehrunesRazor.esp",
    "DLCOrrery.esp",
    "DLCSpellTomes.esp",
    "DLCThievesDen.esp",
    "DLCVileLair.esp",
    "Knights.esp",
)
DEFAULT_ERROR_PATTERNS = (
    r"\bFatal\b",
    r"\bError:\s",
    r"Failed to (?:open|read) image",
    r"Unsupported (?:record|command|condition)",
    r"Traceback \(most recent call last\)",
)


def utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def file_fingerprint(path: Path, hash_contents: bool = True) -> dict[str, Any]:
    stat = path.stat()
    result: dict[str, Any] = {
        "name": path.name,
        "size": stat.st_size,
        "mtime_ns": stat.st_mtime_ns,
    }
    if hash_contents:
        result["sha256"] = sha256(path)
    return result


def command_version(command: Path, *arguments: str) -> str | None:
    if not command.is_file():
        return None
    completed = subprocess.run(
        [str(command), *arguments],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        encoding="utf-8",
        errors="replace",
        timeout=30,
        check=False,
    )
    lines = [line.strip() for line in completed.stdout.splitlines() if line.strip()]
    return " | ".join(lines[-3:]) if lines else f"exit {completed.returncode}"


def run_command(
    command: list[str],
    *,
    cwd: Path,
    timeout: float,
    environment: dict[str, str] | None = None,
) -> dict[str, Any]:
    started = time.monotonic()
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )
        output = completed.stdout
        return {
            "command": command,
            "duration_seconds": round(time.monotonic() - started, 6),
            "exit_code": completed.returncode,
            "timed_out": False,
            "output": output,
        }
    except subprocess.TimeoutExpired as error:
        output = error.stdout or ""
        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")
        return {
            "command": command,
            "duration_seconds": round(time.monotonic() - started, 6),
            "exit_code": None,
            "timed_out": True,
            "output": output,
        }


def check_log_text(
    text: str,
    *,
    forbidden_patterns: Iterable[str] = DEFAULT_ERROR_PATTERNS,
    allow_patterns: Iterable[str] = (),
) -> dict[str, Any]:
    forbidden = [re.compile(pattern, re.IGNORECASE) for pattern in forbidden_patterns]
    allowed = [re.compile(pattern, re.IGNORECASE) for pattern in allow_patterns]
    findings: list[dict[str, Any]] = []
    for number, line in enumerate(text.splitlines(), 1):
        if any(pattern.search(line) for pattern in allowed):
            continue
        matched = [pattern.pattern for pattern in forbidden if pattern.search(line)]
        if matched:
            findings.append({"line": number, "patterns": matched, "text": line})
    return {"passed": not findings, "findings": findings}


def check_log_file(
    path: Path,
    *,
    forbidden_patterns: Iterable[str] = DEFAULT_ERROR_PATTERNS,
    allow_patterns: Iterable[str] = (),
) -> dict[str, Any]:
    result = check_log_text(
        path.read_text(encoding="utf-8", errors="replace"),
        forbidden_patterns=forbidden_patterns,
        allow_patterns=allow_patterns,
    )
    result["path"] = str(path)
    return result


def _metric(compare: str, name: str, reference: Path, actual: Path) -> tuple[float, float | None]:
    completed = subprocess.run(
        [compare, "-metric", name, str(reference), str(actual), "null:"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    # ImageMagick reports metrics on stderr and returns 1 for a valid mismatch.
    if completed.returncode not in (0, 1):
        raise RuntimeError(f"ImageMagick {name} failed: {completed.stderr.strip()}")
    values = re.findall(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?", completed.stderr)
    if not values:
        raise RuntimeError(f"ImageMagick {name} returned no metric: {completed.stderr.strip()}")
    return float(values[0]), float(values[1]) if len(values) > 1 else None


def _perceptual_hash(magick: str, path: Path) -> int:
    completed = subprocess.run(
        [magick, str(path), "-alpha", "off", "-colorspace", "Gray", "-resize", "9x8!", "-depth", "8", "gray:-"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0 or len(completed.stdout) != 72:
        raise RuntimeError(f"Unable to compute perceptual hash for {path}: {completed.stderr.decode(errors='replace')}")
    result = 0
    for row in range(8):
        offset = row * 9
        for column in range(8):
            result <<= 1
            result |= completed.stdout[offset + column] > completed.stdout[offset + column + 1]
    return result


def _changed_ratio(magick: str, reference: Path, actual: Path) -> float:
    completed = subprocess.run(
        [
            magick,
            str(reference),
            str(actual),
            "-compose",
            "difference",
            "-composite",
            "-threshold",
            "0",
            "-format",
            "%[fx:mean]",
            "info:",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"Unable to count changed pixels: {completed.stderr.strip()}")
    return float(completed.stdout.strip())


def compare_images(
    reference: Path,
    actual: Path,
    *,
    minimum_ssim: float = 0.995,
    maximum_phash: float = 4.0,
    maximum_changed_ratio: float = 0.001,
) -> dict[str, Any]:
    compare = shutil.which("compare")
    identify = shutil.which("identify")
    magick = shutil.which("magick")
    if not compare or not identify or not magick:
        raise RuntimeError("ImageMagick 'compare', 'identify', and 'magick' are required")
    dimensions = subprocess.run(
        [identify, "-format", "%w %h", str(reference)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
        check=True,
    ).stdout.split()
    if len(dimensions) != 2:
        raise RuntimeError(f"Unable to determine dimensions of {reference}")
    actual_dimensions = subprocess.run(
        [identify, "-format", "%w %h", str(actual)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
        check=True,
    ).stdout.split()
    if dimensions != actual_dimensions:
        return {
            "reference": str(reference),
            "actual": str(actual),
            "reference_dimensions": [int(value) for value in dimensions],
            "actual_dimensions": [int(value) for value in actual_dimensions],
            "passed": False,
            "reason": "dimension mismatch",
        }
    pixels = int(dimensions[0]) * int(dimensions[1])
    _, normalized_ssim_distance = _metric(compare, "SSIM", reference, actual)
    if normalized_ssim_distance is None:
        raise RuntimeError("ImageMagick returned no normalized SSIM value")
    ssim = 1.0 - normalized_ssim_distance
    phash = (_perceptual_hash(magick, reference) ^ _perceptual_hash(magick, actual)).bit_count()
    changed_ratio = _changed_ratio(magick, reference, actual)
    absolute_error = changed_ratio * pixels
    passed = ssim >= minimum_ssim and phash <= maximum_phash and changed_ratio <= maximum_changed_ratio
    return {
        "reference": str(reference),
        "actual": str(actual),
        "width": int(dimensions[0]),
        "height": int(dimensions[1]),
        "ssim": ssim,
        "phash": phash,
        "absolute_error_pixels": absolute_error,
        "changed_ratio": changed_ratio,
        "thresholds": {
            "minimum_ssim": minimum_ssim,
            "maximum_phash": maximum_phash,
            "maximum_changed_ratio": maximum_changed_ratio,
        },
        "passed": passed,
    }


def inspect_image(
    path: Path,
    *,
    minimum_entropy: float = 0.01,
    minimum_mean: float = 0.001,
    maximum_mean: float = 0.999,
) -> dict[str, Any]:
    magick = shutil.which("magick")
    if not magick:
        raise RuntimeError("ImageMagick 'magick' is required")
    completed = subprocess.run(
        [
            magick,
            str(path),
            "-alpha",
            "off",
            "-colorspace",
            "Gray",
            "-format",
            "%w %h %[fx:mean] %[entropy]",
            "info:",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"Unable to inspect image {path}: {completed.stderr.strip()}")
    values = completed.stdout.split()
    if len(values) != 4:
        raise RuntimeError(f"Unexpected image inspection result for {path}: {completed.stdout!r}")
    width, height = int(values[0]), int(values[1])
    mean, entropy = float(values[2]), float(values[3])
    passed = width > 0 and height > 0 and minimum_mean <= mean <= maximum_mean and entropy >= minimum_entropy
    return {
        "path": str(path),
        "width": width,
        "height": height,
        "mean": mean,
        "entropy": entropy,
        "thresholds": {
            "minimum_entropy": minimum_entropy,
            "minimum_mean": minimum_mean,
            "maximum_mean": maximum_mean,
        },
        "passed": passed,
    }


def expand_value(value: Any, variables: dict[str, str]) -> Any:
    if isinstance(value, str):
        try:
            return value.format_map(variables)
        except KeyError as error:
            raise ValueError(f"Unknown scenario placeholder: {error.args[0]}") from error
    if isinstance(value, list):
        return [expand_value(item, variables) for item in value]
    if isinstance(value, dict):
        return {key: expand_value(item, variables) for key, item in value.items()}
    return value


def validate_scenario_manifest(raw: dict[str, Any]) -> None:
    if not isinstance(raw, dict):
        raise ValueError("Scenario manifest must be a JSON object")
    if raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"Unsupported scenario schema: {raw.get('schema_version')!r}")
    if not isinstance(raw.get("name"), str) or not raw["name"]:
        raise ValueError("Scenario manifest requires a non-empty name")
    command = raw.get("command")
    if not isinstance(command, list) or not command or not all(isinstance(item, str) for item in command):
        raise ValueError("Scenario manifest command must be a non-empty string list")
    actions = raw.get("actions", [])
    if not isinstance(actions, list):
        raise ValueError("Scenario actions must be a list")
    m14 = raw.get("m14")
    if m14 is not None:
        if not isinstance(m14, dict):
            raise ValueError("Scenario m14 section must be an object")
        event_file = m14.get("event_file")
        if not isinstance(event_file, str) or not event_file:
            raise ValueError("M14 scenarios require an output-relative event_file")
        for field in ("required_events", "forbidden_events", "required_event_order"):
            if field in m14 and (not isinstance(m14[field], list) or not all(isinstance(item, dict) for item in m14[field])):
                raise ValueError(f"M14 {field} must be a list of event match objects")
        if "forbidden_event_names" in m14 and (
            not isinstance(m14["forbidden_event_names"], list)
            or not all(isinstance(item, str) and item for item in m14["forbidden_event_names"])
        ):
            raise ValueError("M14 forbidden_event_names must be a list of non-empty strings")
        if "forbidden_reason_substrings" in m14 and (
            not isinstance(m14["forbidden_reason_substrings"], list)
            or not all(isinstance(item, str) and item for item in m14["forbidden_reason_substrings"])
        ):
            raise ValueError("M14 forbidden_reason_substrings must be a list of non-empty strings")
        if "maximum_repeated_events" in m14 and (
            not isinstance(m14["maximum_repeated_events"], dict)
            or not all(isinstance(value, int) and value >= 1 for value in m14["maximum_repeated_events"].values())
        ):
            raise ValueError("M14 maximum_repeated_events must map event names to positive integers")
    for index, action in enumerate(actions):
        if not isinstance(action, dict) or not isinstance(action.get("type"), str):
            raise ValueError(f"Scenario action {index} must be an object with a type")
        action_type = action["type"]
        if action_type not in SCENARIO_ACTION_TYPES:
            raise ValueError(f"Unsupported scenario action: {action_type!r}")
        if m14 is not None and action_type in FORBIDDEN_M14_ACTION_TYPES:
            raise ValueError(f"M14 scenarios cannot use direct-mutation action {action_type!r}")
        if m14 is not None and action_type == "command":
            raise ValueError("M14 scenarios must use typed controls instead of arbitrary commands")
        if action_type == "m14_assert_events":
            for field in ("required", "forbidden", "required_events", "forbidden_events", "required_event_order"):
                if field in action and (not isinstance(action[field], list)
                                        or not all(isinstance(item, dict) for item in action[field])):
                    raise ValueError(f"M14 event matcher field {field!r} must be a list of objects")
        if m14 is not None and action_type == "m14_advance_clock":
            has_hour = "game_hour" in action
            has_delta = "hours" in action
            if has_hour == has_delta:
                raise ValueError("m14_advance_clock requires exactly one of game_hour or hours")
            try:
                value = float(action["game_hour"] if has_hour else action["hours"])
            except (TypeError, ValueError, OverflowError) as error:
                raise ValueError("m14_advance_clock time must be numeric") from error
            if not math.isfinite(value) or value < 0.0 or (has_hour and value >= 24.0):
                raise ValueError("m14_advance_clock time is outside its valid range")
            if "day" in action and (not isinstance(action["day"], int) or action["day"] < 0):
                raise ValueError("m14_advance_clock day must be a non-negative integer")
        if m14 is not None and action_type == "m14_checkpoint":
            for field in ("expected_day",):
                if field in action and (not isinstance(action[field], int) or action[field] < 1):
                    raise ValueError(f"M14 checkpoint {field} must be a positive integer")
            for field in ("expected_hour", "clock_tolerance_hours"):
                if field in action:
                    try:
                        value = float(action[field])
                    except (TypeError, ValueError, OverflowError) as error:
                        raise ValueError(f"M14 checkpoint {field} must be numeric") from error
                    if not math.isfinite(value) or value < 0.0 or (field == "expected_hour" and value >= 24.0):
                        raise ValueError(f"M14 checkpoint {field} is outside its valid range")
        if m14 is not None and action_type == "m14_observe":
            if not isinstance(action.get("actor"), str) or not action["actor"]:
                raise ValueError("m14_observe requires a stable actor FormKey")
            for field in ("label", "expected_base", "expected_cell"):
                if field in action and (not isinstance(action[field], str) or not action[field]):
                    raise ValueError(f"m14_observe {field} must be a non-empty string")
        if m14 is not None and action_type == "m14_obstruction":
            reference = action.get("reference")
            if not isinstance(reference, str) or not re.fullmatch(r"(?:0x[0-9A-Fa-f]{1,8}|[A-Za-z_][A-Za-z0-9_]*)", reference):
                raise ValueError("m14_obstruction reference must be a FormID or editor ID")
            if action.get("operation") not in ("add", "remove"):
                raise ValueError("m14_obstruction operation must be add or remove")
        if m14 is not None and action_type == "m14_debug_navigation":
            if action.get("mode", "navmesh") != "navmesh":
                raise ValueError("m14_debug_navigation currently supports only navmesh mode")


def _scenario_output_path(output: Path, value: Any, label: str) -> Path:
    relative = Path(str(value))
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"{label} must stay below its output directory: {relative}")
    return output / relative


def _read_m14_events(path: Path) -> list[dict[str, Any]]:
    if not path.is_file():
        return []
    events: list[dict[str, Any]] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if not line.strip():
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError as error:
            raise ValueError(f"Invalid M14 event JSON at {path}:{line_number}: {error}") from error
        if not isinstance(value, dict) or not isinstance(value.get("event"), str):
            raise ValueError(f"Invalid M14 event object at {path}:{line_number}")
        events.append(value)
    return events


def _m14_event_matches(event: dict[str, Any], expected: dict[str, Any]) -> bool:
    return all(event.get(key) == value for key, value in expected.items())


def _validate_m14_events(manifest: dict[str, Any], output: Path) -> dict[str, Any]:
    config = manifest.get("m14")
    if not isinstance(config, dict):
        return {"enabled": False, "passed": True, "events": 0}
    event_path = _scenario_output_path(output, config.get("event_file", "ai-events.jsonl"), "M14 event file")
    events = _read_m14_events(event_path)
    failures: list[str] = []
    if config.get("require_event_file", True) and not event_path.is_file():
        failures.append(f"missing structured AI event stream: {event_path}")
    minimum_count = int(config.get("minimum_event_count", 0))
    maximum_count = config.get("maximum_event_count")
    if len(events) < minimum_count:
        failures.append(f"M14 event count {len(events)} is below {minimum_count}")
    if maximum_count is not None and len(events) > int(maximum_count):
        failures.append(f"M14 event count {len(events)} exceeds {maximum_count}")
    for expected in config.get("required_events", []):
        if not isinstance(expected, dict) or not any(_m14_event_matches(event, expected) for event in events):
            failures.append(f"missing required M14 event: {expected}")
    for forbidden in config.get("forbidden_events", []):
        if not isinstance(forbidden, dict):
            failures.append(f"invalid forbidden M14 event matcher: {forbidden}")
        elif any(_m14_event_matches(event, forbidden) for event in events):
            failures.append(f"forbidden M14 event observed: {forbidden}")

    order = config.get("required_event_order", [])
    cursor = 0
    for expected in order:
        found = next((index for index in range(cursor, len(events))
                      if _m14_event_matches(events[index], expected)), None)
        if found is None:
            failures.append(f"M14 event order requirement was not met after index {cursor}: {expected}")
            break
        cursor = found + 1

    forbidden_names = {
        "direct-teleport", "test-teleport", "scenario-teleport", "tes3-ai", "tes3-fallback",
        "unsupported", "unresolved", "route-loop", "phase-loop", "deferred-m14",
    }
    forbidden_names.update(str(value) for value in config.get("forbidden_event_names", []))
    reason_fragments = {
        "direct-teleport", "test-teleport", "scenario-teleport", "tes3-ai", "tes3-fallback",
        "unsupported", "unresolved-package", "deferred-m14",
    }
    reason_fragments.update(str(value).casefold() for value in config.get("forbidden_reason_substrings", []))
    for index, event in enumerate(events):
        event_name = str(event.get("event", ""))
        reason = str(event.get("reason", "")).casefold()
        if event_name.casefold() in {name.casefold() for name in forbidden_names}:
            failures.append(f"forbidden M14 event type at index {index}: {event_name}")
            break
        if any(fragment in reason for fragment in reason_fragments):
            failures.append(f"forbidden M14 event reason at index {index}: {event.get('reason')}")
            break
        for key, value in event.items():
            if isinstance(value, float) and not math.isfinite(value):
                failures.append(f"non-finite M14 event field at index {index}: {key}")
                break

    maximum_repeated = config.get("maximum_repeated_events", {})
    event_counts = collections.Counter(str(event["event"]) for event in events)
    for event_name, maximum in maximum_repeated.items():
        actual = event_counts.get(str(event_name), 0)
        if actual > int(maximum):
            failures.append(f"M14 event type {event_name!r} repeated {actual} times, maximum is {maximum}")
    if config.get("maximum_route_blocked") is not None:
        actual = event_counts.get("route-blocked", 0)
        if actual > int(config["maximum_route_blocked"]):
            failures.append(f"M14 route-blocked events {actual} exceed {config['maximum_route_blocked']}")
    if config.get("maximum_no_progress_events") is not None:
        no_progress_names = {"route-blocked", "low-process-reconcile-failed", "route-no-progress"}
        actual = sum(1 for event in events if str(event.get("event")) in no_progress_names)
        if actual > int(config["maximum_no_progress_events"]):
            failures.append(
                f"M14 no-progress events {actual} exceed {config['maximum_no_progress_events']}"
            )

    maximum_phase_repeat = config.get("maximum_phase_repeat", 8)
    if maximum_phase_repeat is not None:
        previous: tuple[Any, ...] | None = None
        repeat = 0
        for event in events:
            identity = (
                event.get("event"), event.get("actor"), event.get("package"),
                event.get("from"), event.get("to"), event.get("reason"),
            )
            if event.get("event") == "phase" and identity == previous:
                repeat += 1
                if repeat >= int(maximum_phase_repeat):
                    failures.append(f"M14 phase transition repeated {repeat + 1} times: {identity}")
                    break
            else:
                previous = identity
                repeat = 0
    return {
        "enabled": True,
        "path": str(event_path),
        "events": len(events),
        "event_types": dict(sorted(event_counts.items())),
        "failures": failures,
        "passed": not failures,
    }


def _m14_run_console_commands(commands: list[str], *, environment: dict[str, str], output: Path,
                              timeout: float, settle_seconds: float = 0.25) -> dict[str, Any]:
    """Run the small, typed M14 console control surface through the live UI."""

    executable = shutil.which("xdotool")
    if not executable:
        raise RuntimeError("xdotool is required for M14 console controls")
    if not commands:
        raise ValueError("M14 console control requires at least one command")

    executed: list[list[str]] = []
    outputs: list[str] = []
    exit_code = 0

    def run(arguments: list[str]) -> None:
        nonlocal exit_code
        command = [executable, *arguments]
        executed.append(command)
        completed = subprocess.run(
            command,
            cwd=output,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )
        outputs.append(completed.stdout)
        exit_code = exit_code or completed.returncode

    run(["search", "--onlyvisible", "--name", "OpenMW", "windowfocus", "--sync", "%@"])
    run(["key", "grave"])
    # Console opening is asynchronous; settle focus before the first typed
    # control so a prior UI widget cannot consume part of the command.
    time.sleep(0.35)
    for command in commands:
        run(["type", "--delay", "1", command])
        run(["key", "Return"])
        time.sleep(settle_seconds)
    run(["key", "grave"])
    # Closing the console returns focus to the game asynchronously. Let one
    # settled interval elapse before a following save/checkpoint observes the
    # control's effect.
    time.sleep(settle_seconds)
    # Console focus can remain on the MyGUI layer after a calendar-changing
    # command. Reassert the game window so the next ordinary input (notably
    # F5 in a checkpoint) is delivered to OpenMW.
    run(["search", "--onlyvisible", "--name", "OpenMW", "windowfocus", "--sync", "%@"])
    return {
        "commands": executed,
        "console_commands": commands,
        "exit_code": exit_code,
        "output": "".join(outputs),
        "passed": exit_code == 0,
    }


def _m14_latest_runtime_state(output: Path) -> tuple[dict[str, Any], Path]:
    checkpoints = sorted((path for path in (output / "checkpoints").glob("*.json") if path.is_file()),
                         key=lambda path: path.stat().st_mtime_ns, reverse=True)
    for checkpoint in checkpoints:
        value = json.loads(checkpoint.read_text(encoding="utf-8"))
        state = value.get("runtime_state") if isinstance(value, dict) else None
        if isinstance(state, dict):
            return state, checkpoint
    save = _single_save(output)
    return tes4_state.load_save(save), save


def _m14_days_in_month(year: int, month: int) -> int:
    days = (31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31)
    leap = year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)
    return days[month] + int(month == 1 and leap)


def _m14_add_clock_hours(year: int, month: int, day: int, hour: float, delta: float) -> tuple[int, int, int, float, int]:
    total = hour + delta
    day_delta = math.floor(total / 24.0)
    target_hour = total % 24.0
    target_year, target_month, target_day = year, month, day
    for _ in range(day_delta):
        target_day += 1
        if target_day > _m14_days_in_month(target_year, target_month):
            target_day = 1
            target_month += 1
            if target_month == 12:
                target_month = 0
                target_year += 1
    return target_year, target_month, target_day, target_hour, day_delta


def _m14_current_clock(output: Path, events: list[dict[str, Any]]) -> tuple[int, int, int, float]:
    try:
        state, _ = _m14_latest_runtime_state(output)
        clock = state.get("clock", {})
        if isinstance(clock, dict):
            return (
                int(clock.get("year", 433)),
                int(clock.get("month", 0)),
                int(clock.get("day", 1)),
                float(clock.get("hour", 0.0)),
            )
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError):
        pass
    for event in reversed(events):
        try:
            return 433, 0, 1, float(event["game_hour"])
        except (KeyError, TypeError, ValueError, OverflowError):
            continue
    raise RuntimeError("m14_advance_clock(hours=...) requires a prior checkpoint or AI event clock")


def _start_xvfb(output: Path, width: int, height: int) -> tuple[subprocess.Popen[str], str]:
    executable = shutil.which("Xvfb")
    if not executable:
        raise RuntimeError("Xvfb is required by this scenario")
    for number in range(91, 150):
        display = f":{number}"
        socket = Path(f"/tmp/.X11-unix/X{number}")
        if socket.exists():
            continue
        log = (output / "xvfb.log").open("w", encoding="utf-8")
        process = subprocess.Popen(
            [executable, display, "-screen", "0", f"{width}x{height}x24", "-nolisten", "tcp"],
            stdout=log,
            stderr=subprocess.STDOUT,
            encoding="utf-8",
        )
        for _ in range(50):
            if socket.exists():
                return process, display
            if process.poll() is not None:
                break
            time.sleep(0.02)
        process.terminate()
        process.wait(timeout=5)
        log.close()
    raise RuntimeError("Unable to allocate an Xvfb display")


def _run_action(action: dict[str, Any], *, environment: dict[str, str], output: Path) -> dict[str, Any]:
    action_type = action.get("type")
    started = time.monotonic()
    if action_type == "sleep":
        time.sleep(float(action.get("seconds", 0)))
        return {"type": action_type, "passed": True, "duration_seconds": time.monotonic() - started}
    if action_type in ("gamepad_button", "gamepad_axis"):
        if _VIRTUAL_GAMEPAD is None:
            raise RuntimeError("gamepad action requires virtual_gamepad=true in the scenario")
        if action_type == "gamepad_button":
            button = str(action["value"]).casefold()
            if button not in VirtualGamepad.BUTTONS:
                raise ValueError(f"Unknown gamepad button {button!r}")
            _VIRTUAL_GAMEPAD.event(VirtualGamepad.EV_KEY, VirtualGamepad.BUTTONS[button], 1)
            time.sleep(float(action.get("hold_seconds", 0.08)))
            _VIRTUAL_GAMEPAD.event(VirtualGamepad.EV_KEY, VirtualGamepad.BUTTONS[button], 0)
        else:
            axis = str(action["axis"]).casefold()
            if axis not in VirtualGamepad.AXES:
                raise ValueError(f"Unknown gamepad axis {axis!r}")
            value = max(-1.0, min(1.0, float(action["value"])))
            if axis.endswith("trigger"):
                raw = round(max(0.0, value) * 32767)
            else:
                raw = round(value * (1 if axis.startswith("dpad") else 32767))
            _VIRTUAL_GAMEPAD.event(VirtualGamepad.EV_ABS, VirtualGamepad.AXES[axis], raw)
        return {"type": action_type, "passed": True, "duration_seconds": time.monotonic() - started}
    if action_type == "assert_file":
        pattern = Path(str(action["path_glob"]))
        if pattern.is_absolute() or ".." in pattern.parts:
            raise ValueError(f"Scenario file assertion must stay below its output directory: {pattern}")
        matches = sorted(path for path in output.glob(pattern.as_posix()) if path.is_file())
        expected_count = action.get("expected_count")
        count_ok = bool(matches) if expected_count is None else len(matches) == int(expected_count)
        minimum_size = int(action.get("minimum_size", 0))
        required = [value.encode("ascii") for value in action.get("contains_ascii", [])]
        forbidden = [value.encode("ascii") for value in action.get("forbidden_ascii", [])]
        files = []
        passed = count_ok
        for path in matches:
            data = path.read_bytes()
            missing = [value.decode("ascii") for value in required if value not in data]
            unexpected = [value.decode("ascii") for value in forbidden if value in data]
            file_passed = len(data) >= minimum_size and not missing and not unexpected
            passed = passed and file_passed
            files.append(
                {
                    "path": str(path),
                    "size": len(data),
                    "missing_ascii": missing,
                    "unexpected_ascii": unexpected,
                    "passed": file_passed,
                }
            )
        return {
            "type": action_type,
            "path_glob": pattern.as_posix(),
            "expected_count": expected_count,
            "matches": files,
            "duration_seconds": round(time.monotonic() - started, 6),
            "passed": passed,
        }
    if action_type == "m14_checkpoint":
        checkpoint_path = _scenario_output_path(
            output, action.get("checkpoint", f"checkpoints/{action.get('name', 'checkpoint')}.json"),
            "M14 checkpoint",
        )
        failures: list[str] = []
        save: Path | None = None
        state: dict[str, Any] | None = None
        validation: dict[str, Any] = {"passed": False, "failures": ["checkpoint was not read"]}
        try:
            save = _single_save(output)
            state = tes4_state.load_save(save)
            validation = validate_m14_runtime_state(state)
            clock = state.get("clock", {}) if isinstance(state, dict) else {}
            if isinstance(clock, dict):
                if "expected_day" in action and int(clock.get("day", -1)) != int(action["expected_day"]):
                    failures.append(
                        f"M14 checkpoint calendar day {clock.get('day')} does not match {action['expected_day']}"
                    )
                if "expected_hour" in action:
                    expected_hour = float(action["expected_hour"])
                    tolerance = float(action.get("clock_tolerance_hours", 0.1))
                    actual_hour = float(clock.get("hour", float("nan")))
                    if not math.isfinite(actual_hour) or abs(actual_hour - expected_hour) > tolerance:
                        failures.append(
                            f"M14 checkpoint game hour {actual_hour!r} is outside "
                            f"{expected_hour!r} +/- {tolerance!r}"
                        )
            minimum_actor_count = int(action.get("minimum_actor_count", 0))
            if validation["actor_count"] < minimum_actor_count:
                failures.append(
                    f"M14 checkpoint has {validation['actor_count']} actors, expected at least {minimum_actor_count}"
                )
            required_types = {int(value) for value in action.get("required_package_types", [])}
            observed_types = {int(item.get("package_type", 255)) for item in state.get("actor_ai", [])}
            missing_types = sorted(required_types - observed_types)
            if missing_types:
                failures.append(f"M14 checkpoint is missing package types {missing_types}")
            required_phases = {int(value) for value in action.get("required_phases", [])}
            observed_phases = {int(item.get("phase", 0)) for item in state.get("actor_ai", [])}
            missing_phases = sorted(required_phases - observed_phases)
            if missing_phases:
                failures.append(f"M14 checkpoint is missing phases {missing_phases}")
            required_actors = {str(value) for value in action.get("required_actor_keys", [])}
            observed_actors = {str(item.get("actor", "null")) for item in state.get("actor_ai", [])}
            missing_actors = sorted(required_actors - observed_actors)
            if missing_actors:
                failures.append(f"M14 checkpoint is missing actors {missing_actors}")
            for name, minimum, actual in (
                ("companion", int(action.get("minimum_companion_count", 0)), validation["companion_count"]),
                ("mount", int(action.get("minimum_mount_count", 0)), validation["mount_count"]),
                ("detection vector", int(action.get("minimum_detection_vector_count", 0)),
                 validation["detection_vector_count"]),
            ):
                if actual < minimum:
                    failures.append(f"M14 checkpoint has {actual} {name}s, expected at least {minimum}")
            required_mounted_actor = action.get("required_mounted_actor")
            required_mounted_horse = action.get("required_mounted_horse")
            if required_mounted_actor is not None:
                mounted = [relation for relation in state.get("mounts", [])
                           if bool(relation.get("mounted", False))
                           and str(relation.get("rider", "null")) == str(required_mounted_actor)]
                if required_mounted_horse is not None:
                    mounted = [relation for relation in mounted
                               if str(relation.get("horse", "null")) == str(required_mounted_horse)]
                if not mounted:
                    failures.append(
                        f"M14 checkpoint has no mounted relation for actor {required_mounted_actor}"
                    )
            required_dismounted_actor = action.get("required_dismounted_actor")
            if required_dismounted_actor is not None:
                if any(bool(relation.get("mounted", False))
                       and str(relation.get("rider", "null")) == str(required_dismounted_actor)
                       for relation in state.get("mounts", [])):
                    failures.append(
                        f"M14 checkpoint still has actor {required_dismounted_actor} mounted"
                    )
            if not validation["passed"]:
                failures.extend(str(value) for value in validation["failures"])
        except (OSError, RuntimeError, ValueError) as error:
            failures.append(str(error))
        report = {
            "type": action_type,
            "save": str(save) if save else None,
            "validation": validation,
            "runtime_state": state,
            "failures": failures,
            "duration_seconds": round(time.monotonic() - started, 6),
            "passed": not failures and validation.get("passed", False),
        }
        checkpoint_path.parent.mkdir(parents=True, exist_ok=True)
        write_json(checkpoint_path, report)
        report["checkpoint"] = str(checkpoint_path)
        result = dict(report)
        result.pop("runtime_state", None)
        return result
    if action_type == "m14_assert_events":
        event_file = action.get("event_file", "ai-events.jsonl")
        config = {
            "m14": {
                "event_file": event_file,
                "require_event_file": True,
                "minimum_event_count": action.get("minimum_event_count", 0),
                "maximum_event_count": action.get("maximum_event_count"),
                "required_events": action.get("required_events", action.get("required", [])),
                "forbidden_events": action.get("forbidden_events", action.get("forbidden", [])),
                "required_event_order": action.get("required_event_order", []),
                "forbidden_event_names": action.get("forbidden_event_names", []),
                "forbidden_reason_substrings": action.get("forbidden_reason_substrings", []),
                "maximum_repeated_events": action.get("maximum_repeated_events", {}),
                "maximum_route_blocked": action.get("maximum_route_blocked"),
                "maximum_no_progress_events": action.get("maximum_no_progress_events"),
                "maximum_phase_repeat": action.get("maximum_phase_repeat", 8),
            }
        }
        result = _validate_m14_events(config, output)
        result.update({"type": action_type, "duration_seconds": round(time.monotonic() - started, 6)})
        return result
    if action_type == "m14_observe":
        observation_path = _scenario_output_path(
            output, action.get("observation", f"observations/{action['actor'].replace(':', '_')}.json"),
            "M14 observation",
        )
        source_value = action.get("checkpoint")
        source_path: Path
        if source_value is not None:
            source_path = _scenario_output_path(output, source_value, "M14 observation checkpoint")
            source_value_json = json.loads(source_path.read_text(encoding="utf-8"))
            state = source_value_json.get("runtime_state") if isinstance(source_value_json, dict) else None
            if not isinstance(state, dict):
                raise ValueError(f"M14 observation source has no runtime_state: {source_path}")
        else:
            state, source_path = _m14_latest_runtime_state(output)
        validation = validate_m14_runtime_state(state)
        actor_key = str(action["actor"])
        actor_state = next((item for item in state.get("actor_ai", [])
                            if str(item.get("actor", "")) == actor_key), None)
        failures = list(validation["failures"])
        if actor_state is None:
            failures.append(f"M14 observation actor is absent: {actor_key}")
        else:
            expected_base = action.get("expected_base")
            if expected_base is not None and str(actor_state.get("base", "null")) != str(expected_base):
                failures.append(
                    f"M14 observation actor {actor_key} has base {actor_state.get('base')!r}, "
                    f"expected {expected_base!r}"
                )
            expected_cell = action.get("expected_cell")
            if expected_cell is not None and str(actor_state.get("cell", "null")) != str(expected_cell):
                failures.append(
                    f"M14 observation actor {actor_key} has cell {actor_state.get('cell')!r}, "
                    f"expected {expected_cell!r}"
                )
        events = _read_m14_events(
            _scenario_output_path(output, action.get("event_file", "ai-events.jsonl"), "M14 event file")
        )
        report = {
            "type": action_type,
            "actor": actor_key,
            "label": action.get("label"),
            "source": str(source_path),
            "state": actor_state,
            "clock": state.get("clock"),
            "event_count": len(events),
            "event_types": dict(sorted(collections.Counter(str(event["event"]) for event in events).items())),
            "validation": validation,
            "failures": failures,
            "duration_seconds": round(time.monotonic() - started, 6),
            "passed": not failures,
        }
        write_json(observation_path, report)
        report["observation"] = str(observation_path)
        return report
    if action_type == "m14_advance_clock":
        events = _read_m14_events(output / "ai-events.jsonl")
        if "game_hour" in action:
            target_hour = float(action["game_hour"])
            current_year, current_month, current_day, _ = _m14_current_clock(output, events)
            target_year = int(action.get("year", current_year))
            target_month = int(action.get("month", current_month))
            target_day = int(action.get("day", current_day))
            elapsed_days = 0
        else:
            current_year, current_month, current_day, current_hour = _m14_current_clock(output, events)
            target_year, target_month, target_day, target_hour, elapsed_days = _m14_add_clock_hours(
                current_year, current_month, current_day, current_hour, float(action["hours"])
            )
            if "year" in action:
                target_year = int(action["year"])
            if "month" in action:
                target_month = int(action["month"])
            if "day" in action:
                target_day = int(action["day"])
        commands = []
        # OpenMW's console accepts literal values for `set`, but does not
        # evaluate expressions such as `GameDaysPassed + 1`.  GameDay,
        # GameMonth, GameYear, and GameHour are the authoritative calendar
        # facade used by the Oblivion profile, so advance those fields
        # explicitly when a duration crosses midnight.
        if elapsed_days > 0:
            # Set the complete target date explicitly. DateTimeManager keeps
            # the calendar facade separate from the raw GameHour global, so a
            # large hour literal can advance its internal day without making
            # the corresponding GameDay value persist in a save.
            if target_year != current_year:
                commands.append(f"set GameYear to {target_year}")
            if target_month != current_month:
                commands.append(f"set GameMonth to {target_month}")
            commands.append(f"set GameDay to {target_day}")
            commands.append(f"set GameHour to {target_hour:.9f}")
        else:
            if target_year != current_year:
                commands.append(f"set GameYear to {target_year}")
            if target_month != current_month:
                commands.append(f"set GameMonth to {target_month}")
            if target_day != current_day or "day" in action or "hours" in action:
                commands.append(f"set GameDay to {target_day}")
            commands.append(f"set GameHour to {target_hour:.9f}")
        control = _m14_run_console_commands(
            commands, environment=environment, output=output,
            timeout=float(action.get("timeout_seconds", 30)),
            settle_seconds=float(action.get("settle_seconds", 0.25)),
        )
        control.update({
            "type": action_type,
            "target_year": target_year,
            "target_month": target_month,
            "target_day": target_day if ("day" in action or "hours" in action) else None,
            "target_hour": target_hour,
            "duration_seconds": round(time.monotonic() - started, 6),
        })
        return control
    if action_type == "m14_obstruction":
        operation = str(action["operation"])
        # The fixture supplies a stable reference that is already part of the
        # real cell. "add" enables that obstruction; "remove" disables it.
        console_command = ("enable" if operation == "add" else "disable") + " " + str(action["reference"])
        control = _m14_run_console_commands(
            [console_command], environment=environment, output=output,
            timeout=float(action.get("timeout_seconds", 30)),
            settle_seconds=float(action.get("settle_seconds", 0.25)),
        )
        control.update({
            "type": action_type,
            "operation": operation,
            "reference": str(action["reference"]),
            "duration_seconds": round(time.monotonic() - started, 6),
        })
        return control
    if action_type == "m14_debug_navigation":
        control = _m14_run_console_commands(
            ["togglenavmesh"], environment=environment, output=output,
            timeout=float(action.get("timeout_seconds", 30)),
            settle_seconds=float(action.get("settle_seconds", 0.25)),
        )
        control.update({
            "type": action_type,
            "mode": "navmesh",
            "duration_seconds": round(time.monotonic() - started, 6),
        })
        return control
    if action_type == "screenshot":
        executable = shutil.which("import")
        if not executable:
            raise RuntimeError("ImageMagick 'import' is required for screenshot actions")
        destination = output / str(action["name"])
        destination.parent.mkdir(parents=True, exist_ok=True)
        command = [executable, "-window", str(action.get("window", "root")), str(destination)]
    elif action_type in (
        "key",
        "key_down",
        "key_up",
        "key_held",
        "key_hold",
        "type",
        "type_held",
        "mouse_move",
        "mouse_move_absolute",
        "mouse_click",
        "mouse_down",
        "mouse_up",
        "focus_window",
    ):
        executable = shutil.which("xdotool")
        if not executable:
            raise RuntimeError("xdotool is required for input actions")
        if action_type == "key":
            command = [executable, "key", str(action["value"])]
        elif action_type in ("key_held", "key_hold", "type_held"):
            if action_type in ("key_held", "key_hold"):
                keysyms = [str(action["value"])]
            else:
                aliases = {" ": "space", ".": "period", "-": "minus", "_": "underscore"}
                keysyms = [aliases.get(character, character) for character in str(action["value"])]
                if any(not (keysym.isalnum() or keysym in aliases.values()) for keysym in keysyms):
                    raise ValueError("type_held supports letters, digits, spaces, periods, hyphens, and underscores")
            hold_seconds = float(
                action.get("seconds", 0) if action_type == "key_hold" else action.get("hold_seconds", 0.08)
            )
            pause_seconds = float(action.get("pause_seconds", 0.04))
            if hold_seconds < 0 or pause_seconds < 0:
                raise ValueError("held input timing must use non-negative durations")
            if action_type != "key_hold" and hold_seconds == 0:
                raise ValueError("held input timing must use a positive hold")
            if action_type == "key_hold" and hold_seconds == 0:
                return {
                    "type": action_type,
                    "commands": [],
                    "exit_code": 0,
                    "output": "",
                    "duration_seconds": round(time.monotonic() - started, 6),
                    "passed": True,
                }
            outputs = []
            return_code = 0
            commands = []
            for keysym in keysyms:
                for verb in ("keydown", "keyup"):
                    held_command = [executable, verb, keysym]
                    commands.append(held_command)
                    completed = subprocess.run(
                        held_command,
                        cwd=output,
                        env=environment,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        encoding="utf-8",
                        errors="replace",
                        timeout=float(action.get("timeout_seconds", 30)),
                        check=False,
                    )
                    outputs.append(completed.stdout)
                    return_code = return_code or completed.returncode
                    time.sleep(hold_seconds if verb == "keydown" else pause_seconds)
            return {
                "type": action_type,
                "commands": commands,
                "exit_code": return_code,
                "output": "".join(outputs),
                "duration_seconds": round(time.monotonic() - started, 6),
                "passed": return_code == int(action.get("expected_exit", 0)),
            }
        elif action_type == "key_down":
            command = [executable, "keydown", str(action["value"])]
        elif action_type == "key_up":
            command = [executable, "keyup", str(action["value"])]
        elif action_type == "mouse_move":
            command = [executable, "mousemove_relative", "--", str(action["x"]), str(action["y"])]
        elif action_type == "mouse_move_absolute":
            command = [executable, "mousemove", str(action["x"]), str(action["y"])]
        elif action_type == "mouse_click":
            command = [executable, "click", str(action.get("button", 1))]
        elif action_type == "mouse_down":
            command = [executable, "mousedown", str(action.get("button", 1))]
        elif action_type == "mouse_up":
            command = [executable, "mouseup", str(action.get("button", 1))]
        elif action_type == "focus_window":
            command = [
                executable,
                "search",
                "--onlyvisible",
                "--name",
                str(action["name"]),
                "windowfocus",
                "--sync",
                "%@",
            ]
        else:
            command = [executable, "type", "--delay", str(action.get("delay_ms", 20)), str(action["value"])]
    elif action_type == "command":
        command = [str(value) for value in action["command"]]
    else:
        raise ValueError(f"Unsupported scenario action: {action_type!r}")
    completed = subprocess.run(
        command,
        cwd=output,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        encoding="utf-8",
        errors="replace",
        timeout=float(action.get("timeout_seconds", 30)),
        check=False,
    )
    result = {
        "type": action_type,
        "command": command,
        "exit_code": completed.returncode,
        "output": completed.stdout,
        "duration_seconds": round(time.monotonic() - started, 6),
        "passed": completed.returncode == int(action.get("expected_exit", 0)),
    }
    if action_type == "screenshot" and result["passed"] and action.get("inspect", True):
        result["image_inspection"] = inspect_image(
            destination,
            minimum_entropy=float(action.get("minimum_entropy", 0.01)),
            minimum_mean=float(action.get("minimum_mean", 0.001)),
            maximum_mean=float(action.get("maximum_mean", 0.999)),
        )
        result["passed"] = result["image_inspection"]["passed"]
    return result


def run_scenario(manifest_path: Path, output: Path, variables: dict[str, str]) -> dict[str, Any]:
    global _VIRTUAL_GAMEPAD
    raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    validate_scenario_manifest(raw)
    variables = dict(variables)
    variables.setdefault("source", str(Path(__file__).resolve().parents[1]))
    variables.setdefault("python", sys.executable)
    variables["output"] = str(output)
    variables["manifest"] = str(manifest_path)
    manifest = expand_value(raw, variables)
    output.mkdir(parents=True, exist_ok=True)
    for directory_name in manifest.get("directories", []):
        relative = Path(str(directory_name))
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"Scenario directory must stay below its output directory: {relative}")
        (output / relative).mkdir(parents=True, exist_ok=True)
    for generated in manifest.get("files", []):
        relative = Path(str(generated["path"]))
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"Scenario-generated path must stay below its output directory: {relative}")
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(str(generated["content"]), encoding="utf-8")
    environment = dict(os.environ)
    environment.update({str(key): str(value) for key, value in manifest.get("environment", {}).items()})
    environment.setdefault("OPENMW_SUPPRESS_ERROR_DIALOG", "1")
    m14_config = manifest.get("m14")
    if isinstance(m14_config, dict):
        event_path = _scenario_output_path(output, m14_config.get("event_file", "ai-events.jsonl"), "M14 event file")
        event_path.parent.mkdir(parents=True, exist_ok=True)
        environment["OPENMW_OBLIVION_AI_EVENTS"] = str(event_path)
        if "fixed_seed" in m14_config:
            fixed_seed = int(m14_config["fixed_seed"])
            if fixed_seed <= 0:
                raise ValueError("M14 fixed_seed must be a positive integer")
            environment["OPENMW_OBLIVION_AI_SEED"] = str(fixed_seed)
    xvfb_process: subprocess.Popen[str] | None = None
    started = time.monotonic()
    try:
        if manifest.get("virtual_gamepad", False):
            _VIRTUAL_GAMEPAD = VirtualGamepad()
        if manifest.get("xvfb", False):
            xvfb_process, display = _start_xvfb(
                output, int(manifest.get("width", 1280)), int(manifest.get("height", 720))
            )
            environment["DISPLAY"] = display
            environment.setdefault("SDL_VIDEODRIVER", "x11")
        command = [str(value) for value in manifest["command"]]
        log_path = output / "process.log"
        with log_path.open("w", encoding="utf-8") as log:
            process = subprocess.Popen(
                command,
                cwd=Path(manifest.get("cwd", variables.get("source", "."))),
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
                encoding="utf-8",
            )
            action_results: list[dict[str, Any]] = []
            for action in manifest.get("actions", []):
                if process.poll() is not None:
                    break
                action_results.append(_run_action(action, environment=environment, output=output))
            if manifest.get("terminate_after_actions", False) and process.poll() is None:
                process.send_signal(signal.SIGTERM)
            timed_out = False
            try:
                process.wait(timeout=float(manifest.get("timeout_seconds", 60)))
            except subprocess.TimeoutExpired:
                timed_out = True
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
        log_text = log_path.read_text(encoding="utf-8", errors="replace")
        expected = [str(value) for value in manifest.get("expected_log", [])]
        missing_expected = [pattern for pattern in expected if not re.search(pattern, log_text, re.MULTILINE)]
        forbidden = [str(value) for value in manifest.get("forbidden_log", [])]
        forbidden_findings = check_log_text(log_text, forbidden_patterns=forbidden)["findings"]
        expected_exit = manifest.get("expected_exit", 0)
        exit_ok = process.returncode == expected_exit
        if expected_exit == "timeout":
            exit_ok = timed_out
        passed = (
            exit_ok
            and not missing_expected
            and not forbidden_findings
            and all(result["passed"] for result in action_results)
        )
        m14_result = _validate_m14_events(manifest, output)
        passed = passed and m14_result.get("passed", True)
        result = {
            "schema_version": SCHEMA_VERSION,
            "name": manifest.get("name", manifest_path.stem),
            "manifest": str(manifest_path),
            "command": command,
            "exit_code": process.returncode,
            "expected_exit": expected_exit,
            "timed_out": timed_out,
            "missing_expected_log": missing_expected,
            "forbidden_log_findings": forbidden_findings,
            "actions": action_results,
            "m14": m14_result,
            "duration_seconds": round(time.monotonic() - started, 6),
            "passed": passed,
        }
        write_json(output / "scenario.json", result)
        return result
    finally:
        if _VIRTUAL_GAMEPAD is not None:
            _VIRTUAL_GAMEPAD.close()
            _VIRTUAL_GAMEPAD = None
        if xvfb_process is not None and xvfb_process.poll() is None:
            xvfb_process.terminate()
            try:
                xvfb_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                xvfb_process.kill()
                xvfb_process.wait(timeout=5)


def discover_files(data_dir: Path, suffixes: set[str]) -> list[Path]:
    return sorted(
        (path for path in data_dir.iterdir() if path.is_file() and path.suffix.lower() in suffixes),
        key=lambda path: path.name.casefold(),
    )


def summarize_test_log(text: str) -> dict[str, Any]:
    started: list[str] = []
    passed: list[str] = []
    failed: list[str] = []
    for line in text.splitlines():
        if "TEST_START" in line:
            started.append(line.split("TEST_START", 1)[1].strip())
        elif "TEST_OK" in line:
            passed.append(line.split("TEST_OK", 1)[1].strip())
        elif "TEST_FAILED" in line:
            failed.append(line.split("TEST_FAILED", 1)[1].strip())
    return {
        "started": len(started),
        "passed": len(passed),
        "failed": len(failed),
        "failed_tests": failed,
        "incomplete_tests": sorted(set(started) - set(passed) - set(failed)),
    }


def run_morrowind_regression(openmw: Path, source: Path, build: Path, data: Path, output: Path) -> dict[str, Any]:
    xvfb_run = shutil.which("xvfb-run")
    if not xvfb_run:
        raise RuntimeError("xvfb-run is required for the Morrowind regression gate")
    command = [
        xvfb_run,
        "-a",
        str(openmw),
        "--config",
        str(source / "scripts" / "data" / "morrowind_tests"),
        "--data",
        str(data),
        "--resources",
        str(build / "resources"),
        "--no-sound=1",
    ]
    result = run_command(command, cwd=source, timeout=300)
    output.mkdir(parents=True, exist_ok=True)
    log_path = output / "morrowind-tests.log"
    log_path.write_text(result.pop("output"), encoding="utf-8")
    summary = summarize_test_log(log_path.read_text(encoding="utf-8", errors="replace"))
    result.update(summary)
    result["log"] = str(log_path)
    result["passed_gate"] = (
        result["exit_code"] == 0
        and not result["timed_out"]
        and summary["started"] > 0
        and summary["started"] == summary["passed"]
        and summary["failed"] == 0
        and not summary["incomplete_tests"]
    )
    return result


def _run_logged_gate(command: list[str], source: Path, output: Path, name: str) -> dict[str, Any]:
    result = run_command(command, cwd=source, timeout=300)
    output.mkdir(parents=True, exist_ok=True)
    log_path = output / f"{name}.log"
    log_path.write_text(result.pop("output"), encoding="utf-8")
    result["log"] = str(log_path)
    result["passed"] = result["exit_code"] == 0 and not result["timed_out"]
    return result


def m3_acceptance_passed(
    tests: dict[str, dict[str, Any]],
    scenarios: dict[str, dict[str, Any]],
    morrowind_regression: dict[str, Any],
) -> bool:
    return (
        bool(tests)
        and bool(scenarios)
        and all(result.get("passed", False) for result in tests.values())
        and all(result.get("passed", False) for result in scenarios.values())
        and morrowind_regression.get("passed_gate", False)
    )


def render_m3_acceptance_html(report: dict[str, Any]) -> str:
    rows = []
    for category in ("tests", "scenarios"):
        for name, result in report[category].items():
            rows.append(
                "<tr><td>{}</td><td>{}</td><td>{}</td></tr>".format(
                    html.escape(category),
                    html.escape(name),
                    "PASS" if result.get("passed") else "FAIL",
                )
            )
    regression = report["morrowind_regression"]
    rows.append(
        "<tr><td>regression</td><td>Morrowind integration</td><td>{}</td></tr>".format(
            "PASS" if regression.get("passed_gate") else "FAIL"
        )
    )
    status = "PASS" if report["passed"] else "FAIL"
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>OpenMW Oblivion M3 acceptance</title>
<style>body{{font-family:sans-serif;max-width:1000px;margin:2rem auto}}table{{border-collapse:collapse}}
td,th{{border:1px solid #aaa;padding:.3rem .6rem;text-align:left}}code{{white-space:pre-wrap}}</style></head>
<body><h1>OpenMW Oblivion M3 acceptance: {status}</h1>
<p>Generated {html.escape(report['generated_at'])} from revision
<code>{html.escape(report['repository']['revision'])}</code>.</p>
<table><tr><th>Gate</th><th>Check</th><th>Result</th></tr>{''.join(rows)}</table>
<h2>Content fingerprints</h2><pre>{html.escape(json.dumps(report['content'], indent=2, sort_keys=True))}</pre>
</body></html>"""


def run_m3_acceptance(args: argparse.Namespace) -> dict[str, Any]:
    source = args.source.resolve()
    build = args.build.resolve()
    oblivion_data = args.oblivion_data.resolve()
    morrowind_data = args.morrowind_data.resolve()
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise RuntimeError(f"M3 acceptance output directory must be empty: {output}")

    openmw = build / "openmw"
    openmw_tests = build / "openmw-tests"
    components_tests = build / "components-tests"
    resources = build / "resources"
    oblivion_master = oblivion_data / "Oblivion.esm"
    morrowind_master = morrowind_data / "Morrowind.esm"
    for required in (
        source,
        build,
        openmw,
        openmw_tests,
        components_tests,
        resources,
        oblivion_master,
        morrowind_master,
    ):
        if not required.exists():
            raise FileNotFoundError(required)

    started = time.monotonic()
    tests = {
        "game_profile": _run_logged_gate(
            [str(components_tests), "--gtest_filter=GameProfileTest.*"], source, output / "tests", "game-profile"
        ),
        "oblivion_profile_services": _run_logged_gate(
            [str(openmw_tests), "--gtest_filter=OblivionProfileServicesTest.*"],
            source,
            output / "tests",
            "oblivion-profile-services",
        ),
        "compatibility_harness": _run_logged_gate(
            [
                sys.executable,
                "-m",
                "unittest",
                "discover",
                "-s",
                "scripts/tests",
                "-p",
                "test_oblivion_compat.py",
            ],
            source,
            output / "tests",
            "compatibility-harness",
        ),
    }
    variables = {
        "source": str(source),
        "openmw": str(openmw),
        "resources": str(resources),
        "oblivion_data": str(oblivion_data),
        "morrowind_data": str(morrowind_data),
    }
    manifest_dir = source / "scripts" / "data" / "oblivion_compat"
    scenario_manifests = {
        "oblivion_interior": "oblivion_cell_smoke.json",
        "oblivion_exterior": "oblivion_exterior_smoke.json",
        "wrong_profile": "oblivion_wrong_profile.json",
        "morrowind_visual_save_load": "morrowind_boot_campaign.json",
    }
    scenarios = {
        name: run_scenario(manifest_dir / manifest, output / "scenarios" / name, variables)
        for name, manifest in scenario_manifests.items()
    }
    morrowind_regression = run_morrowind_regression(
        openmw, source, build, morrowind_data, output / "morrowind-integration"
    )
    revision = run_command(["git", "rev-parse", "HEAD"], cwd=source, timeout=10)["output"].strip()
    status = run_command(["git", "status", "--short"], cwd=source, timeout=10)["output"].splitlines()
    report = {
        "schema_version": SCHEMA_VERSION,
        "milestone": "M3",
        "generated_at": utc_now(),
        "duration_seconds": round(time.monotonic() - started, 6),
        "repository": {"source": str(source), "revision": revision, "status": status},
        "content": {
            "oblivion": file_fingerprint(oblivion_master),
            "morrowind": file_fingerprint(morrowind_master),
        },
        "tests": tests,
        "scenarios": scenarios,
        "morrowind_regression": morrowind_regression,
    }
    report["passed"] = m3_acceptance_passed(tests, scenarios, morrowind_regression)
    write_json(output / "acceptance.json", report)
    (output / "acceptance.html").write_text(render_m3_acceptance_html(report), encoding="utf-8")
    return report


def _single_save(directory: Path) -> Path:
    matches = sorted(directory.glob("userdata/saves/*/*.omwsave"))
    if len(matches) != 1:
        raise RuntimeError(f"Expected exactly one OpenMW save below {directory}, found {len(matches)}")
    return matches[0]


def _state_comparison(expected: dict[str, Any], save: Path, report_path: Path) -> dict[str, Any]:
    actual = tes4_state.load_save(save)
    comparison = tes4_state.compare(expected, actual)
    write_json(report_path, comparison)
    return {
        "passed": comparison["passed"],
        "report": str(report_path),
        "expected_sha256": comparison["expected_sha256"],
        "actual_sha256": comparison["actual_sha256"],
        "globals": len(actual["globals"]),
        "references": len(actual["references"]),
        "dynamic_references": sum(item["key"].startswith("dynamic:") for item in actual["references"]),
        "reference_inventories": sum(bool(item["inventory"]) for item in actual["references"]),
    }


def validate_m5_runtime_state(label: str, state: dict[str, Any]) -> dict[str, Any]:
    references = {item["key"]: item for item in state["references"]}
    player_inventory = {item["base"]: item["count"] for item in state["player"]["inventory"]}
    failures: list[str] = []

    def reference(local_id: int) -> dict[str, Any]:
        key = f"content:oblivion.esm:{local_id:06x}"
        if key not in references:
            failures.append(f"missing reference {key}")
            return {"custom_state": {}, "inventory": [], "position": [0.0] * 6}
        return references[key]

    if label == "closed_wall":
        x, y, z = state["player"]["position"][:3]
        if not (630.0 < x < 715.0 and -40.0 <= y < 240.0 and z > -220.0):
            failures.append(f"closed wall did not bound player: position={x},{y},{z}")
    elif label == "wall_open":
        wall = reference(0x1FC41)
        if wall.get("enabled", True) or wall["custom_state"].get("opened") is not True:
            failures.append("secret wall was not opened and disabled")
        x, y = state["player"]["position"][:2]
        if math.hypot(x - 672.0, y + 40.0) <= 50.0:
            failures.append(f"player did not move after opening wall: position={x},{y}")
    elif label == "take":
        item = reference(0x1FC0F)
        if not item.get("deleted") or item["custom_state"].get("taken") is not True:
            failures.append("loose item was not taken from the cell")
        if player_inventory.get("content:oblivion.esm:023f6e", 0) < 1:
            failures.append("taken skull is absent from native player inventory")
    elif label == "container":
        container = reference(0x521E6)
        if container["inventory"] or container["custom_state"].get("opened") is not True:
            failures.append("container inventory was not transferred")
    elif label == "book":
        if reference(0x5E300)["custom_state"].get("read") is not True:
            failures.append("book read state was not recorded")
    elif label == "flora":
        if reference(0x38870)["custom_state"].get("harvested") is not True:
            failures.append("flora harvest state was not recorded")
    elif label == "owned":
        owned = reference(0x564E9)
        if owned.get("deleted") or owned["custom_state"].get("ownership_checked") is not True:
            failures.append("owned item was removed or ownership was not enforced")
    elif label == "locked":
        locked = reference(0x159857)
        if locked["custom_state"].get("lock_checked") is not True or not locked["inventory"]:
            failures.append("locked container changed activation state")
    elif label == "animated_door":
        rotation = reference(0x4D4A5)["position"][5]
        if abs(rotation) < 1.0:
            failures.append(f"animated door did not reach its open angle: rotation={rotation}")
    elif label == "teleport":
        if state["player"]["cell"] != "content:oblivion.esm:01fbb9":
            failures.append(f"teleport did not reach ImperialDungeon01: {state['player']['cell']}")
    elif label == "key_route":
        carrier = reference(0x15985A)
        if carrier["inventory"] or carrier["custom_state"].get("opened") is not True:
            failures.append("tutorial key carrier was not looted")
        if player_inventory.get("content:oblivion.esm:159826", 0) < 1:
            failures.append("tutorial Iron Key is absent from native player inventory")
        if state["player"]["cell"] != "content:oblivion.esm:022ff6":
            failures.append(f"keyed transition did not reach ImperialDungeon04: {state['player']['cell']}")
    else:
        failures.append(f"unknown M5 state check {label}")

    return {
        "passed": not failures,
        "label": label,
        "failures": failures,
        "player_cell": state["player"]["cell"],
        "player_position": state["player"]["position"],
        "player_inventory_items": len(state["player"]["inventory"]),
        "references": len(state["references"]),
    }


def validate_m13_runtime_state(state: dict[str, Any]) -> dict[str, Any]:
    """Validate the deterministic official-item matrix used by both M13 courses."""

    expected_counts = {
        "content:oblivion.esm:017829": 24,  # Arrow1Iron
        "content:oblivion.esm:0105e3": 1,   # MortarPestle
        "content:oblivion.esm:01c6d1": 1,   # IronCuirass
        "content:oblivion.esm:0243d9": 1,   # SKLxArmorer1
        "content:oblivion.esm:0888be": 1,   # novice fire scroll after one use
        "content:oblivion.esm:0229ad": 1,   # MiddleShirt01
        "content:oblivion.esm:03368c": 2,   # Potato after one use
        "content:oblivion.esm:092d8a": 1,   # ImperialPrisonKey
        "content:oblivion.esm:02cf9f": 1,   # Torch02
        "content:oblivion.esm:00000f": 500, # Gold001
        "content:oblivion.esm:00000c": 2,   # RepairHammer
        "content:oblivion.esm:00000a": 5,   # Lockpick
        "content:oblivion.esm:098496": 1,   # restore-health potion after one use
        "content:oblivion.esm:041fa5": 1,   # SSAbsSTRForSTR1
        "content:oblivion.esm:023d67": 1,   # SoulGemEmpty1Petty
        "content:oblivion.esm:000c0c": 1,   # WeapIronLongsword
    }
    expected_slots = {
        "content:oblivion.esm:017829": 1 << 17,
        "content:oblivion.esm:01c6d1": 1 << 2,
        "content:oblivion.esm:02cf9f": 1 << 18,
        "content:oblivion.esm:000c0c": 1 << 16,
    }
    failures: list[str] = []
    inventory = state.get("player", {}).get("inventory", [])
    by_base: dict[str, list[dict[str, Any]]] = {}
    for item in inventory:
        by_base.setdefault(str(item.get("base")), []).append(item)

    if state.get("schema_version") != 4:
        failures.append(f"expected schema version 4, got {state.get('schema_version')}")
    if set(by_base) != set(expected_counts):
        failures.append(
            "item identity set differs: "
            f"missing={sorted(set(expected_counts) - set(by_base))} "
            f"unexpected={sorted(set(by_base) - set(expected_counts))}"
        )
    for base, count in expected_counts.items():
        entries = by_base.get(base, [])
        if len(entries) != 1:
            failures.append(f"{base} has {len(entries)} stacks instead of one")
            continue
        item = entries[0]
        if item.get("count") != count:
            failures.append(f"{base} count={item.get('count')} expected={count}")
        slots = int(item.get("equipped_slots", 0))
        if slots != expected_slots.get(base, 0):
            failures.append(f"{base} equipped_slots={slots} expected={expected_slots.get(base, 0)}")
        if item.get("hotkey", -1) != -1 or item.get("owner", "null") != "null":
            failures.append(f"{base} unexpectedly retained hotkey or owner metadata")

    for base, condition in (
        ("content:oblivion.esm:01c6d1", 300),
        ("content:oblivion.esm:000c0c", 140),
    ):
        if by_base.get(base) and by_base[base][0].get("condition") != condition:
            failures.append(f"{base} condition was not preserved at {condition}")
    for base, entries in by_base.items():
        if entries and float(entries[0].get("charge", -1)) != -1.0:
            failures.append(f"unenchanted matrix item {base} acquired a TES3 enchantment charge")
    torch = by_base.get("content:oblivion.esm:02cf9f", [])
    if torch:
        remaining = float(torch[0].get("remaining_usage_time", -1))
        if not 0 < remaining <= 1000:
            failures.append(f"torch remaining_usage_time={remaining} is outside (0, 1000]")

    occupied = 0
    for entries in by_base.values():
        if not entries:
            continue
        slots = int(entries[0].get("equipped_slots", 0))
        if occupied & slots:
            failures.append("persisted equipment slots overlap")
        occupied |= slots
    return {
        "passed": not failures,
        "failures": failures,
        "schema_version": state.get("schema_version"),
        "item_categories": len(expected_counts),
        "inventory_stacks": len(inventory),
        "equipped_slots": occupied,
    }


def validate_m14_runtime_state(state: dict[str, Any]) -> dict[str, Any]:
    """Validate the persisted native AI contract independently of the engine."""

    failures: list[str] = []
    if not isinstance(state, dict):
        return {
            "passed": False,
            "failures": ["TES4 runtime state is not an object"],
            "schema_version": None,
            "actor_count": 0,
            "overlay_count": 0,
            "companion_count": 0,
            "mount_count": 0,
            "detection_vector_count": 0,
        }

    def collection(name: str) -> list[Any]:
        value = state.get(name, [])
        if not isinstance(value, list):
            failures.append(f"TES4 runtime-state {name} is not a list")
            return []
        if len(value) > tes4_state.MAX_COLLECTION:
            failures.append(f"TES4 runtime-state {name} exceeds the size limit")
            return []
        return value

    def text_value(item: dict[str, Any], name: str, default: str = "null", *, required: bool = False) -> str:
        value = item.get(name, default)
        if not isinstance(value, str):
            failures.append(f"TES4 runtime-state {name} is not a string")
            return default
        if required and (not value or value == "null"):
            failures.append(f"TES4 runtime-state {name} is null or empty")
        return value

    def integer_value(item: dict[str, Any], name: str, default: int, *, minimum: int | None = None,
                      maximum: int | None = None) -> int:
        if name not in item:
            return default
        value = item[name]
        if isinstance(value, bool) or not isinstance(value, int):
            failures.append(f"TES4 runtime-state {name} is not an integer")
            return default
        if minimum is not None and value < minimum or maximum is not None and value > maximum:
            failures.append(f"TES4 runtime-state {name} is outside its valid range")
            return default
        return value

    def real_value(item: dict[str, Any], name: str, default: float = 0.0, *, minimum: float | None = None) -> float:
        if name not in item:
            return default
        value = item[name]
        if isinstance(value, bool):
            failures.append(f"TES4 runtime-state {name} is not numeric")
            return default
        try:
            result = float(value)
        except (TypeError, ValueError, OverflowError):
            failures.append(f"TES4 runtime-state {name} is not numeric")
            return default
        if not math.isfinite(result) or minimum is not None and result < minimum:
            failures.append(f"TES4 runtime-state {name} is outside its valid range")
            return default
        return result

    def boolean_value(item: dict[str, Any], name: str, default: bool = False) -> bool:
        if name not in item:
            return default
        value = item[name]
        if not isinstance(value, bool):
            failures.append(f"TES4 runtime-state {name} is not boolean")
            return default
        return value

    def position_value(item: dict[str, Any], name: str) -> None:
        value = item.get(name)
        if not isinstance(value, list) or len(value) != 6:
            failures.append(f"TES4 runtime-state {name} is not a six-component position")
            return
        for component in value:
            if isinstance(component, bool):
                failures.append(f"TES4 runtime-state {name} contains a boolean component")
                return
            try:
                if not math.isfinite(float(component)):
                    raise ValueError
            except (TypeError, ValueError, OverflowError):
                failures.append(f"TES4 runtime-state {name} contains a non-finite component")
                return

    def schedule_window_value(item: dict[str, Any], name: str) -> None:
        value = item.get(name)
        if value is None:
            return
        if not isinstance(value, dict):
            failures.append(f"TES4 runtime-state {name} is not an object")
            return
        for side in ("start", "end"):
            instant = value.get(side)
            if isinstance(instant, list) and len(instant) == 4:
                year, month, day = instant[:3]
                hour = instant[3]
                if any(isinstance(component, bool) or not isinstance(component, int)
                       for component in (year, month, day)):
                    failures.append(f"TES4 runtime-state {name}.{side} has non-integer date fields")
                    continue
                if isinstance(hour, bool):
                    failures.append(f"TES4 runtime-state {name}.{side} has a non-numeric hour")
                    continue
                try:
                    hour = float(hour)
                except (TypeError, ValueError, OverflowError):
                    failures.append(f"TES4 runtime-state {name}.{side} has a non-numeric hour")
                    continue
            elif isinstance(instant, dict):
                year = integer_value(instant, "year", 1)
                month = integer_value(instant, "month", 0)
                day = integer_value(instant, "day", 1)
                hour = real_value(instant, "hour")
            else:
                failures.append(f"TES4 runtime-state {name}.{side} is not a calendar value")
                continue
            if not tes4_state._valid_calendar([year, month, day, hour]):
                failures.append(f"TES4 runtime-state {name}.{side} is not a valid calendar instant")
        real_value(value, "duration_hours", minimum=0.0)

    if state.get("schema_version") != 5:
        failures.append(f"expected schema version 5, got {state.get('schema_version')}")
    rng = state.get("ai_rng_state", 0)
    if isinstance(rng, bool) or not isinstance(rng, int) or rng <= 0:
        failures.append("AI RNG state is not a non-zero integer")

    procedure_for_type = {index: index + 1 for index in range(13)}
    actor_keys: set[str] = set()
    actor_ai = collection("actor_ai")
    for actor in actor_ai:
        if not isinstance(actor, dict):
            failures.append("TES4 runtime-state actor AI entry is not an object")
            continue
        key = text_value(actor, "actor", required=True)
        if key == "null" or key in actor_keys:
            failures.append(f"duplicate or null actor identity: {key}")
        actor_keys.add(key)
        for identity in ("base", "cell"):
            text_value(actor, identity, required=True)
        source = integer_value(actor, "source", 0, minimum=0, maximum=2)
        package_type = integer_value(actor, "package_type", 255, minimum=0, maximum=255)
        procedure = integer_value(actor, "procedure", 0, minimum=0, maximum=13)
        package = text_value(actor, "package")
        script_package = text_value(actor, "script_package")
        if source == 0:
            if package != "null" or package_type != 255 or procedure != 0:
                failures.append(f"idle actor {key} contains active package state")
        elif source in (1, 2):
            if package == "null" or procedure_for_type.get(package_type) != procedure:
                failures.append(f"actor {key} has inconsistent package/procedure state")
        else:
            failures.append(f"actor {key} has invalid package source {source}")
        if source == 2 and script_package == "null":
            failures.append(f"script-owned actor {key} has no script package")
        pathgrid = text_value(actor, "pathgrid")
        path_node = integer_value(actor, "path_node", 0, minimum=0, maximum=0xffffffff)
        if pathgrid == "null" and path_node != 0:
            failures.append(f"actor {key} has a path node without a pathgrid")
        has_destination = boolean_value(actor, "has_destination")
        destination_cell = text_value(actor, "destination_cell")
        if has_destination and destination_cell == "null":
            failures.append(f"actor {key} has destination coordinates without a destination cell")
        action_reserved = boolean_value(actor, "action_reserved")
        if action_reserved and package_type in (3, 8):
            if text_value(actor, "action_item") == "null":
                failures.append(f"actor {key} reserves an item package without an item")
        for counter in ("selection_generation", "route_generation", "transition_generation"):
            integer_value(actor, counter, 0, minimum=0, maximum=0xffffffffffffffff)
        integer_value(actor, "list_index", 0, minimum=0, maximum=0xffffffff)
        integer_value(actor, "repath_attempts", 0, minimum=0, maximum=8)
        integer_value(actor, "formation_index", -1, minimum=-1, maximum=0x7fffffff)
        phase = integer_value(actor, "phase", 0, minimum=0, maximum=12)
        integer_value(actor, "tier", 0, minimum=0, maximum=1)
        integer_value(actor, "boundary", 0, minimum=0, maximum=3)
        integer_value(actor, "condition_result", 0, minimum=0, maximum=3)
        interruption = text_value(actor, "interruption_reason", "")
        if len(interruption) > 1024:
            failures.append(f"actor {key} interruption reason is too long")
        if phase == 3 and text_value(actor, "door") == "null":
            failures.append(f"actor {key} is in door phase without an intended door")
        for timer in (
            "action_timer", "duration_remaining", "no_progress_seconds", "door_cooldown", "low_process_timer",
            "next_low_process_tick",
        ):
            real_value(actor, timer, minimum=0.0)
        position_value(actor, "destination_position")
        position_value(actor, "last_valid_position")
        schedule_window_value(actor, "schedule_window")

    path_points = collection("path_points")
    overlay_keys: set[tuple[str, int]] = set()
    for point in path_points:
        if not isinstance(point, dict):
            failures.append("TES4 runtime-state path-point overlay is not an object")
            continue
        pathgrid = text_value(point, "pathgrid")
        node = integer_value(point, "node", 0, minimum=0, maximum=0xffffffff)
        boolean_value(point, "enabled", True)
        key = (pathgrid, node)
        if key[0] == "null" or key in overlay_keys:
            failures.append(f"duplicate or null path-point overlay: {key}")
        overlay_keys.add(key)

    actor_ai_by_key = {
        str(actor.get("actor", "null")): actor for actor in actor_ai if isinstance(actor, dict)
    }
    companion_pairs: set[tuple[str, str]] = set()
    companion_edges: dict[str, list[str]] = {}
    companion_groups: dict[str, set[int]] = {}
    companions = collection("companions")
    for relation in companions:
        if not isinstance(relation, dict):
            failures.append("TES4 runtime-state companion relation is not an object")
            continue
        leader = text_value(relation, "leader")
        member = text_value(relation, "member")
        group = text_value(relation, "group", leader)
        side_with = text_value(relation, "side_with")
        formation = integer_value(relation, "formation_index", -1, minimum=-1, maximum=0x7fffffff)
        if (leader == "null" or member == "null" or group == "null" or leader == member
                or (leader, member) in companion_pairs or formation < -1
                or (side_with != "null" and side_with == leader)):
            failures.append(f"invalid or duplicate companion relation: {leader}->{member}")
            continue
        companion_pairs.add((leader, member))
        companion_edges.setdefault(leader, []).append(member)
        member_state = actor_ai_by_key.get(member)
        if member_state is not None and (
            (str(member_state.get("target", "null")) != "null"
             and str(member_state.get("target")) != leader)
            or (str(member_state.get("companion_group", "null")) != "null"
                and str(member_state.get("companion_group")) != group)
            or str(member_state.get("companion_side_with", "null")) != side_with
        ):
            failures.append(f"companion relation {leader}->{member} is not reciprocal")
        if formation >= 0 and formation in companion_groups.setdefault(group, set()):
            failures.append(f"companion group {group} reuses formation index {formation}")
        if formation >= 0:
            companion_groups[group].add(formation)

    visiting: set[str] = set()
    visited: set[str] = set()

    def visit_companion(key: str) -> None:
        if key in visiting:
            failures.append("companion relations contain a cycle")
            return
        if key in visited:
            return
        visiting.add(key)
        for member in companion_edges.get(key, []):
            visit_companion(member)
        visiting.remove(key)
        visited.add(key)

    for leader in sorted(companion_edges):
        visit_companion(leader)

    horses: set[str] = set()
    riders: set[str] = set()
    mount_pairs: set[tuple[str, str]] = set()
    mounts = collection("mounts")
    for relation in mounts:
        if not isinstance(relation, dict):
            failures.append("TES4 runtime-state mount relation is not an object")
            continue
        horse = text_value(relation, "horse")
        rider = text_value(relation, "rider")
        pair = (horse, rider)
        if (horse == "null" or rider == "null" or horse == rider or pair in mount_pairs
                or horse in horses or rider in riders):
            failures.append(f"invalid or duplicate mount relation: {horse}->{rider}")
            continue
        mount_pairs.add(pair)
        horses.add(horse)
        riders.add(rider)
        horse_state = actor_ai_by_key.get(horse)
        rider_state = actor_ai_by_key.get(rider)
        mounted = boolean_value(relation, "mounted")
        if mounted:
            if ((horse_state is not None and str(horse_state.get("rider", "null")) != rider)
                    or (rider_state is not None and str(rider_state.get("mount", "null")) != horse)):
                failures.append(f"mounted relation {horse}->{rider} is not reciprocal")
        elif ((horse_state is not None and str(horse_state.get("rider", "null")) == rider)
              or (rider_state is not None and str(rider_state.get("mount", "null")) == horse)):
            failures.append(f"inactive mount relation {horse}->{rider} has active actor state")

    for key, actor in actor_ai_by_key.items():
        mount = str(actor.get("mount", "null"))
        rider = str(actor.get("rider", "null"))
        if mount in actor_ai_by_key and str(actor_ai_by_key[mount].get("rider", "null")) != key:
            failures.append(f"actor {key} mount state is not reciprocal")
        if rider in actor_ai_by_key and str(actor_ai_by_key[rider].get("mount", "null")) != key:
            failures.append(f"actor {key} rider state is not reciprocal")

    # Test fixtures must observe movement through the native door/action path;
    # a marker that claims direct position teleportation is evidence of a
    # forbidden scenario shortcut and invalidates the save report.
    def has_forbidden_marker(value: Any) -> bool:
        if isinstance(value, dict):
            for name, item in value.items():
                lowered = str(name).casefold().replace("_", "-")
                if lowered in {"direct-teleport", "test-teleport", "scenario-teleport"}:
                    return True
                if has_forbidden_marker(item):
                    return True
        elif isinstance(value, list):
            return any(has_forbidden_marker(item) for item in value)
        return False

    if has_forbidden_marker(state):
        failures.append("runtime state contains a direct-teleport test marker")

    detection_vectors = collection("detection_vectors")
    detection_pairs: set[tuple[str, str]] = set()
    for vector in detection_vectors:
        if not isinstance(vector, dict):
            failures.append("TES4 runtime-state detection vector is not an object")
            continue
        observer = text_value(vector, "observer")
        target = text_value(vector, "target")
        score = real_value(vector, "score", -1.0)
        line_of_sight = boolean_value(vector, "line_of_sight")
        detected = boolean_value(vector, "detected")
        if (observer == "null" or target == "null" or observer == target
                or (observer, target) in detection_pairs):
            failures.append("detection vector identity is null, self-referential, or duplicated")
        detection_pairs.add((observer, target))
        if not math.isfinite(score) or not 0.0 <= score <= 100.0:
            failures.append("detection vector score is outside [0, 100]")
        if not line_of_sight and detected:
            failures.append("occluded detection vector is marked detected")

    return {
        "passed": not failures,
        "failures": failures,
        "schema_version": state.get("schema_version"),
        "actor_count": len(actor_ai),
        "overlay_count": len(path_points),
        "companion_count": len(companions),
        "mount_count": len(mounts),
        "detection_vector_count": len(detection_vectors),
    }


def run_m14_audit(args: argparse.Namespace) -> dict[str, Any]:
    """Audit winning native PACK/PGRD data without starting the engine."""

    source = args.source.resolve()
    data = args.oblivion_data.resolve()
    output = args.output.resolve()
    if not source.is_dir():
        raise FileNotFoundError(source)
    if not data.is_dir():
        raise FileNotFoundError(data)
    count_lock = args.count_lock.resolve() if args.count_lock else None
    report = tes4_m14.audit(
        data,
        OFFICIAL_PLUGIN_ORDER,
        count_lock_path=count_lock,
        write_count_lock=args.write_count_lock,
    )
    report.update(
        {
            "generated_at": utc_now(),
            "repository": {
                "source": str(source),
                "revision": run_command(["git", "rev-parse", "HEAD"], cwd=source, timeout=10)["output"].strip(),
                "status": run_command(["git", "status", "--short"], cwd=source, timeout=10)["output"].splitlines(),
            },
            "oblivion_data": str(data),
            "count_lock_path": str(count_lock) if count_lock else None,
        }
    )
    output.mkdir(parents=True, exist_ok=True)
    write_json(output / "m14-audit.json", report)
    rows = []
    checks = {
        "count lock": report["count_lock"],
        "reachable condition functions": {
            "passed": not report["unsupported"]["condition_functions"],
        },
        "condition run-on contexts": {
            "passed": not report["unsupported"].get("condition_run_on", {}),
        },
        "actor package references": {"passed": not report["unsupported"]["actor_packages"]},
        "routing references": {"passed": not report["unsupported"]["invalid_references"]},
        "cell pathgrid uniqueness": {"passed": not report["ambiguous_pathgrid_cells"]},
    }
    for name, result in checks.items():
        rows.append(
            f"<tr><td>{html.escape(name)}</td><td>{'PASS' if result.get('passed') else 'FAIL'}</td></tr>"
        )
    status = "PASS" if report["passed"] else "FAIL"
    (output / "m14-audit.html").write_text(
        "<!doctype html><html lang='en'><head><meta charset='utf-8'><title>M14 audit</title>"
        "<style>body{font-family:sans-serif;max-width:1100px;margin:2rem auto}"
        "table{border-collapse:collapse}td,th{border:1px solid #aaa;padding:.35rem .7rem}</style></head><body>"
        f"<h1>OpenMW Oblivion M14 audit: {status}</h1>"
        f"<p>Winning PACK: {report['summary']['winning_pack_count']}; "
        f"winning PGRD: {report['summary']['winning_pgrd_count']}; "
        f"pathgrid nodes: {report['summary']['pathgrid_node_count']}.</p>"
        f"<table><tr><th>Gate</th><th>Result</th></tr>{''.join(rows)}</table>"
        f"<p>Package fingerprint: <code>{html.escape(report['package_fingerprint'])}</code><br>"
        f"Pathgrid fingerprint: <code>{html.escape(report['pathgrid_fingerprint'])}</code></p>"
        "</body></html>",
        encoding="utf-8",
    )
    return report


def run_m4_acceptance(args: argparse.Namespace) -> dict[str, Any]:
    source = args.source.resolve()
    build = args.build.resolve()
    oblivion_data = args.oblivion_data.resolve()
    morrowind_data = args.morrowind_data.resolve()
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise RuntimeError(f"M4 acceptance output directory must be empty: {output}")
    output.mkdir(parents=True, exist_ok=True)

    openmw = build / "openmw"
    openmw_tests = build / "openmw-tests"
    components_tests = build / "components-tests"
    resources = build / "resources"
    for required in (
        openmw,
        openmw_tests,
        components_tests,
        resources,
        oblivion_data / "Oblivion.esm",
        morrowind_data / "Morrowind.esm",
    ):
        if not required.exists():
            raise FileNotFoundError(required)

    started = time.monotonic()
    tests = {
        "runtime_state": _run_logged_gate(
            [
                str(components_tests),
                "--gtest_filter=FilesGetHash.sha256*:SavedGameProfile.*:ESM4RuntimeState.*",
            ],
            source,
            output / "tests",
            "runtime-state",
        ),
        "profile_services": _run_logged_gate(
            [str(openmw_tests), "--gtest_filter=OblivionProfileServicesTest.*"],
            source,
            output / "tests",
            "profile-services",
        ),
        "compatibility_harness": _run_logged_gate(
            [sys.executable, "-m", "unittest", "scripts.tests.test_oblivion_compat"],
            source,
            output / "tests",
            "compatibility-harness",
        ),
    }
    variables = {
        "source": str(source),
        "openmw": str(openmw),
        "resources": str(resources),
        "oblivion_data": str(oblivion_data),
        "morrowind_data": str(morrowind_data),
    }
    manifests = source / "scripts" / "data" / "oblivion_compat"
    scenarios: dict[str, dict[str, Any]] = {}
    comparisons: dict[str, dict[str, Any]] = {}

    for label, manifest_name in (
        ("interior", "oblivion_save_smoke.json"),
        ("exterior", "oblivion_exterior_save_smoke.json"),
    ):
        save_dir = output / "scenarios" / f"{label}_save"
        scenarios[f"{label}_save"] = run_scenario(manifests / manifest_name, save_dir, variables)
        original = _single_save(save_dir)
        reload_dir = output / "scenarios" / f"{label}_reload"
        mutated_save = reload_dir / "userdata" / "saves" / original.parent.name / original.name
        expected = tes4_state.mutate_for_acceptance(tes4_state.load_save(original), label)
        tes4_state.write_save(original, mutated_save, expected)
        write_json(reload_dir / "expected-state.json", expected)
        reload_variables = dict(variables, savegame=str(mutated_save))
        scenarios[f"{label}_reload"] = run_scenario(
            manifests / "oblivion_reload_resave.json", reload_dir, reload_variables
        )
        comparisons[label] = _state_comparison(
            expected, _single_save(reload_dir), reload_dir / "state-comparison.json"
        )

    reorder_save_dir = output / "scenarios" / "plugin_reorder_save"
    scenarios["plugin_reorder_save"] = run_scenario(
        manifests / "oblivion_reorder_save.json", reorder_save_dir, variables
    )
    reorder_original = _single_save(reorder_save_dir)
    reorder_reload_dir = output / "scenarios" / "plugin_reorder_reload"
    reorder_mutated = (
        reorder_reload_dir / "userdata" / "saves" / reorder_original.parent.name / reorder_original.name
    )
    reorder_expected = tes4_state.mutate_for_acceptance(tes4_state.load_save(reorder_original), "plugin-reorder")
    tes4_state.write_save(reorder_original, reorder_mutated, reorder_expected)
    write_json(reorder_reload_dir / "expected-state.json", reorder_expected)
    scenarios["plugin_reorder_reload"] = run_scenario(
        manifests / "oblivion_reorder_reload_resave.json",
        reorder_reload_dir,
        dict(variables, savegame=str(reorder_mutated)),
    )
    comparisons["plugin_reorder"] = _state_comparison(
        reorder_expected, _single_save(reorder_reload_dir), reorder_reload_dir / "state-comparison.json"
    )

    diagnostic_source = _single_save(output / "scenarios" / "interior_save")
    diagnostics_dir = output / "diagnostics"
    diagnostics_dir.mkdir(parents=True, exist_ok=True)
    source_state = tes4_state.load_save(diagnostic_source)
    missing_state = json.loads(json.dumps(source_state))
    missing_state["content"].append(
        {"plugin": "openmw-m4-missing.esp", "fingerprint": "sha256:" + "0" * 64}
    )
    missing_save = diagnostics_dir / "missing-content.omwsave"
    tes4_state.write_save(diagnostic_source, missing_save, missing_state)
    bad_state = json.loads(json.dumps(source_state))
    bad_state["content"][0]["fingerprint"] = "sha256:" + "f" * 64
    bad_save = diagnostics_dir / "bad-fingerprint.omwsave"
    tes4_state.write_save(diagnostic_source, bad_save, bad_state)
    corrupt_save = diagnostics_dir / "corrupt.omwsave"
    corrupt_data = bytearray(diagnostic_source.read_bytes())
    magic_offset = corrupt_data.find(tes4_state.MAGIC)
    if magic_offset < 0:
        raise RuntimeError("Saved game has no TES4 runtime-state magic")
    corrupt_data[magic_offset] ^= 0xFF
    corrupt_save.write_bytes(corrupt_data)

    failure_cases = {
        "missing_content": (missing_save, "TES4 runtime state requires missing content file openmw-m4-missing.esp"),
        "bad_fingerprint": (bad_save, "TES4 runtime state content fingerprint mismatch for oblivion.esm"),
        "corrupt_state": (corrupt_save, "Invalid TES4 runtime-state magic"),
    }
    for label, (save, diagnostic) in failure_cases.items():
        scenarios[label] = run_scenario(
            manifests / "oblivion_load_failure.json",
            output / "scenarios" / label,
            dict(
                variables,
                game_data=str(oblivion_data),
                content="Oblivion.esm",
                savegame=str(save),
                diagnostic=diagnostic,
            ),
        )
    scenarios["cross_profile"] = run_scenario(
        manifests / "oblivion_load_failure.json",
        output / "scenarios" / "cross_profile",
        dict(
            variables,
            game_data=str(morrowind_data),
            content="Morrowind.esm",
            savegame=str(diagnostic_source),
            diagnostic="Saved game profile 'oblivion' cannot be loaded by active profile 'morrowind'",
        ),
    )
    scenarios["morrowind_visual_save_load"] = run_scenario(
        manifests / "morrowind_boot_campaign.json",
        output / "scenarios" / "morrowind_visual_save_load",
        variables,
    )
    morrowind_regression = run_morrowind_regression(
        openmw, source, build, morrowind_data, output / "morrowind-integration"
    )

    revision = run_command(["git", "rev-parse", "HEAD"], cwd=source, timeout=10)["output"].strip()
    status = run_command(["git", "status", "--short"], cwd=source, timeout=10)["output"].splitlines()
    report = {
        "schema_version": SCHEMA_VERSION,
        "milestone": "M4",
        "generated_at": utc_now(),
        "duration_seconds": round(time.monotonic() - started, 6),
        "repository": {"source": str(source), "revision": revision, "status": status},
        "content": {
            "oblivion": file_fingerprint(oblivion_data / "Oblivion.esm"),
            "morrowind": file_fingerprint(morrowind_data / "Morrowind.esm"),
        },
        "tests": tests,
        "scenarios": scenarios,
        "state_comparisons": comparisons,
        "morrowind_regression": morrowind_regression,
    }
    report["passed"] = (
        all(item.get("passed", False) for item in tests.values())
        and all(item.get("passed", False) for item in scenarios.values())
        and all(item.get("passed", False) for item in comparisons.values())
        and morrowind_regression.get("passed_gate", False)
    )
    write_json(output / "acceptance.json", report)
    rows = []
    for category in ("tests", "scenarios", "state_comparisons"):
        for name, result in report[category].items():
            rows.append(
                f"<tr><td>{html.escape(category)}</td><td>{html.escape(name)}</td>"
                f"<td>{'PASS' if result.get('passed') else 'FAIL'}</td></tr>"
            )
    rows.append(
        "<tr><td>regression</td><td>Morrowind integration</td><td>{}</td></tr>".format(
            "PASS" if morrowind_regression.get("passed_gate") else "FAIL"
        )
    )
    status_text = "PASS" if report["passed"] else "FAIL"
    (output / "acceptance.html").write_text(
        "<!doctype html><html lang='en'><head><meta charset='utf-8'><title>M4 acceptance</title>"
        "<style>body{font-family:sans-serif;max-width:1100px;margin:2rem auto}table{border-collapse:collapse}"
        "td,th{border:1px solid #aaa;padding:.3rem .6rem}</style></head><body>"
        f"<h1>OpenMW Oblivion M4 acceptance: {status_text}</h1><p>Generated {report['generated_at']}.</p>"
        f"<table><tr><th>Gate</th><th>Check</th><th>Result</th></tr>{''.join(rows)}</table>"
        f"<h2>Content fingerprints</h2><pre>{html.escape(json.dumps(report['content'], indent=2, sort_keys=True))}</pre>"
        "</body></html>",
        encoding="utf-8",
    )
    return report


def run_m5_acceptance(args: argparse.Namespace) -> dict[str, Any]:
    source = args.source.resolve()
    build = args.build.resolve()
    oblivion_data = args.oblivion_data.resolve()
    morrowind_data = args.morrowind_data.resolve()
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise RuntimeError(f"M5 acceptance output directory must be empty: {output}")
    output.mkdir(parents=True, exist_ok=True)

    openmw = build / "openmw"
    openmw_tests = build / "openmw-tests"
    components_tests = build / "components-tests"
    esmtool = build / "esmtool"
    resources = build / "resources"
    required = (
        openmw,
        openmw_tests,
        components_tests,
        esmtool,
        resources,
        oblivion_data / "Oblivion.esm",
        morrowind_data / "Morrowind.esm",
        args.proton.resolve(),
        args.oblivion_install.resolve() / "Oblivion.exe",
        args.original_prefix.resolve(),
    )
    for path in required:
        if not path.exists():
            raise FileNotFoundError(path)

    started = time.monotonic()
    tests = {
        "runtime_state": _run_logged_gate(
            [str(components_tests), "--gtest_filter=ESM4RuntimeState.*:SavedGameProfile.*"],
            source,
            output / "tests",
            "runtime-state",
        ),
        "profile_services": _run_logged_gate(
            [str(openmw_tests), "--gtest_filter=OblivionProfileServicesTest.*"],
            source,
            output / "tests",
            "profile-services",
        ),
        "compatibility_harness": _run_logged_gate(
            [sys.executable, "-m", "unittest", "scripts.tests.test_oblivion_compat"],
            source,
            output / "tests",
            "compatibility-harness",
        ),
    }

    base_variables = {
        "source": str(source),
        "openmw": str(openmw),
        "resources": str(resources),
        "oblivion_data": str(oblivion_data),
        "morrowind_data": str(morrowind_data),
    }
    manifests = source / "scripts" / "data" / "oblivion_compat"
    scenarios: dict[str, dict[str, Any]] = {}
    state_checks: dict[str, dict[str, Any]] = {}

    collision_dir = output / "scenarios" / "closed_wall"
    scenarios["closed_wall"] = run_scenario(
        manifests / "oblivion_m5_collision.json", collision_dir, base_variables
    )
    state_checks["closed_wall"] = validate_m5_runtime_state(
        "closed_wall", tes4_state.load_save(_single_save(collision_dir))
    )

    scenario_specs = {
        "wall_open": {
            "start": "ImperialDungeon01::ref=0x1fc41::side=south",
            "look_lr_key": "KP_6",
            "look_lr_seconds": "0",
            "look_ud_key": "KP_2",
            "look_ud_seconds": "0",
            "approach_seconds": "0",
            "after_seconds": "3",
            "expected_interaction": "M5 interaction: kind=activate result=opened",
        },
        "take": {
            "start": "ImperialDungeon01::ref=0x1fc0f",
            "look_lr_key": "KP_6",
            "look_lr_seconds": "0",
            "look_ud_key": "KP_2",
            "look_ud_seconds": "3.2",
            "approach_seconds": "0",
            "after_seconds": "0",
            "expected_interaction": "M5 interaction: kind=take result=taken",
        },
        "container": {
            "start": "ImperialDungeon01::ref=0x521e6",
            "look_lr_key": "KP_6",
            "look_lr_seconds": "0",
            "look_ud_key": "KP_2",
            "look_ud_seconds": "3.2",
            "approach_seconds": "0",
            "after_seconds": "0",
            "expected_interaction": "M5 interaction: kind=loot result=looted",
        },
        "book": {
            "start": "GoblinJimsCave::ref=0x5e300",
            "look_lr_key": "KP_6",
            "look_lr_seconds": "0.7",
            "look_ud_key": "KP_2",
            "look_ud_seconds": "2.2",
            "approach_seconds": "0",
            "after_seconds": "0",
            "expected_interaction": "M5 interaction: kind=read result=read",
        },
        "flora": {
            "start": "ImperialDungeon04::ref=0x38870",
            "look_lr_key": "KP_6",
            "look_lr_seconds": "0",
            "look_ud_key": "KP_2",
            "look_ud_seconds": "3.2",
            "approach_seconds": "0",
            "after_seconds": "0",
            "expected_interaction": "M5 interaction: kind=harvest result=harvested",
        },
        "owned": {
            # The named reference is the east-side camera anchor.  From that
            # approach the center ray selects the adjacent owned silverware
            # reference 0x564e9 without the unowned 0x564e0 occluding it.
            "start": "AnvilTheCountsArmsPrivateRooms::ref=0x564ee::side=east",
            "look_lr_key": "KP_6",
            "look_lr_seconds": "0",
            "look_ud_key": "KP_2",
            "look_ud_seconds": "2.55",
            "approach_seconds": "0",
            "after_seconds": "0",
            "expected_interaction": (
                "M5 interaction: kind=take result=owned ref=content:oblivion.esm:0564e9"
            ),
        },
        "locked": {
            "start": "ImperialDungeon01::ref=0x159857",
            "look_lr_key": "KP_6",
            "look_lr_seconds": "0",
            "look_ud_key": "KP_2",
            "look_ud_seconds": "3.2",
            "approach_seconds": "0",
            "after_seconds": "0",
            "expected_interaction": "M5 interaction: kind=loot result=locked",
        },
        "animated_door": {
            "start": "BrumaMagesGuildBasement::ref=0x4d4a5",
            "look_lr_key": "KP_6",
            "look_lr_seconds": "1.5",
            "look_ud_key": "KP_2",
            "look_ud_seconds": "0",
            "approach_seconds": "0",
            "after_seconds": "0",
            "expected_interaction": "M5 door activation: result=animated",
        },
        "teleport": {
            "start": "ImperialDungeon04::ref=0x25041",
            "look_lr_key": "KP_6",
            "look_lr_seconds": "0",
            "look_ud_key": "KP_2",
            "look_ud_seconds": "0",
            "approach_seconds": "0",
            "after_seconds": "0",
            "expected_interaction": "M5 door activation: result=teleport",
        },
    }
    for label, values in scenario_specs.items():
        scenario_dir = output / "scenarios" / label
        variables = dict(base_variables, label=label, **values)
        scenarios[label] = run_scenario(
            manifests / "oblivion_m5_interaction.json", scenario_dir, variables
        )
        state_checks[label] = validate_m5_runtime_state(
            label, tes4_state.load_save(_single_save(scenario_dir))
        )

    key_route_dir = output / "scenarios" / "key_route"
    scenarios["key_route"] = run_scenario(
        manifests / "oblivion_m5_key_route.json",
        key_route_dir,
        dict(base_variables, walk_seconds="1.55"),
    )
    state_checks["key_route"] = validate_m5_runtime_state(
        "key_route", tes4_state.load_save(_single_save(key_route_dir))
    )

    scenarios["morrowind_visual_save_load"] = run_scenario(
        manifests / "morrowind_boot_campaign.json",
        output / "scenarios" / "morrowind_visual_save_load",
        base_variables,
    )

    copied_prefix = output / "original-prefix"
    shutil.copytree(args.original_prefix.resolve(), copied_prefix, symlinks=True)
    original_variables = {
        "proton": str(args.proton.resolve()),
        "oblivion_exe": str(args.oblivion_install.resolve() / "Oblivion.exe"),
        "oblivion_install": str(args.oblivion_install.resolve()),
        "steam_root": str(args.steam_root.resolve()),
        "original_prefix": str(copied_prefix),
    }
    scenarios["original_prison_viewpoint"] = run_scenario(
        manifests / "oblivion_m5_original_prison.json",
        output / "scenarios" / "original_prison_viewpoint",
        original_variables,
    )

    original_capture = output / "scenarios" / "original_prison_viewpoint" / "original-prison-start.png"
    original_menu = output / "scenarios" / "original_prison_viewpoint" / "original-main-menu.png"
    original_loaded = output / "scenarios" / "original_prison_viewpoint" / "original-loaded-save.png"
    original_cell_loaded = (
        output / "scenarios" / "original_prison_viewpoint" / "original-prison-cell-loaded.png"
    )
    openmw_capture = output / "scenarios" / "wall_open" / "before.png"
    original_transition = (
        compare_images(original_menu, original_capture)
        if original_menu.is_file() and original_capture.is_file()
        else {"passed": False, "reason": "missing original-game capture"}
    )
    original_scene_changed = bool(
        original_transition.get("ssim", 1.0) < 0.9
        and original_transition.get("changed_ratio", 0.0) > 0.2
    )
    original_cell_transition = (
        compare_images(original_loaded, original_cell_loaded)
        if original_loaded.is_file() and original_cell_loaded.is_file()
        else {"passed": False, "reason": "missing loaded-save or prison-cell capture"}
    )
    original_cell_changed = bool(
        original_cell_transition.get("ssim", 1.0) < 0.9
        and original_cell_transition.get("changed_ratio", 0.0) > 0.5
    )
    original_viewpoint_stability = (
        compare_images(original_cell_loaded, original_capture)
        if original_cell_loaded.is_file() and original_capture.is_file()
        else {"passed": False, "reason": "missing prison viewpoint captures"}
    )
    original_viewpoint_stable = bool(
        original_viewpoint_stability.get("ssim", 0.0) > 0.97
        and original_viewpoint_stability.get("phash", 64) <= 2
    )
    paired_capture = {
        "passed": (
            original_capture.is_file()
            and openmw_capture.is_file()
            and original_scene_changed
            and original_cell_changed
            and original_viewpoint_stable
        ),
        # ImperialDungeon01 is a deliberately dark fixed viewpoint.  In
        # addition to proving that pixels changed, reject the bright animated
        # main menu: its movement otherwise looks like a scene transition to
        # ordinary image-difference metrics.
        "original": (
            inspect_image(original_capture, maximum_mean=0.35)
            if original_capture.is_file()
            else {"passed": False}
        ),
        "openmw": inspect_image(openmw_capture) if openmw_capture.is_file() else {"passed": False},
        "original_menu_to_scene": original_transition,
        "original_scene_changed": original_scene_changed,
        "original_loaded_save_to_prison": original_cell_transition,
        "original_prison_cell_changed": original_cell_changed,
        "original_prison_viewpoint_stability": original_viewpoint_stability,
        "original_prison_viewpoint_stable": original_viewpoint_stable,
    }
    paired_capture["passed"] = bool(
        paired_capture["passed"]
        and paired_capture["original"].get("passed")
        and paired_capture["openmw"].get("passed")
    )

    form_graph = run_form_graph(
        argparse.Namespace(
            source=source,
            esmtool=esmtool,
            oblivion_data=oblivion_data,
            output=output / "form-graph",
            allowlist=None,
            timeout=900,
        )
    )
    morrowind_regression = run_morrowind_regression(
        openmw, source, build, morrowind_data, output / "morrowind-integration"
    )

    revision = run_command(["git", "rev-parse", "HEAD"], cwd=source, timeout=10)["output"].strip()
    status = run_command(["git", "status", "--short"], cwd=source, timeout=10)["output"].splitlines()
    report = {
        "schema_version": SCHEMA_VERSION,
        "milestone": "M5",
        "generated_at": utc_now(),
        "duration_seconds": round(time.monotonic() - started, 6),
        "repository": {"source": str(source), "revision": revision, "status": status},
        "content": {
            "oblivion": file_fingerprint(oblivion_data / "Oblivion.esm"),
            "morrowind": file_fingerprint(morrowind_data / "Morrowind.esm"),
        },
        "tests": tests,
        "scenarios": scenarios,
        "state_checks": state_checks,
        "paired_capture": paired_capture,
        "form_graph": form_graph,
        "morrowind_regression": morrowind_regression,
    }
    report["passed"] = (
        all(item.get("passed", False) for item in tests.values())
        and all(item.get("passed", False) for item in scenarios.values())
        and all(item.get("passed", False) for item in state_checks.values())
        and paired_capture["passed"]
        and form_graph.get("passed", False)
        and morrowind_regression.get("passed_gate", False)
    )
    write_json(output / "acceptance.json", report)
    rows = []
    for category in ("tests", "scenarios", "state_checks"):
        for name, result in report[category].items():
            rows.append(
                f"<tr><td>{html.escape(category)}</td><td>{html.escape(name)}</td>"
                f"<td>{'PASS' if result.get('passed') else 'FAIL'}</td></tr>"
            )
    for name, result in (
        ("paired original/OpenMW capture", paired_capture),
        ("M2 FormKey graph", form_graph),
        ("Morrowind integration", {"passed": morrowind_regression.get("passed_gate")}),
    ):
        rows.append(
            f"<tr><td>regression</td><td>{html.escape(name)}</td>"
            f"<td>{'PASS' if result.get('passed') else 'FAIL'}</td></tr>"
        )
    status_text = "PASS" if report["passed"] else "FAIL"
    (output / "acceptance.html").write_text(
        "<!doctype html><html lang='en'><head><meta charset='utf-8'><title>M5 acceptance</title>"
        "<style>body{font-family:sans-serif;max-width:1100px;margin:2rem auto}table{border-collapse:collapse}"
        "td,th{border:1px solid #aaa;padding:.3rem .6rem}</style></head><body>"
        f"<h1>OpenMW Oblivion M5 acceptance: {status_text}</h1><p>Generated {report['generated_at']}.</p>"
        f"<table><tr><th>Gate</th><th>Check</th><th>Result</th></tr>{''.join(rows)}</table>"
        "</body></html>",
        encoding="utf-8",
    )
    return report


def audit_plugin(esmtool: Path, plugin: Path, source: Path, include_census: bool) -> dict[str, Any]:
    result = file_fingerprint(plugin)
    parsed = run_command([str(esmtool), "-q", "dump", str(plugin)], cwd=source, timeout=120)
    result.update(
        {
            "parse_exit_code": parsed["exit_code"],
            "parse_duration_seconds": parsed["duration_seconds"],
            "parse_timed_out": parsed["timed_out"],
            "parse_output": parsed["output"].splitlines()[-20:],
            "parse_passed": parsed["exit_code"] == 0 and not parsed["timed_out"],
        }
    )
    if not include_census or not result["parse_passed"]:
        return result
    counts: collections.Counter[str] = collections.Counter()
    unsupported: collections.Counter[str] = collections.Counter()
    skipped_calls: collections.Counter[str] = collections.Counter()
    skipped_bytes: collections.Counter[str] = collections.Counter()
    started = time.monotonic()
    process = subprocess.Popen(
        [str(esmtool), "dump", str(plugin)],
        cwd=source,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        encoding="utf-8",
        errors="replace",
    )
    assert process.stdout is not None
    for line in process.stdout:
        match = re.match(r"  Record: ([A-Z0-9_]{4})\s*$", line)
        if match:
            counts[match.group(1)] += 1
        match = re.match(r"  Unsupported record: ([A-Z0-9_]{4})\s*$", line)
        if match:
            unsupported[match.group(1)] += 1
        match = re.match(
            r"  Skipped subrecord: ([A-Z0-9_]{4})/([A-Z0-9_]{4}) calls=([0-9]+) bytes=([0-9]+)\s*$",
            line,
        )
        if match:
            key = f"{match.group(1)}/{match.group(2)}"
            skipped_calls[key] += int(match.group(3))
            skipped_bytes[key] += int(match.group(4))
    process.wait(timeout=30)
    result["census"] = {
        "duration_seconds": round(time.monotonic() - started, 6),
        "exit_code": process.returncode,
        "parsed_record_counts": dict(sorted(counts.items())),
        "unsupported_record_counts": dict(sorted(unsupported.items())),
        "skipped_subrecord_counts": {
            key: {"calls": skipped_calls[key], "bytes": skipped_bytes[key]}
            for key in sorted(skipped_calls)
        },
        "parsed_records": sum(counts.values()),
        "unsupported_records": sum(unsupported.values()),
    }
    return result


def audit_archive(bsatool: Path, archive: Path, source: Path, hash_contents: bool) -> dict[str, Any]:
    result = file_fingerprint(archive, hash_contents=hash_contents)
    started = time.monotonic()
    process = subprocess.Popen(
        [str(bsatool), "list", str(archive)],
        cwd=source,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        encoding="utf-8",
        errors="replace",
    )
    count = 0
    tail: collections.deque[str] = collections.deque(maxlen=20)
    assert process.stdout is not None
    for line in process.stdout:
        tail.append(line.rstrip())
        if line.strip() and not line.startswith("BSA archive"):
            count += 1
    process.wait(timeout=30)
    result.update(
        {
            "list_exit_code": process.returncode,
            "list_duration_seconds": round(time.monotonic() - started, 6),
            "listed_entries": count,
            "list_output_tail": list(tail),
            "list_passed": process.returncode == 0,
        }
    )
    return result


def render_baseline_html(report: dict[str, Any]) -> str:
    plugins = report.get("oblivion", {}).get("plugins", [])
    archives = report.get("oblivion", {}).get("archives", [])
    rows = "".join(
        "<tr><td>{}</td><td>{}</td><td>{}</td><td>{:.3f}</td></tr>".format(
            html.escape(plugin["name"]),
            plugin["size"],
            "PASS" if plugin["parse_passed"] else "FAIL",
            plugin["parse_duration_seconds"],
        )
        for plugin in plugins
    )
    archive_rows = "".join(
        "<tr><td>{}</td><td>{}</td><td>{}</td><td>{:.3f}</td></tr>".format(
            html.escape(archive["name"]),
            archive["size"],
            archive.get("listed_entries", 0),
            archive.get("list_duration_seconds", 0.0),
        )
        for archive in archives
    )
    status = "PASS" if report.get("passed") else "FAIL"
    morrowind = report.get("morrowind_regression")
    morrowind_html = "<p>Not run.</p>"
    if morrowind:
        morrowind_html = "<pre>{}</pre>".format(html.escape(json.dumps(morrowind, indent=2, sort_keys=True)))
    standalone = report.get("standalone_baseline")
    standalone_html = "<p>Not run.</p>"
    if standalone:
        standalone_html = "<pre>{}</pre>".format(html.escape(json.dumps(standalone, indent=2, sort_keys=True)))
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>OpenMW Oblivion baseline</title>
<style>body{{font-family:sans-serif;max-width:1100px;margin:2rem auto}}table{{border-collapse:collapse}}
td,th{{border:1px solid #aaa;padding:.3rem .6rem;text-align:left}}code{{white-space:pre-wrap}}</style></head>
<body><h1>OpenMW Oblivion baseline: {status}</h1>
<p>Generated {html.escape(report['generated_at'])} from revision
<code>{html.escape(report['repository']['revision'])}</code>.</p>
<h2>Content plugins</h2><table><tr><th>Name</th><th>Bytes</th><th>Parse</th><th>Seconds</th></tr>{rows}</table>
<h2>Archives</h2><table><tr><th>Name</th><th>Bytes</th><th>Entries</th><th>Seconds</th></tr>{archive_rows}</table>
<h2>Morrowind regression</h2>{morrowind_html}
<h2>Standalone Oblivion baseline</h2>{standalone_html}
<h2>Summary</h2><pre>{html.escape(json.dumps(report.get('summary', {}), indent=2, sort_keys=True))}</pre>
</body></html>"""


def build_baseline(args: argparse.Namespace) -> dict[str, Any]:
    source = args.source.resolve()
    build = args.build.resolve()
    data = args.oblivion_data.resolve()
    output = args.output.resolve()
    esmtool = build / "esmtool"
    bsatool = build / "bsatool"
    openmw = build / "openmw"
    for required in (source, build, data, esmtool, bsatool):
        if not required.exists():
            raise FileNotFoundError(required)
    plugins = discover_files(data, {".esm", ".esp"})
    archives = discover_files(data, {".bsa"})
    if not any(path.name.casefold() == "oblivion.esm" for path in plugins):
        raise RuntimeError(f"Oblivion.esm is not present in {data}")
    revision = run_command(["git", "rev-parse", "HEAD"], cwd=source, timeout=10)["output"].strip()
    status = run_command(["git", "status", "--short"], cwd=source, timeout=10)["output"].splitlines()
    census_all = args.census_all or args.require_lossless_tes4
    plugin_reports = [
        audit_plugin(
            esmtool,
            plugin,
            source,
            include_census=census_all or plugin.name.casefold() == "oblivion.esm",
        )
        for plugin in plugins
    ]
    archive_reports = [audit_archive(bsatool, archive, source, args.hash_archives) for archive in archives]
    report: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "generated_at": utc_now(),
        "repository": {"source": str(source), "revision": revision, "status": status},
        "host": {
            "platform": platform.platform(),
            "python": sys.version,
            "machine": platform.machine(),
        },
        "tools": {
            "openmw": command_version(openmw, "--version"),
            "esmtool": command_version(esmtool, "--version"),
            "bsatool": command_version(bsatool, "--version"),
            "xvfb": shutil.which("Xvfb"),
            "xdotool": shutil.which("xdotool"),
            "imagemagick_compare": shutil.which("compare"),
            "ffmpeg": shutil.which("ffmpeg"),
        },
        "oblivion": {
            "data_directory": str(data),
            "plugins": plugin_reports,
            "archives": archive_reports,
        },
    }
    plugin_passes = sum(1 for item in plugin_reports if item["parse_passed"])
    archive_passes = sum(1 for item in archive_reports if item["list_passed"])
    if args.morrowind_data:
        report["morrowind_regression"] = run_morrowind_regression(
            openmw,
            source,
            build,
            args.morrowind_data.resolve(),
            output / "morrowind",
        )
    if args.run_standalone:
        report["standalone_baseline"] = run_scenario(
            source / "scripts" / "data" / "oblivion_compat" / "oblivion_standalone_baseline.json",
            output / "standalone",
            {
                "source": str(source),
                "openmw": str(openmw),
                "resources": str(build / "resources"),
                "oblivion_data": str(data),
            },
        )
    censused_plugins = [item for item in plugin_reports if "census" in item]
    unsupported_records = sum(item["census"]["unsupported_records"] for item in censused_plugins)
    census_failures = sum(1 for item in censused_plugins if item["census"]["exit_code"] != 0)
    observed_skips = sorted(
        {
            key
            for item in censused_plugins
            for key in item["census"].get("skipped_subrecord_counts", {})
        }
    )
    allowed_skips: set[str] = set()
    allowlist_path: Path | None = None
    if args.require_lossless_tes4:
        allowlist_path = (
            args.tes4_subrecord_allowlist.resolve()
            if args.tes4_subrecord_allowlist
            else source / "scripts" / "data" / "oblivion_compat" / "tes4_skipped_subrecords.json"
        )
        allowlist = json.loads(allowlist_path.read_text(encoding="utf-8"))
        allowed_skips = {
            f"{item['record']}/{item['subrecord']}"
            for item in allowlist.get("allowed", [])
        }
    unallowlisted_skips = sorted(set(observed_skips) - allowed_skips)
    report["summary"] = {
        "plugins": len(plugin_reports),
        "plugins_parsed": plugin_passes,
        "plugins_censused": len(censused_plugins),
        "census_failures": census_failures,
        "unsupported_records": unsupported_records,
        "observed_skipped_subrecord_types": len(observed_skips),
        "unallowlisted_skipped_subrecords": unallowlisted_skips,
        "archives": len(archive_reports),
        "archives_listed": archive_passes,
    }
    lossless_gate_passed = (
        not args.require_lossless_tes4
        or (
            len(censused_plugins) == len(plugin_reports)
            and census_failures == 0
            and unsupported_records == 0
            and not unallowlisted_skips
        )
    )
    report["lossless_tes4_gate"] = {
        "required": args.require_lossless_tes4,
        "passed": lossless_gate_passed,
        "subrecord_allowlist": str(allowlist_path) if allowlist_path else None,
        "observed_skips": observed_skips,
        "unallowlisted_skips": unallowlisted_skips,
    }
    report["passed"] = (
        plugin_passes == len(plugin_reports)
        and archive_passes == len(archive_reports)
        and lossless_gate_passed
        and (not args.morrowind_data or report["morrowind_regression"]["passed_gate"])
    )
    write_json(output / "baseline.json", report)
    (output / "baseline.html").write_text(render_baseline_html(report), encoding="utf-8")
    return report


def parse_variables(values: list[str]) -> dict[str, str]:
    result: dict[str, str] = {}
    for value in values:
        if "=" not in value:
            raise ValueError(f"Expected NAME=VALUE, got {value!r}")
        key, item = value.split("=", 1)
        result[key] = item
    return result


M10_RECORD_FIELDS = {
    "CLMT": {
        "Model": ("meshes", None),
        "SunTexture": ("textures", ".dds"),
        "SunGlareTexture": ("textures", ".dds"),
    },
    "WTHR": {
        "Model": ("meshes", None),
        "LowerCloudTexture": ("textures", ".dds"),
        "UpperCloudTexture": ("textures", ".dds"),
    },
    "WATR": {"Texture": ("textures", ".dds")},
    "SOUN": {"SoundFile": ("sound", ".mp3")},
    "SNDR": {"SoundFile": ("sound", ".mp3")},
}
M10_MEDIA_EXTENSIONS = {".bik", ".dds", ".flac", ".lip", ".mp3", ".nif", ".ogg", ".wav"}


def normalize_vfs_path(value: str) -> str:
    """Return the case-insensitive path spelling used by the OpenMW VFS."""
    return re.sub(r"/+", "/", value.replace("\\", "/").strip().lstrip("/")).casefold()


def m10_resource_candidates(raw: str, prefix: str, replacement_extension: str | None) -> list[str]:
    """Mirror the native ResourceHelpers prefix/extension/fallback search order."""
    raw_path = normalize_vfs_path(raw)
    prefix = normalize_vfs_path(prefix).rstrip("/")
    marker = f"{prefix}/"
    position = raw_path.find(marker)
    original = raw_path[position:] if position >= 0 else f"{prefix}/{raw_path}"
    original = original.rstrip("/") if not raw.rstrip().endswith(("/", "\\")) else original.rstrip("/") + "/"
    changed = original
    if replacement_extension and not original.endswith("/"):
        suffix = Path(original).suffix
        changed = original[: -len(suffix)] + replacement_extension if suffix else original + replacement_extension
    candidates = [changed]
    if original != changed:
        candidates.append(original)
    if not original.endswith("/"):
        basename = original.rsplit("/", 1)[-1]
        fallback_original = f"{prefix}/{basename}"
        fallback_changed = fallback_original
        if replacement_extension:
            suffix = Path(fallback_original).suffix
            fallback_changed = (
                fallback_original[: -len(suffix)] + replacement_extension
                if suffix
                else fallback_original + replacement_extension
            )
        candidates.append(fallback_changed)
        if fallback_original != fallback_changed:
            candidates.append(fallback_original)
    return list(dict.fromkeys(candidates))


def parse_m10_record_assets(output: str, plugin: str) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    current: dict[str, Any] | None = None
    for line in output.splitlines():
        match = re.fullmatch(r"  Record: ([A-Z0-9_]{4})", line)
        if match:
            record_type = match.group(1)
            current = {"plugin": plugin, "record": record_type, "id": "", "editor_id": ""}
            if record_type in M10_RECORD_FIELDS:
                records.append(current)
            else:
                current = None
            continue
        if current is None:
            continue
        match = re.fullmatch(r"  ([A-Za-z]+): ?(.*)", line)
        if not match:
            continue
        name, value = match.groups()
        if name == "Id":
            current["id"] = value
        elif name == "EditorId":
            current["editor_id"] = value
        elif name in M10_RECORD_FIELDS[current["record"]] and value:
            current.setdefault("assets", []).append({"field": name, "raw": value})
    return records


def resolve_m10_record_assets(records: list[dict[str, Any]], vfs_entries: set[str]) -> list[dict[str, Any]]:
    references: list[dict[str, Any]] = []
    for record in records:
        fields = M10_RECORD_FIELDS[record["record"]]
        for asset in record.get("assets", []):
            prefix, extension = fields[asset["field"]]
            candidates = m10_resource_candidates(asset["raw"], prefix, extension)
            if candidates[0].endswith("/"):
                matches = sorted(
                    entry
                    for entry in vfs_entries
                    if entry.startswith(candidates[0]) and Path(entry).suffix in M10_MEDIA_EXTENSIONS
                )
            else:
                matches = [candidate for candidate in candidates if candidate in vfs_entries][:1]
            references.append(
                {
                    "plugin": record["plugin"],
                    "record": record["record"],
                    "id": record["id"],
                    "editor_id": record["editor_id"],
                    "field": asset["field"],
                    "raw": asset["raw"],
                    "candidates": candidates,
                    "resolved": matches,
                    "passed": bool(matches),
                }
            )
    return references


def validate_m10_asset_exceptions(
    missing_references: list[dict[str, Any]], exceptions: dict[str, Any]
) -> dict[str, Any]:
    rules = exceptions.get("allowed", [])
    matched_counts = [0 for _ in rules]
    unreviewed: list[dict[str, Any]] = []
    for reference in missing_references:
        matching_rules: list[int] = []
        for index, rule in enumerate(rules):
            matches = True
            for field in ("plugin", "record", "id", "editor_id", "field", "raw"):
                if field in rule and reference.get(field) != rule[field]:
                    matches = False
                pattern = rule.get(field + "_pattern")
                if pattern is not None and re.fullmatch(pattern, str(reference.get(field, ""))) is None:
                    matches = False
            if matches:
                matching_rules.append(index)
        if len(matching_rules) == 1:
            matched_counts[matching_rules[0]] += 1
        else:
            unreviewed.append(reference)
    stale_or_changed = []
    for index, rule in enumerate(rules):
        if matched_counts[index] != rule.get("expected_count"):
            stale_or_changed.append(
                {
                    "rule": index,
                    "description": rule.get("description", ""),
                    "expected_count": rule.get("expected_count"),
                    "actual_count": matched_counts[index],
                }
            )
    return {
        "passed": not unreviewed and not stale_or_changed,
        "reviewed_count": sum(matched_counts),
        "unreviewed": unreviewed,
        "stale_or_changed_rules": stale_or_changed,
    }


def _m10_inventory_categories(entries: set[str]) -> dict[str, list[str]]:
    audio_extensions = {".flac", ".mp3", ".ogg", ".wav"}
    return {
        "loading_images": sorted(
            path for path in entries if path.startswith("textures/menus/loading/") and Path(path).suffix == ".dds"
        ),
        "music": sorted(path for path in entries if path.startswith("music/") and Path(path).suffix in audio_extensions),
        "sky_meshes": sorted(path for path in entries if path.startswith("meshes/sky/") and Path(path).suffix == ".nif"),
        "sky_textures": sorted(
            path for path in entries if path.startswith("textures/sky/") and Path(path).suffix == ".dds"
        ),
        "sound_effects": sorted(
            path
            for path in entries
            if path.startswith("sound/")
            and not path.startswith("sound/voice/")
            and Path(path).suffix in audio_extensions
        ),
        "videos": sorted(path for path in entries if path.startswith("video/") and Path(path).suffix == ".bik"),
        "voice_audio": sorted(
            path for path in entries if path.startswith("sound/voice/") and Path(path).suffix in audio_extensions
        ),
        "voice_lip": sorted(path for path in entries if path.startswith("sound/voice/") and Path(path).suffix == ".lip"),
        "water_textures": sorted(
            path for path in entries if path.startswith("textures/water/") and Path(path).suffix == ".dds"
        ),
    }


def m10_count_lock_from_report(report: dict[str, Any]) -> dict[str, Any]:
    return {
        "schema_version": SCHEMA_VERSION,
        "official_content": report["official_content"],
        "expected": {
            "archive_count": report["summary"]["archive_count"],
            "vfs_entry_count": report["summary"]["vfs_entry_count"],
            "record_counts": report["summary"]["record_counts"],
            "reference_count": report["summary"]["reference_count"],
            "directory_reference_count": report["summary"]["directory_reference_count"],
            "resolved_file_count": report["summary"]["resolved_file_count"],
            "reviewed_missing_count": report["summary"]["reviewed_missing_count"],
            "missing_reference_fingerprint": report["summary"]["missing_reference_fingerprint"],
            "inventory_counts": report["summary"]["inventory_counts"],
            "inventory_fingerprint": report["summary"]["inventory_fingerprint"],
            "reference_fingerprint": report["summary"]["reference_fingerprint"],
        },
    }


def validate_m10_asset_count_lock(report: dict[str, Any], count_lock: dict[str, Any]) -> dict[str, Any]:
    actual = m10_count_lock_from_report(report)
    failures: list[str] = []
    if count_lock.get("schema_version") != SCHEMA_VERSION:
        failures.append("count-lock schema version differs")
    if count_lock.get("official_content") != actual["official_content"]:
        failures.append("official plugin identity differs from the M10 count lock")
    expected = count_lock.get("expected", {})
    for name, value in actual["expected"].items():
        if expected.get(name) != value:
            failures.append(f"{name}: expected {expected.get(name)!r}, got {value!r}")
    return {"passed": not failures, "failures": failures}


def run_m10_asset_audit(args: argparse.Namespace) -> dict[str, Any]:
    source = args.source.resolve()
    build = args.build.resolve()
    data = args.oblivion_data.resolve()
    output = args.output.resolve()
    esmtool = build / "esmtool"
    bsatool = build / "bsatool"
    for required in (source, build, data, esmtool, bsatool):
        if not required.exists():
            raise FileNotFoundError(required)

    plugins = [data / name for name in OFFICIAL_PLUGIN_ORDER]
    missing_plugins = [str(path) for path in plugins if not path.is_file()]
    if missing_plugins:
        raise RuntimeError("missing official plugins: " + ", ".join(missing_plugins))

    vfs_entries: set[str] = set()
    archive_reports: list[dict[str, Any]] = []
    for archive in discover_files(data, {".bsa"}):
        result = run_command([str(bsatool), "list", str(archive)], cwd=source, timeout=args.timeout)
        entries = {
            normalize_vfs_path(line)
            for line in result["output"].splitlines()
            if line.strip() and not line.startswith("BSA archive")
        }
        entries.discard("")
        vfs_entries.update(entries)
        archive_reports.append(
            {
                "name": archive.name,
                "exit_code": result["exit_code"],
                "entry_count": len(entries),
                "duration_seconds": result["duration_seconds"],
            }
        )
    for path in data.rglob("*"):
        if path.is_file() and path.suffix.casefold() not in {".bsa", ".esm", ".esp"}:
            vfs_entries.add(normalize_vfs_path(path.relative_to(data).as_posix()))

    records: list[dict[str, Any]] = []
    plugin_reports: list[dict[str, Any]] = []
    record_types = list(M10_RECORD_FIELDS)
    for plugin in plugins:
        command = [str(esmtool), "dump"]
        for record_type in record_types:
            command.extend(["-t", record_type])
        command.append(str(plugin))
        result = run_command(command, cwd=source, timeout=args.timeout)
        parsed = parse_m10_record_assets(result["output"], plugin.name)
        records.extend(parsed)
        plugin_reports.append(
            {
                **file_fingerprint(plugin),
                "exit_code": result["exit_code"],
                "record_count": len(parsed),
                "duration_seconds": result["duration_seconds"],
            }
        )

    references = resolve_m10_record_assets(records, vfs_entries)
    missing_references = [reference for reference in references if not reference["passed"]]
    exceptions_path = args.exceptions.resolve()
    exceptions = json.loads(exceptions_path.read_text(encoding="utf-8"))
    exception_review = validate_m10_asset_exceptions(missing_references, exceptions)
    inventory = _m10_inventory_categories(vfs_entries)
    inventory_paths = sorted({path for paths in inventory.values() for path in paths})
    reference_lines = sorted(
        "\t".join(
            (
                reference["plugin"],
                reference["record"],
                reference["id"],
                reference["field"],
                normalize_vfs_path(reference["raw"]),
                *reference["resolved"],
            )
        )
        for reference in references
    )
    record_counts = dict(sorted(collections.Counter(record["record"] for record in records).items()))
    missing_lines = sorted(
        "\t".join(
            (
                reference["plugin"],
                reference["record"],
                reference["id"],
                reference["editor_id"],
                reference["field"],
                normalize_vfs_path(reference["raw"]),
            )
        )
        for reference in missing_references
    )
    summary = {
        "archive_count": len(archive_reports),
        "vfs_entry_count": len(vfs_entries),
        "record_counts": record_counts,
        "reference_count": len(references),
        "missing_reference_count": len(missing_references),
        "reviewed_missing_count": exception_review["reviewed_count"],
        "missing_reference_fingerprint": "sha256:"
        + hashlib.sha256("\n".join(missing_lines).encode()).hexdigest(),
        "directory_reference_count": sum(reference["candidates"][0].endswith("/") for reference in references),
        "resolved_file_count": sum(len(reference["resolved"]) for reference in references),
        "inventory_counts": {name: len(paths) for name, paths in inventory.items()},
        "inventory_fingerprint": "sha256:" + hashlib.sha256("\n".join(inventory_paths).encode()).hexdigest(),
        "reference_fingerprint": "sha256:" + hashlib.sha256("\n".join(reference_lines).encode()).hexdigest(),
    }
    report: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "generated_at": utc_now(),
        "official_content": [
            {name: plugin[name] for name in ("name", "size", "sha256")} for plugin in plugin_reports
        ],
        "archives": archive_reports,
        "plugins": plugin_reports,
        "references": references,
        "missing_references": missing_references,
        "exception_review": exception_review,
        "exceptions": str(exceptions_path),
        "inventory": inventory,
        "summary": summary,
    }
    count_lock_path = args.count_lock.resolve()
    if count_lock_path.is_file():
        count_lock = json.loads(count_lock_path.read_text(encoding="utf-8"))
        report["count_lock"] = validate_m10_asset_count_lock(report, count_lock)
    else:
        report["count_lock"] = {"passed": False, "failures": [f"missing count lock: {count_lock_path}"]}
    archive_passed = all(item["exit_code"] == 0 for item in archive_reports)
    plugin_passed = all(item["exit_code"] == 0 for item in plugin_reports)
    report["passed"] = (
        archive_passed and plugin_passed and exception_review["passed"] and report["count_lock"]["passed"]
    )
    write_json(output / "m10-assets.json", report)
    return report


def _m11_actor_inventory(entries: set[str]) -> dict[str, list[str]]:
    actor_roots = ("meshes/characters/_male/", "meshes/characters/_1stperson/")
    return {
        "actor_skeletons": sorted(
            path
            for path in entries
            if path.startswith(actor_roots) and Path(path).name.startswith("skeleton") and path.endswith(".nif")
        ),
        "actor_animations": sorted(
            path for path in entries if path.startswith(actor_roots) and path.endswith(".kf")
        ),
        "creature_skeletons": sorted(
            path
            for path in entries
            if path.startswith("meshes/creatures/") and Path(path).name == "skeleton.nif"
        ),
        "creature_animations": sorted(
            path for path in entries if path.startswith("meshes/creatures/") and path.endswith(".kf")
        ),
        "facegen_tri": sorted(
            path for path in entries if path.startswith("meshes/characters/") and path.endswith(".tri")
        ),
        "facegen_egm": sorted(
            path for path in entries if path.startswith("meshes/characters/") and path.endswith(".egm")
        ),
        "facegen_egt": sorted(
            path for path in entries if path.startswith("meshes/characters/") and path.endswith(".egt")
        ),
        "armor_models": sorted(path for path in entries if path.startswith("meshes/armor/") and path.endswith(".nif")),
        "clothing_models": sorted(
            path for path in entries if path.startswith("meshes/clothes/") and path.endswith(".nif")
        ),
        "voice_audio": sorted(
            path
            for path in entries
            if path.startswith("sound/voice/") and Path(path).suffix in {".mp3", ".ogg", ".wav", ".flac"}
        ),
        "voice_lip": sorted(path for path in entries if path.startswith("sound/voice/") and path.endswith(".lip")),
    }


def _parse_m11_races(output: str, plugin: str) -> list[dict[str, Any]]:
    races: list[dict[str, Any]] = []
    current: dict[str, Any] | None = None
    for line in output.splitlines():
        if line == "  Record: RACE":
            current = {
                "plugin": plugin,
                "id": "",
                "editor_id": "",
                "flags": 0,
                "parts": [],
                "facegen": {},
            }
            races.append(current)
            continue
        if line.startswith("  Record: "):
            current = None
            continue
        if current is None:
            continue
        match = re.fullmatch(r"  (Id|EditorId|RaceFlags): ?(.*)", line)
        if match:
            name, value = match.groups()
            if name == "Id":
                current["id"] = value
            elif name == "EditorId":
                current["editor_id"] = value
            else:
                current["flags"] = int(value)
            continue
        match = re.fullmatch(r"  FaceGen(Shape|Texture)Modes(Male|Female): (\d+)(?:/(\d+))?", line)
        if match:
            mode, sex, symmetric, asymmetric = match.groups()
            current["facegen"][f"{mode.casefold()}_{sex.casefold()}"] = [
                int(symmetric),
                *([int(asymmetric)] if asymmetric is not None else []),
            ]
            continue
        match = re.fullmatch(r"  ((?:Head|Body)Part(?:Male|Female))(\d+): (.*)\t(.*)", line)
        if match:
            kind, index, mesh, texture = match.groups()
            current["parts"].append(
                {"kind": kind, "index": int(index), "mesh": mesh.strip(), "texture": texture.strip()}
            )
    return races


def _resolve_m11_race_assets(races: list[dict[str, Any]], entries: set[str]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for race in races:
        for part in race["parts"]:
            for field, prefix, extension in (("mesh", "meshes", None), ("texture", "textures", ".dds")):
                raw = part[field]
                if not raw:
                    continue
                candidates = m10_resource_candidates(raw, prefix, extension)
                resolved = next((candidate for candidate in candidates if candidate in entries), "")
                result.append(
                    {
                        "plugin": race["plugin"],
                        "race": race["editor_id"],
                        "id": race["id"],
                        "part": part["kind"],
                        "index": part["index"],
                        "field": field,
                        "raw": raw,
                        "resolved": resolved,
                        "passed": bool(resolved),
                    }
                )
    return result


M11_BIPED_SLOTS = {
    0: "head",
    1: "hair",
    2: "upper_body",
    3: "lower_body",
    4: "hands",
    5: "feet",
    6: "right_ring",
    7: "left_ring",
    8: "amulet",
    9: "weapon",
    10: "back_weapon",
    11: "side_weapon",
    12: "quiver",
    13: "shield",
    14: "torch",
    15: "tail",
}


def _parse_m11_equipment(output: str, plugin: str) -> list[dict[str, Any]]:
    equipment: list[dict[str, Any]] = []
    current: dict[str, Any] | None = None
    for line in output.splitlines():
        match = re.fullmatch(r"  Record: (ARMO|CLOT)", line)
        if match:
            current = {
                "plugin": plugin,
                "type": match.group(1),
                "id": "",
                "editor_id": "",
                "slots": 0,
                "model_male": "",
                "model_female": "",
            }
            equipment.append(current)
            continue
        if line.startswith("  Record: "):
            current = None
            continue
        if current is None:
            continue
        match = re.fullmatch(r"  (Id|EditorId|BipedSlots|ModelMale|ModelFemale): ?(.*)", line)
        if not match:
            continue
        name, value = match.groups()
        if name == "Id":
            current["id"] = value
        elif name == "EditorId":
            current["editor_id"] = value
        elif name == "BipedSlots":
            current["slots"] = int(value)
        else:
            current[f"model_{name.removeprefix('Model').casefold()}"] = value.strip()
    return equipment


def _resolve_m11_equipment_assets(
    equipment: list[dict[str, Any]], entries: set[str]
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    references: list[dict[str, Any]] = []
    sex_matrix: list[dict[str, Any]] = []
    for record in equipment:
        resolved_models: dict[str, str] = {}
        for sex in ("male", "female"):
            raw = record[f"model_{sex}"]
            if not raw:
                continue
            candidates = m10_resource_candidates(raw, "meshes", ".nif")
            resolved = next((candidate for candidate in candidates if candidate in entries), "")
            resolved_models[sex] = resolved
            references.append(
                {
                    "plugin": record["plugin"],
                    "type": record["type"],
                    "id": record["id"],
                    "editor_id": record["editor_id"],
                    "sex": sex,
                    "raw": raw,
                    "resolved": resolved,
                    "passed": bool(resolved),
                }
            )
        # Oblivion intentionally leaves the female MOD3 empty when the male
        # MODL is shared by both sexes (and vice versa for a few dresses).
        male = resolved_models.get("male") or resolved_models.get("female") or ""
        female = resolved_models.get("female") or resolved_models.get("male") or ""
        sex_matrix.append(
            {
                "plugin": record["plugin"],
                "type": record["type"],
                "id": record["id"],
                "editor_id": record["editor_id"],
                "slots": record["slots"],
                "slot_names": [name for bit, name in M11_BIPED_SLOTS.items() if record["slots"] & (1 << bit)],
                "male": male,
                "female": female,
                "passed": bool(male and female),
            }
        )
    return references, sex_matrix


def m11_count_lock_from_report(report: dict[str, Any]) -> dict[str, Any]:
    return {
        "schema_version": SCHEMA_VERSION,
        "official_content": report["official_content"],
        "expected": {
            "archive_count": report["summary"]["archive_count"],
            "vfs_entry_count": report["summary"]["vfs_entry_count"],
            "inventory_counts": report["summary"]["inventory_counts"],
            "inventory_fingerprint": report["summary"]["inventory_fingerprint"],
            "creature_family_count": report["summary"]["creature_family_count"],
            "creature_family_fingerprint": report["summary"]["creature_family_fingerprint"],
            "voice_pair_count": report["summary"]["voice_pair_count"],
            "unpaired_voice_audio_count": report["summary"]["unpaired_voice_audio_count"],
            "unpaired_voice_lip_count": report["summary"]["unpaired_voice_lip_count"],
            "missing_companion_count": report["summary"]["missing_companion_count"],
            "race_record_count": report["summary"]["race_record_count"],
            "playable_race_count": report["summary"]["playable_race_count"],
            "race_asset_reference_count": report["summary"]["race_asset_reference_count"],
            "missing_race_asset_count": report["summary"]["missing_race_asset_count"],
            "race_asset_fingerprint": report["summary"]["race_asset_fingerprint"],
            "equipment_record_count": report["summary"]["equipment_record_count"],
            "equipment_asset_reference_count": report["summary"]["equipment_asset_reference_count"],
            "missing_equipment_asset_count": report["summary"]["missing_equipment_asset_count"],
            "missing_equipment_sex_model_count": report["summary"]["missing_equipment_sex_model_count"],
            "equipment_slot_counts": report["summary"]["equipment_slot_counts"],
            "equipment_fingerprint": report["summary"]["equipment_fingerprint"],
        },
    }


def run_m11_asset_audit(args: argparse.Namespace) -> dict[str, Any]:
    source = args.source.resolve()
    build = args.build.resolve()
    data = args.oblivion_data.resolve()
    output = args.output.resolve()
    bsatool = build / "bsatool"
    esmtool = build / "esmtool"
    for required in (source, build, data, bsatool, esmtool):
        if not required.exists():
            raise FileNotFoundError(required)

    plugins = [data / name for name in OFFICIAL_PLUGIN_ORDER]
    missing_plugins = [str(path) for path in plugins if not path.is_file()]
    if missing_plugins:
        raise RuntimeError("missing official plugins: " + ", ".join(missing_plugins))

    entries: set[str] = set()
    archives: list[dict[str, Any]] = []
    for archive in discover_files(data, {".bsa"}):
        result = run_command([str(bsatool), "list", str(archive)], cwd=source, timeout=args.timeout)
        listed = {
            normalize_vfs_path(line)
            for line in result["output"].splitlines()
            if line.strip() and not line.startswith("BSA archive")
        }
        listed.discard("")
        entries.update(listed)
        archives.append(
            {
                "name": archive.name,
                "entry_count": len(listed),
                "exit_code": result["exit_code"],
                "duration_seconds": result["duration_seconds"],
            }
        )
    for path in data.rglob("*"):
        if path.is_file() and path.suffix.casefold() not in {".bsa", ".esm", ".esp"}:
            entries.add(normalize_vfs_path(path.relative_to(data).as_posix()))

    races: list[dict[str, Any]] = []
    equipment: list[dict[str, Any]] = []
    plugin_reports: list[dict[str, Any]] = []
    for plugin in plugins:
        result = run_command(
            [str(esmtool), "dump", "-t", "RACE", "-t", "ARMO", "-t", "CLOT", str(plugin)],
            cwd=source,
            timeout=args.timeout,
        )
        parsed = _parse_m11_races(result["output"], plugin.name)
        parsed_equipment = _parse_m11_equipment(result["output"], plugin.name)
        races.extend(parsed)
        equipment.extend(parsed_equipment)
        plugin_reports.append(
            {
                "name": plugin.name,
                "exit_code": result["exit_code"],
                "race_count": len(parsed),
                "equipment_count": len(parsed_equipment),
                "duration_seconds": result["duration_seconds"],
            }
        )
    race_assets = _resolve_m11_race_assets(races, entries)
    missing_race_assets = [reference for reference in race_assets if not reference["passed"]]
    playable_races = [race for race in races if race["flags"] & 1]
    race_lines = sorted(
        "\t".join(
            (
                reference["plugin"],
                reference["race"],
                reference["part"],
                str(reference["index"]),
                reference["field"],
                reference["resolved"],
            )
        )
        for reference in race_assets
    )
    equipment_assets, equipment_sex_matrix = _resolve_m11_equipment_assets(equipment, entries)
    missing_equipment_assets = [reference for reference in equipment_assets if not reference["passed"]]
    missing_equipment_sex_models = [record for record in equipment_sex_matrix if not record["passed"]]
    equipment_slot_counts = {
        name: sum(bool(record["slots"] & (1 << bit)) for record in equipment)
        for bit, name in M11_BIPED_SLOTS.items()
    }
    equipment_lines = sorted(
        "\t".join(
            (
                record["plugin"],
                record["type"],
                record["id"],
                str(record["slots"]),
                record["male"],
                record["female"],
            )
        )
        for record in equipment_sex_matrix
    )

    inventory = _m11_actor_inventory(entries)
    creature_families: list[dict[str, Any]] = []
    for skeleton in inventory["creature_skeletons"]:
        directory = skeleton.rsplit("/", 1)[0] + "/"
        groups = sorted(
            Path(path).stem
            for path in inventory["creature_animations"]
            if path.startswith(directory) and "/" not in path[len(directory) :]
        )
        creature_families.append(
            {
                "directory": directory,
                "skeleton": skeleton,
                "animation_count": len(groups),
                "has_idle": "idle" in groups,
                "has_death": "death" in groups or any(group.startswith("death") for group in groups),
                "groups": groups,
            }
        )

    # These three 32x32 EGTs are shared color grids sampled by several body
    # meshes; unlike head EGTs, their basename intentionally is not a NIF.
    shared_texture_grids = {
        "meshes/characters/_male/body.egt",
        "meshes/characters/_male/upperbodyhumanfemale.egt",
        "meshes/characters/_male/upperbodyhumanmale.egt",
    }
    companions: list[dict[str, str]] = []
    for extension in (".tri", ".egm", ".egt"):
        for path in sorted(entry for entry in entries if entry.startswith("meshes/") and entry.endswith(extension)):
            nif = path[: -len(extension)] + ".nif"
            if nif not in entries and path not in shared_texture_grids:
                companions.append({"asset": path, "expected": nif})

    audio_stems = {str(Path(path).with_suffix("")) for path in inventory["voice_audio"]}
    lip_stems = {str(Path(path).with_suffix("")) for path in inventory["voice_lip"]}
    paired = audio_stems & lip_stems
    inventory_paths = sorted(path for values in inventory.values() for path in values)
    family_lines = [
        f'{item["directory"]}\t{item["animation_count"]}\t{int(item["has_idle"])}\t{int(item["has_death"])}'
        for item in creature_families
    ]
    summary = {
        "archive_count": len(archives),
        "vfs_entry_count": len(entries),
        "inventory_counts": {name: len(paths) for name, paths in inventory.items()},
        "inventory_fingerprint": "sha256:" + hashlib.sha256("\n".join(inventory_paths).encode()).hexdigest(),
        "creature_family_count": len(creature_families),
        "creature_family_fingerprint": "sha256:" + hashlib.sha256("\n".join(family_lines).encode()).hexdigest(),
        "voice_pair_count": len(paired),
        "unpaired_voice_audio_count": len(audio_stems - lip_stems),
        "unpaired_voice_lip_count": len(lip_stems - audio_stems),
        "missing_companion_count": len(companions),
        "race_record_count": len(races),
        "playable_race_count": len(playable_races),
        "race_asset_reference_count": len(race_assets),
        "missing_race_asset_count": len(missing_race_assets),
        "race_asset_fingerprint": "sha256:" + hashlib.sha256("\n".join(race_lines).encode()).hexdigest(),
        "equipment_record_count": len(equipment),
        "equipment_asset_reference_count": len(equipment_assets),
        "missing_equipment_asset_count": len(missing_equipment_assets),
        "missing_equipment_sex_model_count": len(missing_equipment_sex_models),
        "equipment_slot_counts": equipment_slot_counts,
        "equipment_fingerprint": "sha256:"
        + hashlib.sha256("\n".join(equipment_lines).encode()).hexdigest(),
    }
    report: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "generated_at": utc_now(),
        "official_content": [
            {name: fingerprint[name] for name in ("name", "size", "sha256")}
            for fingerprint in (file_fingerprint(plugin) for plugin in plugins)
        ],
        "archives": archives,
        "plugins": plugin_reports,
        "inventory": inventory,
        "races": races,
        "race_asset_references": race_assets,
        "missing_race_assets": missing_race_assets,
        "equipment": equipment,
        "equipment_asset_references": equipment_assets,
        "missing_equipment_assets": missing_equipment_assets,
        "equipment_sex_matrix": equipment_sex_matrix,
        "missing_equipment_sex_models": missing_equipment_sex_models,
        "creature_families": creature_families,
        "missing_companions": companions,
        "shared_body_texture_grids": sorted(shared_texture_grids),
        "summary": summary,
    }
    count_lock_path = args.count_lock.resolve()
    if args.write_count_lock:
        write_json(count_lock_path, m11_count_lock_from_report(report))
    if count_lock_path.is_file():
        expected = json.loads(count_lock_path.read_text(encoding="utf-8"))
        actual = m11_count_lock_from_report(report)
        failures = []
        if expected != actual:
            failures.append("official M11 actor asset inventory differs from its count lock")
        report["count_lock"] = {"passed": not failures, "failures": failures}
    else:
        report["count_lock"] = {"passed": False, "failures": [f"missing count lock: {count_lock_path}"]}
    required_actor_skeletons = {
        "meshes/characters/_male/skeleton.nif",
        "meshes/characters/_male/skeletonbeast.nif",
        "meshes/characters/_1stperson/skeleton.nif",
    }
    report["checks"] = {
        "required_actor_skeletons": required_actor_skeletons.issubset(inventory["actor_skeletons"]),
        "actor_idle": any(Path(path).name == "idle.kf" for path in inventory["actor_animations"]),
        "actor_death": any(Path(path).name.startswith("death") for path in inventory["actor_animations"]),
        "creature_animation_families": all(item["animation_count"] > 0 for item in creature_families),
        "facegen_companions": not companions,
        "voice_lip_pairs": bool(paired),
        "playable_race_sexes": len(playable_races) >= 10
        and all(
            # TES4 stores one shared nine-entry head table. Sex-specific ears
            # occupy indices 1 and 2; there is no second female head table.
            # Beast races intentionally omit the separate human ear meshes.
            sum(part["kind"] == "HeadPartMale" and bool(part["mesh"]) for part in race["parts"]) >= 7
            # Most humanoid body table entries select only a skin texture;
            # the actual naked-body geometry comes from the biped slot mesh.
            and any(
                part["kind"] == "BodyPartMale" and bool(part["mesh"] or part["texture"])
                for part in race["parts"]
            )
            and any(
                part["kind"] == "BodyPartFemale" and bool(part["mesh"] or part["texture"])
                for part in race["parts"]
            )
            for race in playable_races
        ),
        "playable_race_facegen": len(playable_races) >= 10
        and all(
            race["facegen"]
            == {
                "shape_male": [50, 30],
                # TES4 stores one common race basis rather than the separate
                # male/female arrays introduced in later file formats.
                "shape_female": [0, 0],
                "texture_male": [50],
                "texture_female": [0],
            }
            for race in playable_races
        ),
        "race_assets_resolve": not missing_race_assets,
        "equipment_assets_resolve": not missing_equipment_assets,
        "equipment_sex_matrix": not missing_equipment_sex_models,
        "equipment_biped_slots": all(
            equipment_slot_counts[name] > 0
            for name in (
                "head",
                "hair",
                "upper_body",
                "lower_body",
                "hands",
                "feet",
                "right_ring",
                "left_ring",
                "amulet",
                "shield",
                "tail",
            )
        ),
    }
    report["passed"] = (
        all(archive["exit_code"] == 0 for archive in archives)
        and all(plugin["exit_code"] == 0 for plugin in plugin_reports)
        and all(report["checks"].values())
        and report["count_lock"]["passed"]
    )
    write_json(output / "m11-assets.json", report)
    return report


def validate_form_graph_report(report: dict[str, Any], allowlist: dict[str, Any]) -> dict[str, Any]:
    unresolved = report.get("unresolved", [])
    rules = allowlist.get("allowed", [])
    if not isinstance(unresolved, list) or not isinstance(rules, list):
        raise ValueError("Form graph report and allowlist must contain arrays")

    match_fields = ("source", "target", "plugin", "record", "subrecord", "reason")
    matched_counts = [0] * len(rules)
    unreviewed: list[dict[str, Any]] = []
    for edge in unresolved:
        matches = [
            index
            for index, rule in enumerate(rules)
            if all(field not in rule or rule[field] == edge.get(field) for field in match_fields)
        ]
        if not matches:
            unreviewed.append(edge)
            continue
        # Rules are ordered from narrow to broad and each edge is charged to
        # the first matching rule, making expected counts deterministic.
        matched_counts[matches[0]] += 1

    stale_or_changed = []
    for index, rule in enumerate(rules):
        expected = rule.get("expected_count")
        if expected is not None and expected != matched_counts[index]:
            stale_or_changed.append(
                {
                    "rule": index,
                    "description": rule.get("description", ""),
                    "expected_count": expected,
                    "actual_count": matched_counts[index],
                }
            )

    cycles = report.get("enable_parent_cycles", [])
    passed = (
        report.get("restart_stable") is True
        and report.get("runtime_reorder_stable") is True
        and not cycles
        and not unreviewed
        and not stale_or_changed
    )
    return {
        "passed": passed,
        "key_count": report.get("key_count"),
        "revision_count": report.get("revision_count"),
        "reference_count": report.get("reference_count"),
        "fingerprint": report.get("fingerprint"),
        "restart_stable": report.get("restart_stable"),
        "runtime_reorder_stable": report.get("runtime_reorder_stable"),
        "unresolved_count": len(unresolved),
        "reviewed_exception_count": sum(matched_counts),
        "unreviewed": unreviewed,
        "exception_rule_counts": matched_counts,
        "stale_or_changed_rules": stale_or_changed,
        "enable_parent_cycles": cycles,
    }


def render_form_graph_html(result: dict[str, Any], allowlist: dict[str, Any]) -> str:
    rows = []
    counts = result.get("exception_rule_counts", [])
    for index, rule in enumerate(allowlist.get("allowed", [])):
        rows.append(
            "<tr>"
            f"<td>{index + 1}</td>"
            f"<td><code>{html.escape(str(rule.get('target', '')))}</code></td>"
            f"<td>{counts[index] if index < len(counts) else 0}</td>"
            f"<td>{html.escape(str(rule.get('description', '')))}</td>"
            "</tr>"
        )
    status = "PASS" if result.get("passed") else "FAIL"
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>M2 FormKey graph {status}</title>
<style>body{{font:16px sans-serif;max-width:1200px;margin:2rem auto;padding:0 1rem}}code{{font-family:monospace}}
table{{border-collapse:collapse;width:100%}}th,td{{border:1px solid #aaa;padding:.45rem;text-align:left}}
.pass{{color:#176b27}}.fail{{color:#a31313}}</style></head><body>
<h1 class="{'pass' if result.get('passed') else 'fail'}">M2 FormKey graph: {status}</h1>
<p><b>Keys:</b> {result.get('key_count')} &nbsp; <b>Revisions:</b> {result.get('revision_count')}
&nbsp; <b>References:</b> {result.get('reference_count')}</p>
<p><b>Fingerprint:</b> <code>{html.escape(str(result.get('fingerprint')))}</code><br>
<b>Restart stable:</b> {result.get('restart_stable')} &nbsp;
<b>Runtime-index reorder stable:</b> {result.get('runtime_reorder_stable')} &nbsp;
<b>Unreviewed:</b> {len(result.get('unreviewed', []))} &nbsp;
<b>Enable-parent cycles:</b> {len(result.get('enable_parent_cycles', []))}</p>
<h2>Reviewed official-content exceptions</h2>
<table><thead><tr><th>#</th><th>Target</th><th>Edges</th><th>Review</th></tr></thead>
<tbody>{''.join(rows)}</tbody></table></body></html>"""


def run_form_graph(args: argparse.Namespace) -> dict[str, Any]:
    source = args.source.resolve()
    data = args.oblivion_data.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    esmtool = args.esmtool.resolve()
    if not esmtool.is_file():
        raise ValueError(f"esmtool does not exist: {esmtool}")
    plugins = [data / name for name in OFFICIAL_PLUGIN_ORDER]
    missing = [str(path) for path in plugins if not path.is_file()]
    if missing:
        raise ValueError(f"Official Oblivion plugins are missing: {', '.join(missing)}")

    report_path = output / "form-graph.json"
    command_result = run_command(
        [str(esmtool), "graph", str(report_path), *(str(path) for path in plugins)],
        cwd=source,
        timeout=args.timeout,
    )
    (output / "form-graph.log").write_text(command_result["output"], encoding="utf-8")
    if command_result["exit_code"] != 0 or command_result["timed_out"]:
        raise RuntimeError(
            f"TES4 graph audit failed (exit={command_result['exit_code']}, timed_out={command_result['timed_out']})"
        )
    report = json.loads(report_path.read_text(encoding="utf-8"))
    allowlist_path = (
        args.allowlist.resolve()
        if args.allowlist
        else source / "scripts" / "data" / "oblivion_compat" / "tes4_form_graph_exceptions.json"
    )
    allowlist = json.loads(allowlist_path.read_text(encoding="utf-8"))
    result = validate_form_graph_report(report, allowlist)
    result.update(
        {
            "schema_version": SCHEMA_VERSION,
            "report": str(report_path),
            "state": str(report_path) + ".formkeys.bin",
            "allowlist": str(allowlist_path),
            "plugins": [path.name for path in plugins],
            "command_duration_seconds": command_result["duration_seconds"],
        }
    )
    write_json(output / "acceptance.json", result)
    (output / "acceptance.html").write_text(render_form_graph_html(result, allowlist), encoding="utf-8")
    return result


def validate_m6_report(
    report: dict[str, Any], count_lock: dict[str, Any], oblivion_data: Path
) -> dict[str, Any]:
    """Validate identity, payload, coverage, and official-content count locks."""
    failures: list[str] = []
    expected = count_lock["aggregate"]
    for name in (
        "unit_count",
        "source_count",
        "compiled_count",
        "source_only_count",
        "compiled_only_count",
        "source_payload_bytes",
        "compiled_payload_bytes",
        "reference_count",
        "corpus_fingerprint",
    ):
        if report.get(name) != expected[name]:
            failures.append(f"aggregate {name}: expected {expected[name]!r}, got {report.get(name)!r}")
    if report.get("contexts") != expected["contexts"]:
        failures.append(f"context counts differ: expected {expected['contexts']}, got {report.get('contexts')}")
    for name, value in expected["scda"].items():
        if report.get("scda", {}).get(name) != value:
            failures.append(
                f"SCDA {name}: expected {value!r}, got {report.get('scda', {}).get(name)!r}"
            )
    if report.get("frontend_failures") != 0:
        failures.append(f"frontend reported {report.get('frontend_failures')} failures")
    if report.get("cache_entries") != report.get("unit_count"):
        failures.append("compilation cache does not contain exactly one entry per unit")

    expected_plugins = count_lock["plugins"]
    expected_names = [item["name"] for item in expected_plugins]
    if expected_names != list(OFFICIAL_PLUGIN_ORDER):
        failures.append("count-lock plugin order is not the canonical official order")
    content: list[dict[str, Any]] = []
    for item in expected_plugins:
        path = oblivion_data / item["name"]
        if not path.is_file():
            failures.append(f"missing official plugin: {path}")
            continue
        actual = {"name": path.name, "size": path.stat().st_size, "sha256": sha256(path)}
        content.append(actual)
        if actual["size"] != item["size"] or actual["sha256"] != item["sha256"]:
            failures.append(f"content fingerprint differs for {item['name']}")

    units = report.get("units", [])
    ids = [unit.get("id") for unit in units]
    if len(ids) != len(set(ids)):
        failures.append("corpus contains duplicate stable unit identities")
    plugin_counts: dict[str, dict[str, Any]] = {
        name: {"units": 0, "source": 0, "compiled": 0, "contexts": collections.Counter()}
        for name in expected_names
    }
    for unit in units:
        plugin = unit.get("plugin")
        if plugin not in plugin_counts:
            failures.append(f"unit {unit.get('id')} names unexpected plugin {plugin!r}")
            continue
        counts = plugin_counts[plugin]
        counts["units"] += 1
        counts["source"] += unit.get("source") is not None
        counts["compiled"] += unit.get("compiled_payload_fingerprint") is not None
        counts["contexts"][unit.get("context")] += 1
        if unit.get("source") is None:
            failures.append(f"unit has no portable source: {unit.get('id')}")
        if unit.get("source_payload_fingerprint") != unit.get("source_fingerprint"):
            failures.append(f"source payload fingerprint changed during compilation: {unit.get('id')}")
        if not unit.get("ast_fingerprint") or not unit.get("program_fingerprint"):
            failures.append(f"unit has no AST/native IR: {unit.get('id')}")
        if not unit.get("reference_fingerprint"):
            failures.append(f"unit has no reference-table fingerprint: {unit.get('id')}")
        if not unit.get("cache_stable"):
            failures.append(f"unit cache result is unstable: {unit.get('id')}")
        if unit.get("diagnostics"):
            failures.append(f"unit has frontend diagnostics: {unit.get('id')}")
        if unit.get("compiled_payload_fingerprint") is not None:
            if not unit.get("scda_decoded") or not unit.get("scda_header_size_matches"):
                failures.append(f"unit SCDA failed lossless decode or header-size check: {unit.get('id')}")

    for item in expected_plugins:
        actual = plugin_counts[item["name"]]
        for field in ("units", "source", "compiled"):
            if actual[field] != item[field]:
                failures.append(
                    f"{item['name']} {field}: expected {item[field]}, got {actual[field]}"
                )
        contexts = dict(sorted(actual["contexts"].items()))
        if contexts != item["contexts"]:
            failures.append(
                f"{item['name']} contexts: expected {item['contexts']}, got {contexts}"
            )

    coverage = report.get("coverage", [])
    coverage_names = [item.get("name") for item in coverage]
    if len(coverage) != expected["coverage_entries"]:
        failures.append(
            f"coverage registry: expected {expected['coverage_entries']} entries, got {len(coverage)}"
        )
    if coverage_names != sorted(set(coverage_names)):
        failures.append("coverage registry names are not unique and deterministically sorted")
    for item in coverage:
        if item.get("command_uses", 0) + item.get("condition_uses", 0) <= 0:
            failures.append(f"unused coverage registry entry: {item.get('name')}")
        if not item.get("contexts"):
            failures.append(f"coverage registry entry has no execution context: {item.get('name')}")

    return {
        "passed": not failures,
        "failures": failures,
        "content": content,
        "plugin_counts": {
            name: {
                "units": value["units"],
                "source": value["source"],
                "compiled": value["compiled"],
                "contexts": dict(sorted(value["contexts"].items())),
            }
            for name, value in plugin_counts.items()
        },
    }


def render_m6_acceptance_html(report: dict[str, Any]) -> str:
    rows = []
    for name, result in report["tests"].items():
        rows.append(
            f"<tr><td>{html.escape(name)}</td><td>{'PASS' if result.get('passed') else 'FAIL'}</td></tr>"
        )
    rows.extend(
        (
            f"<tr><td>official count lock</td><td>{'PASS' if report['count_lock']['passed'] else 'FAIL'}</td></tr>",
            f"<tr><td>independent AST/native IR</td><td>{'PASS' if report['independent_reference']['passed'] else 'FAIL'}</td></tr>",
            f"<tr><td>repeat determinism</td><td>{'PASS' if report['determinism']['passed'] else 'FAIL'}</td></tr>",
        )
    )
    corpus = report["corpus"]
    status = "PASS" if report["passed"] else "FAIL"
    return f"""<!doctype html><html lang="en"><head><meta charset="utf-8"><title>M6 {status}</title>
<style>body{{font:16px sans-serif;max-width:1100px;margin:2rem auto}}table{{border-collapse:collapse}}
td,th{{border:1px solid #aaa;padding:.4rem .7rem}}</style></head><body>
<h1>OpenMW Oblivion M6 acceptance: {status}</h1>
<p>Units: {corpus['unit_count']}; source: {corpus['source_count']}; compiled: {corpus['compiled_count']};
coverage entries: {len(corpus['coverage'])}; fingerprint: <code>{html.escape(corpus['corpus_fingerprint'])}</code>.</p>
<table><tr><th>Offline gate</th><th>Result</th></tr>{''.join(rows)}</table>
<p>SCDA decoded: {corpus['scda']['decoded']}; exact control structure: {corpus['scda']['structure_matches']}.
The remaining differences are retained per unit for progressive bytecode comparison.</p></body></html>"""


def run_m6_acceptance(args: argparse.Namespace) -> dict[str, Any]:
    """Run the M6 gate without starting OpenMW or either game executable."""
    source = args.source.resolve()
    build = args.build.resolve()
    oblivion_data = args.oblivion_data.resolve()
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise RuntimeError(f"M6 acceptance output directory must be empty: {output}")
    output.mkdir(parents=True, exist_ok=True)
    esmtool = build / "esmtool"
    components_tests = build / "components-tests"
    openmw_tests = build / "openmw-tests"
    for required in (esmtool, components_tests, openmw_tests):
        if not required.is_file():
            raise FileNotFoundError(required)
    plugins = [oblivion_data / name for name in OFFICIAL_PLUGIN_ORDER]
    missing = [str(path) for path in plugins if not path.is_file()]
    if missing:
        raise FileNotFoundError(", ".join(missing))
    count_lock_path = (
        args.count_lock.resolve()
        if args.count_lock
        else source / "scripts" / "data" / "oblivion_compat" / "m6_obscript_count_lock.json"
    )
    count_lock = json.loads(count_lock_path.read_text(encoding="utf-8"))
    started = time.monotonic()

    corpus_paths = (output / "corpus-1.json", output / "corpus-2.json")
    corpus_runs = []
    for index, path in enumerate(corpus_paths, 1):
        result = run_command(
            [str(esmtool), "-q", "obscript", str(path), *(str(plugin) for plugin in plugins)],
            cwd=source,
            timeout=args.timeout,
        )
        (output / f"corpus-{index}.log").write_text(result.pop("output"), encoding="utf-8")
        result["passed"] = result["exit_code"] == 0 and not result["timed_out"] and path.is_file()
        corpus_runs.append(result)
    if not all(result["passed"] for result in corpus_runs):
        raise RuntimeError("one or more native ObScript corpus audits failed")
    corpus = json.loads(corpus_paths[0].read_text(encoding="utf-8"))
    count_lock_result = validate_m6_report(corpus, count_lock, oblivion_data)
    determinism = {
        "first_sha256": sha256(corpus_paths[0]),
        "second_sha256": sha256(corpus_paths[1]),
    }
    determinism["passed"] = determinism["first_sha256"] == determinism["second_sha256"]

    reference_path = output / "independent-reference.json"
    reference_command = run_command(
        [sys.executable, str(source / "scripts" / "obscript_reference.py"),
         str(corpus_paths[0]), "--output", str(reference_path)],
        cwd=source,
        timeout=args.timeout,
    )
    (output / "independent-reference.log").write_text(reference_command.pop("output"), encoding="utf-8")
    reference = (
        json.loads(reference_path.read_text(encoding="utf-8"))
        if reference_path.is_file()
        else {"checked_units": 0, "failure_count": 1, "failures": [{"kind": "missing-output"}]}
    )
    reference["passed"] = (
        reference_command["exit_code"] == 0
        and not reference_command["timed_out"]
        and reference.get("checked_units") == corpus.get("unit_count")
        and reference.get("failure_count") == 0
    )
    reference["command"] = reference_command

    tests = {
        "focused_frontend": _run_logged_gate(
            [str(components_tests), "--gtest_filter=ObScript*", "--gtest_color=no"],
            source, output / "tests", "focused-frontend",
        ),
        "all_components": _run_logged_gate(
            [str(components_tests), "--gtest_color=no"], source, output / "tests", "all-components"
        ),
        "all_openmw_units": _run_logged_gate(
            [str(openmw_tests), "--gtest_color=no"], source, output / "tests", "all-openmw-units"
        ),
        "compatibility_harness": _run_logged_gate(
            [sys.executable, "-m", "unittest", "scripts.tests.test_oblivion_compat"],
            source, output / "tests", "compatibility-harness",
        ),
    }
    if args.sanitized_build:
        sanitized_tests = args.sanitized_build.resolve() / "components-tests"
        if not sanitized_tests.is_file():
            raise FileNotFoundError(sanitized_tests)
        tests["asan_ubsan_frontend"] = _run_logged_gate(
            [
                "cmake", "-E", "env",
                "ASAN_OPTIONS=detect_leaks=1:halt_on_error=1",
                "UBSAN_OPTIONS=halt_on_error=1",
                str(sanitized_tests), "--gtest_filter=ObScript*", "--gtest_color=no",
            ],
            source, output / "tests", "asan-ubsan-frontend",
        )
    revision = run_command(["git", "rev-parse", "HEAD"], cwd=source, timeout=10)["output"].strip()
    status = run_command(["git", "status", "--short"], cwd=source, timeout=10)["output"].splitlines()
    acceptance = {
        "schema_version": SCHEMA_VERSION,
        "milestone": "M6",
        "offline_only": True,
        "generated_at": utc_now(),
        "duration_seconds": round(time.monotonic() - started, 6),
        "repository": {"source": str(source), "revision": revision, "status": status},
        "count_lock_path": str(count_lock_path),
        "count_lock": count_lock_result,
        "corpus_runs": corpus_runs,
        "corpus": corpus,
        "determinism": determinism,
        "independent_reference": reference,
        "tests": tests,
    }
    acceptance["passed"] = (
        count_lock_result["passed"]
        and determinism["passed"]
        and reference["passed"]
        and all(result["passed"] for result in tests.values())
    )
    write_json(output / "acceptance.json", acceptance)
    (output / "acceptance.html").write_text(render_m6_acceptance_html(acceptance), encoding="utf-8")
    return acceptance


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    baseline = subparsers.add_parser("baseline", help="audit an installed Oblivion data set")
    baseline.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    baseline.add_argument("--build", type=Path, required=True)
    baseline.add_argument("--oblivion-data", type=Path, required=True)
    baseline.add_argument("--output", type=Path, required=True)
    baseline.add_argument("--hash-archives", action="store_true")
    baseline.add_argument(
        "--census-all",
        action="store_true",
        help="collect record-family counts for every discovered ESM and ESP",
    )
    baseline.add_argument(
        "--require-lossless-tes4",
        action="store_true",
        help="census every plugin and fail if esmtool reports an unsupported record family",
    )
    baseline.add_argument(
        "--tes4-subrecord-allowlist",
        type=Path,
        help="JSON allowlist for structurally parsed but semantically deferred TES4 subrecords",
    )
    baseline.add_argument("--morrowind-data", type=Path)
    baseline.add_argument("--run-standalone", action="store_true")

    log = subparsers.add_parser("check-log", help="reject unexpected diagnostics in a log")
    log.add_argument("path", type=Path)
    log.add_argument("--forbid", action="append", default=[])
    log.add_argument("--allow", action="append", default=[])
    log.add_argument("--report", type=Path)

    image = subparsers.add_parser("compare-image", help="compare two deterministic captures")
    image.add_argument("reference", type=Path)
    image.add_argument("actual", type=Path)
    image.add_argument("--minimum-ssim", type=float, default=0.995)
    image.add_argument("--maximum-phash", type=float, default=4.0)
    image.add_argument("--maximum-changed-ratio", type=float, default=0.001)
    image.add_argument("--report", type=Path)

    inspect = subparsers.add_parser("inspect-image", help="reject black, white, or empty captures")
    inspect.add_argument("path", type=Path)
    inspect.add_argument("--minimum-entropy", type=float, default=0.01)
    inspect.add_argument("--minimum-mean", type=float, default=0.001)
    inspect.add_argument("--maximum-mean", type=float, default=0.999)
    inspect.add_argument("--report", type=Path)

    scenario = subparsers.add_parser("scenario", help="execute a deterministic scenario manifest")
    scenario.add_argument("manifest", type=Path)
    scenario.add_argument("--output", type=Path, required=True)
    scenario.add_argument("--variable", action="append", default=[])

    graph = subparsers.add_parser("form-graph", help="audit stable FormKeys across all official Oblivion plugins")
    graph.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    graph.add_argument("--esmtool", type=Path, required=True)
    graph.add_argument("--oblivion-data", type=Path, required=True)
    graph.add_argument("--output", type=Path, required=True)
    graph.add_argument("--allowlist", type=Path)
    graph.add_argument("--timeout", type=float, default=900)

    m3 = subparsers.add_parser("m3-acceptance", help="run the complete M3 standalone-boot acceptance gate")
    m3.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    m3.add_argument("--build", type=Path, required=True)
    m3.add_argument("--oblivion-data", type=Path, required=True)
    m3.add_argument("--morrowind-data", type=Path, required=True)
    m3.add_argument("--output", type=Path, required=True)

    m4 = subparsers.add_parser("m4-acceptance", help="run the complete M4 native-runtime-state acceptance gate")
    m4.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    m4.add_argument("--build", type=Path, required=True)
    m4.add_argument("--oblivion-data", type=Path, required=True)
    m4.add_argument("--morrowind-data", type=Path, required=True)
    m4.add_argument("--output", type=Path, required=True)

    m5 = subparsers.add_parser("m5-acceptance", help="run the complete M5 interactive-prison acceptance gate")
    m5.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    m5.add_argument("--build", type=Path, required=True)
    m5.add_argument("--oblivion-data", type=Path, required=True)
    m5.add_argument("--morrowind-data", type=Path, required=True)
    m5.add_argument("--output", type=Path, required=True)
    m5.add_argument("--proton", type=Path, required=True)
    m5.add_argument("--oblivion-install", type=Path, required=True)
    m5.add_argument("--original-prefix", type=Path, required=True)
    m5.add_argument("--steam-root", type=Path, required=True)

    m6 = subparsers.add_parser("m6-acceptance", help="run the complete offline M6 ObScript frontend gate")
    m6.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    m6.add_argument("--build", type=Path, required=True)
    m6.add_argument("--oblivion-data", type=Path, required=True)
    m6.add_argument("--output", type=Path, required=True)
    m6.add_argument("--sanitized-build", type=Path)
    m6.add_argument("--count-lock", type=Path)
    m6.add_argument("--timeout", type=float, default=900)

    m10_assets = subparsers.add_parser(
        "m10-assets", help="resolve and count-lock official M10 environment and media assets"
    )
    m10_assets.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    m10_assets.add_argument("--build", type=Path, required=True)
    m10_assets.add_argument("--oblivion-data", type=Path, required=True)
    m10_assets.add_argument("--output", type=Path, required=True)
    m10_assets.add_argument(
        "--count-lock",
        type=Path,
        default=Path(__file__).resolve().parent / "data" / "oblivion_compat" / "oblivion_m10_asset_counts.json",
    )
    m10_assets.add_argument(
        "--exceptions",
        type=Path,
        default=Path(__file__).resolve().parent / "data" / "oblivion_compat" / "oblivion_m10_asset_exceptions.json",
    )
    m10_assets.add_argument("--timeout", type=float, default=120)

    m11_assets = subparsers.add_parser(
        "m11-assets", help="count-lock official actor, FaceGen, animation, creature, and voice/lip assets"
    )
    m11_assets.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    m11_assets.add_argument("--build", type=Path, required=True)
    m11_assets.add_argument("--oblivion-data", type=Path, required=True)
    m11_assets.add_argument("--output", type=Path, required=True)
    m11_assets.add_argument(
        "--count-lock",
        type=Path,
        default=Path(__file__).resolve().parent / "data" / "oblivion_compat" / "oblivion_m11_asset_counts.json",
    )
    m11_assets.add_argument("--write-count-lock", action="store_true")
    m11_assets.add_argument("--timeout", type=float, default=120)

    m14_audit = subparsers.add_parser(
        "m14-audit", help="audit winning native Oblivion PACK/PGRD and actor navigation data"
    )
    m14_audit.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    m14_audit.add_argument("--oblivion-data", type=Path, required=True)
    m14_audit.add_argument("--output", type=Path, required=True)
    m14_audit.add_argument(
        "--count-lock",
        type=Path,
        default=Path(__file__).resolve().parent / "data" / "oblivion_compat" / "oblivion_m14_data_counts.json",
    )
    m14_audit.add_argument("--write-count-lock", action="store_true")

    runtime = subparsers.add_parser("runtime-state", help="inspect or rewrite the native T4ST record in an OpenMW save")
    runtime.add_argument(
        "operation", choices=("inspect", "m13-verify", "m14-verify", "mutate", "compare", "corrupt", "missing-content", "bad-fingerprint")
    )
    runtime.add_argument("save", type=Path)
    runtime.add_argument("--output", type=Path)
    runtime.add_argument("--expected", type=Path)
    runtime.add_argument("--report", type=Path)
    runtime.add_argument("--label", default="m4-acceptance")
    return parser


def run_runtime_state(args: argparse.Namespace) -> dict[str, Any]:
    state = tes4_state.load_save(args.save)
    if args.operation == "inspect":
        result = {"passed": True, "save": str(args.save), "state": state}
    elif args.operation == "m13-verify":
        result = validate_m13_runtime_state(state)
        result["save"] = str(args.save)
    elif args.operation == "m14-verify":
        result = validate_m14_runtime_state(state)
        result["save"] = str(args.save)
    elif args.operation == "compare":
        if args.expected is None:
            raise ValueError("runtime-state compare requires --expected")
        expected = json.loads(args.expected.read_text(encoding="utf-8"))
        result = tes4_state.compare(expected, state)
        result.update({"save": str(args.save), "expected_path": str(args.expected)})
    else:
        if args.output is None:
            raise ValueError(f"runtime-state {args.operation} requires --output")
        if args.operation == "mutate":
            rewritten = tes4_state.mutate_for_acceptance(state, args.label)
            tes4_state.write_save(args.save, args.output, rewritten)
            if args.expected:
                write_json(args.expected, rewritten)
        elif args.operation == "missing-content":
            rewritten = json.loads(json.dumps(state))
            rewritten["content"].append(
                {"plugin": "openmw-m4-missing.esp", "fingerprint": "sha256:" + "0" * 64}
            )
            tes4_state.write_save(args.save, args.output, rewritten)
        elif args.operation == "bad-fingerprint":
            rewritten = json.loads(json.dumps(state))
            if not rewritten["content"]:
                raise RuntimeError("TES4 runtime state has no content identity to corrupt")
            rewritten["content"][0]["fingerprint"] = "sha256:" + "f" * 64
            tes4_state.write_save(args.save, args.output, rewritten)
        elif args.operation == "corrupt":
            data = args.save.read_bytes()
            marker = data.find(tes4_state.MAGIC)
            if marker < 0:
                raise RuntimeError("OpenMW save has no TES4 runtime-state magic")
            corrupted = bytearray(data)
            corrupted[marker] ^= 0xFF
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(corrupted)
        else:
            raise AssertionError(args.operation)
        result = {
            "passed": True,
            "operation": args.operation,
            "source": str(args.save),
            "output": str(args.output),
            "source_sha256": sha256(args.save),
            "output_sha256": sha256(args.output),
        }
    if args.report:
        write_json(args.report, result)
    return result


def main(argv: list[str] | None = None) -> int:
    args = make_parser().parse_args(argv)
    try:
        if args.command == "baseline":
            result = build_baseline(args)
        elif args.command == "check-log":
            forbidden = args.forbid if args.forbid else DEFAULT_ERROR_PATTERNS
            result = check_log_file(args.path, forbidden_patterns=forbidden, allow_patterns=args.allow)
            if args.report:
                write_json(args.report, result)
        elif args.command == "compare-image":
            result = compare_images(
                args.reference,
                args.actual,
                minimum_ssim=args.minimum_ssim,
                maximum_phash=args.maximum_phash,
                maximum_changed_ratio=args.maximum_changed_ratio,
            )
            if args.report:
                write_json(args.report, result)
        elif args.command == "inspect-image":
            result = inspect_image(
                args.path,
                minimum_entropy=args.minimum_entropy,
                minimum_mean=args.minimum_mean,
                maximum_mean=args.maximum_mean,
            )
            if args.report:
                write_json(args.report, result)
        elif args.command == "scenario":
            variables = parse_variables(args.variable)
            variables.setdefault("source", str(Path(__file__).resolve().parents[1]))
            result = run_scenario(args.manifest.resolve(), args.output.resolve(), variables)
        elif args.command == "form-graph":
            result = run_form_graph(args)
        elif args.command == "m3-acceptance":
            result = run_m3_acceptance(args)
        elif args.command == "m4-acceptance":
            result = run_m4_acceptance(args)
        elif args.command == "m5-acceptance":
            result = run_m5_acceptance(args)
        elif args.command == "m6-acceptance":
            result = run_m6_acceptance(args)
        elif args.command == "m10-assets":
            result = run_m10_asset_audit(args)
        elif args.command == "m11-assets":
            result = run_m11_asset_audit(args)
        elif args.command == "m14-audit":
            result = run_m14_audit(args)
        elif args.command == "runtime-state":
            result = run_runtime_state(args)
        else:
            raise AssertionError(args.command)
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        print(f"oblivion-compat: {error}", file=sys.stderr)
        return 2
    printable = result
    if args.command == "m6-acceptance":
        printable = {
            "passed": result.get("passed", False),
            "milestone": "M6",
            "offline_only": True,
            "unit_count": result.get("corpus", {}).get("unit_count"),
            "compiled_count": result.get("corpus", {}).get("compiled_count"),
            "corpus_fingerprint": result.get("corpus", {}).get("corpus_fingerprint"),
            "evidence": str(args.output.resolve() / "acceptance.json"),
        }
    elif args.command == "m10-assets":
        printable = {
            "passed": result.get("passed", False),
            "milestone": "M10",
            "summary": result.get("summary", {}),
            "exception_review": result.get("exception_review", {}),
            "count_lock": result.get("count_lock", {}),
            "evidence": str(args.output.resolve() / "m10-assets.json"),
        }
    elif args.command == "m11-assets":
        printable = {
            "passed": result.get("passed", False),
            "milestone": "M11",
            "summary": result.get("summary", {}),
            "checks": result.get("checks", {}),
            "count_lock": result.get("count_lock", {}),
            "evidence": str(args.output.resolve() / "m11-assets.json"),
        }
    elif args.command == "m14-audit":
        printable = {
            "passed": result.get("passed", False),
            "milestone": "M14",
            "summary": result.get("summary", {}),
            "unsupported": result.get("unsupported", {}),
            "count_lock": result.get("count_lock", {}),
            "evidence": str(args.output.resolve() / "m14-audit.json"),
        }
    print(json.dumps(printable, indent=2, sort_keys=True))
    return 0 if result.get("passed", False) else 1


if __name__ == "__main__":
    raise SystemExit(main())
