"""Private file-backed SDL controls for diagnostic runtime scenarios."""

from pathlib import Path
import hashlib
import json
import math
import re
import shlex
import shutil
import subprocess
import time


INPUT_ACTIONS = frozenset({"key", "key_down", "keys_down", "keys_transition", "key_up", "key_hold", "key_held",
                          "type", "type_held", "focus_window", "screenshot", "mouse_move",
                          "mouse_click", "mouse_down", "mouse_up"})
PASSIVE_ACTIONS = frozenset({"sleep", "wait_log", "assert_file"})
GAMEPAD_ACTIONS = frozenset({"gamepad_button", "gamepad_axis"})
CONTROLLER_BUTTONS = {name: i for i, name in enumerate((
    "a", "b", "x", "y", "back", "guide", "start", "leftstick", "rightstick",
    "leftshoulder", "rightshoulder", "dpad_up", "dpad_down", "dpad_left", "dpad_right"))}
CONTROLLER_AXES = {name: i for i, name in enumerate((
    "left_x", "left_y", "right_x", "right_y", "left_trigger", "right_trigger"))}


def validation_actions(actions, _depth=0):
    """Include bounded-loop inputs in the same preflight as ordinary inputs."""
    if _depth > 8:
        raise ValueError('repeat_until_log nesting exceeds eight levels')
    if not isinstance(actions, list):
        raise ValueError('Scenario actions must be a list')
    for action in actions:
        yield action
        if isinstance(action, dict) and action.get('type') == 'repeat_until_log':
            for field in ('actions', 'cleanup_actions'):
                nested = action.get(field, [])
                if not isinstance(nested, list) or any(not isinstance(a, dict) for a in nested):
                    raise ValueError(f'repeat_until_log {field} must be an action list')
                yield from validation_actions(nested, _depth + 1)
        if isinstance(action, dict) and action.get('type') == 'branch_on_log':
            cases = action.get('cases')
            if not isinstance(cases, dict) or not cases:
                raise ValueError('branch_on_log requires nonempty cases')
            for name, nested in cases.items():
                if not isinstance(name, str) or not name or not isinstance(nested, list):
                    raise ValueError('branch_on_log cases must map names to action lists')
                yield from validation_actions(nested, _depth + 1)


def scancode(value):
    if not isinstance(value, str) or not value:
        raise ValueError("Offscreen keys must be nonempty strings")
    aliases = {"Return": 40, "Escape": 41, "BackSpace": 42, "Tab": 43, "space": 44,
               "grave": 53, "KP_2": 90, "Shift_L": 225, "Control_L": 224, "Alt_L": 226}
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


def receipt_timeout(value):
    if type(value) not in (int, float) or not math.isfinite(value) or value <= 0:
        raise ValueError("SDL input receipt timeout must be a finite positive number")
    return float(value)


def chord_scancodes(values):
    if not isinstance(values, list) or not 1 <= len(values) <= 16:
        raise ValueError("Batched keys require one through sixteen key names")
    codes = [scancode(value) for value in values]
    if len(set(codes)) != len(codes):
        raise ValueError("Batched keys must have distinct scancodes")
    return codes


def transition_scancodes(action):
    release = chord_scancodes(action.get("release"))
    press = chord_scancodes(action.get("press"))
    if len(release) + len(press) > 16 or set(release) & set(press):
        raise ValueError("Key transitions require at most sixteen distinct scancodes")
    return release, press


def mouse_delta(value):
    if type(value) is not int or not -(2 ** 31) <= value < 2 ** 31:
        raise ValueError("Relative mouse motion requires signed 32-bit integers")
    return value


def observed_motion_pattern(source):
    if not isinstance(source, dict) or set(source) != {'path', 'pattern'}:
        raise ValueError('Observed motion needs a log path and pattern')
    if (not isinstance(source['path'], str) or not source['path']
            or Path(source['path']).is_absolute() or '..' in Path(source['path']).parts):
        raise ValueError('Observed motion needs a relative scenario log path')
    pattern = re.compile(source['pattern'], re.MULTILINE)
    if set(pattern.groupindex) != {'x', 'y'}:
        raise ValueError('Observed motion needs named x/y integer groups')
    return pattern


