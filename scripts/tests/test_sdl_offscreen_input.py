"""Exercise input replay against real SDL, without an engine or display socket."""

import os
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
int main() {
    if (SDL_Init(SDL_INIT_VIDEO)) return 2;
    SDL_Event e{};
    while (SDL_PollEvent(&e)) {}
    std::cout << "ready" << std::endl;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "push") {
            e = {}; e.type = SDL_USEREVENT; SDL_PushEvent(&e);
            std::cout << "pushed" << std::endl;
        } else {
            e = {};
            int result = SDL_PollEvent(&e), count = 0;
            const auto* keys = SDL_GetKeyboardState(&count);
            std::cout << result << " " << e.type << " " << e.key.keysym.scancode
                      << " " << e.key.keysym.sym << " " << e.key.keysym.mod
                      << " " << static_cast<int>(keys[SDL_SCANCODE_A])
                      << " " << static_cast<int>(keys[SDL_SCANCODE_LSHIFT])
                      << " " << count << std::endl;
        }
    }
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

    def tearDown(self):
        for process in self.processes:
            process.stdin.close()
            process.wait(timeout=10)
            process.stdout.close()
            process.stderr.close()
            self.assertEqual(process.returncode, 0)

    def launch(self, *, enabled=True, driver="offscreen"):
        env = dict(os.environ, SDL_VIDEODRIVER=driver, LD_PRELOAD=str(self.library))
        env.pop("OPENMW_SDL_INPUT", None)
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
