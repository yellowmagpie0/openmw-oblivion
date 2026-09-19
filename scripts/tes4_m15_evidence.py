"""M15 scenario contracts and causal evidence; never a mechanics implementation.

The checked-in schema is the single source for structural validation. Only
the small schema vocabulary used there is implemented; unsupported keywords
fail rather than silently weakening validation. Semantic checks additionally
constrain normal input, filesystem paths, identities, and evidence provenance.
"""

from __future__ import annotations

import hashlib
import json
import math
import os
import re
import time
import uuid
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any

import tes4_runtime_state


SCHEMA = Path(__file__).parent / "data/oblivion_compat/m15.schema.json"
KEY = re.compile(r"^(content:[^:/\\]+:[0-9a-f]{6}|dynamic:[^:/\\]+:[0-9a-f]{16})$")
NORMAL_KEYS = frozenset((
    "w", "a", "s", "d", "e", "r", "f", "q", "c", "space", "Tab", "Escape", "Return",
    "Shift_L", "Control_L", "F5", "F9", "1", "2", "3", "4", "5", "6", "7", "8",
))
INPUT_TYPES = frozenset((
    "key", "key_down", "key_up", "key_held", "key_hold", "mouse_click", "mouse_down", "mouse_up",
    "mouse_move", "mouse_move_absolute", "gamepad_button", "gamepad_axis",
))
RENDER_ENV = frozenset(("SDL_VIDEODRIVER", "LIBGL_ALWAYS_SOFTWARE", "GALLIUM_DRIVER"))
ERROR_PATTERNS = (
    r"\sE\]",
    r"Unsupported ObScript command", r"deferred command=", r"Error in frame", r"onFrame failed",
    r"AddressSanitizer", r"UndefinedBehaviorSanitizer", r"runtime error:", r"Segmentation fault",
    r"Assertion.*failed", r"Console diagnostic:",
)


def _error(message: str) -> ValueError:
    return ValueError("M15: " + message)


class ContractSchemaError(ValueError):
    pass


def _number(value: Any) -> bool:
    return type(value) is int or (type(value) is float and math.isfinite(value))


def _equal(left: Any, right: Any) -> bool:
    if isinstance(left, dict) or isinstance(right, dict):
        return (isinstance(left, dict) and isinstance(right, dict) and left.keys() == right.keys()
                and all(_equal(left[key], right[key]) for key in left))
    if isinstance(left, list) or isinstance(right, list):
        return (isinstance(left, list) and isinstance(right, list) and len(left) == len(right)
                and all(_equal(a, b) for a, b in zip(left, right)))
    if type(left) is bool or type(right) is bool:
        return type(left) is type(right) and left == right
    return left == right


def _shape(value: Any, schema: dict[str, Any], label: str, root: dict[str, Any] | None = None) -> None:
    root = schema if root is None else root
    supported = {
        "$schema", "$defs", "$ref", "title", "type", "properties", "required", "additionalProperties",
        "const", "enum", "oneOf", "items", "minItems", "minLength", "maxLength", "pattern",
        "minimum", "maximum", "exclusiveMinimum",
    }
    if set(schema) - supported:
        raise ContractSchemaError(f"M15: unsupported contract keywords: {sorted(set(schema) - supported)}")
    if "$ref" in schema:
        ref = schema["$ref"]
        if not ref.startswith("#/$defs/") or ref.count("/") != 2:
            raise ContractSchemaError("M15: only local contract definitions may be referenced")
        _shape(value, root["$defs"][ref.rsplit("/", 1)[1]], label, root)
    if "oneOf" in schema:
        matches = 0
        for variant in schema["oneOf"]:
            try:
                _shape(value, variant, label, root)
                matches += 1
            except ContractSchemaError:
                raise
            except ValueError:
                pass
        if matches != 1:
            raise _error(f"{label} must match exactly one typed variant")
    kind = schema.get("type")
    kinds = {
        "object": lambda x: isinstance(x, dict), "array": lambda x: isinstance(x, list),
        "string": lambda x: isinstance(x, str), "integer": lambda x: _number(x) and int(x) == x,
        "number": _number, "boolean": lambda x: type(x) is bool,
    }
    if kind is not None and (kind not in kinds or not kinds[kind](value)):
        raise _error(f"{label} must have type {kind}")
    if "const" in schema and not _equal(value, schema["const"]):
        raise _error(f"{label} must equal {schema['const']!r}")
    if "enum" in schema and not any(_equal(value, item) for item in schema["enum"]):
        raise _error(f"{label} has an invalid enum value")
    if isinstance(value, dict):
        missing = set(schema.get("required", [])) - value.keys()
        if missing:
            raise _error(f"{label} missing {sorted(missing)}")
        props = schema.get("properties", {})
        for key, item in value.items():
            child = props.get(key, schema.get("additionalProperties", {}))
            if child is False:
                raise _error(f"{label} unknown field {key!r}")
            if child is not True:
                _shape(item, child, f"{label}.{key}", root)
    if isinstance(value, list):
        if len(value) < schema.get("minItems", 0):
            raise _error(f"{label} has too few items")
        for index, item in enumerate(value):
            _shape(item, schema.get("items", {}), f"{label}[{index}]", root)
    if isinstance(value, str):
        if not schema.get("minLength", 0) <= len(value) <= schema.get("maxLength", len(value)):
            raise _error(f"{label} has invalid length")
        if "pattern" in schema and not re.search(schema["pattern"], value):
            raise _error(f"{label} has invalid syntax")
    if _number(value):
        if "minimum" in schema and value < schema["minimum"]:
            raise _error(f"{label} is below minimum")
        if "maximum" in schema and value > schema["maximum"]:
            raise _error(f"{label} is above maximum")
        if "exclusiveMinimum" in schema and value <= schema["exclusiveMinimum"]:
            raise _error(f"{label} is not above minimum")


