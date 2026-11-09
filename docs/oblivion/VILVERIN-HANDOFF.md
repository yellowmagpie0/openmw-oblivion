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
- Actually collected Mead `0b1201`, two DRAT1 attributes for60 seconds plus REFA2:
  [contract](MEAD-POTION-CONTRACT.json),
  [original-game oracle](MEAD-POTION-ORACLE.json),
  [receipt](MEAD-POTION-VERIFICATION.json).

Latest closed chunk: Mead. Ordinary selected-stack use consumes one bottle and
applies the complete mixed profile atomically. Schema94 stores separate resolved
attribute contributions and elapsed clocks for each application. Same-source
drinks stack and expire independently. Native Intelligence/Willpower and derived
Magicka/Fatigue follow the original control. Reload neither reapplies contributions
nor repeats instant restoration; expiry reverses only the owned contributions.
Legacy negative modifiers do not imply a timer. The native tooltip shows all three
authored effects. General resistance/absorption and arbitrary potion effects stay
outside this zero-Magic-Resistance Player course.

Normal and instrumented five-stage courses each pass127 semantic checks. Full3,281
component and2,266 engine tests pass per configuration at one unchanged source
fingerprint; full604 Python checks are reused with their original provenance and
unchanged source hashes. The real Chameleon course passes90 schema94 regression
checks. Structural, original-game, semantic and runtime evidence are distinguished
in the receipt. Rendered ASan disables leak detection; full engine tests enable it.

Reusable latest normal checkpoint:
`build/oblivion-compat/mead-use/normal-development-01-completed-restart/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `7c9f46eb893c161f94dab6e6161b05fefb4a6ec285dff996b03e007b89494566`.
Health29.73584, Magicka80, Fatigue170, Intelligence40, Willpower30; Mead0,
Flash Bolt0, Chameleon0, nativeAV46=0. Bone1, Crab Meat1, Lettuce1, Rice1 retained.
Club equipped. No potion/ingredient/scroll timer, pending cast or projectile;
next projectile19. Ambush `06c356`, sitting `06c359`, and lower-room bandits
`04bb72`/`04bb73`/`06bfba` remain dead with Player killer. Sack `06c2ad` stays empty.

Actual route advance: tripwire `06bf50` and short maces already work; one11.5-point
hit survives restart without replay. Lower room reached (81 route +9 room checks),
then three original bandits defeated with carried Club and starter healing. Bottles
`06c362` and `06c363` collected by ordinary activation. No combat/trap change is
claimed. Retained route/menu/aim failures and the failed worn-axe battle remain in
`build/oblivion-compat/vilverin-trap-passage`; links/provenance are in the contract.

Current next action: resume forward gameplay from the completed Mead checkpoint,
recover through ordinary available healing as needed, and choose the next actual
content that exposes an implementation gap. Consumable gaps have shrunk; the
expedition/return gap remains. Finish each bounded chunk before widening scope.

Remaining acceptance gaps: expedition continuation/return; general M16 effects
and UI; native spawn-root/child identity, grouped actors, respawn, all Player
levels and full bow tactics. Restoration chest `04bb8b` remains deferred behind
the retained partial route/ledge gap. The pre-existing learned target-SPEL mastery
gate reads Alteration AV20 rather than Destruction AV22; address with a future
actual learned-spell case. Chameleon uses approximate alpha0.75; exact refraction
and complete stealth outcome parity remain open. Historical evidence stays in
the linked receipts. The autonomous original-content iteration remains active.
