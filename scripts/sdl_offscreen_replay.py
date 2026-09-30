"""Private file-backed SDL controls for diagnostic runtime scenarios."""

from pathlib import Path
import hashlib
import json
import math
import shlex
import shutil
import subprocess
import time


INPUT_ACTIONS = frozenset({"key", "key_down", "key_up", "key_hold", "key_held",
                          "type", "type_held", "focus_window", "screenshot"})
PASSIVE_ACTIONS = frozenset({"sleep", "wait_log", "assert_file"})


def scancode(value):
    if not isinstance(value, str) or not value:
        raise ValueError("Offscreen keys must be nonempty strings")
    aliases = {"Return": 40, "Escape": 41, "BackSpace": 42, "Tab": 43, "space": 44,
               "grave": 53, "Shift_L": 225, "Control_L": 224, "Alt_L": 226}
    if value in aliases:
        return aliases[value]
    if len(value) == 1 and "a" <= value.lower() <= "z":
        return 4 + ord(value.lower()) - ord("a")
    if value in "123456789" and len(value) == 1:
        return 30 + int(value) - 1
    if value == "0":
        return 39
    if value.startswith("F") and value[1:].isdigit() and 1 <= int(value[1:]) <= 12:
        return 57 + int(value[1:])
    raise ValueError(f"Unsupported offscreen key: {value!r}")


def validate(manifest):
    if type(manifest.get("sdl_offscreen_input", False)) is not bool:
        raise ValueError("sdl_offscreen_input must be a boolean")
    if not manifest.get("sdl_offscreen_input", False):
        return
    if manifest.get("xvfb") or manifest.get("virtual_gamepad") or "m14" in manifest or "m15" in manifest:
        raise ValueError("SDL offscreen controls currently support isolated diagnostic scenarios only")
    for action in manifest.get("actions", []):
        if action.get("type") not in INPUT_ACTIONS | PASSIVE_ACTIONS:
            raise ValueError(f"Unsupported offscreen action: {action.get('type')!r}")
        if action["type"].startswith("key"):
            scancode(action["value"])


class Replay:
    def __init__(self, output, environment):
        self.output = Path(output).resolve()
        self.path = self.output / "sdl-input.txt"
        with self.path.open("xb"):
            pass
        self.sequence = 0
        self.expected = b""
        source = Path(__file__).with_name("sdl_offscreen_input.cpp")
        library = self.output / "sdl-input.so"
        flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "sdl2"], text=True))
        command = ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
                   str(source), *flags, "-ldl", "-o", str(library)]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        (self.output / "sdl-input-build.log").write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError("SDL replay compilation failed; inspect sdl-input-build.log")
        environment.update(SDL_VIDEODRIVER="offscreen", LIBGL_ALWAYS_SOFTWARE="1", EGL_PLATFORM="surfaceless",
                           MESA_SHADER_CACHE_DISABLE="true", OPENMW_SDL_INPUT=str(self.path))
        previous = environment.get("LD_PRELOAD", "")
        environment["LD_PRELOAD"] = (previous + ":" if previous else "") + str(library)
        self.provenance = {"source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                           "library_sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
                           "compile_command": command,
                           "environment": {key: environment[key] for key in
                                           ("SDL_VIDEODRIVER", "LIBGL_ALWAYS_SOFTWARE", "EGL_PLATFORM",
                                            "MESA_SHADER_CACHE_DISABLE", "OPENMW_SDL_INPUT", "LD_PRELOAD")},
                           "scope": "SDL input delivery; gameplay results require separate observations"}
        (self.output / "sdl-input-provenance.json").write_text(json.dumps(self.provenance, indent=2) + "\n")

    def send(self, operation, argument=None, timeout=10):
        self.sequence += 1
        line = f"{self.sequence} {operation}" + (f" {argument}" if argument is not None else "") + "\n"
        data = line.encode("ascii")
        with self.path.open("ab") as output:
            output.write(data)
        self.expected += data
        deadline = time.monotonic() + timeout
        receipt = Path(str(self.path) + ".delivered")
        while time.monotonic() < deadline:
            delivered = receipt.read_bytes() if receipt.exists() else b""
            if delivered == self.expected:
                return self.sequence
            if not self.expected.startswith(delivered):
                raise RuntimeError("SDL input receipt differs from requested input")
            time.sleep(.01)
        raise TimeoutError("SDL input was not delivered before its deadline")

    def key(self, code, seconds=.08):
        if not math.isfinite(seconds) or seconds < 0:
            raise ValueError("Input hold duration must be finite and non-negative")
        self.send("down", code)
        time.sleep(seconds)
        self.send("up", code)

    def action(self, action):
        kind = action["type"]
        first = self.sequence + 1
        if kind == "focus_window":
            self.send("focus")
        elif kind in ("type", "type_held"):
            text = action["value"]
            if not isinstance(text, str) or any(not 32 <= ord(c) <= 126 for c in text):
                raise ValueError("Offscreen text requires printable ASCII")
            for offset in range(0, len(text), 31):
                self.send("text", text[offset:offset + 31].encode("ascii").hex())
        elif kind in ("key_down", "key_up"):
            self.send("down" if kind == "key_down" else "up", scancode(action["value"]))
        elif kind.startswith("key"):
            seconds = float(action.get("seconds", 0) if kind == "key_hold" else action.get("hold_seconds", .08))
            pause = float(action.get("pause_seconds", .04))
            if not math.isfinite(pause) or pause < 0:
                raise ValueError("Input pause must be finite and non-negative")
            if kind != "key_hold" or seconds != 0:
                self.key(scancode(action["value"]), seconds)
                time.sleep(pause)
        elif kind == "screenshot":
            images = self.output / "userdata/screenshots"
            before = set(images.glob("*.png"))
            self.key(69)
            deadline = time.monotonic() + float(action.get("timeout_seconds", 30))
            while time.monotonic() < deadline:
                candidates = set(images.glob("*.png")) - before
                if len(candidates) == 1:
                    source = candidates.pop()
                    data = source.read_bytes()
                    if data.endswith(b"\x00\x00\x00\x00IEND\xaeB\x60\x82"):
                        destination = self.output / action["name"]
                        if not destination.resolve().is_relative_to(self.output):
                            raise ValueError("Screenshot must stay below output")
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copyfile(source, destination)
                        break
                elif len(candidates) > 1:
                    raise RuntimeError("Screenshot action produced ambiguous files")
                time.sleep(.05)
            else:
                raise TimeoutError("Native screenshot was not completed")
        else:
            raise ValueError(f"Unsupported SDL input action: {kind}")
        return {"type": kind, "passed": True, "backend": "sdl-offscreen",
                "first_input_sequence": first, "last_input_sequence": self.sequence}
