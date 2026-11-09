# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. The full expedition and full milestones remain open.

Accepted bounded slices:

- Original Player Flare casting, original tutorial goblin defeat and continuation:
  [Player Flare receipt](PLAYER-FLARE-VERIFICATION.json).
- Original single-actor LVLC bandits, concrete NPC gear and complete saved movement
  ownership: [leveled-actor receipt](VILVERIN-LEVELLED-ACTORS-VERIFICATION.json).
- Original mudcrab CREA leveled microtreasure, ordinary death/corpse transfer and
  authoritative saved inventory: [mudcrab receipt](MUDCRAB-LOOT-VERIFICATION.json).
- Original first-area food sack `06c2ad`, LVLI `053f26` count4, concrete transfer
  and repeat-activation persistence:
  [container receipt](VILVERIN-CONTAINER-LOOT-VERIFICATION.json) and
  [contract](VILVERIN-CONTAINER-LOOT-CONTRACT.json).

The latest slice passes 145 semantic checks, 32 focused inventory checks, both
full 2,257 engine suites, 28 original-instruction cases and typed/raw agreement
for the container/list and 11 entries. Unchanged full 3,272 component evidence in
both configurations is reused with an empty component-source/test diff.
Normal input traverses original door `066ac8`, kills original ambush NPC `06c356`
with four Flare casts and reaches the sack at Health 67.679. Actual normal loot:
Lettuce 2 + Rice 1. Instrumented fresh loot: Leek 1 + Pear 1 + Potato 1 + Rice 1.
Both configurations retain the normal transfer and empty source; old saved
empty stock stays empty. Normal images show the actual sack and food quantities.

Current chunk: select/reproduce use of one actual earned Lettuce as a bounded
M16 ingredient slice. Shared ingredient projection omits native effects and seeds legacy defaults.
Actual use reaches a potion-only consumer and is rejected; hovering also errors
on missing Morrowind `fWortChanceValue`. Baseline01 retains Lettuce2 after one
normal drag-to-avatar attempt with depleted Fatigue. Original ingestion rules
are next; do not substitute a TES3 GMST to hide the missing native path.
Keep the same character and prove item/resource/effect ownership through restart.
Ingredient use and progression were excluded from container acceptance.

Reusable latest normal checkpoint:
`build/oblivion-compat/vilverin-containers/food-sack-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `85e7f595386586eeed29b02d4cb3328370a7322c84f05f4e9a35b65c9afcddd7`.
Carries original Bone 1/Crab Meat 1 plus actual Lettuce 2/Rice 1; ambush NPC remains
dead. Current C++ source is `d095f71345`; tested dirty `9ee1764b5e` source,
source fingerprints, binary/resource/input hashes and failures are in the receipt.

The restoration-chest candidate `04bb8b` is deferred behind a repeated partial
route/ledge gap; its records and probes remain linked in the current contract.
Native scripted spawn-root/child identity, grouped actors, respawn, all Player
levels, full bow tactics and full M13/M16/UI completion remain separate.

Rendered ASan uses leak detection off; full engine test-process leak checking is
on. ASan SDL replay requires actual ELF `/usr/lib64/libasan.so.8.0.0` before the
input shim. The compiler's libasan path is a linker script here. The initial
engine run's 79 X11-only failures pass unchanged-source on an owned `-displayfd`
server, followed by the full 2,257 pass; see the linked display-recovery receipt.
