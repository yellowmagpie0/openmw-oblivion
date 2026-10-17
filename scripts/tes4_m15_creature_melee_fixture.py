"""Place a stock Creature clone with only an explicit stationary CSTY policy.

Stats, natural damage, reach, meshes, sounds and inventory remain source data.
Packages, scripts and spells are removed from the clone so production combat
intent owns this bounded physical observation. The plugin contains no outcomes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from tes4_m14_audit import _subrecords
from tes4_m15_audit import combat_style
from tes4_m15_creature_fixture import build as placement
from tes4_m15_fixture import group, record, records, string, sub


def build(source: bytes, recipe: dict) -> tuple[bytes, dict]:
    fields = {"version", "purpose", "source_sha256", "master", "cell", "template", "creature", "reference", "style", "position"}
    if not isinstance(recipe, dict) or set(recipe) != fields:
        raise ValueError("invalid stationary Creature recipe fields")
    own = [recipe[k] for k in ("creature", "reference", "style")]
    if (any(type(x) is not int or not 0x01000800 <= x <= 0x01ffffff for x in own)
            or len(set(own)) != 3):
        raise ValueError("invalid or colliding Creature fixture identities")
    base_recipe = {k: v for k, v in recipe.items() if k not in {"template", "style"}}
    base_recipe["creature"] = recipe["template"]
    placed, _ = placement(source, base_recipe)
    templates = [p for t, i, p in records(source) if t == "CREA" and i == recipe["template"]]
    if len(templates) != 1:
        raise ValueError("missing or duplicate Creature template")
    original = _subrecords(templates[0], "stationary Creature", "CREA")
    changed = {"EDID", "FULL", "SCRI", "PKID", "SPLO", "ZNAM"}
    retained = [(s["name"], s["payload"]) for s in original if s["name"] not in changed]
    creature = string("EDID", "M15StationaryCreature") + string("FULL", "M15 stationary Dog")
    creature += b"".join(sub(k, v) for k, v in retained)
    creature += sub("ZNAM", struct.pack("<I", recipe["style"]))
    style = bytearray(124)
    style[37], style[80], style[112] = 100, 34, 1
    struct.pack_into("<2f", style, 84, 1, 1)
    struct.pack_into("<3f", style, 92, 250, 1000, 325)
    struct.pack_into("<2f", style, 104, 500, 325)
    struct.pack_into("<fI", style, 116, 1, 1)
    rows = list(records(placed)); cell = recipe["cell"]; label = struct.pack("<I", cell)
    cell_payload = next(p for t, _, p in rows if t == "CELL")
    actor_payload = next(p for t, _, p in rows if t == "ACRE")
    actor_fields = _subrecords(actor_payload, "placed Creature", "ACRE")
    actor_payload = b"".join(sub(s["name"], struct.pack("<I", recipe["creature"]) if s["name"] == "NAME"
                                 else s["payload"]) for s in actor_fields)
    cells = group(struct.pack("<I", cell % 10), 2, group(struct.pack("<I", cell // 10 % 10), 3,
                  record("CELL", cell, cell_payload)
                  + group(label, 6, group(label, 8, record("ACRE", recipe["reference"], actor_payload, 0x400)))))
    header = sub("HEDR", struct.pack("<fII", 1, 4, (max(own) & 0xffffff) + 1))
    header += string("CNAM", "OpenMW M15 native stationary Creature")
    header += string("MAST", recipe["master"]) + sub("DATA", struct.pack("<Q", len(source)))
    output = record("TES4", 0, header)
    output += group(b"CREA", 0, record("CREA", recipe["creature"], creature))
    output += group(b"CSTY", 0, record("CSTY", recipe["style"], string("EDID", "M15CreatureStyle") + sub("CSTD", style)))
    output += group(b"CELL", 0, cells)
    reopened = list(records(output))
    actual = _subrecords(next(p for t, _, p in reopened if t == "CREA"), "Creature readback", "CREA")
    if (len(reopened) != 5 or [(s["name"], s["payload"]) for s in actual if s["name"] not in changed] != retained
            or any(s["name"] in {"SCRI", "PKID", "SPLO"} for s in actual)):
        raise ValueError("stationary Creature readback differs")
    return output, {"source_sha256": recipe["source_sha256"], "sha256": hashlib.sha256(output).hexdigest(),
                    "record_count": 4, "script_count": 0, "creature_fields_changed": sorted(changed),
                    "stats_and_assets_preserved": True, "style": combat_style(bytes(style)), "runtime_accepted": False}


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
