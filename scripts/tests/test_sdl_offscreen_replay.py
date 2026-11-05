import sys
from pathlib import Path
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import sdl_offscreen_replay as replay


class SdlOffscreenReplayTest(unittest.TestCase):
    def test_xvfb_replay_requires_exclusive_private_server(self):
        replay.validate(dict(sdl_xvfb_input=True, xvfb=True,
                             actions=[dict(type="keys_down", values=["u", "space"])]))
        for options in [dict(sdl_xvfb_input=True), dict(sdl_xvfb_input=True, xvfb=False),
                        dict(sdl_xvfb_input=True, xvfb=1), dict(sdl_xvfb_input="true", xvfb=True),
                        dict(sdl_xvfb_input=True, xvfb=True, sdl_offscreen_input=True),
                        dict(sdl_xvfb_input=True, xvfb=True, m15={}),
                        dict(sdl_xvfb_input=True, xvfb=True, m14={})]:
            with self.subTest(options=options), self.assertRaises(ValueError):
                replay.validate(options)

    def test_observed_motion_uses_latest_complete_bounded_sample(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            log = output / 'process.log'
            log.write_text('é\nmotion x=20 y=30\nmotion x=-4 y=12\nmotion x=99 y=5')
            action = dict(type='mouse_move', x=0, y=0, from_log=dict(path='process.log',
                pattern=r'motion x=(?P<x>-?\d+) y=(?P<y>-?\d+)$'))
            resolved, receipt = replay.observed_mouse_motion(action, output)
            self.assertEqual((resolved['x'], resolved['y']), (-4, 12))
            self.assertEqual(action['x'], 0)
            self.assertEqual(receipt['sample'], 'motion x=-4 y=12')
            self.assertEqual(log.read_bytes()[receipt['byte_offset']:].splitlines()[0], b'motion x=-4 y=12')
            log.write_text('motion x=8193 y=0\n')
            with self.assertRaises(ValueError):
                replay.observed_mouse_motion(action, output)
            action['from_log']['path'] = '../outside.log'
            with self.assertRaises(ValueError):
                replay.observed_mouse_motion(action, output)
            for source in [dict(path='../outside.log', pattern=action['from_log']['pattern']),
                           dict(path='process.log', pattern=r'motion (\d+) (\d+)'), None]:
                action['from_log'] = source
                with self.assertRaises(ValueError):
                    replay.validate(dict(sdl_offscreen_input=True, actions=[action]))

    def test_key_transition_rejects_invalid_or_overlapping_keys_before_delivery(self):
        control = object.__new__(replay.Replay)
        control.sequence = 0
        control.send_many = mock.Mock()
        for release, press in [(None, ["F5"]), (["u"], []), ([], ["F5"]),
                               (["u", "U"], ["F5"]), (["U"], ["u"]),
                               ([24], ["F5"]), (["u"], ["quit"]),
                               (list("abcdefghijklmnop"), ["q"])]:
            action = dict(type="keys_transition", release=release, press=press)
            with self.subTest(action=action):
                with self.assertRaises(ValueError):
                    replay.validate(dict(sdl_offscreen_input=True, actions=[action]))
                with self.assertRaises(ValueError):
                    control.action(action)
        control.send_many.assert_not_called()
        with self.assertRaises(ValueError):
            replay.validate(dict(actions=[dict(type="keys_transition", release=["u"], press=["F5"])]))

    def test_release_and_press_share_one_batch_and_wait_for_all_receipts(self):
        with tempfile.TemporaryDirectory() as directory:
            control = object.__new__(replay.Replay)
            control.path = Path(directory) / "input"
            control.path.write_bytes(b"")
            control.sequence, control.expected = 0, b""
            control.receipt_timeout_seconds = 10
            receipt = Path(str(control.path) + ".delivered")
            expected = b"1 up 24\n2 down 62\n"
            sleeps = []
            def deliver(_):
                self.assertEqual(control.path.read_bytes(), expected)
                sleeps.append(True)
                receipt.write_bytes(b"1 up 24\n" if len(sleeps) == 1 else expected)
            action = dict(type="keys_transition", release=["u"], press=["F5"])
            replay.validate(dict(sdl_offscreen_input=True, actions=[action]))
            with mock.patch.object(replay.time, "sleep", side_effect=deliver):
                result = control.action(action)
            self.assertEqual(len(sleeps), 2)
            self.assertEqual((result['first_input_sequence'], result['last_input_sequence']), (1, 2))
            self.assertEqual(receipt.read_bytes(), expected)

    def test_batched_keys_validate_before_delivery(self):
        control = object.__new__(replay.Replay)
        control.sequence = 0
        control.send_many = mock.Mock()
        for values in [None, "u space", [], [24], ["u", "U"], ["u", "quit"], ["a"] * 17]:
            action = dict(type="keys_down", values=values)
            with self.subTest(values=values):
                with self.assertRaises(ValueError): replay.validate(dict(sdl_offscreen_input=True, actions=[action]))
                with self.assertRaises(ValueError): control.action(action)
        control.send_many.assert_not_called()
        with self.assertRaises(ValueError): replay.validate(dict(actions=[dict(type="keys_down", values=["u", "space"])]))

    def test_batched_keys_write_together_and_wait_for_every_receipt(self):
        with tempfile.TemporaryDirectory() as directory:
            control = object.__new__(replay.Replay)
            control.path = Path(directory) / "input"
            control.path.write_bytes(b"")
            control.sequence, control.expected = 0, b""
            control.receipt_timeout_seconds = 10
            receipt = Path(str(control.path) + ".delivered")
            expected = b"1 down 24\n2 down 44\n"
            sleeps = []
            def deliver(_):
                self.assertEqual(control.path.read_bytes(), expected)
                sleeps.append(True)
                receipt.write_bytes(b"1 down 24\n" if len(sleeps) == 1 else expected)
            with mock.patch.object(replay.time, "sleep", side_effect=deliver):
                result = control.action(dict(type="keys_down", values=["u", "space"]))
            self.assertEqual(len(sleeps), 2)
            self.assertEqual((result['first_input_sequence'], result['last_input_sequence']), (1, 2))
            self.assertEqual(receipt.read_bytes(), expected)

    def test_controller_receipts_use_sdl_controller_enums_and_check_dpad_neutral(self):
        control = object.__new__(replay.Replay)
        control.send = mock.Mock()
        control.controller_button('rightshoulder', True)
        control.controller_axis('right_x', -16384)
        control.controller_axis('right_trigger', 32767)
        control.controller_axis('dpad_x', -1)
        control.controller_axis('dpad_x', 0)
        self.assertEqual(control.send.call_args_list, [
            mock.call('controller-button', '10 1'), mock.call('controller-axis', '2 -16384'),
            mock.call('controller-axis', '5 32767'), mock.call('controller-button', '13 1'),
            mock.call('controller-button', '14 0'), mock.call('controller-button', '13 0'),
            mock.call('controller-button', '14 0')])

    def test_mouse_buttons_deliver_acknowledged_down_up_and_click_protocol(self):
        control = object.__new__(replay.Replay)
        control.sequence = 0
        control.send = mock.Mock()
        for button in range(1, 6):
            for kind, operation in (('mouse_down', 'button-down'), ('mouse_up', 'button-up')):
                action = dict(type=kind, button=button)
                replay.validate(dict(sdl_offscreen_input=True, actions=[action]))
                control.action(action)
                control.send.assert_called_with(operation, button)
        control.send.reset_mock()
        with mock.patch.object(replay.time, 'sleep') as sleep:
            action = dict(type='mouse_click', button=1, hold_seconds=.125)
            replay.validate(dict(sdl_offscreen_input=True, actions=[action]))
            control.action(action)
            self.assertEqual(control.send.call_args_list, [mock.call('button-down', 1), mock.call('button-up', 1)])
            sleep.assert_called_once_with(.125)

    def test_mouse_buttons_and_click_duration_reject_before_delivery(self):
        control = object.__new__(replay.Replay)
        control.sequence = 0
        control.send = mock.Mock()
        actions = [dict(type=kind, button=value) for kind in ('mouse_down', 'mouse_up', 'mouse_click')
                   for value in (0, 6, -1, True, 1.0, '1', None, '1\n2 quit')]
        actions += [dict(type='mouse_click', button=1, hold_seconds=value)
                    for value in (-1, True, '1', None, float('nan'), float('inf'))]
        for action in actions:
            with self.subTest(action=action):
                with self.assertRaises(ValueError): replay.validate(dict(sdl_offscreen_input=True, actions=[action]))
                with self.assertRaises(ValueError): control.action(action)
        control.send.assert_not_called()

    def test_relative_mouse_motion_uses_signed_sdl_delta_protocol(self):
        control = object.__new__(replay.Replay)
        control.sequence = 0
        control.send = mock.Mock()
        for x, y in [(-31, 80), (0, 0), (-(2 ** 31), 2 ** 31 - 1)]:
            action = {"type": "mouse_move", "x": x, "y": y}
            replay.validate({"sdl_offscreen_input": True, "actions": [action]})
            control.action(action)
            control.send.assert_called_with("motion", f"{x} {y}")

    def test_relative_mouse_motion_rejects_malformed_deltas_before_delivery(self):
        control = object.__new__(replay.Replay)
        control.sequence = 0
        control.send = mock.Mock()
        for value in [None, True, 1.0, "1", "1\n2 quit", float("nan"), float("inf"),
                      -(2 ** 31) - 1, 2 ** 31]:
            for axis in ("x", "y"):
                action = {"type": "mouse_move", "x": 0, "y": 0, axis: value}
                with self.subTest(action=action):
                    with self.assertRaises(ValueError):
                        replay.validate({"sdl_offscreen_input": True, "actions": [action]})
                    with self.assertRaises(ValueError):
                        control.action(action)
        control.send.assert_not_called()

    def test_key_map_and_rejection_of_chords_or_raw_codes(self):
        for name, code in [("a", 4), ("z", 29), ("1", 30), ("9", 38), ("0", 39),
                           ("F5", 62), ("F12", 69), ("Return", 40), ("grave", 53),
                           ("KP_2", 90), ("Shift_L", 225), ("Control_L", 224)]:
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
