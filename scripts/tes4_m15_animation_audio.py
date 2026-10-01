"""Reproducible synthetic native animation-sound fixture and PCM evidence checks.

This exercises a native SOUN/text-key dispatch, not stock attack audio or damage.
Generated plugins, keyframes, tones and captured output belong in ignored evidence.
"""
from __future__ import annotations

import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys
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


def write_fixture(output: Path, key_editor_id: str) -> dict:
    """Create a tone SOUN and an ordinary group with a positive/missing Sound key."""
    if not isinstance(key_editor_id, str) or not re.fullmatch(r"[A-Za-z0-9_]{1,64}", key_editor_id):
        raise ValueError("fixture key editor ID must be 1–64 ASCII letters, digits or underscores")
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
    data = b"NetImmerse File Format, Version 4.0.0.2\n" + unsigned(0x04000002) + unsigned(5)
    data += string("NiSequenceStreamHelper") + string("SyntheticM15Audio") + integer(1) + integer(3)
    data += string("NiTextKeyExtraData") + integer(2) + unsigned(0) + unsigned(5)
    for time, key in [(0, "handtohandattackleft: start"), (.15, "Sound: " + key_editor_id),
                      (.2, "hit"), (.6, "a:r"), (1, "handtohandattackleft: stop")]:
        data += floating(time) + string(key)
    data += string("NiStringExtraData") + integer(-1) + unsigned(0) + string("Bip01")
    data += string("NiKeyframeController") + integer(-1) + struct.pack("<H", 8)
    data += floating(1) + floating(0) + floating(0) + floating(1) + integer(-1) + integer(4)
    data += string("NiKeyframeData") + unsigned(0) + unsigned(0) + unsigned(0) + unsigned(1) + integer(0)
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
        "schema_version": 1,
        "key_editor_id": key_editor_id,
        "sound_editor_id": "M15FixtureTone",
        "local_sound_form_id": "00000800",
        "tone_frequency_hz": 1000,
        "tone_seconds": .25,
        "scope": "Synthetic native SOUN and shared NetImmerse4 loader; no stock TES4 KF or contact/audio acceptance.",
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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    fixture = commands.add_parser("fixture")
    fixture.add_argument("--output", type=Path, required=True)
    fixture.add_argument("--key-editor-id", required=True)
    check = commands.add_parser("check")
    check.add_argument("--wave", type=Path, required=True)
    check.add_argument("--expectations", type=Path, required=True)
    check.add_argument("--case", choices=("positive", "negative"), required=True)
    check.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "fixture":
        result = write_fixture(args.output, args.key_editor_id)
    else:
        if args.output.exists():
            raise FileExistsError(args.output)
        result = analyse_audio(args.wave, json.loads(args.expectations.read_text()), args.case)
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: value for key, value in result.items() if key != "windows"}, indent=2))
    return 0 if result.get("passed", True) else 1


if __name__ == "__main__":
    raise SystemExit(main())
