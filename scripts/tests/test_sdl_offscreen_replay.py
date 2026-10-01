import sys
from pathlib import Path
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import sdl_offscreen_replay as replay


class SdlOffscreenReplayTest(unittest.TestCase):
    def test_key_map_and_rejection_of_chords_or_raw_codes(self):
        for name, code in [("a", 4), ("z", 29), ("1", 30), ("9", 38), ("0", 39),
                           ("F5", 62), ("F12", 69), ("Return", 40), ("grave", 53),
                           ("Shift_L", 225), ("Control_L", 224)]:
            self.assertEqual(replay.scancode(name), code)
        for name in [None, 62, "", "F13", "ctrl+a", "0x3e", "Return;quit"]:
            with self.subTest(name=name), self.assertRaises(ValueError):
                replay.scancode(name)

    def test_protocol_requires_exact_delivery_and_rejects_other_receipts(self):
        with tempfile.TemporaryDirectory() as directory:
            control = object.__new__(replay.Replay)
            control.path = Path(directory) / "input"
            control.path.write_bytes(b"")
            control.sequence, control.expected = 0, b""
            control.receipt_timeout_seconds = 10
            receipt = Path(str(control.path) + ".delivered")
            with mock.patch.object(replay.time, "sleep", side_effect=lambda _: receipt.write_bytes(
                    control.path.read_bytes())):
                self.assertEqual(control.send("down", 62), 1)
                self.assertEqual(control.send("up", 62), 2)
            self.assertEqual(control.path.read_bytes(), b"1 down 62\n2 up 62\n")
            receipt.write_bytes(b"1 down 69\n")
            with self.assertRaisesRegex(RuntimeError, "differs"):
                control.send("quit")

    def test_slow_frame_receipt_uses_configured_timeout(self):
        with tempfile.TemporaryDirectory() as directory:
            control = object.__new__(replay.Replay)
            control.path = Path(directory) / "input"
            control.path.write_bytes(b"")
            control.sequence, control.expected = 0, b""
            control.receipt_timeout_seconds = 10
            control.receipt_timeout_seconds = 30
            receipt = Path(str(control.path) + ".delivered")
            with mock.patch.object(replay.time, "monotonic", side_effect=[0, 11, 12]), \
                    mock.patch.object(replay.time, "sleep", side_effect=lambda _: receipt.write_bytes(
                        control.path.read_bytes())):
                self.assertEqual(control.send("down", 62), 1)
            self.assertEqual(receipt.read_bytes(), b"1 down 62\n")

    def test_manifest_receipt_timeout_rejects_nonfinite_or_nonpositive(self):
        for value in [0, -1, True, "30", float("nan"), float("inf")]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                replay.validate({"sdl_offscreen_input": True, "sdl_input_timeout_seconds": value})
        replay.validate({"sdl_offscreen_input": True, "sdl_input_timeout_seconds": 300})

    def test_missing_delivery_times_out(self):
        with tempfile.TemporaryDirectory() as directory:
            control = object.__new__(replay.Replay)
            control.path = Path(directory) / "input"
            control.path.write_bytes(b"")
            control.sequence, control.expected = 0, b""
            control.receipt_timeout_seconds = 10
            with self.assertRaises(TimeoutError):
                control.send("quit", timeout=0)

    def test_invalid_timing_and_text_reject_before_sending_input(self):
        control = object.__new__(replay.Replay)
        control.sequence = 0
        control.send = mock.Mock()
        for action in [{"type": "key_hold", "value": "w", "seconds": -1},
                       {"type": "key_held", "value": "w", "pause_seconds": -1},
                       {"type": "key_hold", "value": "w", "seconds": float("nan")},
                       {"type": "type", "value": "command\nquit"}]:
            with self.subTest(action=action), self.assertRaises(ValueError):
                control.action(action)
        control.send.assert_not_called()

    def test_text_chunks_fit_sdl_payload_without_changing_text(self):
        control = object.__new__(replay.Replay)
        control.sequence = 0
        control.send = mock.Mock()
        text = "a" * 31 + " b" * 20
        control.action({"type": "type_held", "value": text})
        chunks = [bytes.fromhex(call.args[1]) for call in control.send.call_args_list]
        self.assertEqual(b"".join(chunks).decode("ascii"), text)
        self.assertTrue(all(0 < len(chunk) <= 31 for chunk in chunks))


if __name__ == "__main__":
    unittest.main()
