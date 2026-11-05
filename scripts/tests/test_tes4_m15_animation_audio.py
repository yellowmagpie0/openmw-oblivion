import math
import copy
import json
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock
import wave

from scripts import tes4_m15_animation_audio as audio


class NativeAnimationAudioTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def pcm(self, frequency=1000, channels=2, frames=4800):
        path = self.root / "capture.wav"
        with wave.open(str(path), "wb") as stream:
            stream.setnchannels(channels)
            stream.setsampwidth(2)
            stream.setframerate(48000)
            stream.writeframes(b"".join(struct.pack("<h", round(8000 * math.sin(2 * math.pi * frequency * n / 48000)))
                                      * channels for n in range(frames)))
        return path

    def test_detector_rejects_silence_and_wrong_frequency_and_negative_rejects_tone(self):
        tone = self.pcm()
        self.assertTrue(audio.analyse_audio(tone, audio.DEFAULT_EXPECTATIONS, "positive")["passed"])
        self.assertFalse(audio.analyse_audio(tone, audio.DEFAULT_EXPECTATIONS, "negative")["passed"])
        wrong = self.pcm(2000)
        self.assertFalse(audio.analyse_audio(wrong, audio.DEFAULT_EXPECTATIONS, "positive")["passed"])
        self.assertTrue(audio.analyse_audio(wrong, audio.DEFAULT_EXPECTATIONS, "negative")["passed"])
        silent = self.pcm(0)
        self.assertFalse(audio.analyse_audio(silent, audio.DEFAULT_EXPECTATIONS, "positive")["passed"])
        self.assertTrue(audio.analyse_audio(silent, audio.DEFAULT_EXPECTATIONS, "negative")["passed"])

    def test_antiphase_stereo_cannot_hide_a_tone_from_negative_control(self):
        path = self.root / "antiphase.wav"
        with wave.open(str(path), "wb") as stream:
            stream.setnchannels(2)
            stream.setsampwidth(2)
            stream.setframerate(48000)
            stream.writeframes(b"".join(struct.pack("<hh", value, -value) for value in
                                      [round(8000 * math.sin(2 * math.pi * 1000 * n / 48000))
                                       for n in range(4800)]))
        self.assertTrue(audio.analyse_audio(path, audio.DEFAULT_EXPECTATIONS, "positive")["passed"])
        self.assertFalse(audio.analyse_audio(path, audio.DEFAULT_EXPECTATIONS, "negative")["passed"])

    def test_detector_rejects_wrong_format_truncation_short_and_malformed_tolerances(self):
        with self.assertRaises(ValueError):
            audio.analyse_audio(self.pcm(channels=1), audio.DEFAULT_EXPECTATIONS, "positive")
        with self.assertRaises(ValueError):
            audio.analyse_audio(self.pcm(frames=10), audio.DEFAULT_EXPECTATIONS, "positive")
        path = self.pcm()
        path.write_bytes(path.read_bytes()[:-10])
        with self.assertRaises(ValueError):
            audio.analyse_audio(path, audio.DEFAULT_EXPECTATIONS, "positive")
        for key, value in [("window_frames", True), ("window_frames", 48001), ("tone_frequency_hz", 24000),
                           ("positive_minimum_rms", math.nan), ("negative_maximum_tone_rms", -1),
                           ("maximum_seconds", 121), ("sample_width", 4), ("positive_minimum_rms", 0),
                           ("positive_minimum_tone_energy_fraction", 0), ("negative_maximum_tone_rms", .02),
                           ("window_frames", 1)]:
            with self.subTest(key=key, value=value):
                expectations = dict(audio.DEFAULT_EXPECTATIONS)
                expectations[key] = value
                with self.assertRaises(ValueError):
                    audio.analyse_audio(path, expectations, "positive")

    def test_fixture_preserves_outputs_and_rejects_malformed_editor_id_before_writing(self):
        output = self.root / "fixture"
        for value in ("", "name\0tail", "../name", "é", "x" * 65):
            with self.assertRaises(ValueError):
                audio.write_fixture(output, value)
            self.assertFalse(output.exists())
        first = audio.write_fixture(output, "M15FixtureTone")
        files = {path: (output / path).read_bytes() for path in first["files"]}
        with self.assertRaises(FileExistsError):
            audio.write_fixture(output, "M15MissingTone")
        for path, data in files.items():
            self.assertEqual((output / path).read_bytes(), data)

    def test_negative_fixture_changes_only_animation_key_and_retains_native_sound_and_tone(self):
        positive = self.root / "positive"
        negative = self.root / "negative"
        left = audio.write_fixture(positive, "M15FixtureTone")
        right = audio.write_fixture(negative, "M15MissingTone")
        changed = [name for name in left["files"] if left["files"][name] != right["files"][name]]
        self.assertEqual(changed, ["meshes/characters/_1stperson/handtohandattackleft.kf"])
        self.assertEqual(left["sound_editor_id"], right["sound_editor_id"])

    def test_native_fixture_phase_negative_changes_only_kf_and_rejects_options_before_write(self):
        output = self.root / "bad-argument"
        for value in ("", "unknown", None, 1, []):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    audio.write_fixture(output, "M15FixtureTone", value)
                self.assertFalse(output.exists())
        positive = self.root / "complete"
        negative = self.root / "missing-hit"
        left = audio.write_fixture(positive, "M15FixtureTone")
        right = audio.write_fixture(negative, "M15FixtureTone", "missing-hit")
        changed = [name for name in left["files"] if left["files"][name] != right["files"][name]]
        self.assertEqual(changed, ["meshes/characters/_1stperson/handtohandattackleft.kf"])
        self.assertEqual(left["keyframe_version"], "20.0.0.5")
        self.assertEqual(right["phase_keys"], "missing-hit")
        data = (positive / changed[0]).read_bytes()
        self.assertTrue(data.startswith(b"Gamebryo File Format, Version 20.0.0.5\n"))
        self.assertIn(b"NiControllerSequence", data)
        self.assertIn(b"\x03\0\0\0Hit", data)
        self.assertNotIn(b"\x03\0\0\0Hit", (negative / changed[0]).read_bytes())

    def spatial_recipe(self):
        return json.loads((Path(__file__).parents[1] / "data/oblivion_compat/m15_spatial_audio_calibration.json").read_text())

    def spatial_pcm(self, gains=(1, .2, 0, 1), frequency=1000, offset=0):
        path = self.root / "spatial.wav"
        samples = bytearray(48000 * (9 + offset) * 4)
        starts = [n + offset for n in (1, 3, 5, 7)]
        for start, gain in zip(starts, gains):
            for n in range(12000):
                value = round(8000 * gain * math.sin(2 * math.pi * frequency * n / 48000))
                struct.pack_into("<hh", samples, (start * 48000 + n) * 4, value, value)
        with wave.open(str(path), "wb") as stream:
            stream.setnchannels(2)
            stream.setsampwidth(2)
            stream.setframerate(48000)
            stream.writeframes(samples)
        emitters = self.spatial_recipe()["emitters"]
        sequence = emitters + [emitters[0]]
        log = "\n".join(f"[00:{start // 60:02d}:{start % 60:02d}.000 I] M7 ObScript: playsound3d "
                        f"sound=content:m15audiofixture.esp:000800 ref={e['reference']} played=true"
                        for start, e in zip(starts, sequence))
        return path, log

    def test_long_spatial_capture_preserves_request_clock_across_detector_chunks(self):
        path, log = self.spatial_pcm(offset=119)
        result = audio.analyse_spatial(path, log, self.spatial_recipe())
        self.assertTrue(result["passed"])
        self.assertEqual(result["clusters"][0]["start"], 120)

    def test_spatial_measurement_requires_attenuation_silence_and_repeated_positive_control(self):
        recipe = self.spatial_recipe()
        path, log = self.spatial_pcm()
        self.assertTrue(audio.analyse_spatial(path, log, recipe)["passed"])
        for gains in ((1, 1, 0, 1), (1, 0, 0, 1), (1, .2, .2, 1), (1, .2, 0, 0), (1, .2, 0, .5)):
            with self.subTest(gains=gains):
                path, log = self.spatial_pcm(gains)
                self.assertFalse(audio.analyse_spatial(path, log, recipe)["passed"])

    def test_spatial_measurement_rejects_wrong_signal_requests_and_clock_alignment(self):
        recipe = self.spatial_recipe()
        path, log = self.spatial_pcm(frequency=2000)
        self.assertFalse(audio.analyse_spatial(path, log, recipe)["passed"])
        path, log = self.spatial_pcm()
        for wrong in (log.replace("played=true", "played=false", 1), log + "\n" + log.splitlines()[0],
                      log.replace("00:00:07.000", "00:00:08.000"), log.replace("032ae4", "032b01")):
            self.assertFalse(audio.analyse_spatial(path, wrong, recipe)["passed"])

    def test_spatial_source_preflight_rejects_pending_state_and_bad_emitter_binding(self):
        recipe = self.spatial_recipe()
        state = dict(physical_actions=dict(pending=[]), native_melee_states=[dict(strike=None)],
                     player=dict(reference="dynamic:player:0000000000000001", cell=recipe["cell"],
                                 position=recipe["player_position"]),
                     references=[dict(key=e["reference"], base=e["base"], cell=recipe["cell"],
                                      enabled=True, position=e["position"]) for e in recipe["emitters"]])
        self.assertTrue(audio.spatial_preflight(state, recipe)["passed"])
        for lane in ("pending", "strike", "disabled", "moved", "cell", "nonfinite"):
            changed = copy.deepcopy(state)
            if lane == "pending": changed["physical_actions"]["pending"] = [1]
            if lane == "strike": changed["native_melee_states"][0]["strike"] = dict(id=1)
            if lane == "disabled": changed["references"][0]["enabled"] = False
            if lane == "moved": changed["references"][0]["position"][0] += 10
            if lane == "cell": changed["references"][0]["cell"] = "other"
            if lane == "nonfinite": changed["references"][0]["position"][0] = math.nan
            with self.subTest(lane=lane), self.assertRaises(ValueError):
                audio.spatial_preflight(changed, recipe)

    def test_spatial_preparation_reuses_base_and_private_source_without_per_frame_world_reports(self):
        try:
            import tes4_runtime_state as runtime
        except ModuleNotFoundError:
            from scripts import tes4_runtime_state as runtime
        recipe = self.spatial_recipe()
        source = self.root / "input.omwsave"
        source.write_bytes(b"pristine source")
        recipe["source"] = str(source)
        recipe["source_sha256"] = hashlib.sha256(source.read_bytes()).hexdigest()
        recipe["data_directories"] = ["plugins"]
        recipe["base_manifest"] = str((Path(__file__).parents[1] / recipe["base_manifest"].removeprefix("scripts/")).resolve())
        plugins = self.root / "plugins"
        plugins.mkdir()
        content = []
        for name in ("Oblivion.esm", "M15StationaryOpponent.esp", "M15CombinedMasteryChance.esp"):
            path = plugins / name
            path.write_bytes(name.encode())
            content.append(dict(plugin=name.lower(), fingerprint="sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()))
        data = self.root / "data"
        data.mkdir()
        state = dict(content=content, physical_actions=dict(pending=[]), native_melee_states=[],
                     player=dict(reference="dynamic:player:0000000000000001", cell=recipe["cell"], position=recipe["player_position"]),
                     references=[dict(key=e["reference"], base=e["base"], cell=recipe["cell"],
                                      enabled=True, position=e["position"]) for e in recipe["emitters"]])
        base = Path(recipe["base_manifest"])
        original = base.read_bytes()
        output = self.root / "case"
        with mock.patch.object(runtime, "load_save", return_value=state):
            audio.prepare_spatial(self.root, data, recipe, output)
            with self.assertRaises(FileExistsError):
                audio.prepare_spatial(self.root, data, recipe, output)
        generated = json.loads((output / "manifest.json").read_text())
        self.assertNotIn("OPENMW_OBSCRIPT_REPORT", generated["environment"])
        self.assertTrue(all(a["abort_on_failure"] for a in generated["actions"] if a["type"] == "wait_log"))
        self.assertEqual((output / "userdata/saves/M15Review/Quicksave.omwsave").read_bytes(), b"pristine source")
        self.assertEqual(source.read_bytes(), b"pristine source")
        self.assertEqual(base.read_bytes(), original)
