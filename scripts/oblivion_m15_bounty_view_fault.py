#!/usr/bin/env python3
"""Prepare the private BOUN conflict used by the M15 S3 admission scenario.

Run `prepare COURSE` beside the scenario runner, then `wait-resave COURSE`.
The course must contain its own Quicksave copy. Original input saves are never
used as a writable slot. Native payload validity and equality are checked here;
the engine's whole-save admission is expected to reject the shared-view fault.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.tes4_runtime_state import _save_records, load_save


def events(course: Path) -> list[dict]:
    path = course / "m15-events.jsonl"
    return [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []


def wait_for_saves(course: Path, count: int) -> list[dict]:
    deadline = time.monotonic() + 650
    while time.monotonic() < deadline:
        observations = events(course)
        if sum(event["event"] == "save-complete" for event in observations) >= count:
            return observations
        time.sleep(0.2)
    raise RuntimeError(f"Actual F5 did not complete {count} saves")


def prepare(course: Path) -> None:
    slots = list(course.glob("userdata/saves/*/Quicksave.omwsave"))
    if len(slots) != 1 or not slots[0].resolve().is_relative_to(course):
        raise RuntimeError("Expected one private course Quicksave slot")
    slot = slots[0]
    wait_for_saves(course, 1)
    before = course.parent / "before-rejection.omwsave"
    rejected = course.parent / "rejected-input.omwsave"
    if before.exists() or rejected.exists():
        raise RuntimeError("Refusing to overwrite previous rejection evidence")
    raw = slot.read_bytes()
    fields = [(start, end) for _, _, tag, subs in _save_records(raw) if tag == b"PLAY"
              for name, start, end in subs if name == b"BOUN"]
    if len(fields) != 1 or fields[0][1] - fields[0][0] != 12:
        raise RuntimeError("Expected one int32 Player BOUN field")
    offset = fields[0][0] + 8
    if struct.unpack_from("<i", raw, offset)[0] != -4:
        raise RuntimeError("The fixture must first publish shared bounty -4")
    state = load_save(slot)
    if state["schema_version"] != 45:
        raise RuntimeError("The fixture must first resave at schema45")
    changed = bytearray(raw)
    struct.pack_into("<i", changed, offset, -3)
    if [i for i, (a, b) in enumerate(zip(raw, changed)) if a != b] != [offset]:
        raise RuntimeError("The shared-view fault must change exactly one byte")
    before.write_bytes(raw)
    rejected.write_bytes(changed)
    if load_save(rejected) != state:
        raise RuntimeError("The fault changed native payload state")
    slot.write_bytes(changed)
    fault = {"record": "PLAY", "subrecord": "BOUN", "before": -4, "after": -3,
             "byte_offset": offset, "native_state_identical": True}
    (course.parent / "fault-source.json").write_text(json.dumps(fault, indent=2) + "\n")
    (course / "mutation-receipt.txt").write_text("prepared shared bounty conflict\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["prepare", "wait-resave"])
    parser.add_argument("course", type=Path)
    args = parser.parse_args()
    course = args.course.resolve()
    if args.mode == "prepare":
        prepare(course)
    else:
        observations = wait_for_saves(course, 2)
        if sum(event["event"] == "load-complete" for event in observations) != 1:
            raise RuntimeError("Rejected quickload unexpectedly replaced the running world")
        (course / "resave-receipt.txt").write_text("resaved preserved running game\n")


if __name__ == "__main__":
    main()
