"""Independent JSON-ready inspection of shared WTHR state in engine saves.

Native WXVR1 catalogs identify weather without load-order indices. Legacy saves
retain numeric weather values; this module does not infer missing identities.
"""
from __future__ import annotations

import math
import struct
from pathlib import Path

from scripts.tes4_runtime_state import RuntimeStateError, _global_identity, _save_records


def decode_weather_state(data: bytes) -> dict:
    records = list(_save_records(data))
    weather = [subs for _, _, tag, subs in records if tag == b"WTHR"]
    if len(weather) != 1:
        raise RuntimeStateError("Expected one WTHR record")
    headers = [subs for _, _, tag, subs in records if tag == b"TES3"]
    masters = [data[a + 8:b].split(b"\0", 1)[0].decode("utf-8")
               for subs in headers for tag, a, b in subs if tag == b"MAST"]
    fields = [(tag, data[a + 8:b]) for tag, a, b in weather[0]]
    offset = 0

    def peek(tag):
        return offset < len(fields) and fields[offset][0] == tag

    def take(tag):
        nonlocal offset
        if not peek(tag):
            raise RuntimeStateError(f"Expected weather subrecord {tag!r}")
        value = fields[offset][1]
        offset += 1
        return value

    def number(tag, fmt):
        value = take(tag)
        if len(value) != struct.calcsize(fmt):
            raise RuntimeStateError(f"Invalid weather subrecord size: {tag!r}")
        return struct.unpack(fmt, value)[0]

    def key(value):
        kind, plugin, local = _global_identity(value.decode("utf-8").split("\0", 1)[0])
        if kind != "content":
            raise RuntimeStateError("Weather identity must be a content key")
        return f"content:{plugin}:{local:06x}"

    def region(value):
        if not value or value == b"\0":
            return "null"
        if value[:1] == b"\x03":
            if len(value) != 9:
                raise RuntimeStateError("Invalid weather FormId size")
            local, file = struct.unpack("<Ii", value[1:])
            if not 0 <= file < len(masters):
                raise RuntimeStateError("Weather FormId is outside saved master list")
            return key(f"content:{masters[file]}:{local:06x}".encode())
        if value[:1] == b"\x01":
            if len(value) < 5:
                raise RuntimeStateError("Truncated weather string RefId")
            size = struct.unpack("<I", value[1:5])[0]
            if len(value) != size + 5:
                raise RuntimeStateError("Invalid weather string RefId size")
            value = value[5:]
        elif value[:1] == b"\x02":
            value = value[1:]
        elif value and value[0] <= 6:
            raise RuntimeStateError("Unsupported weather region identity type")
        text = value.split(b"\0", 1)[0].decode("utf-8")
        return ("string:" + "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in text)) if text else "null"

    result = {"current_region": region(take(b"CREG")), "time_passed": number(b"TMPS", "<f"),
              "fast_forward": bool(number(b"FAST", "<?")), "update_time": number(b"WUPD", "<f"),
              "transition": number(b"TRFC", "<f")}
    current = number(b"CWTH", "<i")
    next_weather = number(b"NWTH", "<i")
    queued = number(b"QWTH", "<i")
    result["override"] = bool(number(b"OWTH", "<?")) if peek(b"OWTH") else False
    if any(not math.isfinite(result[name]) for name in ("time_passed", "update_time", "transition")):
        raise RuntimeStateError("Nonfinite weather timer or transition")
    catalog = None
    result["identity_version"] = None
    if peek(b"WXVR"):
        if number(b"WXVR", "<I") != 1:
            raise RuntimeStateError("Unsupported weather identity version")
        catalog = []
        while peek(b"WXID"):
            catalog.append(key(take(b"WXID")))
        if not catalog or len(set(catalog)) != len(catalog):
            raise RuntimeStateError("Empty or duplicate weather identity catalog")
        result["identity_version"] = 1
    result["catalog"] = catalog

    def weather_key(index, optional=False, inactive=False):
        if index == -1 and (optional or inactive):
            return None
        if index < 0:
            raise RuntimeStateError("Invalid negative weather index")
        if catalog is None:
            return index
        if index >= len(catalog):
            if inactive:
                return None
            raise RuntimeStateError("Weather index is outside saved identity catalog")
        return catalog[index]

    result.update(current_weather=weather_key(current), next_weather=weather_key(next_weather, True),
                  queued_weather=weather_key(queued, True), regions={})
    while peek(b"RGNN"):
        identity = region(take(b"RGNN"))
        if identity in result["regions"]:
            raise RuntimeStateError("Duplicate weather region identity")
        active = number(b"RGNW", "<i")
        if peek(b"RGDF") and catalog is None:
            raise RuntimeStateError("Weather fallback requires an identity catalog")
        fallback = number(b"RGDF", "<i") if peek(b"RGDF") else 0
        chances, order = [], []
        explicit = None
        total = 0
        seen = set()
        while peek(b"RGNC"):
            chance = number(b"RGNC", "<B")
            selected = peek(b"RGIX")
            if explicit is not None and selected != explicit:
                raise RuntimeStateError("Incomplete weather selection order")
            explicit = selected
            index = number(b"RGIX", "<i") if selected else len(chances)
            if selected:
                if catalog is None or index < -1 or index >= len(catalog):
                    raise RuntimeStateError("Invalid explicit weather selection index")
                if index >= 0 and index in seen:
                    raise RuntimeStateError("Duplicate weather selection identity")
                seen.add(index)
            order.append(weather_key(index, inactive=chance == 0 or total >= 100))
            chances.append(chance)
            if total < 100:
                total += chance
        if catalog is None and fallback != 0:
            raise RuntimeStateError("Weather fallback requires an identity catalog")
        result["regions"][identity] = {"weather": weather_key(active, True), "chances": chances,
            "selection_order": order, "fallback": weather_key(fallback)}
    if offset != len(fields):
        raise RuntimeStateError("Unexpected or unordered weather subrecords")
    return result


def read_weather_state(path: str | Path) -> dict:
    return decode_weather_state(Path(path).read_bytes())
