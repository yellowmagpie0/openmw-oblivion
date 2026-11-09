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

Latest accepted slice: one actual earned Lettuce use, bounded M16 first-effect
REFA (authored magnitude15/duration1 only). See the
[contract](LETTUCE-INGREDIENT-CONTRACT.json),
[original-game oracle](LETTUCE-INGREDIENT-ORACLE.json) and
[verification](LETTUCE-INGREDIENT-VERIFICATION.json). Native INGR effects/value
and known first-effect tooltip replace the legacy defaults/GMST path. One use
spends one Lettuce and owns 5 Fatigue over 1 second in schema92 actor values.
Normal and ASan use → pending restart → completed restart each pass 51 semantic
checks. Normal saves resume 1.455+3.545; ASan resumes 1.331+3.669; neither replays.
Each configuration passes full 3,275 component and 2,260 engine tests; full 600
Python tests and independent typed/raw agreement for 173 ingredients/656 effects
also pass. Actual jump depletion, images, live sanitizer environment and matching
normal/instrumented C++ sources are recorded. Fixture-schema recovery retains
future-version rejection and pending-food preservation.

Current chunk: original carried Flash Bolt scroll `0888bf`, ENCH `05c057`:
single instant20 fire Target effect. [Contract](FLASH-BOLT-SCROLL-CONTRACT.json).
Normal baseline01 opens an empty scroll-reading window and retains the whole
inventory/Magicka/Health. Nearby original sitting bandit `06c359` has Health16
and no fire/magic resistance. Next: independent original-game consumption phase/
resource check, then native scroll selection/cast/projectile ownership. Keep
Chameleon/general scrolls and full M16 outside this slice.

Reusable latest normal checkpoint:
`build/oblivion-compat/ingredient-use/lettuce-completed-restart-03/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `95f35e0ef3416eddbbcc15eab2de73bd40b8ba78b1d7d803027a3d5a02e6efeb`.
Carries original Bone1/Crab Meat1 plus earned Lettuce1/Rice1; ambush NPC remains
dead and the sack empty. Fatigue170, no pending ingredient effect. Both normal and instrumented continuations are accepted. Final source, binary
and loaded-library hashes and implementation revision are in the ingredient
receipt. Earlier container provenance stays in its linked receipt.

The restoration-chest candidate `04bb8b` is deferred behind a repeated partial
route/ledge gap; its records and probes remain linked in the current contract.
Native scripted spawn-root/child identity, grouped actors, respawn, all Player
levels, full bow tactics and full M13/M16/UI completion remain separate.

Rendered ASan uses leak detection off; full engine test-process leak checking is
on. ASan SDL replay requires actual ELF `/usr/lib64/libasan.so.8.0.0` before the
input shim. The compiler's libasan path is a linker script here. The initial
engine run's 79 X11-only failures pass unchanged-source on an owned `-displayfd`
server, followed by the full 2,257 pass; see the linked display-recovery receipt.
