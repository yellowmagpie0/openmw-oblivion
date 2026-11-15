#!/usr/bin/env python3
"""Sequential, isolated Windows TES4 stationary-view performance probes.

Run from a completed build, with no other game instance or build running.
Startup latency is process launch to the first buffered gameplay statistics,
including the profiler's three-frame delay; it is not a cold-disk measurement.
"""

import argparse
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import time


LOCATIONS = {
    "prison": "ImperialDungeon01",
    "city": "ICMarketDistrict01::ref=0x038490::side=east",
    "bridge": "OldBridgeExterior::ref=0x60423::side=south",
    "cave": "GoblinJimsCave::ref=0x67067::side=south",
}


def read_frames(path):
    frame = {}
    camera = 0
    with path.open(encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if line.startswith("Stats Viewer"):
                if frame:
                    yield frame
                frame = {"FrameNumber": int(line.split()[-1])}
                camera = 0
            elif line.startswith("Stats Camera"):
                camera += 1
            elif line.startswith("    "):
                key, value = line.strip().rsplit(maxsplit=1)
                if camera:
                    key += f" Camera {camera}"
                try:
                    frame[key] = float(value)
                except ValueError:
                    pass
    # The final record can be incomplete after forced termination.


def summarize(path, warmup, seconds):
    frames = [f for f in read_frames(path) if "script_time_taken" in f]
    if not frames:
        raise RuntimeError("No gameplay frames in statistics")
    start = frames[0]["Reference time"] + warmup
    selected = [f for f in frames if start <= f.get("Reference time", -1) < start + seconds]
    durations = [f["Frame duration"] for f in selected if f.get("Frame duration", 0) > 0]
    if len(durations) < 30:
        raise RuntimeError(f"Insufficient gameplay samples: {len(durations)}")
    ordered = sorted(durations)
    result = {
        "frames": len(durations),
        "sample_seconds": sum(durations),
        "fps": 1 / statistics.mean(durations),
        "median_frame_ms": statistics.median(durations) * 1000,
        "p95_frame_ms": ordered[math.ceil(0.95 * len(ordered)) - 1] * 1000,
        "p99_frame_ms": ordered[math.ceil(0.99 * len(ordered)) - 1] * 1000,
        "frames_over_50ms": sum(d > 0.05 for d in durations),
    }
    result["mean_ms"] = {
        key: statistics.mean(f[key] for f in selected if key in f) * 1000
        for key in sorted(set().union(*(f.keys() for f in selected)))
        if key.endswith("_time_taken") or "traversal time taken" in key.lower()
    }
    return result


def close_process(process):
    """Ask only this process's windows to close, then record forced shutdown."""
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    from ctypes import wintypes
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]

    @callback_type
    def close(window, _):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(window, ctypes.byref(pid))
        if pid.value == process.pid:
            user32.PostMessageW(window, 0x0010, 0, 0)  # WM_CLOSE -> SDL_QUIT
        return True

    user32.EnumWindows(close, 0)
    try:
        process.wait(timeout=30)
        return False
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        return True


