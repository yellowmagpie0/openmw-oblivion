# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. The full expedition and full milestones remain open.

Latest closed chunk: original Amulet of Kings0250a0 inventory OnEquip.
[Contract](AMULET-KINGS-EQUIP-CONTRACT.json),
[original script audit](AMULET-KINGS-EQUIP-ORACLE.json),
[receipt](AMULET-KINGS-EQUIP-VERIFICATION.json). Actual avatar equip runs original
027589: its message appears, GetContainer resolves the Player, and it unequips
its retained Amulet. Schema97 persists the item's own identity and locals.
Fresh-process restart does not replay OnEquip; a repeated actual attempt uses
the same identity and executes once. General inventory events/lifecycle and
stacked scripted items remain open.

Three normal and three rendered instrumented cases pass, with exact save wire
roundtrips and live load/save boundary audits. Full normal3,285 components/
2,271 engine/608 Python and instrumented3,285/2,271 pass. Instrumented test leak
checks are on; rendered leak checks are off. The first actual equip exposed a
nested bound-displaced inventory JSON layout mismatch; canonical JSON was
repaired and fresh courses pass. Original fingerprints/failures remain linked.

Current outgoing normal checkpoint:
`build/oblivion-compat/amulet-kings/normal-repeat-equip-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `f97573f62938c26f2230c7ba8b51f7de4065d58285c7bcc1d4b0295ab8518e9b`.
Player alive40.414HP,19 arrows, bow condition87.60; existing fight armor wear
remains. One equipped Torch02 has973 fuel, two spares1000. Amulet count1,
unworn, identity `dynamic:inventory-script:0000000000000001`. Cell0463f5
(Vilverin02 / Vilverin Canosel), no pending bow/flight; bandit06bfc2 dead with
Player credit. Normal Heal Minor Wounds and torch split/equip already worked;
they are continuation checks, not new implementations. Earlier loot and spent
Star of the West history remain.

Previous chunk: [ordinary Player bow](PLAYER-BOW-VERIFICATION.json) and
[native provenance](PLAYER-BOW-ORACLE.json). Authored draw/attachment, independent
flight and save/restart work. An arrow, three ordinary Flare casts, and a final
arrow defeat real bandit06bfc2 with one Player death credit and preserved stock.
Fourteen normal/instrumented cases pass. Arrow recovery and full S5 remain open.

Current chunk: [original bedroll activation](BEDROLL-REST-CONTRACT.json).
Real unowned bedroll06bf97/base01d5bf shows its focus prompt at73 units but
ordinary Activate does nothing. Native furniture still uses the generic named
class/NullAction; native bed-rest UI/recovery backend already exists. Next:
default Player bed action with existing ownership/danger checks, real open/
cancel and one-hour recovery/restart acceptance; do not claim a new rest formula.

Route discovery: the old stair blockage was not a proven movement defect.
Original plate06bf96 requires distance<64 and opens wall06bf95. An ordinary
jump clears sarcophagus066e60; stepping within35.6 triggers original scripts.
`next-content/normal-secret-wall-restart-passage-01` crosses the opened wall
after a fresh process with Health40.414 unchanged. This is existing behavior,
not another implementation. Keep that exploration checkpoint separate from the
bedroll contract's genuine camp input; no teleports or forced state changes.

Remaining acceptance gaps: expedition continuation/return; general M16 effects
and UI; spawn-root/child identity, grouped actors, respawn, all Player levels;
full S5/M15, arrow recovery, XP, sound/reaction/view/device matrices and NPC bow
tactics. Zombie06c2c6 remains deferred at actual `normal-stair-descent-01`:
ordinary jumps clear the prior slab, but further descent stops after25 units.
That retained exploratory checkpoint predates this bandit defeat. Restoration
chest04bb8b remains deferred behind its earlier partial route/ledge gap. Learned
target-SPEL mastery reads Alteration AV20 rather than Destruction AV22; use a
future actual spell case. Chameleon is approximate alpha0.75. The earlier Flare
provocation candidate did not establish a gap. Historical evidence stays linked.

Earlier accepted slices:

- [Player Flare and tutorial goblin](PLAYER-FLARE-VERIFICATION.json).
- [Original single-actor LVLC bandits](VILVERIN-LEVELLED-ACTORS-VERIFICATION.json).
- [Mudcrab CREA loot](MUDCRAB-LOOT-VERIFICATION.json).
- [Food sack and concrete LVLI transfer](VILVERIN-CONTAINER-LOOT-VERIFICATION.json).
- [Lettuce first-effect REFA](LETTUCE-INGREDIENT-VERIFICATION.json).
- [Flash Bolt scroll](FLASH-BOLT-SCROLL-VERIFICATION.json).
- [Chameleon scroll](CHAMELEON-SCROLL-VERIFICATION.json).
- [Welkynd Stone](WELKYND-STONE-VERIFICATION.json).
- [Mead](MEAD-POTION-VERIFICATION.json).
- [Star of the West and ordinary daily recharge](STAR-WEST-POWER-VERIFICATION.json).

The autonomous original-content iteration remains active.
