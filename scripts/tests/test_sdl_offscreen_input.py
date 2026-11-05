"""Exercise input replay against real SDL, without an engine, including a private Xvfb display."""

import os
import sys
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


class SdlOffscreenInputTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="m15-sdl-replay-")
        cls.root = Path(cls.directory.name)
        flags = shlex.split(subprocess.check_output(
            ["pkg-config", "--cflags", "--libs", "sdl2"], text=True))
        source = Path(__file__).resolve().parents[1] / "sdl_offscreen_input.cpp"
        cls.library = cls.root / "replay.so"
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
                        str(source), *flags, "-ldl", "-o", str(cls.library)], check=True)
        driver = cls.root / "driver.cpp"
        driver.write_text(r'''
#include <SDL.h>
#include <iostream>
#include <string>
#include <sstream>
int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)) return 2;
    SDL_GameController* controller = nullptr;
    SDL_Event e{};
    while (SDL_PollEvent(&e)) {}
    std::cout << "ready" << std::endl;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "controller") {
            const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
            controller = SDL_GameControllerOpen(index);
            std::cout << "controller " << (controller != nullptr) << std::endl;
        } else if (line.rfind("axis ", 0) == 0 || line.rfind("padbutton ", 0) == 0) {
            std::istringstream fields(line);
            std::string kind; int code, value; fields >> kind >> code >> value;
            auto* joystick = SDL_GameControllerGetJoystick(controller);
            int result = kind == "axis" ? SDL_JoystickSetVirtualAxis(joystick, code, value)
                : SDL_JoystickSetVirtualButton(joystick, code, value);
            std::cout << "changed " << result << std::endl;
        } else if (line == "padstate") {
            std::cout << "state " << SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
                      << " " << static_cast<int>(SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A))
                      << std::endl;
        } else if (line == "drain") {
            int downs = 0;
            while (SDL_PollEvent(&e)) if (e.type == SDL_KEYDOWN) ++downs;
            const auto* keys = SDL_GetKeyboardState(nullptr);
            std::cout << "batch " << downs << " " << static_cast<int>(keys[SDL_SCANCODE_U])
                      << " " << static_cast<int>(keys[SDL_SCANCODE_SPACE]) << std::endl;
        } else if (line == "push") {
            e = {}; e.type = SDL_USEREVENT; SDL_PushEvent(&e);
            std::cout << "pushed" << std::endl;
        } else {
            e = {};
            int result = SDL_PollEvent(&e), count = 0;
            if (line == "text") {
                std::cout << "text " << result << " " << e.type << " " << e.text.text << std::endl;
                continue;
            }
            if (line == "window") {
                std::cout << "window " << result << " " << e.type << " " << e.window.windowID
                          << " " << static_cast<int>(e.window.event) << std::endl;
                continue;
            }
            if (line == "motion") {
                const auto* keys = SDL_GetKeyboardState(nullptr);
                std::cout << "motion " << result << " " << e.type << " " << e.motion.windowID
                          << " " << e.motion.which << " " << e.motion.state
                          << " " << e.motion.xrel << " " << e.motion.yrel
                          << " " << static_cast<int>(keys[SDL_SCANCODE_A]) << std::endl;
                continue;
            }
            if (line == "button") {
                const auto* keys = SDL_GetKeyboardState(nullptr);
                std::cout << "button " << result << " " << e.type << " " << e.button.windowID
                          << " " << e.button.which << " " << static_cast<int>(e.button.button)
                          << " " << static_cast<int>(e.button.state) << " " << static_cast<int>(e.button.clicks)
                          << " " << SDL_GetMouseState(nullptr, nullptr)
                          << " " << static_cast<int>(keys[SDL_SCANCODE_A]) << std::endl;
                continue;
            }
            const auto* keys = SDL_GetKeyboardState(&count);
            std::cout << result << " " << e.type << " " << e.key.keysym.scancode
                      << " " << e.key.keysym.sym << " " << e.key.keysym.mod
                      << " " << static_cast<int>(keys[SDL_SCANCODE_A])
                      << " " << static_cast<int>(keys[SDL_SCANCODE_LSHIFT])
                      << " " << count << std::endl;
        }
    }
    if (controller) SDL_GameControllerClose(controller);
    SDL_Quit();
}
''')
        cls.driver = cls.root / "driver"
        subprocess.run(["c++", "-std=c++17", str(driver), *flags, "-o", str(cls.driver)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def setUp(self):
        self.input = self.root / f"{self._testMethodName}.txt"
        self.input.write_text("")
        self.processes = []
        self.servers = []

    def tearDown(self):
        for process in self.processes:
            process.stdin.close()
            process.wait(timeout=10)
            process.stdout.close()
            process.stderr.close()
            self.assertEqual(process.returncode, 0)

        for server in self.servers:
            server.terminate()
            server.wait(timeout=10)

    def private_display(self):
        sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
        from oblivion_compat import _start_xvfb
        process, display = _start_xvfb(self.root, 320, 240)
        self.servers.append(process)
        return display

    def launch(self, *, enabled=True, driver="offscreen", replay_environment=None):
        env = dict(os.environ, SDL_VIDEODRIVER=driver, LD_PRELOAD=str(self.library))
        for key in ("OPENMW_SDL_INPUT", "OPENMW_SDL_REPLAY_VIDEO_DRIVER", "OPENMW_SDL_REPLAY_XVFB_DISPLAY"):
            env.pop(key, None)
        env.update(replay_environment or {})
        if enabled:
            env["OPENMW_SDL_INPUT"] = str(self.input)
        process = subprocess.Popen([str(self.driver)], env=env, text=True,
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.processes.append(process)
        self.assertEqual(process.stdout.readline().strip(), "ready")
        return process

    def command(self, process, command="poll"):
        process.stdin.write(command + "\n")
        process.stdin.flush()
        return process.stdout.readline().strip()

    def append(self, text):
        with self.input.open("a") as output:
            output.write(text)

    def event(self, process):
        return list(map(int, self.command(process).split()))

    def test_private_xvfb_delivers_keys_and_receipts(self):
        display = self.private_display()
        process = self.launch(driver="x11", replay_environment=dict(DISPLAY=display,
            OPENMW_SDL_REPLAY_VIDEO_DRIVER="x11", OPENMW_SDL_REPLAY_XVFB_DISPLAY=display))
        self.append("1 down 4\n2 up 4\n")
        self.assertEqual(self.event(process)[1:7], [768, 4, 97, 0, 1, 0])
        self.assertEqual(self.event(process)[1:7], [769, 4, 97, 0, 0, 0])
        self.assertEqual(Path(str(self.input) + ".delivered").read_text(), self.input.read_text())

    def test_xvfb_requires_matching_private_display_and_driver_declaration(self):
        display = self.private_display()
        for index, binding in enumerate([
                dict(OPENMW_SDL_REPLAY_VIDEO_DRIVER="x11"),
                dict(OPENMW_SDL_REPLAY_XVFB_DISPLAY=display),
                dict(OPENMW_SDL_REPLAY_VIDEO_DRIVER="x11", OPENMW_SDL_REPLAY_XVFB_DISPLAY=display + "9"),
                dict(OPENMW_SDL_REPLAY_VIDEO_DRIVER="offscreen", OPENMW_SDL_REPLAY_XVFB_DISPLAY=display)]):
            with self.subTest(binding=binding):
                self.input = self.root / f"{self._testMethodName}-{index}.txt"
                self.input.write_text("")
                process = self.launch(driver="x11", replay_environment=dict(DISPLAY=display, **binding))
                self.append("1 down 4\n")
                self.assertEqual(self.event(process)[1], 256)
                self.assertFalse(Path(str(self.input) + ".delivered").exists())

    def test_keyboard_state_modifiers_and_release(self):
        process = self.launch()
        self.append("1 down 4\n")
        self.assertEqual(self.event(process), [1, 768, 4, 97, 0, 1, 0, 512])
        self.append("2 down 225\n")
        self.assertEqual(self.event(process)[4:7], [1, 1, 1])
        self.append("3 up 4\n4 up 225\n")
        self.assertEqual(self.event(process)[1:7], [769, 4, 97, 1, 0, 1])
        self.assertEqual(self.event(process)[4:7], [0, 0, 0])
        self.assertEqual(Path(str(self.input) + ".delivered").read_text(), self.input.read_text())

    def test_batch_is_consumed_in_one_native_event_drain(self):
        process = self.launch()
        self.append("1 down 24\n2 down 44\n")
        self.assertEqual(self.command(process, 'drain'), 'batch 2 1 1')
        self.append("3 up 24\n4 up 44\n")
        self.assertEqual(self.command(process, 'drain'), 'batch 0 0 0')
        self.assertEqual(Path(str(self.input) + '.delivered').read_text(), self.input.read_text())

    def test_controller_barriers_wait_for_real_device_state_and_preserve_native_events(self):
        process = self.launch()
        receipt = Path(str(self.input) + '.delivered')
        self.append('1 controller-axis 5 32767\n')
        self.assertEqual(self.event(process)[0], 0)  # No controller, no receipt.
        self.assertFalse(receipt.exists())
        self.assertEqual(self.command(process, 'controller'), 'controller 1')
        for _ in range(100):
            if self.event(process)[0] == 0:
                break
        self.assertFalse(receipt.exists())  # Device exists but the requested trigger is not held.
        self.assertEqual(self.command(process, 'axis 5 32767'), 'changed 0')
        native = []
        for _ in range(100):
            native.append(self.event(process)[1])
            if receipt.exists():
                break
        self.assertIn(1616, native)  # Actual SDL_CONTROLLERAXISMOTION precedes acknowledgement.
        self.assertEqual(receipt.read_text(), '1 controller-axis 5 32767\n')
        self.assertEqual(self.command(process, 'padstate'), 'state 32767 0')
        self.append('2 controller-button 0 1\n')
        self.assertEqual(self.event(process)[0], 0)
        self.assertEqual(receipt.read_text(), '1 controller-axis 5 32767\n')
        self.assertEqual(self.command(process, 'padbutton 0 1'), 'changed 0')
        native = []
        for _ in range(100):
            native.append(self.event(process)[1])
            if len(receipt.read_text().splitlines()) == 2:
                break
        self.assertIn(1617, native)  # Native SDL_CONTROLLERBUTTONDOWN, not replay-generated.
        self.assertEqual(self.command(process, 'padstate'), 'state 32767 1')
        self.append('3 controller-button 0 1\n')  # Unchanged state needs no fabricated edge.
        self.assertEqual(self.event(process)[1], 32768)
        self.assertEqual(receipt.read_text(), self.input.read_text())
        self.append('4 controller-button 0 0\n5 controller-axis 5 0\n')
        self.assertEqual(self.event(process)[0], 0)
        self.assertEqual(self.command(process, 'padbutton 0 0'), 'changed 0')
        for _ in range(100):
            self.event(process)
            if len(receipt.read_text().splitlines()) == 4:
                break
        self.assertEqual(self.command(process, 'axis 5 -32768'), 'changed 0')
        for _ in range(100):
            self.event(process)
            if len(receipt.read_text().splitlines()) == 5:
                break
        self.assertEqual(self.command(process, 'padstate'), 'state 0 0')
        self.assertEqual(receipt.read_text(), self.input.read_text())

    def test_mouse_buttons_hold_independent_masks_and_preserve_keys_and_motion(self):
        process = self.launch()
        self.append("1 down 4\n2 button-down 1\n3 button-down 3\n4 motion -3 4\n"
                    "5 button-up 1\n6 button-up 3\n7 up 4\n")
        self.assertEqual(self.event(process)[1:4], [768, 4, 97])
        self.assertEqual(self.command(process, 'button'), 'button 1 1025 1 0 1 1 1 1 1')
        self.assertEqual(self.command(process, 'button'), 'button 1 1025 1 0 3 1 1 5 1')
        self.assertEqual(self.command(process, 'motion'), 'motion 1 1024 1 0 5 -3 4 1')
        self.assertEqual(self.command(process, 'button'), 'button 1 1026 1 0 1 0 1 4 1')
        self.assertEqual(self.command(process, 'button'), 'button 1 1026 1 0 3 0 1 0 1')
        self.assertEqual(self.event(process)[1:6], [769, 4, 97, 0, 0])
        self.assertEqual(Path(str(self.input) + '.delivered').read_text(), self.input.read_text())

    def test_mouse_buttons_preserve_native_queue_partial_lines_and_opt_out(self):
        process = self.launch()
        self.append('1 button-down 1\n2 button-up 1')
        self.assertEqual(self.command(process, 'push'), 'pushed')
        self.assertEqual(self.event(process)[1], 32768)
        self.assertEqual(self.command(process, 'button'), 'button 1 1025 1 0 1 1 1 1 0')
        self.assertEqual(self.command(process, 'button'), 'button 0 0 0 0 0 0 0 1 0')
        self.append('\n')
        self.assertEqual(self.command(process, 'button'), 'button 1 1026 1 0 1 0 1 0 0')
        disabled = self.launch(enabled=False)
        self.assertEqual(self.command(disabled, 'button'), 'button 0 0 0 0 0 0 0 0 0')

    def test_mouse_button_domain_rejects_before_acknowledgement(self):
        for operation in ('button-down', 'button-up'):
            for value in ('0', '6', '-1', '1.5', '999999999999', '', '1 extra'):
                with self.subTest(operation=operation, value=value):
                    self.input.write_text('')
                    process = self.launch()
                    self.append(f'1 {operation} {value}\n')
                    self.assertEqual(self.event(process)[1], 256)
                    self.assertFalse(Path(str(self.input) + '.delivered').exists())

    def test_controller_barrier_domain_rejects_before_acknowledgement(self):
        for query in ('controller-axis -1 0', 'controller-axis 6 0', 'controller-axis 0 -32769',
                      'controller-axis 0 32768', 'controller-button 21 0', 'controller-button 0 2',
                      'controller-axis 0 1 extra', 'controller-button 0 1.5'):
            with self.subTest(query=query):
                self.input.write_text('')
                process = self.launch()
                self.append(f'1 {query}\n')
                self.assertEqual(self.event(process)[1], 256)
                self.assertFalse(Path(str(self.input) + '.delivered').exists())

    def test_partial_line_waits_and_native_events_take_priority(self):
        process = self.launch()
        self.append("1 down 4")
        self.assertEqual(self.event(process)[0], 0)
        self.append("\n")
        self.assertEqual(self.command(process, "push"), "pushed")
        self.assertEqual(self.event(process)[1], 32768)
        self.assertEqual(self.event(process)[1:4], [768, 4, 97])
        self.assertEqual(self.event(process)[0], 0)

    def test_without_opt_in_preserves_native_keyboard_and_queue(self):
        process = self.launch(enabled=False)
        self.append("1 down 4\n")
        self.assertEqual(self.event(process)[0], 0)
        self.assertEqual(self.command(process, "push"), "pushed")
        self.assertEqual(self.event(process)[1], 32768)
        self.assertFalse(Path(str(self.input) + ".delivered").exists())

    def test_quit_is_recorded(self):
        process = self.launch()
        self.append("1 quit\n")
        self.assertEqual(self.event(process)[1], 256)
        self.assertEqual(Path(str(self.input) + ".delivered").read_text(), "1 quit\n")

    def test_text_input_and_window_focus_are_sdl_events(self):
        process = self.launch()
        self.append("1 text 746169\n2 focus\n")
        self.assertEqual(self.command(process, "text"), "text 1 771 tai")
        self.assertEqual(self.command(process, "window"), "window 1 512 1 12")
        self.assertEqual(Path(str(self.input) + ".delivered").read_text(), self.input.read_text())

    def test_relative_motion_preserves_native_queue_and_held_keys(self):
        process = self.launch()
        self.append("1 down 4\n2 motion -31 80\n3 motion 0 -2147483648\n4 up 4\n")
        self.assertEqual(self.event(process)[1:4], [768, 4, 97])
        self.assertEqual(self.command(process, "push"), "pushed")
        self.assertEqual(self.event(process)[1], 32768)
        self.assertEqual(self.command(process, "motion"), "motion 1 1024 1 0 0 -31 80 1")
        self.assertEqual(self.command(process, "motion"), "motion 1 1024 1 0 0 0 -2147483648 1")
        self.assertEqual(self.event(process)[5], 0)
        self.assertEqual(Path(str(self.input) + ".delivered").read_text(), self.input.read_text())

    def test_malformed_relative_motion_is_not_delivered(self):
        for index, arguments in enumerate(["0", "0 1 extra", "2147483648 0", "0 -2147483649",
                                            "1.5 0", "nan 0"]):
            with self.subTest(arguments=arguments):
                self.input = self.root / f"{self._testMethodName}-{index}.txt"
                self.input.write_text("")
                process = self.launch()
                self.append(f"1 motion {arguments}\n")
                self.assertEqual(self.event(process)[1], 256)
                self.assertFalse(Path(str(self.input) + ".delivered").exists())

    def test_text_rejects_controls_and_does_not_deliver_them(self):
        process = self.launch()
        self.append("1 text 00\n")
        self.assertEqual(self.event(process)[1], 256)
        self.assertFalse(Path(str(self.input) + ".delivered").exists())

    def test_invalid_sequence_fails_closed(self):
        process = self.launch()
        self.append("2 down 4\n")
        self.assertEqual(self.event(process)[1], 256)
        self.assertEqual(self.event(process)[0], 0)
        self.assertFalse(Path(str(self.input) + ".delivered").exists())

    def test_out_of_range_scancode_fails_closed(self):
        process = self.launch()
        self.append("1 down 512\n")
        self.assertEqual(self.event(process)[1], 256)
        self.assertFalse(Path(str(self.input) + ".delivered").exists())

    def test_other_video_driver_is_rejected(self):
        process = self.launch(driver="dummy")
        self.append("1 down 4\n")
        self.assertEqual(self.event(process)[1], 256)
        self.assertFalse(Path(str(self.input) + ".delivered").exists())


if __name__ == "__main__":
    unittest.main()
