# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. The full expedition and full milestones remain open.

Accepted bounded slices:

- Original Player Flare, tutorial goblin defeat and continuation:
  [receipt](PLAYER-FLARE-VERIFICATION.json).
- Original single-actor LVLC bandits, concrete NPC gear and saved movement:
  [receipt](VILVERIN-LEVELLED-ACTORS-VERIFICATION.json).
- Original mudcrab CREA microtreasure and ordinary corpse transfer:
  [receipt](MUDCRAB-LOOT-VERIFICATION.json).
- Original food sack `06c2ad`, concrete LVLI transfer and empty stock:
  [receipt](VILVERIN-CONTAINER-LOOT-VERIFICATION.json).
- Actually earned Lettuce, first-effect REFA5 over1 second and pending restart:
  [receipt](LETTUCE-INGREDIENT-VERIFICATION.json).
- Carried Flash Bolt `0888bf` / ENCH `05c057`, FIDG20 instant Target:
  [receipt](FLASH-BOLT-SCROLL-VERIFICATION.json).
- Carried Chameleon `084aeb` / ENCH `05b044`, CHML25 for30 seconds Self:
  [receipt](CHAMELEON-SCROLL-VERIFICATION.json).
- Actually collected Welkynd Stone `000191`, native Magicka restoration/use:
  [receipt](WELKYND-STONE-VERIFICATION.json).
- Actually collected Mead `0b1201`, two DRAT1 attributes for60 seconds plus REFA2:
  [contract](MEAD-POTION-CONTRACT.json),
  [original-game oracle](MEAD-POTION-ORACLE.json),
  [receipt](MEAD-POTION-VERIFICATION.json).

Latest closed chunk: actual Welkynd Stone `04929e` / MISC `000191`.
[Contract](WELKYND-STONE-CONTRACT.json), [original oracle](WELKYND-STONE-ORACLE.json),
[receipt](WELKYND-STONE-VERIFICATION.json). Full native Magicka rejects without
spending. A real healing deficit admits one selected stone, restores toward the
current native maximum and debits that instance atomically. Original controls
show80 full retaining2,61→80 spending1, and59→78 under original Mead spending1.
Schema94 unchanged; fresh completion reload has no consumption/restoration replay.

Both normal and instrumented use/restart courses pass42 semantic checks.20 focused
engine checks and fresh full2,269 engine tests pass per configuration. Component
and Python trees are unchanged:3,281 component tests per configuration and604
Python checks retain their earlier provenance in the receipt. Rendered ASan uses
leak detection off; full engine tests use it on. Native full-guard and tooltip
images reviewed in both configurations. Exact sound/menu parity is outside scope.

Reusable latest normal checkpoint:
`build/oblivion-compat/welkynd-stone/normal-development-01-restart/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `291804bb4e836f957f1e1ea8acd24a405d8719b52371ee1e0c641aa5725f865e`.
Health59.49584, Magicka80, Fatigue170, Intelligence40, Willpower30. Welkynd0,
Mead0, Flash Bolt0, Chameleon0, nativeAV46=0. Bone1, Crab Meat1, Lettuce1, Rice1
retained, Club equipped. No owned consumable/scroll timer, cast or projectile;
next projectile19. Prior five bandit deaths retain Player killer; sack `06c2ad`
stays opened and empty, original stone `04929e` remains spent.

Actual route progress: tripwire/short maces already worked; one11.5-point hit,
three lower-room bandit defeats, two Mead pickups and their lifecycles are retained
in the earlier contracts/receipts. Three ordinary starter heals then actual walking
around the low room partition and pickup yield38 acquisition checks for the stone.
Read-only navigation query supplies the room path; early direct approaches and
incompletely aligned controller attempts remain under
`build/oblivion-compat/welkynd-stone`. No navigation/collision implementation change
is claimed. The ordinary unsupported-use baseline and independent original control
remain distinct from the final feature acceptance.

Current next action: resume forward gameplay from the completed Welkynd checkpoint,
use owned ordinary healing as needed, and find the next original content exposing
an implementation gap. Consumable gaps have shrunk; expedition/return remains
open. Finish each bounded acceptance before expanding into adjacent mechanics.

Remaining acceptance gaps: expedition continuation/return; general M16 effects
and UI; native spawn-root/child identity, grouped actors, respawn, all Player
levels and full bow tactics. Restoration chest `04bb8b` remains deferred behind
the retained partial route/ledge gap. The pre-existing learned target-SPEL mastery
gate reads Alteration AV20 rather than Destruction AV22; address with a future
actual learned-spell case. Chameleon uses approximate alpha0.75; exact refraction
and complete stealth outcome parity remain open. Historical evidence stays in
the linked receipts. The autonomous original-content iteration remains active.
