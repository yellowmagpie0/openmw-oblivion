"""Finite stock block-sound mixture assay (requires NumPy).

Version1 declares equal-gain pairs of three PCM assets. Version2 declares two
disjoint asset banks and permits only cross-bank pairs. Both include aligned
pairs and ordered pairs separated by one declared mixer period. Nothing is
fitted to a recording. Clusters measure audible events, not voice counts.
Version3 adds audited SOUN static gains and fixed40ms fingerprints covering at
least90% of each bank, retaining .75 correlation. Fullclip .5 only nominates
neighborhoods; it cannot accept an event. Keep earlier reports unchanged.
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
    version = recipe.get("schema_version")
    fields = {"schema_version", "assets", "sample_rate", "channels", "analysis_rate",
              "mixer_period_frames", "minimum_normalized_correlation", "cluster_gap_seconds"}
    if version in (2, 3):
        fields.add("asset_banks")
    if version == 3:
        fields.add("asset_static_attenuation")
    if set(recipe) != fields:
        raise ValueError("unexpected mixture-assay recipe fields")
    if any(type(recipe[k]) is not int for k in
           ("schema_version", "sample_rate", "channels", "analysis_rate", "mixer_period_frames")):
        raise ValueError("format and mixer fields require integers")
    if version not in (1, 2, 3) or (recipe["sample_rate"], recipe["channels"],
                               recipe["analysis_rate"]) != (48000, 2, 12000):
        raise ValueError("assay requires stereo 48kHz capture and 12kHz analysis")
    if recipe["mixer_period_frames"] != 960:
        raise ValueError("this bank requires the declared 960-frame mixer period")
    if type(recipe["minimum_normalized_correlation"]) not in (int, float) or \
            recipe["minimum_normalized_correlation"] != .75:
        raise ValueError("this assay retains the 0.75 correlation threshold")
    if type(recipe["cluster_gap_seconds"]) not in (int, float) or recipe["cluster_gap_seconds"] != 1:
        raise ValueError("this assay retains the one-second cluster separation")
    assets = recipe["assets"]
    if not isinstance(assets, list) or (len(assets) != 3 if version == 1 else len(assets) < 2):
        raise ValueError("incorrect declared stock asset count")
    if version == 1:
        aligned = list(itertools.combinations_with_replacement(range(3), 2))
        ordered = list(itertools.product(range(3), repeat=2))
    else:
        groups = recipe["asset_banks"]
        if not isinstance(groups, list) or len(groups) != 2 or any(
                not isinstance(group, list) or not group for group in groups):
            raise ValueError("two nonempty declared asset banks required")
        indices = [i for group in groups for i in group]
        if any(type(i) is not int for i in indices) or sorted(indices) != list(range(len(assets))):
            raise ValueError("asset banks must partition all declared assets exactly once")
        aligned = list(itertools.product(*groups))
        ordered = aligned + [(right, left) for left, right in aligned]
    attenuation = recipe.get("asset_static_attenuation")
    if version == 3 and (not isinstance(attenuation, list) or len(attenuation) != len(assets) or
                         any(type(v) is not int or not 0 <= v <= 65535 for v in attenuation)):
        raise ValueError("one unsigned SOUN static attenuation required per asset")
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
        if version == 3:
            values *= 10 ** (-attenuation[len(sources)] / 2000)
        sources.append(values)
    bank = []
    for delay, pairs in ((0, aligned), (240, ordered)):
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
    if recipe["schema_version"] == 3:
        return analyse_landmarks(path, recipe, expected_clusters, bank)
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


def analyse_landmarks(path, recipe, expected_clusters, bank):
    """V3: fixed 40ms fingerprints covering >=90% energy of EACH source.

    Full-clip correlation >=.5 only nominates event neighborhoods. Acceptance
    retains .75 per fingerprint, four acquisition phases, +/-one mixer period,
    >=three sections and >=.9 energy coverage in each bank. Scan the whole
    recording, including silent/restart regions; no event times are supplied.
    Each section also requires .75 partial correlation for each present bank
    after projecting out the other, with >1% residual signal energy to reject
    quantization-only residuals. This conservative criterion can reject changing
    relative gain within a section. V1/V2 reports remain unchanged.
    """
    values, rate, channels = pcm(path, mono=False)
    if (rate, channels) != (48000, 2):
        raise ValueError("capture requires stereo PCM16 at 48kHz")
    values = values[::4]
    if len(values) < max(len(t) for _, t in bank):
        raise ValueError("capture shorter than the declared templates")
    sources = []
    for asset, attenuation in zip(recipe["assets"], recipe["asset_static_attenuation"]):
        data, source_rate, _ = pcm(asset["path"])
        sources.append((data * 10 ** (-attenuation / 2000), source_rate))
    matches, rows = [], []
    for channel in range(channels):
        signal = values[:, channel]
        total, squared = np.r_[0., np.cumsum(signal)], np.r_[0., np.cumsum(signal * signal)]
        for name, template in bank:
            row, hits = correlate(signal, template, total, squared, .5)
            # Keep all distinct local peaks: a nearby stronger periodic peak
            # must not suppress the actual onset before the strict check.
            hits = list(hits)
            nominees = [(int(offset), float(strength)) for i, (offset, strength) in enumerate(hits)
                        if (i == 0 or hits[i-1][0] != offset-1 or strength > hits[i-1][1])
                        and (i+1 == len(hits) or hits[i+1][0] != offset+1 or strength >= hits[i+1][1])]
            row["nomination_threshold_samples"] = row.pop("threshold_samples")
            row.update(template=name, channel=channel, nominees=len(nominees))
            rows.append(row)
            left, right, delay = map(int, re.fullmatch(r"(\d+)\+(\d+)@(\d+)", name).groups())
            parts = [(sources[left], 0), (sources[right], delay)]
            for offset, strength in nominees:
                coverage, landmarks = landmark_coverage(signal, offset, parts)
                if min(coverage) >= .9 and len(landmarks) >= 3:
                    matches.append((offset, name, channel, strength, coverage, landmarks))
    clusters = []
    for offset, name, channel, strength, coverage, landmarks in sorted(matches):
        seconds = offset / 12000
        item = dict(first=seconds, last=seconds, best=seconds, template=name, channel=channel,
                    nomination_correlation=strength, per_bank_energy_coverage=coverage,
                    matched_sections_seconds=landmarks)
        if not clusters or seconds - clusters[-1]["last"] > 1:
            clusters.append(item)
        else:
            old = clusters[-1]
            last = seconds
            if min(coverage) > min(old["per_bank_energy_coverage"]):
                item["first"] = old["first"]
                clusters[-1] = item
            clusters[-1]["last"] = last
    return dict(passed=len(clusters) == expected_clusters, expected_clusters=expected_clusters,
                clusters=clusters, templates=rows, recipe=recipe,
                pcm_sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),
                duration_seconds=len(values) / 12000,
                scope="V3 fixed40ms mixed-bank fingerprints: .75 correlation, .9 energy per bank, "
                      "three sections, four capture phases, +/-20ms; .5 fullclip only nominates. "
                      "Each bank also requires .75 partial correlation and >1% residual energy. "
                      "Audited SOUN gains; temporal clusters do not identify coincident voice count.")


def landmark_coverage(signal, offset, parts):
    energy, matched, landmarks = np.zeros(2), np.zeros(2), []
    size = max(int(len(data) * 12000 / rate) + delay for (data, rate), delay in parts)
    for start in range(0, size, 480):
        phases = []
        for phase in range(4):
            sections = [np.interp((np.arange(480) + start - delay + phase / 4) * rate / 12000,
                                  np.arange(len(data)), data, left=0, right=0)
                        for (data, rate), delay in parts]
            phases.append(sections)
        section_energy = np.array([np.dot(y - y.mean(), y - y.mean()) for y in phases[0]])
        energy += section_energy
        low, high = max(0, offset + start - 240), min(len(signal), offset + start + 480 + 240)
        local = signal[low:high]
        if len(local) < 480:
            continue
        total, squared = np.r_[0., np.cumsum(local)], np.r_[0., np.cumsum(local * local)]
        accepted = False
        for sections in phases:
            template = sum(sections)
            template -= template.mean()
            row, hits = correlate(local, template, total, squared, .75)
            hits = list(hits)
            if not hits:
                continue
            # Each bank must contribute its own identifiable waveform. A loud
            # single bank can correlate >.75 with a mixture; remove the other
            # template's linear component before testing each bank. This is a
            # nuisance projection, not a fitted mixture or a relaxed threshold.
            centered = [y - y.mean() for y in sections]
            energies = [np.dot(y, y) for y in centered]
            cross = np.dot(*centered)
            signal_energy = squared[480:] - squared[:-480] - (total[480:] - total[:-480]) ** 2 / 480
            dots = [np.correlate(local, y, mode='valid') for y in centered]
            valid = np.ones(len(signal_energy), dtype=bool)
            for own, other in [(0, 1), (1, 0)]:
                if energies[own] == 0:
                    continue
                if energies[own] <= 1e-12:
                    valid[:] = False
                    break
                if energies[other] <= 1e-12:
                    numerator = dots[own]
                    remaining = signal_energy
                    target = energies[own]
                else:
                    numerator = dots[own] - dots[other] * cross / energies[other]
                    remaining = signal_energy - dots[other] ** 2 / energies[other]
                    target = energies[own] - cross ** 2 / energies[other]
                norm = np.sqrt(np.maximum(remaining * target, 0))
                partial = np.divide(numerator, norm, out=np.zeros_like(norm), where=norm > 0)
                valid &= (remaining > .01 * signal_energy) & (partial >= .75)
            if any(valid[int(i)] for i, _ in hits):
                accepted = True
                break
        if accepted:
            matched += section_energy
            if min(section_energy) > 0:
                landmarks.append(start / 12000)
    return np.divide(matched, energy, out=np.zeros(2), where=energy > 0).tolist(), landmarks


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
