import math
from pathlib import Path
import struct
import tempfile
import unittest
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
