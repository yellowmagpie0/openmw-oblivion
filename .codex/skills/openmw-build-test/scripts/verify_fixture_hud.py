#!/usr/bin/env python3
"""Measure M15 fixture HUD fills at its declared 1280x720 layout.

Requires ImageMagick, as does the runtime capture harness. This is a color/geometry check for the current default skin,
not image provenance, NPC acceptance, or a general HUD recognizer.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import subprocess
from pathlib import Path

BARS = {"health": (674, 0), "magicka": (689, 2), "fatigue": (704, 1)}


def verify(image_path: Path, expected: dict[str, tuple[float, float]]) -> dict:
    if not expected or any(name not in BARS for name in expected):
        raise ValueError("Select at least one known fixture bar")
    for low, high in expected.values():
        if not (math.isfinite(low) and math.isfinite(high) and 0 <= low <= high <= 1):
            raise ValueError("Fill bounds must be finite ordered fractions in [0,1]")
    data = image_path.read_bytes()
    if (len(data) < 24 or data[:8] != b'\x89PNG\r\n\x1a\n' or data[12:16] != b'IHDR'
            or struct.unpack('>II', data[16:24]) != (1280, 720)):
        raise ValueError("Expected a PNG capture at the declared 1280x720 fixture layout")
    decoded = subprocess.run(
        ['convert', 'png:-', '-depth', '8', 'RGB:-'], input=data, capture_output=True, check=False
    )
    if decoded.returncode or len(decoded.stdout) != 1280 * 720 * 3:
        raise ValueError("Unable to decode one RGB fixture capture")
    bars = {}
    for name, (low, high) in expected.items():
        y, channel = BARS[name]
        xs = []
        for x in range(13, 76):
            offset = (y * 1280 + x) * 3
            pixel = decoded.stdout[offset:offset + 3]
            if pixel[channel] > 50 and all(
                pixel[channel] > 1.3 * pixel[other] for other in range(3) if other != channel
            ):
                xs.append(x)
        contiguous = not xs or xs == list(range(xs[0], xs[-1] + 1))
        fraction = len(xs) / 63
        bars[name] = {
            "pixels": len(xs), "fraction": fraction, "bounds": [low, high],
            "contiguous": contiguous, "passed": contiguous and low <= fraction <= high,
        }
    return {
        "passed": all(bar["passed"] for bar in bars.values()), "image": str(image_path),
        "image_sha256": hashlib.sha256(data).hexdigest(), "bars": bars,
        "scope": "Default-skin fixture HUD fill only; image provenance and gameplay are checked separately",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--bar", action="append", required=True, metavar="NAME:MIN:MAX")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Evidence output must be new")
    try:
        expected = {}
        for text in args.bar:
            name, low, high = text.split(":")
            if name in expected:
                raise ValueError("Duplicate bar expectation")
            expected[name] = (float(low), float(high))
        result = verify(args.image, expected)
    except (ValueError, OSError) as error:
        result = {"passed": False, "error": str(error)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