def observed_mouse_motion(action, output):
    """Resolve measured deltas into ordinary input; never write gameplay state.

    Callers wait for a fresh observer sample before this action. Keep the
    selected sample and byte offset in the receipt for later reinspection.
    """
    source = action['from_log']
    if action['type'] != 'mouse_move':
        raise ValueError('Observed motion needs a mouse action and a log pattern')
    pattern = observed_motion_pattern(source)
    path = (output / source['path']).resolve()
    if not path.is_relative_to(output.resolve()):
        raise ValueError('Observed motion log must be inside the scenario output')
    with path.open('rb') as stream:
        stream.seek(max(0, path.stat().st_size - 65536))
        offset = stream.tell()
        text = stream.read().decode('utf-8', errors='surrogateescape')
    complete_end = text.rfind('\n')
    matches = [match for match in pattern.finditer(text) if match.end() <= complete_end]
    if not matches:
        raise ValueError('No recent complete mouse observation')
    match = matches[-1]
    values = {}
    for axis in ('x', 'y'):
        raw = match[axis]
        if not re.fullmatch(r'-?\d+', raw) or abs(int(raw)) > 8192:
            raise ValueError('Observed mouse delta exceeds the bounded input step')
        values[axis] = int(raw)
    resolved = dict(action, **values)
    return resolved, dict(path=str(path), sample=match[0],
        byte_offset=offset + len(text[:match.start()].encode('utf-8', errors='surrogateescape')), **values)


def mouse_button(value):
    if type(value) is not int or not 1 <= value <= 5:
        raise ValueError("Mouse button must be an SDL button from 1 through 5")
    return value


def mouse_hold(value):
    if type(value) not in (int, float) or not math.isfinite(value) or value < 0:
        raise ValueError("Mouse hold duration must be finite and non-negative")
    return float(value)


def validate(manifest):
    actions = list(validation_actions(manifest.get('actions', [])))
    for flag in ('sdl_offscreen_input', 'sdl_xvfb_input'):
        if type(manifest.get(flag, False)) is not bool:
            raise ValueError(f"{flag} must be a boolean")
    offscreen, xvfb = (manifest.get(flag, False) for flag in ('sdl_offscreen_input', 'sdl_xvfb_input'))
    if (offscreen and xvfb) or (xvfb and manifest.get('xvfb') is not True):
        raise ValueError('SDL Xvfb input requires its own Xvfb server and no offscreen input')
    if not offscreen and not xvfb:
        if any(a.get("type") in ("keys_down", "keys_transition")
               for a in actions if isinstance(a, dict)):
            raise ValueError("Batched keys require SDL offscreen input")
        return
    if (offscreen and manifest.get("xvfb")) or "m14" in manifest or "m15" in manifest:
        raise ValueError("SDL offscreen controls currently support isolated diagnostic scenarios only")
    receipt_timeout(manifest.get("sdl_input_timeout_seconds", 10))
    for action in actions:
        if not isinstance(action, dict):
            raise ValueError('Scenario actions must be objects')
        if action.get('type') in ('repeat_until_log', 'branch_on_log'):
            continue  # The scenario validator checks loop bounds and read-only scope.
        if action.get("type") in GAMEPAD_ACTIONS:
            if not manifest.get("virtual_gamepad", False):
                raise ValueError("Offscreen gamepad actions require virtual_gamepad=true (Linux uinput)")
            # Delivered by the existing uinput backend, not the SDL replay hook.
            continue
        if action.get("type") not in INPUT_ACTIONS | PASSIVE_ACTIONS:
            raise ValueError(f"Unsupported offscreen action: {action.get('type')!r}")
        if action["type"] == "keys_down":
            chord_scancodes(action.get("values"))
        elif action["type"] == "keys_transition":
            transition_scancodes(action)
        elif action["type"].startswith("key"):
            scancode(action["value"])
        elif action["type"] == "mouse_move":
            mouse_delta(action.get("x"))
            mouse_delta(action.get("y"))
            if 'from_log' in action:
                observed_motion_pattern(action['from_log'])
        elif action["type"] in ("mouse_click", "mouse_down", "mouse_up"):
            mouse_button(action.get("button", 1))
            if action["type"] == "mouse_click":
                mouse_hold(action.get("hold_seconds", .08))