def relative_path(value: str) -> Path:
    path = Path(value)
    if not value or path.is_absolute() or ".." in path.parts or path == Path(".") or "\\" in value:
        raise _error(f"expected an output-relative path, got {value!r}")
    return path


def output_path(root: Path, value: str) -> Path:
    path = root / relative_path(value)
    if not path.resolve().is_relative_to(root.resolve()):
        raise _error(f"output path escapes through a symlink: {value}")
    return path


def _unique(values: list[Any], label: str) -> None:
    encoded = [json.dumps(value, sort_keys=True) for value in values]
    if len(set(encoded)) != len(encoded):
        raise _error(f"duplicate {label}")


def validate_manifest(manifest: dict[str, Any]) -> None:
    _shape(manifest, json.loads(SCHEMA.read_text()), "scenario")
    settings = manifest["m15"]
    relative_path(settings["event_file"])
    _unique([case["id"] for case in settings["cases"]], "case ID")
    _unique([(c["actor"], c["target"], c["action_id"]) for c in settings["cases"]], "case action identity")
    _unique([item["path"] for item in settings["inputs"]], "input path")
    _unique([item["path"] for item in settings["artifacts"]], "artifact path")
    _unique([item["key"] for item in settings.get("required_references", [])], "required reference")
    if set(manifest.get("environment", {})) - RENDER_ENV:
        raise _error("only renderer environment overrides are permitted")
    if manifest["command"][0] != "{openmw}":
        # An expanded manifest is checked against the trusted executable again
        # by prepare(); no other program/wrapper is a normal gameplay driver.
        if not Path(manifest["command"][0]).is_absolute():
            raise _error("command must launch the explicit openmw executable")
    if settings["audio"] and any("no-sound" in word for word in manifest["command"]):
        raise _error("an audio case cannot disable sound")
    if settings["audio"] and not any(a["kind"] == "audio" for a in settings["artifacts"]):
        raise _error("an audio case requires captured sound evidence")
    paths = [relative_path(f["path"]).as_posix() for f in manifest["files"]]
    _unique(paths, "generated file")
    if set(paths) != {"config/openmw.cfg", "config/settings.cfg"}:
        raise _error("only the two declared configuration files may be generated")
    seen_exercise = False
    seen_non_setup = False
    snapshots: set[str] = set()
    captures: set[str] = set()
    for action in manifest["actions"]:
        kind = action["type"]
        phase = action["phase"]
        if phase == "setup" and seen_non_setup:
            raise _error("setup cannot resume after exercise/observation")
        seen_non_setup |= phase != "setup"
        if kind in INPUT_TYPES:
            if phase == "observe":
                raise _error("observe phase cannot send input")
            seen_exercise |= phase in ("exercise", "restart-continuation")
        if kind.startswith("key") and action["value"] not in NORMAL_KEYS:
            raise _error("raw keys, chords and console bindings are not ordinary-input controls")
        if kind == "wait_log":
            relative_path(action.get("path", "process.log"))
        if kind == "screenshot":
            path = relative_path(action["name"]).as_posix()
            if not path.endswith(".png") or path in captures:
                raise _error("screenshots require distinct .png paths")
            captures.add(path)
        if kind == "m15_snapshot":
            relative_path(action["save"])
            if action["name"] in snapshots:
                raise _error("snapshot IDs must be unique")
            snapshots.add(action["name"])
    if not seen_exercise:
        raise _error("a course must exercise ordinary input")
    for case in settings["cases"]:
        if case["before"] == case["after"] or {case["before"], case["after"]} - snapshots:
            raise _error("each case requires two distinct declared snapshots")
        _unique([d["event_field"] for d in case["deltas"]], "delta event field")
        if any(d["actor"] not in (case["actor"], case["target"]) for d in case["deltas"]):
            raise _error("state deltas must concern the named actor or target")
        assertions = case.get("assertions", [])
        _unique([(a["snapshot"], a["actor"], a["path"]) for a in assertions], "state assertion")
        if any(a["actor"] not in (case["actor"], case["target"]) for a in assertions):
            raise _error("state assertions must concern the named actor or target")
    reserved = {settings["event_file"], "scenario.json", "m15-session.json", "m15-manifest.json", "process.log", ".m15-origin", "restart-input.omwsave"}
    if len(reserved) != 7 or reserved & captures:
        raise _error("evidence paths collide with reserved output")
    for artifact in settings["artifacts"]:
        path = relative_path(artifact["path"]).as_posix()
        if artifact["kind"] == "image" and path not in captures:
            raise _error("required image has no capture action")
        if artifact["kind"] == "snapshot" and path not in {f"snapshots/{name}.json" for name in snapshots}:
            raise _error("required snapshot has no observation action")


