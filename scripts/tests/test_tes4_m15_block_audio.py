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

    def three_bank_recipe(self, target=0):
        return dict(self.recipe, schema_version=4, asset_banks=[[0], [1], [2]],
                    asset_static_attenuation=[0, 120, 240], required_asset_bank=target)

    def three_bank_capture(self, included, offsets):
        # Independent capture-rate construction; never use the assay bank or
        # correlation implementation to manufacture the expected waveform.
        rng = random.Random(8241)
        signal = [rng.randrange(-3, 4) for _ in range(96000)]
        for source in included:
            gain = 10 ** (-[0, 120, 240][source] / 2000)
            for i, value in enumerate(self.sources[source]):
                for frame in range(4):
                    signal[24000 + offsets[source] + i * 4 + frame] += round(value * gain)
        return self.write("three-bank.wav", signal)

    def test_three_bank_target_accepts_independent_mixture_and_required_component(self):
        for offsets in [(0, 0, 0), (0, 960, 0), (960, 0, 960)]:
            for target in range(3):
                with self.subTest(offsets=offsets, target=target):
                    report = audio.analyse(self.three_bank_capture([0, 1, 2], offsets),
                                           self.three_bank_recipe(target), 1)
                    self.assertTrue(report["passed"], report["clusters"])
                    event = report["clusters"][0]
                    self.assertAlmostEqual(event["best"], .5, places=4)
                    self.assertGreater(event["maximum_correlation"], .99)
                    self.assertGreater(event["bank_checks"][target]["partial_correlation"], .99)
                    self.assertGreater(event["bank_checks"][target]["residual_energy_fraction"], .01)

    def test_three_bank_target_rejects_missing_target_even_with_other_layers(self):
        for target in range(3):
            others = [i for i in range(3) if i != target]
            for included in [[], others[:1], others[1:], others]:
                with self.subTest(target=target, included=included):
                    path = self.three_bank_capture(included, (0, 960, 0))
                    self.assertTrue(audio.analyse(path, self.three_bank_recipe(target), 0)["passed"])
                    self.assertFalse(audio.analyse(path, self.three_bank_recipe(target), 1)["passed"])

    def test_three_bank_target_validates_required_bank_and_partition(self):
        for target in [None, True, -1, 3, "0"]:
            with self.subTest(target=target), self.assertRaises(ValueError):
                audio.templates(self.three_bank_recipe(target))
        for banks in [[[0], [1, 2]], [[0], [1], [1, 2]], [[0], [], [1, 2]]]:
            with self.subTest(banks=banks), self.assertRaises(ValueError):
                audio.templates(dict(self.three_bank_recipe(), asset_banks=banks))

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

    def test_two_asset_banks_accept_only_cross_family_pcm_in_both_orders(self):
        recipe = dict(self.recipe, schema_version=2, asset_banks=[[0], [1, 2]])
        events = [(0, 1, False), (0, 2, True), (2, 0, True)]
        result = audio.analyse(self.capture(events, noise=True), recipe, 3)
        self.assertTrue(result["passed"], result["clusters"])
        self.assertEqual(len(result["templates"]), 12)
        for cluster in result["clusters"]:
            self.assertGreater(cluster["maximum_correlation"], .99)
        # Two clips from one family cannot prove a mixed-family block.
        path = self.capture([(1, 2, False), (0, 0, True)])
        self.assertTrue(audio.analyse(path, recipe, 0)["passed"])
        self.assertFalse(audio.analyse(path, recipe, 1)["passed"])

    def test_two_asset_banks_reject_overlap_missing_assets_and_invalid_indices(self):
        for groups in ([[0], [0, 1, 2]], [[0], [1]], [[], [0, 1, 2]],
                       [[False], [1, 2]], [[0], [1, 3]], [[0, 1, 2]], [[0], [1, 2], []]):
            with self.subTest(groups=groups):
                recipe = dict(self.recipe, schema_version=2, asset_banks=groups)
                with self.assertRaises(ValueError):
                    audio.templates(recipe)

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


