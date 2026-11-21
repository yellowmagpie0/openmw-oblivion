"""Check the original six Kvatch plaza selections and script owners across a cold reload."""
import argparse
from pathlib import Path

from tes4_runtime_state import load_save


def wave(save):
    state = load_save(save)
    prefix = "content:oblivion.esm:"
    keys = {prefix + f"{i:06x}" for i in (0x7200C, 0x7200D, 0x7200E, 0x7200F, 0x72010, 0x72012)}
    refs = {r["key"]: r for r in state["references"] if r["key"] in keys}
    assert refs.keys() == keys
    for ref in refs.values():
        assert ref["enabled"] and not ref["deleted"]
        assert ref["custom_state"]["native.levelled_actor.empty"] is False
    owners = {(i["context"], i["unit"].split("@")[0]) for i in state["script_instances"]
              if i["context"] in keys and i["on_load_fired"]}
    assert owners == {(key, prefix + script) for key in keys for script in ("00c73c", "0c8919")}
    return {key: ref["base"] for key, ref in refs.items()}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    args = parser.parse_args()
    assert wave(args.before) == wave(args.after)
    print("Six enabled actors retained their selected bases and twelve script instances.")