def parse_json(text: str) -> Any:
    def reject(value: str) -> None:
        raise _error(f"nonfinite JSON value {value}")

    def pairs(items: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in items:
            if key in result:
                raise _error(f"duplicate JSON key {key}")
            result[key] = value
        return result

    return json.loads(text, parse_constant=reject, object_pairs_hook=pairs)


def _read_json(path: Path) -> Any:
    return parse_json(path.read_text())


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def actor_state(state: dict[str, Any], actor: str) -> dict[str, Any]:
    if state.get("player", {}).get("reference") == actor:
        return state["player"]
    matches = [r for r in state.get("references", []) if r.get("key") == actor]
    if len(matches) != 1:
        raise _error(f"snapshot must contain exactly one named actor {actor}")
    return matches[0]


def field(value: Any, path: list[str | int]) -> Any:
    for part in path:
        if isinstance(value, dict) and isinstance(part, str) and part in value:
            value = value[part]
        elif isinstance(value, list) and _number(part) and int(part) == part and 0 <= part < len(value):
            value = value[int(part)]
        else:
            raise _error(f"missing observation path {path!r}")
    return value


def validate_events(events: list[dict[str, Any]], run_id: str, epoch: int, pid: int) -> None:
    if (not isinstance(run_id, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,128}", run_id)
            or type(epoch) is not int or epoch <= 0 or type(pid) is not int or pid <= 0):
        raise _error("invalid process/run identity")
    if len(events) < 2:
        raise _error("missing event stream or closing summary")
    tick = -1
    for index, event in enumerate(events, 1):
        if not isinstance(event, dict):
            raise _error("event must be an object")
        if event.get("run_id") != run_id or event.get("epoch") != epoch or event.get("pid") != pid:
            raise _error("foreign run, process or reload epoch")
        if type(event.get("epoch")) is not int or type(event.get("pid")) is not int:
            raise _error("process and epoch must be integers")
        if type(event.get("sequence")) is not int or event["sequence"] != index:
            raise _error("event sequence is missing, duplicated or out of order")
        if type(event.get("tick")) is not int or event["tick"] < tick or event["tick"] < 0:
            raise _error("invalid or decreasing simulation tick")
        if not isinstance(event.get("event"), str) or not event["event"]:
            raise _error("missing event kind")
        for name in ("actor", "target"):
            if name in event and (not isinstance(event[name], str) or not KEY.fullmatch(event[name])):
                raise _error(f"invalid stable event {name} identity")
        if "actor" in event or "target" in event:
            if not {"actor", "target", "action_id", "cause", "result"} <= event.keys():
                raise _error("actor-scoped event lacks causal provenance")
            if any(not isinstance(event[k], str) or not event[k] for k in ("action_id", "cause", "result")):
                raise _error("actor-scoped event has invalid causal provenance")
        tick = event["tick"]
    if events[0]["event"] != "run-start" or events[-1]["event"] != "run-end":
        raise _error("missing run boundaries")
    if any(e["event"] in ("run-start", "run-end") for e in events[1:-1]):
        raise _error("duplicate run boundary")
    last = events[-1]
    if type(last.get("event_count")) is not int or last["event_count"] != len(events) - 1:
        raise _error("summary count does not match lossless stream")
    for name in ("unsupported_count", "error_count", "pending_count"):
        if type(last.get(name)) is not int or last[name] != 0:
            raise _error(f"missing or nonzero summary {name}")


def evaluate_cases(cases: list[dict[str, Any]], events: list[dict[str, Any]],
                   snapshots: dict[str, dict[str, Any]], run_id: str, epoch: int) -> list[dict[str, Any]]:
    results = []
    for case in cases:
        failures = []
        try:
            selected = [event for event in events if all(event.get(k) == case[k]
                        for k in ("actor", "target", "action_id", "incident_id", "projectile_id", "transaction_id")
                        if k in case)]
            if [event["event"] for event in selected] != case["events"]:
                raise _error("wrong/missing/duplicate causal events for named actor, target and action")
            before, after = snapshots[case["before"]], snapshots[case["after"]]
            for snapshot in (before, after):
                if snapshot.get("run_id") != run_id or snapshot.get("epoch") != epoch:
                    raise _error("snapshot is from another run or epoch")
            if before["ordinal"] >= after["ordinal"]:
                raise _error("snapshot ordering is reversed")
            if not before["event_sequence"] < selected[0]["sequence"] <= selected[-1]["sequence"] <= after["event_sequence"]:
                raise _error("causal events are outside the observed state interval")
            # Even zero-delta controls must resolve the actual actor/target.
            for snapshot in (before, after):
                for actor in (case["actor"], case["target"]):
                    actor_state(snapshot["state"], actor)
            for delta in case["deltas"]:
                old = field(actor_state(before["state"], delta["actor"]), delta["path"])
                new = field(actor_state(after["state"], delta["actor"]), delta["path"])
                claimed = selected[-1].get("deltas", {}).get(delta["event_field"])
                if not all(_number(x) for x in (old, new, claimed, delta["expected"])):
                    raise _error("numeric state delta and terminal telemetry must exist and be finite")
                actual = new - old
                tolerance = delta.get("tolerance", 0)
                if not _number(tolerance) or tolerance < 0:
                    raise _error("invalid delta tolerance")
                if abs(actual - delta["expected"]) > tolerance or abs(actual - claimed) > tolerance:
                    raise _error("telemetry, independent observed delta and expected delta disagree")
            for assertion in case.get("assertions", []):
                snapshot = before if assertion["snapshot"] == "before" else after
                actual = field(actor_state(snapshot["state"], assertion["actor"]), assertion["path"])
                expected = assertion["expected"]
                # Canonical paths select whole values: nested metadata and list
                # entries cannot be silently excluded from an exact comparison.
                if assertion["mode"] == "exact":
                    if not _equal(actual, expected):
                        raise _error(f"exact state assertion differs at {assertion['path']!r}")
                elif assertion["mode"] == "numeric":
                    tolerance = assertion["tolerance"]
                    if (not all(_number(v) for v in (actual, expected, tolerance)) or tolerance < 0
                            or abs(actual - expected) > tolerance):
                        raise _error(f"numeric state assertion differs at {assertion['path']!r}")
                else:
                    raise _error("unknown state comparison mode")
        except (ValueError, KeyError, TypeError, IndexError, ArithmeticError) as error:
            failures.append(str(error))
        results.append({"id": case["id"], "passed": not failures, "failures": failures})
    return results


def validate_test_results(path: Path, required: list[str]) -> dict[str, Any]:
    """Require an exact nonempty GoogleTest case inventory, without skips."""
    failures = []
    executed = []
    try:
        if not required or len(set(required)) != len(required):
            raise _error("required test inventory must be nonempty and unique")
        root = ET.parse(path).getroot()
        for case in root.iter("testcase"):
            name = case.attrib["classname"] + "." + case.attrib["name"]
            executed.append(name)
            if (case.get("status") != "run" or case.get("result") != "completed"
                    or case.find("skipped") is not None or case.find("failure") is not None
                    or case.find("error") is not None):
                failures.append(f"test was failed, skipped, disabled or incomplete: {name}")
        if set(executed) != set(required) or len(executed) != len(required):
            failures.append("executed test inventory differs from required cases")
        if int(root.attrib["tests"]) != len(executed):
            failures.append("XML total differs from actual test cases")
        if any(int(root.get(k, "0")) for k in ("failures", "errors", "disabled", "skipped")):
            failures.append("XML reports failed or unexecuted tests")
    except (OSError, ValueError, KeyError, ET.ParseError) as error:
        failures.append(str(error))
    return {"case_count": len(executed), "failures": failures, "passed": not failures}


def read_events(path: Path) -> list[dict[str, Any]]:
    if path.stat().st_size > 256 * 1024 * 1024:
        raise _error("event stream exceeds bounded evidence size")
    result = []
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            if not line.endswith("\n") or len(line) > 1024 * 1024:
                raise _error("truncated or oversized event line")
            result.append(parse_json(line))
            if len(result) > 1_000_000:
                raise _error("event stream exceeds bounded event count")
    return result


def diagnostic_counts(log: str) -> dict[str, int]:
    lines = log.splitlines()
    return {
        "error_log_lines": sum(bool(re.search(r"\sE\]", line)) for line in lines),
        "unsupported_log_lines": sum(bool(re.search(r"Unsupported ObScript command|deferred command=", line))
                                     for line in lines),
        "sanitizer_log_lines": sum(bool(re.search(r"AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:", line))
                                   for line in lines),
    }


def _check_config(manifest: dict[str, Any], output: Path) -> None:
    allowed = {"replace", "resources", "data", "content", "fallback-archive", "start", "skip-menu",
               "new-game", "no-grab", "user-data", "encoding"}
    cfg = next(f["content"] for f in manifest["files"] if f["path"] == "config/openmw.cfg")
    entries: dict[str, list[str]] = {}
    for line in cfg.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, sep, value = line.partition("=")
        key, value = key.strip(), value.strip().strip('"')
        if not sep or key not in allowed:
            raise _error(f"unapproved configuration directive {key!r}")
        entries.setdefault(key, []).append(value)
    if not {"config", "data"}.issubset(entries.get("replace", [])):
        raise _error("configuration must replace inherited config and data paths")
    if entries.get("user-data") != [str(output / "userdata")]:
        raise _error("user-data must be the fresh run directory")
    if not entries.get("content"):
        raise _error("at least one fingerprinted content file is required")
    for directive, role in (("content", "content"), ("fallback-archive", "archive")):
        fingerprinted = {Path(i["path"]).resolve() for i in manifest["m15"]["inputs"] if i["role"] == role}
        for name in entries.get(directive, []):
            if Path(name).name != name:
                raise _error("content/archive names cannot traverse data directories")
            resolved = [(Path(directory) / name).resolve() for directory in entries.get("data", [])
                        if (Path(directory) / name).is_file()]
            if not resolved or any(path not in fingerprinted for path in resolved):
                raise _error(f"{role} {name} is missing or not fingerprinted")
    settings = next(f["content"] for f in manifest["files"] if f["path"] == "config/settings.cfg")
    section = ""
    permitted = {
        "Video": {"resolution x", "resolution y", "fullscreen", "framerate limit", "vsync"},
        "General": {"screenshot format"}, "Models": {"load unsupported nif files"},
        "Shaders": {"force shaders"},
    }
    for line in settings.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1]
            if section not in permitted:
                raise _error(f"unapproved settings section {section}")
        elif "=" not in line or line.split("=", 1)[0].strip() not in permitted.get(section, set()):
            raise _error("unapproved setting or input-binding override")


