"""Build the bounded, script-free M15 expenditure observation GMST fixture.

Only regeneration is disabled. Actor resources, attacks and contact outcomes
are never authored here. JSON is source; the generated plugin stays ignored.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from tes4_m14_audit import _subrecords
from tes4_m15_fixture import group, record, records, string, sub


def build(source: bytes, recipe: dict) -> tuple[bytes, dict]:
    required = {"version", "purpose", "source_sha256", "master", "settings"}
    names = {"fFatigueReturnBase", "fFatigueReturnMult"}
    if (not isinstance(recipe, dict) or set(recipe) != required
            or type(recipe["version"]) is not int or recipe["version"] != 1
            or recipe["master"] != "Oblivion.esm"
            or not isinstance(recipe["purpose"], str) or not recipe["purpose"].strip()
            or hashlib.sha256(source).hexdigest() != recipe["source_sha256"]):
        raise ValueError("invalid or unpinned Fatigue fixture recipe")
    settings = recipe["settings"]
    if (not isinstance(settings, dict) or set(settings) != names
            or any(type(v) not in (int, float) or v != 0 for v in settings.values())):
        raise ValueError("Fatigue fixture may only disable both regeneration settings")
    existing = {}
    for tag, ident, payload in records(source):
        if tag != "GMST":
            continue
        fields = _subrecords(payload, "Fatigue fixture source", tag)
        editors = [x["payload"] for x in fields if x["name"] == "EDID"]
        if len(editors) != 1:
            raise ValueError("ambiguous source GMST editor identity")
        editor = editors[0].rstrip(b"\0").decode("cp1252")
        if editor not in names:
            continue
        data = [x["payload"] for x in fields if x["name"] == "DATA"]
        if editor in existing or ident >> 24 or len(data) != 1 or len(data[0]) != 4:
            raise ValueError("duplicate or malformed source Fatigue GMST")
        existing[editor] = ident
    emitted, rows, own = [], [], 0x800
    for editor in sorted(names):
        if editor in existing:
            ident = existing[editor]
        else:
            ident = 0x01000000 | own
            own += 1
        emitted.append(record("GMST", ident, string("EDID", editor) + sub("DATA", struct.pack("<f", 0))))
        rows.append({"editor_id": editor, "form_id": ident, "value": 0,
                     "kind": "override" if editor in existing else "new"})
    header = sub("HEDR", struct.pack("<fII", 1, len(emitted), own))
    header += string("CNAM", "OpenMW M15 expenditure observation fixture")
    header += string("MAST", recipe["master"]) + sub("DATA", bytes(8))
    output = record("TES4", 0, header) + group(b"GMST", 0, b"".join(emitted))
    reopened = list(records(output))
    if [tag for tag, _, _ in reopened] != ["TES4", "GMST", "GMST"]:
        raise ValueError("Fatigue fixture emitted unexpected records")
    return output, {"source_sha256": recipe["source_sha256"],
                    "sha256": hashlib.sha256(output).hexdigest(), "settings": rows,
                    "record_count": 2, "script_count": 0, "actor_or_item_changes": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("recipe", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output, report = build(args.source.read_bytes(), json.loads(args.recipe.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("xb") as stream:
        stream.write(output)
    args.output.with_suffix(args.output.suffix + ".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
