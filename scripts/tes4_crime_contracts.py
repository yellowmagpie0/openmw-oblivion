"""Schema42 crime/custody wire contract; no crime rule evaluation or gameplay."""
from __future__ import annotations

import math

MAX_COLLECTION = 1_000_000
SCHEMAS = {
    "state": [("next_incident", "Q"), ("next_transaction", "Q"), ("action_retention_floor", "Q"),
              ("incidents", "*incident"), ("arrests", "*arrest"), ("jails", "*jail")],
    "incident": [("request", "request"), ("outcome", "outcome")],
    "request": [("action", "Q"), ("perpetrator", "key"), ("victim", "key"), ("affected_reference", "key"),
                ("cell", "key"), ("ownership", "ownership"), ("offense", "B"), ("item_value", "i"),
                ("count", "i"), ("lawful_combat", "bool"), ("owner_has_claim", "bool")],
    "ownership": [("owner", "key"), ("rank", "i"), ("global", "key")],
    "outcome": [("incident", "Q"), ("report_phase", "B"), ("witnesses", "*witness"), ("bounty_delta", "i"),
                ("infamy_delta", "i"), ("faction_deltas", "*faction"), ("lawful_combat_exception", "bool"),
                ("guard_response_requested", "bool"), ("consequences_committed", "bool")],
    "witness": [("witness", "key"), ("observed", "bool"), ("will_report", "bool")],
    "faction": [("faction", "key"), ("delta", "i")],
    "arrest": [("transaction", "Q"), ("incident", "Q"), ("actor", "key"), ("authority", "key"),
               ("destination", "key"), ("resolution", "B"), ("phase", "B"), ("assessed_fine", "i"),
               ("fine_committed", "bool"), ("confiscation_committed", "bool"), ("transition_committed", "bool")],
    "jail": [("transaction", "Q"), ("actor", "key"), ("prison", "key"), ("evidence", "key"),
             ("belongings", "key"), ("release", "key"), ("phase", "B"), ("sentence_start", "d"),
             ("remaining_hours", "f"), ("property", "*property"), ("property_committed", "bool"),
             ("skill_penalty_committed", "bool"), ("time_penalty_committed", "bool"), ("release_committed", "bool")],
    "property": [("instance", "key"), ("base", "key"), ("count", "i"), ("original_owner", "key"),
                 ("original_ownership_rank", "?i"), ("original_ownership_global", "key"),
                 ("condition", "?f"), ("charge", "?f"), ("quest_item", "bool")],
}


def empty_state():
    return {"next_incident": 1, "next_transaction": 1, "action_retention_floor": 1,
            "incidents": [], "arrests": [], "jails": []}


def require(valid, message):
    if not valid:
        raise ValueError(message)


def key(value, nullable=True):
    require(isinstance(value, str), "Crime identity must be a string")
    if value == "null":
        require(nullable, "Crime contract requires a stable identity")
        return
    parts = value.split(":")
    require(len(parts) == 3, "Malformed crime identity")
    kind, namespace, serial = parts
    require(kind in ("content", "dynamic") and bool(namespace), "Malformed crime identity kind/namespace")
    width = 6 if kind == "content" else 16
    require(len(serial) == width and all(c in "0123456789abcdef" for c in serial)
            and int(serial, 16) > 0, "Noncanonical crime identity serial")
    if kind == "content":
        canonical = namespace.replace("\\", "/").rsplit("/", 1)[-1]
        canonical = "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in canonical)
        require(namespace == canonical, "Noncanonical crime plugin identity")


def validate_shape(value, kind="state"):
    if kind.startswith("*"):
        require(isinstance(value, list) and len(value) <= MAX_COLLECTION, "Crime collection capacity/type")
        for item in value:
            validate_shape(item, kind[1:])
    elif kind.startswith("?"):
        if value is not None:
            validate_shape(value, kind[1:])
    elif kind == "key":
        key(value)
    elif kind == "bool":
        require(type(value) is bool, "Invalid crime boolean")
    elif kind in ("Q", "B", "i"):
        low, high = {"Q": (0, 2**64 - 1), "B": (0, 255), "i": (-2**31, 2**31 - 1)}[kind]
        require(type(value) is int and low <= value <= high, "Crime integer outside wire domain")
    elif kind in ("f", "d"):
        require(type(value) in (int, float) and math.isfinite(value), "Nonfinite/invalid crime float")
        require(kind != "f" or abs(value) <= 3.4028234663852886e38, "Crime float32 overflow")
    else:
        require(isinstance(value, dict) and set(value) == {n for n, _ in SCHEMAS[kind]}, "Crime object fields mismatch")
        for name, field_kind in SCHEMAS[kind]:
            validate_shape(value[name], field_kind)


