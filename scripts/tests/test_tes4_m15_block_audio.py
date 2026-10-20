import copy
import hashlib
import itertools
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import unittest
import wave

from scripts import tes4_m15_block_audio as audio


class NativeBlockMixtureAudioTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.sources = []
        assets = []
        for index in range(3):
            rng = random.Random(1700 + index)
            signal = [rng.randrange(-2000, 2001) for _ in range(600 + index * 120)]
            self.sources.append(signal)
            path = self.write(f"source-{index}.wav", signal, rate=12000, channels=1)
            assets.append(dict(path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
        self.recipe = dict(schema_version=1, assets=assets, sample_rate=48000, channels=2,
                           analysis_rate=12000, mixer_period_frames=960,
                           minimum_normalized_correlation=.75, cluster_gap_seconds=1)

    def write(self, name, signal, *, rate=48000, channels=2, antiphase=False):
        path = self.root / name
        with wave.open(str(path), "wb") as stream:
            stream.setnchannels(channels)
            stream.setsampwidth(2)
            stream.setframerate(rate)
            data = []
            for value in signal:
                data.extend((value, -value) if antiphase else (value,) * channels)
            stream.writeframes(struct.pack(f"<{len(data)}h", *data))
        return path

    def capture(self, events, *, noise=False, antiphase=False):
        # Independent time-domain construction at the capture rate. Do not use
        # the assay's resampling, bank builder or correlation implementation.
        rng = random.Random(1986)
        signal = [rng.randrange(-8, 9) if noise else 0 for _ in range(48000 * (2 * len(events) + 1))]
        for event, (left, right, delayed) in enumerate(events):
            start = (1 + 2 * event) * 48000
            for source, offset in ((left, 0), (right, 960 if delayed else 0)):
                for i, value in enumerate(self.sources[source]):
                    for frame in range(4):
                        signal[start + offset + i * 4 + frame] += value
        return self.write("capture.wav", signal, antiphase=antiphase)

    def test_independent_pcm_contains_every_pair_and_one_period_order(self):
        events = [(a, b, False) for a, b in itertools.combinations_with_replacement(range(3), 2)]
        events += [(a, b, True) for a, b in itertools.product(range(3), repeat=2)]
        result = audio.analyse(self.capture(events, noise=True), self.recipe, 15)
        self.assertTrue(result["passed"], result["clusters"])
        self.assertEqual(len(result["templates"]), 30)  # 15 fixed hypotheses, two channels.
        for index, cluster in enumerate(result["clusters"]):
            self.assertAlmostEqual(cluster["best"], 1 + 2 * index, places=4)
            self.assertLessEqual(cluster["first"], 1 + 2 * index)
            self.assertGreaterEqual(cluster["last"], 1 + 2 * index)
            self.assertGreater(cluster["maximum_correlation"], .99)

    def test_absence_noise_dc_and_unrelated_signal_do_not_pass_positive(self):
        rng = random.Random(9876)
        for name, values in [("silence", [0] * 48000), ("dc", [23] * 48000),
                             ("noise", [rng.randrange(-2000, 2001) for _ in range(48000)])]:
            with self.subTest(name=name):
                path = self.write(name + ".wav", values)
                self.assertTrue(audio.analyse(path, self.recipe, 0)["passed"])
                self.assertFalse(audio.analyse(path, self.recipe, 1)["passed"])

    def test_antiphase_target_cannot_pass_a_zero_replay_check(self):
        path = self.capture([(0, 1, True)], antiphase=True)
        self.assertTrue(audio.analyse(path, self.recipe, 1)["passed"])
        self.assertFalse(audio.analyse(path, self.recipe, 0)["passed"])

    def test_wrong_count_format_truncation_and_short_capture_reject(self):
        path = self.capture([(0, 2, False)])
        self.assertFalse(audio.analyse(path, self.recipe, 2)["passed"])
        for count in (True, -1, 1.0, None):
            with self.assertRaises(ValueError):
                audio.analyse(path, self.recipe, count)
        for path in (self.write("mono.wav", [0] * 48000, channels=1),
                     self.write("rate.wav", [0] * 48000, rate=24000),
                     self.write("short.wav", [0] * 20)):
            with self.assertRaises(ValueError):
                audio.analyse(path, self.recipe, 0)
        path = self.capture([(0, 2, False)])
        path.write_bytes(path.read_bytes()[:-4])
        with self.assertRaises(ValueError):
            audio.analyse(path, self.recipe, 0)

    def test_recipe_cannot_change_threshold_period_or_asset_identity(self):
        for key, value in [("schema_version", True), ("sample_rate", 24000),
                           ("channels", 1), ("mixer_period_frames", 480),
                           ("minimum_normalized_correlation", .7), ("cluster_gap_seconds", 2),
                           ("minimum_normalized_correlation", float("nan"))]:
            with self.subTest(key=key, value=value):
                recipe = dict(self.recipe, **{key: value})
                with self.assertRaises(ValueError):
                    audio.templates(recipe)
        for change in ("hash", "duplicate", "empty", "constant"):
            with self.subTest(change=change):
                recipe = copy.deepcopy(self.recipe)
                if change == "hash":
                    recipe["assets"][0]["sha256"] = "0" * 64
                elif change == "duplicate":
                    recipe["assets"][1] = recipe["assets"][0]
                elif change == "empty":
                    recipe["assets"] = []
                else:
                    path = self.write("constant.wav", [1] * 600, rate=12000, channels=1)
                    recipe["assets"][0] = dict(path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                with self.assertRaises(ValueError):
                    audio.templates(recipe)

    def test_cli_refuses_to_replace_a_historical_report(self):
        path = self.capture([(0, 1, False)])
        recipe = self.root / "recipe.json"
        recipe.write_text(json.dumps(self.recipe))
        output = self.root / "report.json"
        output.write_text("historical failure\n")
        result = subprocess.run([sys.executable, audio.__file__, "--wave", str(path),
                                 "--recipe", str(recipe), "--clusters", "1", "--output", str(output)],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("FileExistsError", result.stderr)
        self.assertEqual(output.read_text(), "historical failure\n")
