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
import subprocess
import sys
import time

from pathlib import Path
from typing import Any, Iterable

sys.path.insert(0, str(Path(__file__).resolve().parent))
import tes4_runtime_state as tes4_state  # noqa: E402


SCHEMA_VERSION = 1
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
    raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    if raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"Unsupported scenario schema: {raw.get('schema_version')!r}")
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
    xvfb_process: subprocess.Popen[str] | None = None
    started = time.monotonic()
    try:
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
            "duration_seconds": round(time.monotonic() - started, 6),
            "passed": passed,
        }
        write_json(output / "scenario.json", result)
        return result
    finally:
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

    runtime = subparsers.add_parser("runtime-state", help="inspect or rewrite the native T4ST record in an OpenMW save")
    runtime.add_argument(
        "operation", choices=("inspect", "mutate", "compare", "corrupt", "missing-content", "bad-fingerprint")
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
    print(json.dumps(printable, indent=2, sort_keys=True))
    return 0 if result.get("passed", False) else 1


if __name__ == "__main__":
    raise SystemExit(main())