class Session:
    """One process epoch with fresh outputs and immutable observation receipts."""

    def __init__(self, manifest: dict[str, Any], output: Path, engine: Path,
                 restart_from: tuple[Path, str] | None = None):
        validate_manifest(manifest)
        self.output = output.resolve()
        self.manifest = manifest
        self.settings = manifest["m15"]
        self.run_id = uuid.uuid4().hex
        self.epoch = 1
        self.pid = 0
        self.started_ns = time.time_ns()
        self.receipts: dict[str, str] = {}
        self.snapshots: dict[str, dict[str, Any]] = {}
        if self.output.exists() and any(self.output.iterdir()):
            raise _error("output directory must be empty; never reuse earlier evidence")
        self.engine = engine.resolve(strict=True)
        command = manifest["command"]
        if Path(command[0]).resolve() != self.engine:
            raise _error("scenario command does not match the trusted openmw executable")
        args = command[1:]
        expected_config = str(self.output / "config")
        if args.count("--config") != 1 or "--replace=config" not in args:
            raise _error("engine launch requires exactly one isolated configuration")
        index = args.index("--config")
        if index + 1 >= len(args) or args[index + 1] != expected_config:
            raise _error("engine must use the generated configuration directory")
        flags = args[:index] + args[index + 2:]
        if set(flags) - {"--replace=config", "--no-grab", "--no-sound=1", "--game-profile=oblivion"}:
            raise _error("unapproved engine launch option")
        if restart_from is None and any(a["phase"] == "restart-continuation" for a in manifest["actions"]):
            raise _error("restart-continuation requires the fresh-process restart driver")
        if restart_from is None and any(a.get("boundary") == "load-complete" for a in manifest["actions"]):
            raise _error("load observation requires the fresh-process restart driver")
        for item in self.settings["inputs"]:
            if digest(Path(item["path"])) != item["sha256"]:
                raise _error(f"input fingerprint differs: {item['path']}")
        _check_config(manifest, self.output)
        self.engine_digest = digest(self.engine)
        # The seed is supplied through the engine's real option, never merely
        # written into an evidence label or an unused environment variable.
        self.command = [*command, f"--random-seed={self.settings['seed']}"]
        self.restart = None
        source_bytes = None
        if restart_from is not None:
            previous, name = restart_from
            if not re.fullmatch(r"[A-Za-z0-9_-]+", name):
                raise _error("invalid continuation snapshot name")
            if not verify_run(previous)["passed"]:
                raise _error("restart source failed independent evidence verification")
            prior = _read_json(previous / "m15-session.json")
            source = _read_json(previous / f"snapshots/{name}.json")
            if prior["epoch"] != 1 or prior["engine_sha256"] != self.engine_digest:
                raise _error("restart requires the same engine and a first-process source")
            if not _equal(prior["inputs"], self.settings["inputs"]):
                raise _error("restart content inputs differ")
            if not any(a["phase"] == "restart-continuation" for a in manifest["actions"]):
                raise _error("second process requires explicit continuation input")
            self.run_id, self.epoch = prior["run_id"], 2
            source_bytes = output_path(previous, source["save"]).read_bytes()
            self.restart = {"source_pid": prior["pid"], "source_epoch": 1,
                            "source_snapshot": name, "save_sha256": source["save_sha256"],
                            "save": "userdata/saves/M15Continuation/Quicksave.omwsave"}
            self.command.append("--load-savegame=" + str(output_path(self.output, self.restart["save"])))
        self.output.mkdir(parents=True, exist_ok=True)
        origin = self.output / ".m15-origin"
        with origin.open("x") as stream:
            stream.write(self.run_id + "\n")
        # Compare timestamps from the same filesystem clock. Wall-clock time_ns
        # and inode timestamps need not have identical resolution/rounding.
        # Fresh exclusive output, run IDs and capture hashes are also required.
        self.filesystem_start_ns = origin.stat().st_mtime_ns
        self._receipt(".m15-origin")
        if source_bytes is not None:
            destination = output_path(self.output, self.restart["save"])
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(source_bytes)
            # Keep an immutable receipt separate from the normal quicksave slot.
            (self.output / "restart-input.omwsave").write_bytes(source_bytes)
            if self._receipt("restart-input.omwsave") != self.restart["save_sha256"]:
                raise _error("continuation save changed during transfer")
        output_path(self.output, self.settings["event_file"]).parent.mkdir(parents=True, exist_ok=True)

    def environment(self) -> dict[str, str]:
        permitted = {"PATH", "HOME", "LANG", "LC_ALL", "TZ", "DISPLAY", "XDG_RUNTIME_DIR",
                     "DBUS_SESSION_BUS_ADDRESS", "PULSE_SERVER"}
        result = {k: v for k, v in os.environ.items() if k in permitted}
        result.update(self.manifest.get("environment", {}))
        result.update({
            "OPENMW_SUPPRESS_ERROR_DIALOG": "1", "OPENMW_M15_RUN_ID": self.run_id,
            "OPENMW_M15_EPOCH": str(self.epoch),
            "OPENMW_M15_EVENTS": str(output_path(self.output, self.settings["event_file"])),
        })
        return result

    def start(self, pid: int) -> None:
        if self.restart is not None and pid == self.restart["source_pid"]:
            raise _error("continuation must use a different process")
        self.pid = pid
        manifest_path = self.output / "m15-manifest.json"
        with manifest_path.open("x") as stream:
            stream.write(json.dumps(self.manifest, sort_keys=True, allow_nan=False) + "\n")
        manifest_digest = self._receipt("m15-manifest.json")
        metadata = {"run_id": self.run_id, "epoch": self.epoch, "pid": pid,
                    "started_ns": self.started_ns, "engine_sha256": self.engine_digest,
                    "filesystem_start_ns": self.filesystem_start_ns,
                    "manifest_sha256": manifest_digest,
                    "inputs": self.settings["inputs"], "command": self.command, "restart": self.restart}
        (self.output / "m15-session.json").write_text(json.dumps(metadata, indent=2) + "\n")
        self._receipt("m15-session.json")
        # settings.cfg is legitimately rewritten by the engine at shutdown;
        # its initial contents are covered by the immutable expanded manifest.
        if output_path(self.output, "config/openmw.cfg").exists():
            self._receipt("config/openmw.cfg")

    def _receipt(self, relative: str) -> str:
        if relative in self.receipts:
            raise _error(f"artifact already captured: {relative}")
        path = output_path(self.output, relative)
        if path.stat().st_mtime_ns < self.filesystem_start_ns:
            raise _error(f"stale artifact: {relative}")
        self.receipts[relative] = digest(path)
        return self.receipts[relative]

    def snapshot(self, action: dict[str, Any]) -> dict[str, Any]:
        name = action["name"]
        if action.get("boundary") == "load-complete" and self.restart is None:
            raise _error("load observation requires continuation provenance")
        source = output_path(self.output, action["save"])
        if source.stat().st_mtime_ns < self.filesystem_start_ns:
            raise _error("snapshot source save predates the process")
        relative_save = f"snapshots/{name}.omwsave"
        copy = output_path(self.output, relative_save)
        copy.parent.mkdir(parents=True, exist_ok=True)
        with copy.open("xb") as stream:
            stream.write(source.read_bytes())
        state = tes4_runtime_state.load_save(copy)
        events = read_events(output_path(self.output, self.settings["event_file"]))
        save_hash = self._receipt(relative_save)
        boundaries = [event for event in events if event.get("event") == action.get("boundary", "save-complete")
                      and event.get("save") == str(source.resolve()) and event.get("save_sha256") == save_hash]
        if not boundaries:
            raise _error("snapshot has no completed engine save acknowledgment for these bytes")
        boundary = boundaries[-1]
        if (boundary.get("run_id") != self.run_id or boundary.get("pid") != self.pid
                or boundary.get("epoch") != self.epoch):
            raise _error("snapshot has no current engine event boundary")
        if any(s["event_sequence"] == boundary["sequence"] for s in self.snapshots.values()):
            raise _error("snapshot cannot reuse a prior completed save")
        live_path = output_path(self.output, self.settings["event_file"]).parent / relative_path(boundary["live"])
        live_relative = live_path.relative_to(self.output).as_posix()
        if digest(output_path(self.output, live_relative)) != boundary["live_sha256"]:
            raise _error("live observation digest differs from engine acknowledgment")
        live_state = _read_json(live_path)
        if not _equal(live_state, state):
            raise _error("independent live observation and decoded disk save disagree")
        for required in self.settings.get("required_references", []):
            actual = actor_state(state, required["key"])
            if any(key not in actual or not _equal(actual[key], value) for key, value in required.items()):
                raise _error(f"required world reference is missing or differs: {required['key']}")
        self._receipt(live_relative)
        snapshot = {"run_id": self.run_id, "epoch": self.epoch, "name": name,
                    "ordinal": len(self.snapshots), "event_sequence": boundary["sequence"],
                    "save": relative_save, "save_sha256": save_hash,
                    "live": live_relative, "live_sha256": boundary["live_sha256"], "state": state}
        relative = f"snapshots/{name}.json"
        with output_path(self.output, relative).open("x") as stream:
            stream.write(json.dumps(snapshot, sort_keys=True, allow_nan=False) + "\n")
        self._receipt(relative)
        self.snapshots[name] = snapshot
        return {"type": "m15_snapshot", "phase": action["phase"], "name": name,
                "save_sha256": snapshot["save_sha256"], "passed": True}

    def record_action(self, action: dict[str, Any], result: dict[str, Any]) -> None:
        result["phase"] = action["phase"]
        if result.get("passed") and action["type"] == "screenshot":
            result["sha256"] = self._receipt(action["name"])

    def finish(self, log: str, actions_complete: bool) -> dict[str, Any]:
        failures = []
        results = []
        try:
            for path in ("process.log", self.settings["event_file"]):
                if output_path(self.output, path).exists():
                    self._receipt(path)
            if not actions_complete:
                raise _error("process exited before every required action completed")
            for pattern in ERROR_PATTERNS:
                if re.search(pattern, log):
                    raise _error(f"engine/unsupported/sanitizer diagnostic matched {pattern}")
            events = read_events(output_path(self.output, self.settings["event_file"]))
            validate_events(events, self.run_id, self.epoch, self.pid)
            for path, fingerprint in self.receipts.items():
                if digest(output_path(self.output, path)) != fingerprint:
                    raise _error(f"captured evidence was replaced or edited: {path}")
            for item in self.settings["inputs"]:
                if digest(Path(item["path"])) != item["sha256"]:
                    raise _error("initial input changed during execution")
            if digest(self.engine) != self.engine_digest:
                raise _error("engine changed during execution")
            for artifact in self.settings["artifacts"]:
                if artifact["path"] not in self.receipts:
                    raise _error(f"required artifact has no capture receipt: {artifact['path']}")
            results = evaluate_cases(self.settings["cases"], events, self.snapshots, self.run_id, self.epoch)
        except (OSError, ValueError, KeyError, TypeError) as error:
            failures.append(str(error))
        if not results:
            results = [{"id": case["id"], "passed": False, "failures": failures or ["case was not executed"]}
                       for case in self.settings["cases"]]
        return {"run_id": self.run_id, "epoch": self.epoch, "pid": self.pid,
                "receipts": dict(self.receipts),
                "diagnostics": diagnostic_counts(log),
                "case_count": len(results), "passed_count": sum(r["passed"] for r in results),
                "cases": results, "failures": failures,
                "passed": not failures and bool(results) and all(r["passed"] for r in results)}