def validate(state):
    validate_shape(state)
    require(all(state[x] > 0 for x in ("next_incident", "next_transaction", "action_retention_floor")), "Zero crime counter")
    incidents, arrests, causes = {}, {}, set()
    for entry in state["incidents"]:
        request, outcome = entry["request"], entry["outcome"]
        for name in ("perpetrator", "cell"):
            key(request[name], False)
        require(request["action"] >= state["action_retention_floor"] and request["offense"] <= 6
                and request["item_value"] >= 0 and request["count"] >= 0, "Invalid crime request")
        ident = outcome["incident"]
        require(0 < ident < state["next_incident"] and ident not in incidents, "Duplicate/reused crime incident")
        incidents[ident] = entry
        cause = tuple(request[x] for x in ("action", "perpetrator", "victim", "affected_reference", "offense"))
        require(cause not in causes, "Duplicate causal offense")
        causes.add(cause)
        require(outcome["report_phase"] <= 3 and (outcome["report_phase"] != 3 or outcome["consequences_committed"]), "Invalid crime report/commit")
        witnesses, factions = set(), set()
        for witness in outcome["witnesses"]:
            key(witness["witness"], False)
            require(witness["witness"] not in witnesses and (not witness["will_report"] or witness["observed"]), "Invalid/duplicate witness")
            witnesses.add(witness["witness"])
        for delta in outcome["faction_deltas"]:
            key(delta["faction"], False)
            require(delta["faction"] not in factions, "Duplicate faction delta")
            factions.add(delta["faction"])
    for arrest in state["arrests"]:
        ident = arrest["transaction"]
        require(0 < ident < state["next_transaction"] and ident not in arrests, "Duplicate/reused arrest transaction")
        arrests[ident] = arrest
        for name in ("actor", "authority"):
            key(arrest[name], False)
        require(arrest["incident"] in incidents, "Dangling arrest incident")
        incident = incidents[arrest["incident"]]
        require(incident["request"]["perpetrator"] == arrest["actor"] and incident["outcome"]["consequences_committed"], "Invalid arrest crime binding")
        resolution, phase = arrest["resolution"], arrest["phase"]
        fine, confiscation, transition = (arrest[x] for x in ("fine_committed", "confiscation_committed", "transition_committed"))
        require(resolution <= 3 and phase <= 4 and arrest["assessed_fine"] >= 0, "Invalid arrest phase/fine")
        require(not (fine or confiscation or transition) or phase in (2, 3), "Arrest consequences precede resolution")
        require(phase != 2 or resolution != 0, "Resolving arrest has no choice")
        require(not fine or resolution == 1, "Fine commit disagrees with choice")
        require(not confiscation or resolution in (1, 2), "Confiscation disagrees with choice")
        require(not transition or (resolution == 2 and arrest["destination"] != "null"), "Transition disagrees with choice/destination")
        require(phase != 3 or (resolution != 0 and (resolution != 1 or fine) and (resolution != 2 or transition)), "Incomplete committed arrest")
        require(phase != 4 or not (fine or confiscation or transition), "Cancelled arrest has consequences")
    jails = set()
    for jail in state["jails"]:
        ident, phase = jail["transaction"], jail["phase"]
        require(ident in arrests and ident not in jails, "Dangling/duplicate jail arrest")
        jails.add(ident)
        for name in ("actor", "prison", "evidence", "belongings", "release"):
            key(jail[name], False)
        require(phase <= 4 and jail["sentence_start"] >= 0 and jail["remaining_hours"] >= 0, "Invalid jail phase/time")
        arrest = arrests[ident]
        require(arrest["actor"] == jail["actor"] and arrest["resolution"] == 2, "Invalid jail arrest binding")
        require(phase != 4 or arrest["phase"] == 4, "Cancelled jail/arrest mismatch")
        require(phase in (0, 4) or arrest["transition_committed"], "Jail transition is uncommitted")
        require(phase in (0, 4) or jail["property_committed"], "Jail property is uncommitted")
        require(jail["release_committed"] == (phase in (2, 3)), "Jail release disagrees with phase")
        require(phase != 4 or not any(jail[x] for x in ("property_committed", "skill_penalty_committed", "time_penalty_committed")), "Cancelled jail has consequences")
        instances = set()
        for item in jail["property"]:
            for name in ("instance", "base"):
                key(item[name], False)
            require(item["instance"] not in instances and item["count"] > 0, "Invalid/duplicate jail property")
            instances.add(item["instance"])
            require(all(item[x] is None or item[x] >= 0 for x in ("condition", "charge")), "Negative property extras")


def read(reader, kind="state"):
    if kind.startswith("*"):
        return [read(reader, kind[1:]) for _ in range(reader.count())]
    if kind.startswith("?"):
        return read(reader, kind[1:]) if read(reader, "bool") else None
    if kind == "key":
        value = reader.string()
        key(value)
        return value
    if kind == "bool":
        value = reader.unpack("<B")
        require(value in (0, 1), "Invalid crime boolean/presence marker")
        return bool(value)
    if kind in ("Q", "B", "i", "f", "d"):
        return reader.unpack("<" + kind)
    return {name: read(reader, field_kind) for name, field_kind in SCHEMAS[kind]}


def write(writer, value, kind="state"):
    if kind.startswith("*"):
        writer.pack("<I", len(value))
        for item in value:
            write(writer, item, kind[1:])
    elif kind.startswith("?"):
        writer.pack("<B", int(value is not None))
        if value is not None:
            write(writer, value, kind[1:])
    elif kind == "key":
        writer.string(value)
    elif kind == "bool":
        writer.pack("<B", int(value))
    elif kind in ("Q", "B", "i", "f", "d"):
        writer.pack("<" + kind, value)
    else:
        for name, field_kind in SCHEMAS[kind]:
            write(writer, value[name], field_kind)
