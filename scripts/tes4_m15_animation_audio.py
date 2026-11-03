"""Reproducible synthetic native animation-sound fixture and PCM evidence checks.

This exercises a native SOUN/text-key dispatch, not stock attack audio or damage.
The same tone fixture also calibrates near/far/beyond native 3D playback.
Generated plugins, keyframes, tones and captured output belong in ignored evidence.
"""
from __future__ import annotations

import argparse
import array
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct
import sys
import tempfile
import wave

DEFAULT_EXPECTATIONS = {
    "sample_rate": 48000,
    "channels": 2,
    "sample_width": 2,
    "tone_frequency_hz": 1000,
    "window_frames": 4800,
    "positive_minimum_rms": 0.01,
    "positive_minimum_tone_energy_fraction": 0.8,
    "negative_maximum_tone_rms": 0.001,
    "maximum_seconds": 120,
}


def write_fixture(output: Path, key_editor_id: str, phase_keys: str = "complete") -> dict:
    """Create a tone SOUN and an ordinary group with a positive/missing Sound key."""
    if not isinstance(key_editor_id, str) or not re.fullmatch(r"[A-Za-z0-9_]{1,64}", key_editor_id):
        raise ValueError("fixture key editor ID must be 1–64 ASCII letters, digits or underscores")
    if not isinstance(phase_keys, str) or phase_keys not in ("complete", "missing-hit"):
        raise ValueError("fixture phase keys must be complete or missing-hit")
    output.mkdir(parents=True, exist_ok=False)
    unsigned = lambda value: struct.pack("<I", value)
    integer = lambda value: struct.pack("<i", value)
    floating = lambda value: struct.pack("<f", value)

    def string(value):
        encoded = value.encode("ascii")
        return unsigned(len(encoded)) + encoded

    def subrecord(tag, data):
        return tag.encode("ascii") + struct.pack("<H", len(data)) + data

    def record(tag, form_id, data):
        return tag.encode("ascii") + unsigned(len(data)) + unsigned(0) + unsigned(form_id) + unsigned(0) + data

    mesh = output / "meshes/characters/_1stperson/handtohandattackleft.kf"
    mesh.parent.mkdir(parents=True)
    # A real TES4-format controller sequence retains original key coordinates.
    # The parser regression uses the same 20.0.0.5/Bethesda0 record layout.
    data = b"Gamebryo File Format, Version 20.0.0.5\n" + unsigned(0x14000005)
    data += b"\x01" + unsigned(10) + unsigned(4) + unsigned(0) + bytes(3)
    data += struct.pack("<H", 4)
    for kind in ("NiControllerSequence", "NiTextKeyExtraData", "NiTransformInterpolator", "NiStringPalette"):
        data += string(kind)
    data += struct.pack("<4H", 0, 1, 2, 3) + unsigned(0)
    data += string("HandToHandAttackLeft") + unsigned(1) + unsigned(1)
    data += integer(2) + integer(-1) + integer(3) + unsigned(0)
    data += unsigned(0xffffffff) * 4
    data += floating(1) + integer(1) + unsigned(2) + floating(1) + floating(0) + floating(1)
    data += integer(-1) + string("Bip01") + integer(3)
    keys = [(0, "Start"), (.15, "Sound: " + key_editor_id)]
    if phase_keys == "complete":
        keys.append((.2, "Hit"))
    keys += [(.6, "a:R"), (1, "End")]
    data += string("") + unsigned(len(keys))
    for time, key in keys:
        data += floating(time) + string(key)
    for value in (0, 0, 0, 1, 0, 0, 0, 1):
        data += floating(value)
    data += integer(-1)
    palette = b"Bip01\0"
    data += unsigned(len(palette)) + palette + unsigned(len(palette))
    data += unsigned(1) + integer(0)
    mesh.write_bytes(data)
    header = record("TES4", 0, subrecord("HEDR", floating(1) + unsigned(1) + unsigned(0x801)))
    sound = record("SOUN", 0x800, subrecord("EDID", b"M15FixtureTone\0")
                   + subrecord("FNAM", b"m15/fixture-tone.wav\0") + subrecord("SNDX", bytes(12)))
    (output / "M15AudioFixture.esp").write_bytes(header + sound)
    tone = output / "sound/m15/fixture-tone.wav"
    tone.parent.mkdir(parents=True)
    with wave.open(str(tone), "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(48000)
        stream.writeframes(b"".join(struct.pack("<h", round(16000 * math.sin(2 * math.pi * 1000 * n / 48000)))
                                  for n in range(12000)))
    result = {
        "schema_version": 2,
        "phase_keys": phase_keys,
        "keyframe_version": "20.0.0.5",
        "key_editor_id": key_editor_id,
        "sound_editor_id": "M15FixtureTone",
        "local_sound_form_id": "00000800",
        "tone_frequency_hz": 1000,
        "tone_seconds": .25,
        "scope": "Synthetic native SOUN and Gamebryo20 controller-sequence fixture; no stock KF or contact/audio acceptance.",
        "files": {str(path.relative_to(output)): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in (mesh, output / "M15AudioFixture.esp", tone)},
    }
    (output / "fixture.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def analyse_audio(path: Path, expectations: dict, case: str) -> dict:
    """Measure a declared tone against independent captured stereo PCM windows."""
    if case not in ("positive", "negative"):
        raise ValueError("audio case must be positive or negative")
    if set(expectations) != set(DEFAULT_EXPECTATIONS):
        raise ValueError("audio expectations require exactly the declared fields")
    for name in ("sample_rate", "channels", "sample_width", "tone_frequency_hz", "window_frames", "maximum_seconds"):
        if type(expectations[name]) is not int or expectations[name] <= 0:
            raise ValueError("invalid audio expectation " + name)
    if expectations["channels"] != 2 or expectations["sample_width"] != 2:
        raise ValueError("audio course requires stereo PCM16")
    rate = expectations["sample_rate"]
    count = expectations["window_frames"]
    if not 0 < expectations["tone_frequency_hz"] < rate / 2 or count > rate or expectations["maximum_seconds"] > 120:
        raise ValueError("unsupported audio frequency/window/duration")
    for name in ("positive_minimum_rms", "positive_minimum_tone_energy_fraction", "negative_maximum_tone_rms"):
        value = expectations[name]
        if type(value) not in (int, float) or not math.isfinite(value) or not 0 <= value <= 1:
            raise ValueError("invalid audio tolerance " + name)
    if expectations["positive_minimum_rms"] <= 0 or expectations["positive_minimum_tone_energy_fraction"] <= 0:
        raise ValueError("positive audio tolerances must reject silence")
    if expectations["negative_maximum_tone_rms"] >= expectations["positive_minimum_rms"]:
        raise ValueError("negative tone tolerance must be below the positive RMS threshold")
    if expectations["tone_frequency_hz"] * count % rate:
        raise ValueError("audio windows must contain a whole number of tone cycles")
    with wave.open(str(path), "rb") as stream:
        if (stream.getframerate(), stream.getnchannels(), stream.getsampwidth()) != (rate, 2, 2):
            raise ValueError("captured audio does not match the declared PCM format")
        frames = stream.getnframes()
        if frames < count or frames > rate * expectations["maximum_seconds"]:
            raise ValueError("captured audio is empty/short or exceeds the declared duration")
        data = stream.readframes(frames)
        if len(data) != frames * 4:
            raise ValueError("truncated captured PCM audio")
    samples = array.array("h", data)
    if sys.byteorder != "little":
        samples.byteswap()
    cosine = [math.cos(2 * math.pi * expectations["tone_frequency_hz"] * i / rate) for i in range(count)]
    sine = [math.sin(2 * math.pi * expectations["tone_frequency_hz"] * i / rate) for i in range(count)]
    windows = []
    for offset in range(0, frames - count + 1, count):
        # Measure channels separately: downmixing can cancel a real antiphase
        # stereo tone and incorrectly let the missing-sound control pass.
        channels = [[samples[2 * (offset + i) + channel] / 32768 for i in range(count)]
                    for channel in range(2)]
        power = sum(value * value for values in channels for value in values) / (2 * count)
        tone = 0.0
        for values in channels:
            real = sum(value * coefficient for value, coefficient in zip(values, cosine))
            imaginary = sum(value * coefficient for value, coefficient in zip(values, sine))
            tone += (real * real + imaginary * imaginary) / (count * count)
        windows.append({"start_seconds": offset / rate, "rms": math.sqrt(power), "tone_rms": math.sqrt(tone),
                        "tone_energy_fraction": tone / power if power else 0})
    best = max(windows, key=lambda row: row["tone_rms"])
    passed = (best["rms"] >= expectations["positive_minimum_rms"]
              and best["tone_energy_fraction"] >= expectations["positive_minimum_tone_energy_fraction"])
    if case == "negative":
        passed = best["tone_rms"] <= expectations["negative_maximum_tone_rms"]
    return {"passed": passed, "case": case, "frames": frames, "seconds": frames / rate,
            "wav_sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "expectations": expectations,
            "best_window": best, "windows": windows,
            "scope": "Synthetic native text-key routing and OpenAL mixing; not stock combat or creature sound acceptance."}


def spatial_preflight(state: dict, recipe: dict) -> dict:
    """Reject unsuitable saved scenes before generating or launching calibration."""
    if recipe.get("schema_version") != 1 or state["physical_actions"]["pending"]:
        raise ValueError("spatial calibration requires a released source")
    if any(row.get("strike") for row in state["native_melee_states"]):
        raise ValueError("spatial calibration source has a strike")
    bounds = recipe["bounds"]
    if (any(type(x) not in (int, float) or not math.isfinite(x) for x in bounds.values())
            or not 0 < bounds["far_min"] < bounds["far_max"] < 1
            or not 0 < bounds["repeat_min"] < 1 < bounds["repeat_max"]
            or not 0 < bounds["silent_maximum_rms"] < .01
            or not 0 < bounds["clock_tolerance_seconds"] < 1):
        raise ValueError("invalid spatial measurement bounds")
    player = state["player"]
    vectors = [player["position"][:3], recipe["player_position"]]
    vectors += [row["position"] for row in recipe["emitters"]]
    if any(len(v) != 3 or any(type(x) not in (int, float) or not math.isfinite(x) for x in v) for v in vectors):
        raise ValueError("invalid spatial position")
    if player["cell"] != recipe["cell"] or math.dist(player["position"][:3], recipe["player_position"]) > .01:
        raise ValueError("spatial source Player binding changed")
    references = {row["key"]: row for row in state["references"]}
    rows = []
    if [row["phase"] for row in recipe["emitters"]] != ["near", "far", "beyond"]:
        raise ValueError("spatial calibration requires near/far/beyond emitters")
    for emitter in recipe["emitters"]:
        ref = references[emitter["reference"]]
        if any(type(x) not in (int, float) or not math.isfinite(x) for x in ref["position"][:3]):
            raise ValueError("invalid emitter position")
        if (not ref["enabled"] or ref["cell"] != player["cell"] or ref["base"] != emitter["base"]
                or ref["key"] == player["reference"]
                or math.dist(ref["position"][:3], emitter["position"]) > .01):
            raise ValueError("spatial emitter binding changed")
        rows.append(dict(phase=emitter["phase"], reference=ref["key"],
                         distance_from_player_origin=math.dist(ref["position"][:3], player["position"][:3])))
    if not (rows[0]["distance_from_player_origin"] < 100
            and 300 < rows[1]["distance_from_player_origin"] < 800
            and rows[2]["distance_from_player_origin"] > 2200):
        raise ValueError("spatial distances do not separate the 100/2000 range")
    return dict(passed=True, emitters=rows,
                scope="Player-origin geometry and released ownership, not measured listener pose or an exact gain law")


def prepare_spatial(repo: Path, data: Path, recipe: dict, output: Path, sanitizer: bool = False) -> dict:
    try:
        import tes4_runtime_state as runtime
    except ModuleNotFoundError:
        from scripts import tes4_runtime_state as runtime
    source = repo / recipe["source"]
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    if sha(source) != recipe["source_sha256"]:
        raise ValueError("spatial source hash changed")
    state = runtime.load_save(source)
    preflight = spatial_preflight(state, recipe)
    directories = [data] + [repo / path for path in recipe["data_directories"]]
    for item in state["content"]:
        candidates = [p for directory in directories for p in directory.iterdir()
                      if p.name.lower() == item["plugin"]]
        if len(candidates) != 1 or "sha256:" + sha(candidates[0]) != item["fingerprint"]:
            raise ValueError("spatial source plugin identity changed: " + item["plugin"])
    base = repo / recipe["base_manifest"]
    manifest = copy.deepcopy(json.loads(base.read_text()))
    if sanitizer:
        if not Path(recipe["sanitizer_environment"]["LD_PRELOAD"]).is_file():
            raise ValueError("missing declared sanitizer runtime")
        manifest["environment"].update(recipe["sanitizer_environment"])
    output.mkdir(parents=True, exist_ok=False)
    fixture = output / "audio-fixture"
    write_fixture(fixture, "M15FixtureTone")
    target = output / "userdata/saves/M15Review/Quicksave.omwsave"
    target.parent.mkdir(parents=True)
    shutil.copy2(source, target)
    manifest.update(name="m15-spatial-audio-calibration", sdl_xvfb_input=True, xvfb=True,
                    timeout_seconds=300)
    manifest.pop("sdl_offscreen_input", None)
    manifest["environment"]["ALSOFT_DRIVERS"] = "wave"
    # This calibration consumes logs and the real F5 save. The legacy animation
    # case's full-world JSON report is unused here and serializes on every frame.
    manifest["environment"].pop("OPENMW_OBSCRIPT_REPORT", None)
    phases = recipe["emitters"] + [recipe["emitters"][0]]
    events = "".join(f"at {2 * (i + 1)} command {e['reference']} PlaySound3D M15FixtureTone\n"
                     for i, e in enumerate(phases))
    for file in manifest["files"]:
        if file["path"] == "test-data/events.txt":
            file["content"] = events
        if file["path"] == "config/openmw.cfg":
            lines = "".join(f'data="{directory.resolve()}"\n' for directory in directories[1:])
            lines += "content=M15StationaryOpponent.esp\ncontent=M15CombinedMasteryChance.esp\n"
            file["content"] = file["content"].replace("content=M15AudioFixture.esp", lines + "content=M15AudioFixture.esp")
        file["content"] = file["content"].replace("{audio_fixture}", str(fixture.resolve()))
    requests = [f"M7 ObScript: playsound3d sound=content:m15audiofixture.esp:000800 ref={e['reference']} played=true"
                for e in phases]
    manifest["actions"] = [dict(type="wait_log", pattern="Applied TES4 runtime state:", timeout_seconds=240, abort_on_failure=True),
        dict(type="wait_log", pattern=r"[\s\S]*".join(re.escape(x) for x in requests), timeout_seconds=120, abort_on_failure=True),
        dict(type="sleep", seconds=2), dict(type="focus_window", name="OpenMW"), dict(type="key", value="F5"),
        dict(type="wait_log", pattern="Writing saved game 'Quicksave'", timeout_seconds=30, abort_on_failure=True),
        dict(type="screenshot", name="spatial-scene.png", minimum_entropy=.03)]
    manifest["expected_log"] = requests
    manifest["forbidden_log"] += ["M15 melee (hit|miss) committed:"]
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    preflight.update(source_sha256=sha(source), base_manifest_sha256=sha(base), helper_sha256=sha(Path(__file__)), recipe=recipe,
                     source_copy_sha256=sha(target), scope="Synthetic spatial calibration, not stock combat or listening")
    (output / "preflight.json").write_text(json.dumps(preflight, indent=2) + "\n")
    return preflight


def analyse_spatial(path: Path, log: str, recipe: dict) -> dict:
    """Match native request timing to calibrated tone gains, retaining silence controls."""
    with wave.open(str(path)) as stream:
        rate, frames = stream.getframerate(), stream.getnframes()
        if frames > rate * 300:
            raise ValueError("spatial capture exceeds its bounded course")
        if frames <= rate * DEFAULT_EXPECTATIONS["maximum_seconds"]:
            audio = analyse_audio(path, DEFAULT_EXPECTATIONS, "positive")
        else:
            # Preserve the full raw recording and its clock, while reusing the
            # existing bounded detector for slower rendered sanitizer cases.
            windows = []
            with tempfile.TemporaryDirectory() as directory:
                offset = 0
                while offset < frames:
                    count = min(frames - offset, rate * DEFAULT_EXPECTATIONS["maximum_seconds"])
                    data = stream.readframes(count)
                    if len(data) != count * stream.getnchannels() * stream.getsampwidth():
                        raise ValueError("truncated spatial PCM")
                    if count < DEFAULT_EXPECTATIONS["window_frames"]:
                        break
                    chunk = Path(directory) / "chunk.wav"
                    with wave.open(str(chunk), "wb") as target:
                        target.setparams(stream.getparams())
                        target.writeframes(data)
                    result = analyse_audio(chunk, DEFAULT_EXPECTATIONS, "positive")
                    windows.extend(dict(row, start_seconds=row["start_seconds"] + offset / rate)
                                   for row in result["windows"])
                    offset += count
            audio = dict(windows=windows, wav_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    emitters = recipe["emitters"] + [recipe["emitters"][0]]
    pattern = (r"\[(\d\d):(\d\d):(\d\d\.\d+) [IV]\] M7 ObScript: playsound3d "
               r"sound=content:m15audiofixture\.esp:000800 ref=(\S+) played=(true|false)")
    requests = []
    for match in re.finditer(pattern, log):
        h, m, s, ref, played = match.groups()
        stamp = int(h) * 3600 + int(m) * 60 + float(s)
        if requests and stamp < requests[-1]["time"]:
            stamp += 86400
        requests.append(dict(time=stamp, reference=ref, played=played == "true"))
    valid_requests = (len(requests) == 4 and all(row["played"] for row in requests)
                      and [r["reference"] for r in requests] == [e["reference"] for e in emitters])
    clusters = []
    for window in audio["windows"]:
        if window["tone_rms"] < .01 or window["tone_energy_fraction"] < .8:
            continue
        if not clusters or window["start_seconds"] - clusters[-1]["last"] > .11:
            clusters.append(dict(start=window["start_seconds"], last=window["start_seconds"], peak=0))
        clusters[-1].update(last=window["start_seconds"], peak=max(clusters[-1]["peak"], window["tone_rms"]))
    checks = dict(requests=valid_requests, three_clusters=len(clusters) == 3)
    measurements = {}
    if valid_requests and len(clusters) == 3:
        near, far, repeated = [row["peak"] for row in clusters]
        bounds = recipe["bounds"]
        expected = [clusters[0]["start"] + row["time"] - requests[0]["time"] for row in requests]
        quiet = [w["tone_rms"] for w in audio["windows"] if expected[2] - .2 <= w["start_seconds"] <= expected[2] + .5]
        measurements = dict(far_gain_ratio=far / ((near + repeated) / 2), repeat_gain_ratio=repeated / near,
                            beyond_maximum_tone_rms=max(quiet) if quiet else None, expected_onsets=expected)
        checks.update(near_audible=near > .05, far_attenuated=bounds["far_min"] <= measurements["far_gain_ratio"] <= bounds["far_max"],
                      near_repeated=bounds["repeat_min"] <= measurements["repeat_gain_ratio"] <= bounds["repeat_max"],
                      beyond_silent=bool(quiet) and max(quiet) <= bounds["silent_maximum_rms"],
                      time_alignment=all(abs(clusters[j]["start"] - expected[i]) <= bounds["clock_tolerance_seconds"]
                                         for j, i in enumerate((0, 1, 3))))
    return dict(passed=all(checks.values()), checks=checks, measurements=measurements, clusters=clusters,
                requests=requests, wav_sha256=audio["wav_sha256"],
                scope="Rendered native SOUN spatial calibration; stock combat identity and listening remain separate")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    fixture = commands.add_parser("fixture")
    fixture.add_argument("--output", type=Path, required=True)
    fixture.add_argument("--key-editor-id", required=True)
    fixture.add_argument("--phase-keys", choices=("complete", "missing-hit"), default="complete")
    check = commands.add_parser("check")
    check.add_argument("--wave", type=Path, required=True)
    check.add_argument("--expectations", type=Path, required=True)
    check.add_argument("--case", choices=("positive", "negative"), required=True)
    check.add_argument("--output", type=Path, required=True)
    prepare = commands.add_parser("spatial-prepare")
    prepare.add_argument("--repo", type=Path, default=Path.cwd())
    prepare.add_argument("--oblivion-data", type=Path, required=True)
    prepare.add_argument("--recipe", type=Path, required=True)
    prepare.add_argument("--output", type=Path, required=True)
    prepare.add_argument("--sanitize", action="store_true")
    spatial = commands.add_parser("spatial-check")
    spatial.add_argument("--scenario", type=Path, required=True)
    spatial.add_argument("--recipe", type=Path, required=True)
    spatial.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "fixture":
        result = write_fixture(args.output, args.key_editor_id, args.phase_keys)
    elif args.command == "spatial-prepare":
        result = prepare_spatial(args.repo, args.oblivion_data, json.loads(args.recipe.read_text()), args.output, args.sanitize)
    elif args.command == "spatial-check":
        import tes4_runtime_state as runtime
        if args.output.exists():
            raise FileExistsError(args.output)
        recipe = json.loads(args.recipe.read_text())
        result = analyse_spatial(args.scenario / "rendered-audio.wav", (args.scenario / "process.log").read_text(), recipe)
        result["saved_scene"] = spatial_preflight(runtime.load_save(
            args.scenario / "userdata/saves/M15Review/Quicksave.omwsave"), recipe)
        result["scenario_passed"] = json.loads((args.scenario / "scenario.json").read_text())["passed"]
        result["passed"] = result["passed"] and result["scenario_passed"]
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    else:
        if args.output.exists():
            raise FileExistsError(args.output)
        result = analyse_audio(args.wave, json.loads(args.expectations.read_text()), args.case)
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: value for key, value in result.items() if key != "windows"}, indent=2))
    return 0 if result.get("passed", True) else 1


if __name__ == "__main__":
    raise SystemExit(main())