def verify_run(output: Path) -> dict[str, Any]:
    """Replay a completed run's captured evidence without launching or changing it.

    Receipts establish integrity relative to the trusted runner's recorded
    result, not a cryptographic signature from an untrusted remote producer.
    The original engine/content paths need not still exist after a checkout;
    their execution-time fingerprints remain in the captured metadata.
    """
    failures: list[str] = []
    cases: list[dict[str, Any]] = []
    settings: dict[str, Any] = {}
    try:
        output = output.resolve()
        recorded = _read_json(output / "scenario.json")
        manifest = _read_json(output / "m15-manifest.json")
        metadata = _read_json(output / "m15-session.json")
        if not all(isinstance(value, dict) for value in (recorded, manifest, metadata)):
            raise _error("run envelope must contain objects")
        validate_manifest(manifest)
        settings = manifest["m15"]
        outcome = recorded.get("m15")
        if not isinstance(outcome, dict) or not isinstance(outcome.get("receipts"), dict):
            raise _error("run has no final capture receipts")
        for key in ("run_id", "epoch", "pid"):
            if outcome.get(key) != metadata[key]:
                raise _error("recorded outcome and process metadata disagree")
        receipts = outcome["receipts"]
        if type(metadata.get("filesystem_start_ns")) is not int or metadata["filesystem_start_ns"] <= 0:
            raise _error("missing filesystem freshness epoch")
        required = {".m15-origin", "m15-manifest.json", "m15-session.json", "process.log", settings["event_file"]}
        required.update(a["path"] for a in settings["artifacts"])
        if not required <= receipts.keys():
            raise _error("run is missing required capture receipts")
        for path, fingerprint in receipts.items():
            if not isinstance(fingerprint, str) or not re.fullmatch(r"[0-9a-f]{64}", fingerprint):
                raise _error("invalid artifact fingerprint")
            if digest(output_path(output, path)) != fingerprint:
                raise _error(f"captured evidence was replaced or edited: {path}")
            if output_path(output, path).stat().st_mtime_ns < metadata["filesystem_start_ns"]:
                raise _error(f"captured evidence predates the run: {path}")
        if (output / ".m15-origin").read_text() != metadata["run_id"] + "\n":
            raise _error("filesystem origin belongs to a different run")
        if receipts["m15-manifest.json"] != metadata["manifest_sha256"]:
            raise _error("manifest differs from launch provenance")
        expected_command = [*manifest["command"], f"--random-seed={settings['seed']}"]
        restart = metadata.get("restart")
        if restart is not None:
            if (metadata["epoch"] != 2 or restart["source_epoch"] != 1
                    or type(restart["source_pid"]) is not int or restart["source_pid"] <= 0
                    or restart["source_pid"] == metadata["pid"]
                    or receipts.get("restart-input.omwsave") != restart["save_sha256"]):
                raise _error("invalid fresh-process continuation provenance")
            # Paths in provenance are original launch paths; replay may be relocated.
            original_output = Path(manifest["command"][manifest["command"].index("--config") + 1]).parent
            expected_command.append("--load-savegame=" + str(original_output / relative_path(restart["save"])))
        elif metadata["epoch"] != 1:
            raise _error("later epoch lacks continuation provenance")
        if metadata["command"] != expected_command or recorded.get("command") != expected_command:
            raise _error("recorded launch differs from the declared engine and seed")
        if not isinstance(recorded.get("actions"), list) or len(recorded["actions"]) != len(manifest["actions"]):
            raise _error("required actions were not all executed")
        if any(a.get("passed") is not True for a in recorded["actions"]):
            raise _error("a recorded action failed")
        for planned, actual in zip(manifest["actions"], recorded["actions"]):
            if actual.get("type") != planned["type"] or actual.get("phase") != planned["phase"]:
                raise _error("recorded action provenance differs from the declared phase/input")
        if (outcome.get("passed") is not True or recorded.get("passed") is not True or recorded.get("actions_complete") is not True
                or recorded.get("timed_out") is not False or recorded.get("exit_code") != 0):
            raise _error("original run failed or did not complete normally")
        log = (output / "process.log").read_text(encoding="utf-8", errors="replace")
        if outcome.get("diagnostics") != diagnostic_counts(log):
            raise _error("recorded diagnostic counts disagree with captured engine log")
        for pattern in ERROR_PATTERNS:
            if re.search(pattern, log):
                raise _error(f"engine/unsupported/sanitizer diagnostic matched {pattern}")
        events = read_events(output_path(output, settings["event_file"]))
        validate_events(events, metadata["run_id"], metadata["epoch"], metadata["pid"])
        snapshots = {}
        boundaries = set()
        for action in manifest["actions"]:
            if action["type"] != "m15_snapshot":
                continue
            name = action["name"]
            relative = f"snapshots/{name}.json"
            if relative not in receipts:
                raise _error(f"snapshot has no capture receipt: {name}")
            snapshot = _read_json(output_path(output, relative))
            if (snapshot["run_id"] != metadata["run_id"] or snapshot["epoch"] != metadata["epoch"]
                    or snapshot["name"] != name or type(snapshot["ordinal"]) is not int
                    or type(snapshot["event_sequence"]) is not int):
                raise _error("snapshot process identity or ordering is invalid")
            sequence = snapshot["event_sequence"]
            if sequence in boundaries or not 1 <= sequence <= len(events):
                raise _error("snapshot reuses or lacks a completed save boundary")
            boundaries.add(sequence)
            boundary = events[sequence - 1]
            if action.get("boundary") == "load-complete" and restart is None:
                raise _error("load snapshot lacks continuation provenance")
            if boundary["event"] != action.get("boundary", "save-complete"):
                raise _error("snapshot did not observe a completed save")
            relative_save = snapshot["save"]
            if relative_save not in receipts or receipts[relative_save] != snapshot["save_sha256"]:
                raise _error("saved-game copy has no matching capture receipt")
            if snapshot["save_sha256"] != boundary["save_sha256"]:
                raise _error("saved-game copy differs from engine acknowledgment")
            state = tes4_runtime_state.load_save(output_path(output, relative_save))
            live_path = output_path(output, settings["event_file"]).parent / relative_path(boundary["live"])
            live_relative = live_path.relative_to(output).as_posix()
            if (live_relative != snapshot["live"] or live_relative not in receipts
                    or receipts[live_relative] != boundary["live_sha256"]
                    or snapshot["live_sha256"] != boundary["live_sha256"]):
                raise _error("live-state copy differs from engine acknowledgment")
            if not _equal(state, snapshot["state"]) or not _equal(state, _read_json(live_path)):
                raise _error("disk save, captured snapshot and live observation disagree")
            for required_reference in settings.get("required_references", []):
                actual = actor_state(state, required_reference["key"])
                if any(k not in actual or not _equal(actual[k], v) for k, v in required_reference.items()):
                    raise _error("required world reference differs")
            snapshots[name] = snapshot
        cases = evaluate_cases(settings["cases"], events, snapshots, metadata["run_id"], metadata["epoch"])
        if outcome.get("case_count") != len(cases) or outcome.get("passed_count") != sum(c["passed"] for c in cases):
            raise _error("recorded case counts disagree with independently replayed results")
    except (OSError, ValueError, KeyError, TypeError, AttributeError, IndexError) as error:
        failures.append(str(error))
    if not cases:
        cases = [{"id": c["id"], "passed": False, "failures": failures or ["case was not verified"]}
                 for c in settings.get("cases", [])]
    return {"kind": "m15-evidence-replay", "case_count": len(cases), "cases": cases,
            "failures": failures, "passed": not failures and bool(cases) and all(c["passed"] for c in cases)}


