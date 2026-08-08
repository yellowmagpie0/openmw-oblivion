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


SCHEMA_VERSION = 1
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
    elif action_type in ("key", "type"):
        executable = shutil.which("xdotool")
        if not executable:
            raise RuntimeError("xdotool is required for input actions")
        if action_type == "key":
            command = [executable, "key", str(action["value"])]
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
    return parser


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
        else:
            raise AssertionError(args.command)
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        print(f"oblivion-compat: {error}", file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result.get("passed", False) else 1


if __name__ == "__main__":
    raise SystemExit(main())
