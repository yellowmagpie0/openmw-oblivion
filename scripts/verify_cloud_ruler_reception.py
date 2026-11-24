"""Verify the courtyard checkpoint and an instrumented, player-controlled MQ04 reception."""

import argparse
import math
import re
from pathlib import Path

from tes4_runtime_state import load_save


def form(local_id):
    return f"content:oblivion.esm:{local_id}"


# Original MQ04 PACK locations, in order from the doors of the hall toward the stairs.
POSTS = {
    "steffan": ("0375a0", 14232, 163717),
    "jena": ("0375a1", 14231, 163574),
    "belisarius": ("03759c", 14230, 163427),
    "caroline": ("03759f", 14637, 163724),
    "roliand": ("03759e", 14641, 163575),
    "cyrus": ("03759d", 14644, 163434),
}
STAGES = [35, 36, 40, 41, 42, 43, 44, 45]
SAMPLE = re.compile(r"(\w+)=\(([^)]+)\)(?: \w+(?= yaw=))?(?: yaw=([-\d.eE+]+))?")


def verify_save(path):
    state = load_save(path)
    quest = next(q for q in state["quests"] if q["quest"] == form("01e726"))
    assert quest["running"] and quest["stage"] == 45 and set(STAGES) <= set(quest["completed_stages"]), quest
    refs = {r["key"]: r for r in state["references"]}
    life = {a["actor"]: a["phase"] for a in state["native_actor_life"]}
    front = (14484, 163885, 32562)
    for key in ("01e745", "01cb98", *(post[0] for post in POSTS.values())):
        ref = refs[form(key)]
        assert not ref["deleted"] and ref["enabled"] and life[form(key)] == 0, key
        if key in ("01e745", "01cb98"):
            assert math.dist(ref["position"][:3], front) < 800, (key, ref["position"])
    assert math.dist(state["player"]["position"][:3], front) < 800
    assert state["player"]["actor_values"]["health.current"] > 0
    print(f"{path.name} retains the complete reception at stage 45 with both companions in the courtyard.")


def verify_run(path):
    stages = [35]
    voices = []
    salutes = set()
    hailed = set()
    player = []
    ready = faced_martin = passed = saluted_together = cleared = False
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        stage = re.search(r"setstage quest=content:oblivion.esm:01e726 stage=(\d+)", line)
        if stage and int(stage[1]) != stages[-1]:
            stages.append(int(stage[1]))
        voice = re.search(r"(?:package-conversation|sayto topic=\S+) info=content:oblivion.esm:(\w+).*voice=sound/.+ duration=([\d.]+)", line)
        if voice:
            assert float(voice[2]) > 0
            voices.append(voice[1])
        salute = re.search(r"pickidle actor=content:oblivion.esm:(\w+) idle=content:oblivion.esm:066393 group=onehandcheer played=true", line)
        if salute:
            salutes.add(salute[1])
        alert = re.search(r"setalert ref=content:oblivion.esm:(\w+) value=1 applied=true", line)
        if alert and stages[-1] == 42:
            hailed.add(alert[1])
        if "Reception t=" not in line:
            continue
        pose = {name: (tuple(map(float, xyz.split(","))), float(yaw) if yaw else None)
                for name, xyz, yaw in SAMPLE.findall(line)}
        player.append(pose["player"][0])
        if len(POSTS.keys() & pose.keys()) != len(POSTS):
            continue
        at_posts = all(math.dist(pose[name][0][:2], (x, y)) < 64 for name, (_, x, y) in POSTS.items())
        martin = pose["martin"][0]
        if stages[-1] == 40 and at_posts and martin[1] < 163427:
            ready = True
        looking = {}
        for name in POSTS:
            (x, y, _), yaw = pose[name]
            assert yaw is not None, name
            looking[name] = math.cos(math.atan2(martin[0] - x, martin[1] - y) - yaw) > math.cos(math.radians(35))
        if stages[-1] == 40 and ready and at_posts and 163427 < martin[1] < 163724 and 14232 < martin[0] < 14637:
            # Cyrus receives his Look instruction after the five waiting Blades.
            passed |= all(value for name, value in looking.items() if name != "cyrus")
        if stages[-1] == 42 and at_posts:
            saluted_together |= all(re.search(rf"\b{name}=\([^)]*\)[^=]*(?:yaw=[^ ]+ )?cheer=true\b", line)
                                    for name in POSTS)
        if stages[-1] == 43 and at_posts:
            faced_martin |= all(looking.values())
        if stages[-1] >= 44 and not at_posts:
            cleared = True
    assert stages == STAGES, stages
    ordered = ["037b4e", "037b4f", "037b50", "037b51", "037b52", "037c38", "037c39",
               "037c3a", "037c3b", "037c3c", "09354c", "09354d", "09354e", "09354f", "08b600", "08b601"]
    assert [v for v in voices if v in ordered] == ordered, voices
    guards = {post[0] for post in POSTS.values()}
    assert voices.count("037c37") == 5 and hailed == salutes == guards, (voices, hailed, salutes)
    assert ready and passed and saluted_together and faced_martin and cleared, (
        ready, passed, saluted_together, faced_martin, cleared)
    assert player[-1][1] - player[0][1] > 2300 and player[-1][2] - player[0][2] > 700
    assert max(math.dist(a, b) for a, b in zip(player, player[1:])) < 550
    print(f"{path.name} confirms the gate exchange, player climb, two occupied ranks, six salutes, "
          "five answering voices, audience facing and dismissal.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("saves", type=Path, nargs="+")
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    if args.log:
        verify_run(args.log)
    for save in args.saves:
        verify_save(save)
