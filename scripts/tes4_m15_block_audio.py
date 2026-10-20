"""Finite stock block-sound mixture assay (requires NumPy).

The declared bank contains equal-gain pairs of three PCM assets: six aligned
pairs and nine ordered pairs separated by one declared mixer period. Nothing
is fitted to a recording. Clusters measure audible events, not voice counts.
Keep reports from earlier single-asset criteria unchanged.
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
from pathlib import Path
import re
import wave

import numpy as np


def pcm(path, *, mono=True):
    with wave.open(str(path), "rb") as stream:
        rate, channels = stream.getframerate(), stream.getnchannels()
        if stream.getsampwidth() != 2 or stream.getcomptype() != "NONE":
            raise ValueError("expected uncompressed PCM16")
        frames = stream.getnframes()
        raw = stream.readframes(frames)
        if not frames or len(raw) != frames * channels * 2:
            raise ValueError("empty or truncated PCM")
    values = np.frombuffer(raw, dtype="<i2").astype(np.float64).reshape(-1, channels)
    return values.mean(axis=1) if mono else values, rate, channels


def templates(recipe):
    if set(recipe) != {"schema_version", "assets", "sample_rate", "channels", "analysis_rate",
                       "mixer_period_frames", "minimum_normalized_correlation", "cluster_gap_seconds"}:
        raise ValueError("unexpected mixture-assay recipe fields")
    if any(type(recipe[k]) is not int for k in
           ("schema_version", "sample_rate", "channels", "analysis_rate", "mixer_period_frames")):
        raise ValueError("format and mixer fields require integers")
    if (recipe["schema_version"], recipe["sample_rate"], recipe["channels"],
            recipe["analysis_rate"]) != (1, 48000, 2, 12000):
        raise ValueError("assay requires stereo 48kHz capture and 12kHz analysis")
    if recipe["mixer_period_frames"] != 960:
        raise ValueError("this bank requires the declared 960-frame mixer period")
    if type(recipe["minimum_normalized_correlation"]) not in (int, float) or \
            recipe["minimum_normalized_correlation"] != .75:
        raise ValueError("this assay retains the 0.75 correlation threshold")
    if type(recipe["cluster_gap_seconds"]) not in (int, float) or recipe["cluster_gap_seconds"] != 1:
        raise ValueError("this assay retains the one-second cluster separation")
    assets = recipe["assets"]
    if not isinstance(assets, list) or len(assets) != 3:
        raise ValueError("exactly three declared stock assets required")
    sources, seen = [], set()
    for asset in assets:
        if not isinstance(asset, dict) or set(asset) != {"path", "sha256"} or \
                not isinstance(asset["path"], str) or not isinstance(asset["sha256"], str) or \
                not re.fullmatch(r"[0-9a-f]{64}", asset["sha256"]) or asset["path"] in seen:
            raise ValueError("invalid or duplicate asset identity")
        seen.add(asset["path"])
        path = Path(asset["path"])
        if hashlib.sha256(path.read_bytes()).hexdigest() != asset["sha256"]:
            raise ValueError("stock asset hash mismatch")
        values, rate, channels = pcm(path)
        if channels != 1 or not 8000 <= rate <= 192000:
            raise ValueError("stock templates require mono PCM at a supported rate")
        values = np.interp(np.arange(int(len(values) * 12000 / rate)) * rate / 12000,
                           np.arange(len(values)), values)
        if len(values) < 2 or not np.any(values != values[0]):
            raise ValueError("stock template has no varying signal")
        sources.append(values)
    bank = []
    for delay, pairs in ((0, itertools.combinations_with_replacement(range(3), 2)),
                         (240, itertools.product(range(3), repeat=2))):
        for left, right in pairs:
            values = np.zeros(max(len(sources[left]), delay + len(sources[right])))
            values[:len(sources[left])] += sources[left]
            values[delay:delay + len(sources[right])] += sources[right]
            values -= values.mean()
            bank.append((f"{left}+{right}@{delay}", values))
    return bank


def analyse(path, recipe, expected_clusters):
    if type(expected_clusters) is not int or expected_clusters < 0:
        raise ValueError("expected cluster count must be a nonnegative integer")
    bank = templates(recipe)
    values, rate, channels = pcm(path, mono=False)
    if (rate, channels) != (48000, 2):
        raise ValueError("capture requires stereo PCM16 at 48kHz")
    values = values[::4]
    if len(values) < max(len(t) for _, t in bank):
        raise ValueError("capture shorter than the declared templates")
    matches, rows = [], []
    for channel in range(channels):
        signal = values[:, channel]
        total = np.r_[0., np.cumsum(signal)]
        squared = np.r_[0., np.cumsum(signal * signal)]
        for name, template in bank:
            row, hits = correlate(signal, template, total, squared, recipe["minimum_normalized_correlation"])
            matches.extend((int(i), name, channel, float(strength)) for i, strength in hits)
            rows.append(dict(template=name, channel=channel, **row))
    clusters = []
    for offset, name, channel, strength in sorted(matches):
        seconds = offset / 12000
        if not clusters or seconds - clusters[-1]["last"] > recipe["cluster_gap_seconds"]:
            clusters.append(dict(first=seconds, last=seconds, best=seconds, template=name, channel=channel,
                                 maximum_correlation=strength))
        else:
            clusters[-1]["last"] = seconds
            if strength > clusters[-1]["maximum_correlation"]:
                clusters[-1].update(best=seconds, template=name, channel=channel, maximum_correlation=strength)
    return dict(passed=len(clusters) == expected_clusters, expected_clusters=expected_clusters,
                clusters=clusters, templates=rows, recipe=recipe,
                pcm_sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),
                duration_seconds=len(values) / 12000,
                scope="Fixed equal-gain two-asset bank, channels measured separately; temporal clusters cannot identify coincident voice count.")


def correlate(values, template, total, squared, threshold):
    """Mean-corrected sliding normalized correlation, with no amplitude fit."""
    size, count = len(template), len(values)
    fft_size = 1 << (count + size - 2).bit_length()
    dot = np.fft.irfft(np.fft.rfft(values, fft_size) *
                       np.fft.rfft(template[::-1], fft_size), fft_size)[size - 1:count]
    energy = squared[size:] - squared[:-size] - (total[size:] - total[:-size]) ** 2 / size
    norm = np.sqrt(np.maximum(energy, 0) * np.dot(template, template))
    correlation = np.divide(dot, norm, out=np.zeros_like(dot), where=norm > 0)
    peak = int(np.argmax(correlation))
    hits = np.flatnonzero(correlation >= threshold)
    return (dict(maximum_correlation=float(correlation[peak]), best_offset_seconds=peak / 12000,
                 threshold_samples=len(hits)), zip(hits, correlation[hits]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wave", type=Path, required=True)
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument("--clusters", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    result = analyse(args.wave, json.loads(args.recipe.read_text()), args.clusters)
    with args.output.open("x") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: result[k] for k in ("passed", "expected_clusters", "clusters")}))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