def verify_restart(output: Path, source_snapshot: str, loaded_snapshot: str,
                   final_snapshot: str) -> dict[str, Any]:
    """Require two independently valid epochs and exact state at normal load."""
    failures = []
    try:
        for name in (source_snapshot, loaded_snapshot, final_snapshot):
            if not re.fullmatch(r"[A-Za-z0-9_-]+", name):
                raise _error("invalid restart snapshot name")
        first, second = output / "first", output / "second"
        for directory in (first, second):
            result = verify_run(directory)
            if not result["passed"]:
                raise _error(f"restart epoch failed: {directory.name}: {result['failures']}")
        a, b = (_read_json(d / "m15-session.json") for d in (first, second))
        if (a["run_id"] != b["run_id"] or a["epoch"] != 1 or b["epoch"] != 2
                or a["pid"] == b["pid"] or a["engine_sha256"] != b["engine_sha256"]
                or not _equal(a["inputs"], b["inputs"])):
            raise _error("restart process identities, engine or content differ")
        restart = b["restart"]
        source = _read_json(first / f"snapshots/{source_snapshot}.json")
        loaded = _read_json(second / f"snapshots/{loaded_snapshot}.json")
        final = _read_json(second / f"snapshots/{final_snapshot}.json")
        if (restart["source_pid"] != a["pid"] or restart["source_snapshot"] != source_snapshot
                or source["save_sha256"] != restart["save_sha256"]
                or loaded["save_sha256"] != source["save_sha256"]):
            raise _error("second process did not load the exact first-process save")
        events = read_events(second / _read_json(second / "m15-manifest.json")["m15"]["event_file"])
        if (events[loaded["event_sequence"] - 1]["event"] != "load-complete"
                or events[final["event_sequence"] - 1]["event"] != "save-complete"
                or loaded["event_sequence"] >= final["event_sequence"]):
            raise _error("restart requires a load followed by a new normal save")
        if not _equal(source["state"], loaded["state"]):
            raise _error("canonical state changed across fresh-process load")
        # The second course's own causal assertions validate ordinary input
        # between load and resave; time/position may legitimately advance.
    except (OSError, ValueError, KeyError, TypeError, AttributeError, IndexError) as error:
        failures.append(str(error))
    return {"kind": "m15-fresh-process-restart", "passed": not failures, "failures": failures,
            "source_snapshot": source_snapshot, "loaded_snapshot": loaded_snapshot,
            "final_snapshot": final_snapshot}
