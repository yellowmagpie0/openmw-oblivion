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

Latest closed chunk: owned Imperial Star of the West `047ade`, ABFA100 instant
Touch against corridor bandit `06c35c`. [Contract](STAR-WEST-POWER-CONTRACT.json),
[independent original controls](STAR-WEST-POWER-ORACLE.json),
[receipt](STAR-WEST-POWER-VERIFICATION.json). Native two-actor transfer, real
retaliation, pending/post-release/completed ownership and daily reuse now work.
Schema95 stores the last Player power use. Ordinary23 hours crossing midnight
still rejects after reload; a further hour admits a miss, whose new use persists.
The TES4 wait action now passes its native eligibility gates without depending
on the TES3 chargen global. General powers/crime/flee and full M16 remain open.

Normal runtime acceptance passes66+66+139 semantic checks; instrumented acceptance
passes98+66, including an actual0.166-second windup save before0.267-second release.
Full3,283 component and2,271 engine tests pass per configuration;606 Python checks
pass. Original reports retain fingerprints and earlier failed driver attempts.
Rendered ASan leak checks are off; full component/engine leak checks are on.

Reusable outgoing normal checkpoint:
`build/oblivion-compat/star-west/normal-recharge-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `dd74a6e89a7fe7b6fc7d5a0a04f56ea0d82328c123772d5a82c6b132941f9818`.
Health110.0, Magicka80, Fatigue170.0, Intelligence40, Willpower30.
Bone1, Crab Meat1, Lettuce1, Rice1 and all other inventory retained; Club equipped,
with ordinary fight wear. Welkynd/Mead/Flash Bolt/Chameleon remain spent. No pending
cast, projectile or consumable timer; projectile serial19. All five prior bandit
deaths retain Player credit; corridor bandit `06c35c` is now also defeated by
ordinary Club input. Sack `06c2ad` stays opened empty; stone `04929e` remains spent.
Power use timestamp97.54186427593231 hours; new daily use is unavailable.

Actual route: forward walking reached the corridor leading toward door `049424`
to Vilverin Canosel. Bandit `06c35e` remains alive. Current chunk: [basic Player bow integration](PLAYER-BOW-CONTRACT.json).
The second corridor bandit was defeated with existing Flare; ordinary door049424
activation reached Canosel0463f5. Its leveled actors materialize correctly.
Actual carried Rusty Iron Bow047aca and24 Iron Arrows017829 are equipped, but
ordinary attack hold/release has no native bow/release owner or ammo debit.
Next action: connect actual authored bow playback to existing native helpers and
complete basic release/flight/restart/real-undead acceptance. The earlier Flare
provocation candidate did not establish a gap (close detection/far obstruction). The full
expedition remains open; historical room/trap/consumable routes stay in receipts.

Remaining acceptance gaps: expedition continuation/return; general M16 effects
and UI; native spawn-root/child identity, grouped actors, respawn, all Player
levels and full bow tactics. Restoration chest `04bb8b` remains deferred behind
the retained partial route/ledge gap. The pre-existing learned target-SPEL mastery
gate reads Alteration AV20 rather than Destruction AV22; address with a future
actual learned-spell case. Chameleon uses approximate alpha0.75; exact refraction
and complete stealth outcome parity remain open. Historical evidence stays in
the linked receipts. The autonomous original-content iteration remains active.