class Replay:
    def __init__(self, output, environment, receipt_timeout_seconds=10, *, video_driver="offscreen"):
        if video_driver not in ('offscreen', 'x11'):
            raise ValueError('SDL replay requires offscreen or private Xvfb')
        self.video_driver = video_driver
        self.receipt_timeout_seconds = receipt_timeout(receipt_timeout_seconds)
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
        environment.update(SDL_VIDEODRIVER=video_driver, LIBGL_ALWAYS_SOFTWARE="1",
                           OPENMW_SDL_REPLAY_VIDEO_DRIVER=video_driver,
                           MESA_SHADER_CACHE_DISABLE="true", OPENMW_SDL_INPUT=str(self.path))
        environment.pop('OPENMW_SDL_REPLAY_XVFB_DISPLAY', None)
        if video_driver == 'offscreen':
            environment['EGL_PLATFORM'] = 'surfaceless'
        else:
            environment.pop('EGL_PLATFORM', None)
        previous = environment.get("LD_PRELOAD", "")
        environment["LD_PRELOAD"] = (previous + ":" if previous else "") + str(library)
        self.provenance = {"receipt_timeout_seconds": self.receipt_timeout_seconds,
                           "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                           "library_sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
                           "compile_command": command,
                           "video_driver": video_driver,
                           "environment": {key: environment[key] for key in
                                           ("SDL_VIDEODRIVER", "LIBGL_ALWAYS_SOFTWARE", "EGL_PLATFORM",
                                            "MESA_SHADER_CACHE_DISABLE", "OPENMW_SDL_INPUT", "LD_PRELOAD",
                                            "OPENMW_SDL_REPLAY_VIDEO_DRIVER") if key in environment},
                           "scope": "SDL input delivery; gameplay results require separate observations"}
        (self.output / "sdl-input-provenance.json").write_text(json.dumps(self.provenance, indent=2) + "\n")

    def bind_xvfb_display(self, environment, display):
        """Bind only after the scenario's Xvfb child reports its ready display."""
        if self.video_driver != 'x11' or not re.fullmatch(r':[0-9]+', display):
            raise ValueError('SDL Xvfb input needs the private server display')
        environment['DISPLAY'] = environment['OPENMW_SDL_REPLAY_XVFB_DISPLAY'] = display
        self.provenance['environment'].update(DISPLAY=display, OPENMW_SDL_REPLAY_XVFB_DISPLAY=display)
        (self.output / 'sdl-input-provenance.json').write_text(json.dumps(self.provenance, indent=2) + '\n')

    def send(self, operation, argument=None, timeout=None):
        return self.send_many([(operation, argument)], timeout)

    def send_many(self, operations, timeout=None):
        if timeout is None:
            timeout = self.receipt_timeout_seconds
        lines = []
        for operation, argument in operations:
            self.sequence += 1
            lines.append(f"{self.sequence} {operation}" + (f" {argument}" if argument is not None else "") + "\n")
        data = "".join(lines).encode("ascii")
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

    def controller_button(self, name, pressed):
        self.send("controller-button", f"{CONTROLLER_BUTTONS[name]} {int(pressed)}")

    def controller_axis(self, name, raw):
        if name in ("dpad_x", "dpad_y"):
            negative, positive = ("dpad_left", "dpad_right") if name == "dpad_x" else ("dpad_up", "dpad_down")
            self.controller_button(negative, raw < 0)
            self.controller_button(positive, raw > 0)
        else:
            self.send("controller-axis", f"{CONTROLLER_AXES[name]} {raw}")

    def action(self, action):
        kind = action["type"]
        first = self.sequence + 1
        if kind == "focus_window":
            self.send("focus")
        elif kind == "mouse_move":
            x, y = mouse_delta(action.get("x")), mouse_delta(action.get("y"))
            self.send("motion", f"{x} {y}")
        elif kind in ("mouse_click", "mouse_down", "mouse_up"):
            button = mouse_button(action.get("button", 1))
            if kind == "mouse_click":
                seconds = mouse_hold(action.get("hold_seconds", .08))
                self.send("button-down", button)
                time.sleep(seconds)
                self.send("button-up", button)
            else:
                self.send("button-down" if kind == "mouse_down" else "button-up", button)
        elif kind in ("type", "type_held"):
            text = action["value"]
            if not isinstance(text, str) or any(not 32 <= ord(c) <= 126 for c in text):
                raise ValueError("Offscreen text requires printable ASCII")
            for offset in range(0, len(text), 31):
                self.send("text", text[offset:offset + 31].encode("ascii").hex())
        elif kind == "keys_down":
            self.send_many([("down", code) for code in chord_scancodes(action.get("values"))])
        elif kind == "keys_transition":
            release, press = transition_scancodes(action)
            # Queue releases and presses together, without a receipt/frame wait
            # between them (e.g. release attack before pressing quicksave).
            self.send_many([("up", code) for code in release] + [("down", code) for code in press])
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
        return {"type": kind, "passed": True,
                "backend": "sdl-xvfb" if getattr(self, 'video_driver', 'offscreen') == 'x11' else "sdl-offscreen",
                "first_input_sequence": first, "last_input_sequence": self.sequence}
