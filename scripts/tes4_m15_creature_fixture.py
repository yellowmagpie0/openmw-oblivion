"""Place one unchanged master Creature in an unchanged master interior CELL.

The recipe is source; plugins are reproducible observation artifacts. No base,
script, combat result, resource value, or animation state is authored here.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from tes4_m14_audit import _subrecords
from tes4_m15_fixture import group, record, records, string, sub


def build(source: bytes, recipe: dict) -> tuple[bytes, dict]:
    fields = {"version", "purpose", "source_sha256", "master", "cell", "creature", "reference", "position"}
    if (set(recipe) != fields or type(recipe["version"]) is not int or recipe["version"] != 1
            or recipe["master"] != "Oblivion.esm" or not isinstance(recipe["purpose"], str)
            or not recipe["purpose"].strip() or hashlib.sha256(source).hexdigest() != recipe["source_sha256"]):
        raise ValueError("invalid or unpinned Creature placement recipe")
    if (any(type(recipe[k]) is not int or not 0 < recipe[k] <= 0xffffff for k in ("cell", "creature"))
            or type(recipe["reference"]) is not int or not 0x01000800 <= recipe["reference"] <= 0x01ffffff):
        raise ValueError("invalid master or plugin FormID")
    pos = recipe["position"]
    if (not isinstance(pos, list) or len(pos) != 6
            or any(type(x) not in (int, float) or not math.isfinite(x) for x in pos)
            or any(abs(x) > 65536 for x in pos[:3]) or any(abs(x) > 2 * math.pi for x in pos[3:])):
        raise ValueError("invalid Creature placement transform")
    selected = {}
    for tag, ident, payload in records(source):
        if (tag, ident) not in {("CELL", recipe["cell"]), ("CREA", recipe["creature"])}:
            continue
        if tag in selected:
            raise ValueError("ambiguous source record")
        selected[tag] = payload
    if set(selected) != {"CELL", "CREA"}:
        raise ValueError("missing Creature or cell")
    cell_fields = _subrecords(selected["CELL"], "placement cell", "CELL")
    data = [s["payload"] for s in cell_fields if s["name"] == "DATA"]
    if len(data) != 1 or not data[0] or not data[0][0] & 1:
        raise ValueError("placement requires an interior cell")
    actor = record("ACRE", recipe["reference"], string("EDID", "M15CreatureObservation")
                   + sub("NAME", struct.pack("<I", recipe["creature"]))
                   + sub("DATA", struct.pack("<6f", *pos)), 0x400)
    cell = recipe["cell"]; label = struct.pack("<I", cell)
    children = group(label, 6, group(label, 8, actor))
    cells = group(struct.pack("<I", cell % 10), 2, group(struct.pack("<I", cell // 10 % 10), 3,
                  record("CELL", cell, selected["CELL"]) + children))
    header = sub("HEDR", struct.pack("<fII", 1, 2, (recipe["reference"] & 0xffffff) + 1))
    header += string("CNAM", "OpenMW M15 Creature observation")
    header += string("MAST", recipe["master"]) + sub("DATA", struct.pack("<Q", len(source)))
    output = record("TES4", 0, header) + group(b"CELL", 0, cells)
    reopened = list(records(output))
    if ([tag for tag, _, _ in reopened] != ["TES4", "CELL", "ACRE"]
            or reopened[1][2] != selected["CELL"]):
        raise ValueError("Creature placement readback mismatch")
    return output, {"source_sha256": recipe["source_sha256"], "sha256": hashlib.sha256(output).hexdigest(),
                    "record_count": 2, "script_count": 0, "base_changes": False, "runtime_accepted": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "recipe", "output"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    output, report = build(args.source.read_bytes(), json.loads(args.recipe.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("xb") as stream:
        stream.write(output)
    args.output.with_suffix(args.output.suffix + ".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