class NativeHitFingerprintAudioTest(unittest.TestCase):
    """Independent analytic 44.1k sources and 48k capture, including controls."""
    write = NativeBlockMixtureAudioTest.write

    def setUp(self):
        NativeBlockMixtureAudioTest.setUp(self)
        import math
        self.functions = []
        assets = []
        for index in range(3):
            rng = random.Random(417 + index)
            tones = [(rng.uniform(300, 4800), rng.uniform(0, 2 * math.pi)) for _ in range(12)]
            def signal(t, tones=tones):
                return sum(150 * math.sin(2 * math.pi * f * t + phase) for f, phase in tones)
            self.functions.append(signal)
            path = self.write(f"analytic-{index}.wav", [round(signal(i / 44100)) for i in range(17640)],
                              rate=44100, channels=1)
            assets.append(dict(path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
        self.fingerprint_recipe = dict(self.recipe, schema_version=3, assets=assets,
                                       asset_banks=[[0], [1, 2]], asset_static_attenuation=[485, 661, 661])

    def analytic_capture(self, events, *, envelope=False):
        rng = random.Random(644)
        signal = [rng.randrange(-2, 3) for _ in range(48000 * (2 * len(events) + 1))]
        for event, (left, right, delay) in enumerate(events):
            for source, offset in [(left, 0), (right, delay)]:
                gain = 10 ** (-self.fingerprint_recipe['asset_static_attenuation'][source] / 2000)
                for i in range(19200):
                    # Analytic time-domain capture does not use template interpolation.
                    scale = (2 if i < 7680 else .35) if envelope else 1
                    at = (1 + 2 * event) * 48000 + offset + i
                    signal[at] += round(self.functions[source](i / 48000) * gain * scale)
        return self.write('analytic-capture.wav', signal)

    def test_v3_independent_rates_both_orders_and_changing_level(self):
        recipe = self.fingerprint_recipe
        for events, envelope in [([(0, 1, 0)], True),
                                 ([(0, 2, 960), (1, 0, 960)], False)]:
            with self.subTest(events=events, envelope=envelope):
                path = self.analytic_capture(events, envelope=envelope)
                result = audio.analyse(path, recipe, len(events))
                self.assertTrue(result['passed'], result['clusters'])
                for cluster in result['clusters']:
                    self.assertGreaterEqual(min(cluster['per_bank_energy_coverage']), .9)
                    self.assertGreaterEqual(len(cluster['matched_sections_seconds']), 3)
                self.assertFalse(audio.analyse(path, recipe, 0)['passed'], 'Audible replay must fail absence')

    def test_v3_credits_each_bank_when_companion_has_a_quiet_intro(self):
        # The intrinsic stock waveform can start quietly. The capture has the
        # same fixed gains/envelope as the sources; no fitted level or timing.
        original = self.functions[1]
        self.functions[1] = lambda t: original(t) * (.001 if t < .08 else 1)
        path = self.write("quiet-intro.wav", [round(self.functions[1](i / 44100)) for i in range(17640)],
                          rate=44100, channels=1)
        recipe = copy.deepcopy(self.fingerprint_recipe)
        recipe["assets"][1] = dict(path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        result = audio.analyse(self.analytic_capture([(0, 1, 0)]), recipe, 1)
        self.assertTrue(result["passed"], result["clusters"])
        self.assertGreaterEqual(min(result["clusters"][0]["per_bank_energy_coverage"]), .9)
        self.assertGreaterEqual(len(result["clusters"][0]["matched_sections_seconds"]), 3)
        for events in [[(0, 0, 0)], [(1, 2, 0)]]:
            with self.subTest(missing_bank=events):
                self.assertTrue(audio.analyse(self.analytic_capture(events), recipe, 0)["passed"])

    def test_v3_changing_relative_level_may_leave_insufficient_coverage(self):
        # A discontinuous envelope on independently delayed clips changes the
        # relative mixture inside fingerprint sections. Keep this limitation
        # explicit: acceptance must fail rather than relax the 90% criterion.
        path = self.analytic_capture([(0, 2, 960)], envelope=True)
        self.assertFalse(audio.analyse(path, self.fingerprint_recipe, 1)['passed'])

    def test_v3_single_bank_and_unrelated_sound_cannot_prove_mixture(self):
        for events in [[(0, 0, 0)], [(1, 2, 960)], []]:
            path = self.analytic_capture(events)
            result = audio.analyse(path, self.fingerprint_recipe, 0)
            self.assertTrue(result['passed'], result['clusters'])
            self.assertFalse(audio.analyse(path, self.fingerprint_recipe, 1)['passed'])

    def test_v3_inaudible_bank_cannot_be_skipped_as_silence(self):
        path = self.analytic_capture([(0, 0, 0)])
        for attenuation in [3000, 65535]:
            recipe = dict(self.fingerprint_recipe, asset_static_attenuation=[0, attenuation, attenuation])
            self.assertTrue(audio.analyse(path, recipe, 0)['passed'])

    def test_v3_requires_whole_clip_energy_and_multiple_sections(self):
        path = self.analytic_capture([(0, 1, 0)])
        # Keep a recognizable 80ms excerpt; it cannot satisfy 90% energy/three sections.
        values, _, _ = audio.pcm(path, mono=False)
        values[48000 + 3840:] = 0
        path = self.write('excerpt.wav', values[:, 0].astype(int).tolist())
        self.assertTrue(audio.analyse(path, self.fingerprint_recipe, 0)['passed'])

    def test_v3_gain_metadata_is_strict_and_legacy_schema_stays_strict(self):
        for value in [[], [485, 661], [True, 661, 661], [-1, 661, 661], [485., 661, 661], [65536, 0, 0]]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                audio.templates(dict(self.fingerprint_recipe, asset_static_attenuation=value))
        with self.assertRaises(ValueError):
            audio.templates(dict(self.recipe, asset_static_attenuation=[0, 0, 0]))
