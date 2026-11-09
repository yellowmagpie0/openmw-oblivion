# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. The full expedition and full milestones remain open.

Accepted bounded slices:

- Original Player Flare, tutorial goblin defeat and continuation:
  [receipt](PLAYER-FLARE-VERIFICATION.json).
- Original single-actor LVLC bandits, concrete NPC gear and saved movement:
  [receipt](VILVERIN-LEVELLED-ACTORS-VERIFICATION.json).
- Original mudcrab CREA leveled microtreasure and ordinary corpse transfer:
  [receipt](MUDCRAB-LOOT-VERIFICATION.json).
- Original first-area food sack `06c2ad`, LVLI `053f26` count4, concrete transfer
  and authoritative empty stock: [receipt](VILVERIN-CONTAINER-LOOT-VERIFICATION.json).
- One actually earned Lettuce, bounded first-effect REFA5 over1 second with
  independent use timers and pending restart: [receipt](LETTUCE-INGREDIENT-VERIFICATION.json).
- Original carried Flash Bolt `0888bf` / ENCH `05c057`, one instant20 fire Target
  effect: [contract](FLASH-BOLT-SCROLL-CONTRACT.json),
  [original-game oracle](FLASH-BOLT-SCROLL-ORACLE.json),
  [receipt](FLASH-BOLT-SCROLL-VERIFICATION.json).

Latest closed chunk: Flash Bolt implementation `0c53d9c463`. Normal inventory
and native magic-menu selection spend one selected instance at cast admission,
without a learned-spell mastery gate or Magicka debit. Pending casts retain the
BOOK source after stock disappears. Normal saves at elapsed0.159 and ASan at
0.166, before release0.4; fresh processes deliver one20-point hit to original
sitting bandit `06c359`, then retain the committed death without replay.
Each configuration passes65 actual-save semantic checks and full2,262 engine
tests. Unchanged3,275 component tests per configuration and600 Python tests keep
their original ingredient-use provenance, with source/binary reuse checks.

Native ENCH is resolved directly, never exposed through the generic TES3
getEnchantment property. A real recharge warning and stacking dependency caught
during development are fixed; final courses forbid that warning. General scrolls,
previous-spell fallback and armor-specific scaling remain outside this slice.

Reusable latest normal checkpoint:
`build/oblivion-compat/scroll-casting/flash-bolt-completed-restart-final-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `8d106728715430819ac85490eead6050afdbd5cc7aee38da03435bc5a854d7dc`.
Health67.679, Magicka80, Fatigue170, Destruction5; Flash Bolt0, Chameleon1,
Bone1, Crab Meat1, Lettuce1, Rice1. Ambush `06c356` and sitting bandit `06c359`
remain dead with Player killer; the food sack stays opened and empty. No pending
ingredient timer, cast or projectile; next projectile serial19.

Current next action: select another actually present original content item or
encounter, reproduce its unsupported path with ordinary input, then define the
next bounded implementation. The autonomous content iteration remains active.

Remaining acceptance gaps include the rest of the expedition/return, general
M16 effects/UI, native spawn-root/child identity, grouped actors, respawn, all
Player levels and full bow tactics. Restoration chest `04bb8b` remains deferred
behind the retained partial route/ledge gap. A pre-existing learned target SPEL
mastery gate reads Alteration AV20 instead of Destruction AV22; record this for a
future actual learned-spell case rather than changing it inside the scroll slice.

Rendered ASan uses leak detection off; the full engine test process uses it on.
Actual ELF/runtime/source hashes are in the receipt. Current courses use ordinary
keyboard/mouse input; instrumented startup, mouse feedback and quicksave wait for
fresh frames/acknowledgements. Historical SDL-shim/display recipes remain in the
linked earlier receipts.