def run(args, location, repeat, env):
    profile = args.output / f"{location}-{repeat}"
    profile.mkdir()
    (profile / "user").mkdir()
    (profile / "local-data").mkdir()
    config = {
        "resources": args.build / "resources",
        "data": args.data,
        "data-local": profile / "local-data",
        "user-data": profile / "user",
    }
    lines = ["replace=config"] + [f'{k}="{v.as_posix()}"' for k, v in config.items()]
    lines += ["content=Oblivion.esm", f"start={LOCATIONS[location]}", "skip-menu=1", "new-game=0", "no-grab=1"]
    (profile / "openmw.cfg").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (profile / "settings.cfg").write_text(
        "[Video]\nfullscreen = false\nresolution x = 1280\nresolution y = 720\n"
        "vsync = false\nframerate limit = 0\n"
        "[Models]\nload unsupported nif files = true\n", encoding="utf-8")
    stats_path = profile / "stats.txt"
    env = dict(env, OPENMW_OSG_STATS_FILE=str(stats_path), OPENMW_OSG_STATS_LIST="frame_rate;engine;update;rendering")
    started = time.perf_counter()
    first_stats = None
    forced = False
    with (profile / "stdout.log").open("wb") as out, (profile / "stderr.log").open("wb") as err:
        process = subprocess.Popen([str(args.build / "openmw.exe"), "--replace=config", "--config", str(profile),
                                    "--game-profile=oblivion", "--random-seed=12345"],
                                   cwd=args.build, env=env, stdout=out, stderr=err)
        try:
            offset = 0
            tail = b""
            while process.poll() is None:
                now = time.perf_counter()
                if stats_path.exists():
                    with stats_path.open("rb") as stream:
                        stream.seek(offset)
                        tail = (tail + stream.read())[-65536:]
                        offset = stream.tell()
                    if first_stats is None and b"script_time_taken" in tail:
                        first_stats = now - started
                        print(f"{location}-{repeat}: first gameplay statistics {first_stats:.2f}s", flush=True)
                if first_stats is not None and now - started >= first_stats + args.warmup + args.seconds + 1:
                    break
                if now - started > args.timeout:
                    raise TimeoutError(f"{location}: startup/sample timeout")
                time.sleep(0.05)
        finally:
            if process.poll() is None:
                forced = close_process(process)
    result = {"location": location, "repeat": repeat, "first_gameplay_stats_s": first_stats,
              "exit_code": process.returncode, "forced_shutdown": forced,
              **summarize(stats_path, args.warmup, args.seconds)}
    (profile / "summary.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result), flush=True)
    if first_stats is None or forced or process.returncode != 0:
        raise RuntimeError(f"Unclean benchmark process: {result}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--build", type=Path, default=Path(__file__).resolve().parents[1] / "build-windows")
    parser.add_argument("--data", type=Path, default=Path("C:/Program Files (x86)/Steam/steamapps/common/Oblivion/Data"))
    parser.add_argument("--locations", nargs="+", choices=LOCATIONS, default=list(LOCATIONS))
    parser.add_argument("--repeats", type=int, default=2)
    parser.add_argument("--warmup", type=float, default=5)
    parser.add_argument("--seconds", type=float, default=20)
    parser.add_argument("--timeout", type=float, default=300)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.build = args.build.resolve()
    args.data = args.data.resolve()
    if os.name != "nt":
        parser.error("This runner requires Windows")
    if args.repeats < 1 or args.warmup < 0 or args.seconds <= 0 or args.timeout <= 0:
        parser.error("Invalid repeat count or duration")
    for path in (args.build / "openmw.exe", args.data / "Oblivion.esm"):
        if not path.is_file():
            parser.error(f"Missing {path}")
    args.output.mkdir(parents=True, exist_ok=False)
    repo = Path(__file__).resolve().parents[1]
    prefix = repo / "deps/vcpkg-x64-windows-2022-2026-02-24/installed/x64-windows"
    env = {k: v for k, v in os.environ.items() if not k.startswith(("OPENMW_", "SDL_"))}
    env["PATH"] = os.pathsep.join([str(prefix / "bin"), str(prefix / "bin/Release"), env["PATH"]])
    env["OSG_LIBRARY_PATH"] = str(prefix / "bin/osgPlugins-3.6.5")
    metadata = {"revision": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip(),
                "diff": subprocess.check_output(["git", "diff", "HEAD"], cwd=repo, text=True),
                "binary_sha256": hashlib.sha256((args.build / "openmw.exe").read_bytes()).hexdigest(),
                "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                "cmake_cache_sha256": hashlib.sha256((args.build / "CMakeCache.txt").read_bytes()).hexdigest(),
                "data_path": str(args.data),
                "warmup_seconds": args.warmup, "sample_seconds": args.seconds,
                "metrics": "frame_rate;engine;update;rendering", "runs": []}
    for repeat in range(1, args.repeats + 1):
        for location in args.locations:
            metadata["runs"].append(run(args, location, repeat, env))
            (args.output / "results.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    if metadata["binary_sha256"] != hashlib.sha256((args.build / "openmw.exe").read_bytes()).hexdigest():
        raise RuntimeError("Game executable changed during benchmark")
    if metadata["diff"] != subprocess.check_output(["git", "diff", "HEAD"], cwd=repo, text=True):
        raise RuntimeError("Tracked source changed during benchmark")


if __name__ == "__main__":
    main()
