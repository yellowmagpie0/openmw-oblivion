"""Reproduce the script-free S1 observation fixture from a pinned local master.

Only the declarative recipe belongs in git. The small native master contains
local licensed boot/appearance data and authored floor placements, never a
script that mutates or supplies an expected gameplay outcome.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

import tes4_m14_audit as reader


def sub(name: str, data: bytes) -> bytes:
    if len(name) != 4 or len(data) > 65535:
        raise ValueError("invalid fixture subrecord")
    return name.encode("ascii") + struct.pack("<H", len(data)) + data


def string(name: str, value: str) -> bytes:
    return sub(name, value.encode("cp1252") + b"\0")


def record(name: str, ident: int, data: bytes, flags: int = 0) -> bytes:
    return struct.pack("<4sIIII", name.encode("ascii"), len(data), flags, ident, 0) + data


def group(label: bytes, kind: int, data: bytes) -> bytes:
    return struct.pack("<4sI4sII", b"GRUP", 20 + len(data), label, kind, 0) + data


def records(data: bytes, start: int = 0, end: int | None = None):
    end = len(data) if end is None else end
    while start < end:
        if end - start < 20:
            raise ValueError("truncated fixture source record header")
        tag, size, flags, ident, _ = struct.unpack_from("<4sIIII", data, start)
        tag = tag.decode("ascii")
        finish = start + size if tag == "GRUP" else start + 20 + size
        if finish > end or finish < start + 20:
            raise ValueError("fixture source record exceeds containing group")
        if tag == "GRUP":
            yield from records(data, start + 20, finish)
        else:
            yield tag, ident, reader._payload(data, start + 20, size, flags, "fixture source", tag)
        start = finish


def build(source: bytes, recipe: dict) -> tuple[bytes, dict]:
    if recipe["version"] != 1 or hashlib.sha256(source).hexdigest() != recipe["source_sha256"]:
        raise ValueError("fixture source revision differs from reviewed recipe")
    copy_types = set(recipe["copy_types"])
    if not copy_types <= {"GMST", "HAIR", "EYES"}:
        raise ValueError("fixture cannot copy scripts, quests or arbitrary gameplay records")
    selected = {(r["type"], r["id"]): r for r in recipe["records"]}
    if len(selected) != len(recipe["records"]) or any(k[0] not in {"NPC_", "RACE", "CLAS"} for k in selected):
        raise ValueError("invalid or duplicate boot record selection")
    found = set()
    groups: dict[str, list[bytes]] = {}
    identities = set()
    def add(tag, ident, payload):
        if ident in identities:
            raise ValueError("duplicate fixture FormID")
        identities.add(ident)
        groups.setdefault(tag, []).append(record(tag, ident, payload))
    for tag, ident, payload in records(source):
        key = tag, ident
        if tag not in copy_types and key not in selected:
            continue
        subs = reader._subrecords(payload, "fixture source", tag)
        if key in selected:
            found.add(key)
            specification = selected[key]
            if "fields" in specification:
                subs = [s for s in subs if s["name"] in specification["fields"]]
            else:
                subs = [s for s in subs if s["name"] not in specification["omit"]]
        if any(s["name"] in {"SCRI", "SCTX", "SCDA", "PKID", "SPLO"} for s in subs):
            raise ValueError("observation fixture cannot contain scripts, packages or spells")
        add(tag, ident, b"".join(sub(s["name"], s["payload"]) for s in subs))
    if found != selected.keys():
        raise ValueError("required fixture boot records missing from source")
    cell, floor = recipe["cell"], recipe["floor"]
    add("STAT", floor["id"], string("EDID", "M15Floor") + string("MODL", floor["model"]))
    ambient = bytes(cell["ambient"] + [0])
    lighting = ambient + bytes(8) + struct.pack("<ffii ff", 0., 10000., 0, 0, 0., 1.)
    cell_record = record("CELL", cell["id"], string("EDID", cell["editor_id"])
                         + string("FULL", cell["name"]) + sub("DATA", b"\x01") + sub("XCLL", lighting))
    if not 1 <= floor["radius"] <= 8 or not 1 <= floor["spacing"] <= 4096:
        raise ValueError("fixture floor dimensions are outside bounded course")
    references = []
    for x in range(-floor["radius"], floor["radius"] + 1):
        for y in range(-floor["radius"], floor["radius"] + 1):
            ident = floor["reference_start"] + len(references)
            if ident in identities or ident == cell["id"]:
                raise ValueError("fixture placement identity collides")
            identities.add(ident)
            references.append(record("REFR", ident, sub("NAME", struct.pack("<I", floor["id"]))
                                     + sub("DATA", struct.pack("<6f", x * floor["spacing"],
                                                               y * floor["spacing"], 0, 0, 0, 0))))
    children = group(struct.pack("<I", cell["id"]), 6,
                     group(struct.pack("<I", cell["id"]), 9, b"".join(references)))
    cells = group(struct.pack("<I", 0), 2, group(struct.pack("<I", 0), 3, cell_record + children))
    count = sum(len(rows) for rows in groups.values()) + len(references) + 1
    header = record("TES4", 0, sub("HEDR", struct.pack("<fII", 1.0, count, max(identities) + 1))
                    + string("CNAM", "OpenMW synthetic M15 observation fixture"), 1)
    output = header + b"".join(group(tag.encode("ascii"), 0, b"".join(rows))
                              for tag, rows in sorted(groups.items())) + group(b"CELL", 0, cells)
    # Reopen every emitted record with the same strict TES4 structural reader.
    actual = list(records(output))
    if len(actual) != count + 1:
        raise ValueError("fixture record count mismatch")
    return output, {"kind": "synthetic", "record_count": count, "placed_floors": len(references),
                    "source_sha256": recipe["source_sha256"], "sha256": hashlib.sha256(output).hexdigest(),
                    "script_count": 0, "quest_count": 0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--recipe", type=Path, default=Path(__file__).parent / "data/oblivion_compat/m15_observation_fixture.json")
    args = parser.parse_args()
    result, metadata = build(args.source.read_bytes(), json.loads(args.recipe.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("xb") as stream:
        stream.write(result)
    args.output.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
