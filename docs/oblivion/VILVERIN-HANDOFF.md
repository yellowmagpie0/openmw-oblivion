# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. Expedition and broader content acceptance remain open.

Latest accepted chunk: original Undead Bone Resistances SPEL02b532, including
RSPA/Resist Paralysis AV66. Genuine pre-Canosel source has no crypt Skeleton
state; ordinary entry grants all five innate effects exactly once. Normal and
instrumented fresh-process reloads retain the complete owner and values:
Disease100, Poison100, Frost70, WaterBreathing1, Paralysis100. Schema99 retained.
[Contract](BONE-RESISTANCES-CONTRACT.json),
[independent original facts](BONE-RESISTANCES-ORACLE.json),
[runtime/check receipts](BONE-RESISTANCES-VERIFICATION.json).

Full normal/instrumented2,278 engine checks, 20 ability cases per build and
Python612 pass. Six original-content courses cover grant, cold load, secret-wall
continuation and old-null ownership; 11 accepted primary images inspected.
Engine sanitizer gates include leak checking; rendered courses disable it.
Historical backend acceptance is unchanged. Hostile Paralysis immunity and the
actual physical-contact encounter remain separate runtime gates.

Fresh ability-owned normal continuation:
`build/oblivion-compat/bone-resistances/normal-fresh-secret-wall-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `2e1ff0b90fffaa1c36e550a228f8b182a5981549104c69ec18f2133f8baccc50`.
Player64.4808 Health through the original secret wall, with original inventory.
This replays an earlier genuine expedition branch. Next: continue the actual
encounter from this owned state and select the next observed unsupported piece.
Do not bypass unresolved effects or hand-edit saves.

Earlier crypt branch remains preserved:
`build/oblivion-compat/crypt-skeleton/normal-crypt-skeleton-weapon-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `db0a894a0dbcc3e7b0a71dba3a23c0030a26eb3aca14650150e5f9e52ebd5da6`.
It retains Security6/six earned picks/Player53.0011 Health and the exact owned
IronWarAxe112. Null innate ownership is unknown and remains conservatively
unresolved; the real compatibility course confirms no grant, repair or repeated
equip. Newly constructed unsupported creatures now record known empty ownership
so later support can grant safely. [Equipment chunk](CRYPT-SKELETON-WEAPON-VERIFICATION.json).

Earlier accepted slices and reusable evidence:

- [Security-use extension](SECURITY-USE-VERIFICATION.json): original Gate049bbc
  and Gate049bba; authored1.5XP on explicit success/failure, none on Cancel or
  reload. Real Security5→6 retains fractional XP and Agility/Stealth/per-skill
  counts. Ten normal/instrumented courses; missing tooltip defaults fixed from
  original compiled values. Exact native use-slot/scaling remains uncertified.

- [First Canosel gate](IRON-GATE-LOCK-VERIFICATION.json): Gate049bbd paused
  lock choice, Cancel, failure/success, open-pose cold load and passage. The
  actual old baseline exposed a deferred temporary-message use-after-free;
  FailedAction owns its text. Original menu/Close/failure debits certified;
  original leveled8 vs fork7 remains adjacent. Historical depleted-stock and
  seven-pick sources retain their original binary/source provenance there.

- [Weak Potion of Light](WEAK-POTION-OF-LIGHT-VERIFICATION.json): original
  earned ALCH09847f, authored green20-foot/60-second owned timer; ordinary use,
  active/expired cold loads and movement in eight normal/instrumented courses.
  Schema99; overlapping sources/general M16 remain open.

- [Ayleid cask selection](AYLEID-CASK-SELECTION-VERIFICATION.json): unowned
  Cask04bb85 rolls once; close, one-stack selection, keyboard/gamepad Take All
  and empty cold restart preserve native ownership and inventory.

- [Submerged Ayleid cask lock](AYLEID-CASK-LOCK-VERIFICATION.json): a paused
  choice preserves picks on Cancel. Explicit auto failure spends one pick and
  success unlocks the cask. Cold-load state and held breath are verified.
  Manual tumblers and Skeleton Key remain open; the later Security-use
  extension above is exercised on gates.

- [Novice Bonemeal](BONEMEAL-INGREDIENT-VERIFICATION.json): earned Skeleton
  loot, Damage Fatigue1/sec for3sec with remaining-duration restart; schema98
  retains legacy food5/1. Higher Alchemy/general potency remain open.

- [Original Skeleton archery](SKELETON-ARCHERY-VERIFICATION.json): live
  Creature084b1a equips its actual Iron Bow/Arrows, saves an owned draw, releases
  one saved flight in a fresh process, then hits Player once in another. NPC
  stock6 preserved; wear0.48 once; frame/calendar clock corrected within0.25s.
  Tactical hold/chance/retreat, leading, broken bow drops and NPC misses remain open.
- [Player static-arrow recovery](IRON-ARROW-RECOVERY-VERIFICATION.json): actual
  Canosel floor049be8 impact19→18+world1; fresh pickup→19, repeat/restart retain
  taken tombstone; untaken90-second age plus3-second fade removes once. Settled
  age starts at impact. Actor recovery, full flight aging, process budgets and
  full Havok embedding/bounce remain open.
- [Voice of the Emperor](VOICE-EMPEROR-VERIFICATION.json): Skeleton disposition
  20→50→20, daily ownership, recipient timer and restart. General Charm/resistance,
  caster-hit dispelling and full M16 remain open.
- [Canosel bedroll](BEDROLL-REST-VERIFICATION.json): original unowned FURN06bf97
  rest UI/hour restores40.414→110 Health; reopen/cancel/restart. Poses, owned/
  occupied matrices, leveling and full M13 remain open.
- [Amulet OnEquip](AMULET-KINGS-EQUIP-VERIFICATION.json): original027589,
  GetContainer, independent inventory-script identity/locals, no load replay.
- [Player bow](PLAYER-BOW-VERIFICATION.json): authored draw/attachment, flight/
  restart; arrow, three Flares and final arrow defeat bandit06bfc2.
- [Player Flare/tutorial goblin](PLAYER-FLARE-VERIFICATION.json),
  [levelled bandits](VILVERIN-LEVELLED-ACTORS-VERIFICATION.json),
  [mudcrab loot](MUDCRAB-LOOT-VERIFICATION.json),
  [food sack/LVLI](VILVERIN-CONTAINER-LOOT-VERIFICATION.json),
  [Lettuce](LETTUCE-INGREDIENT-VERIFICATION.json),
  [Flash Bolt](FLASH-BOLT-SCROLL-VERIFICATION.json),
  [Chameleon](CHAMELEON-SCROLL-VERIFICATION.json),
  [Welkynd Stone](WELKYND-STONE-VERIFICATION.json),
  [Mead](MEAD-POTION-VERIFICATION.json),
  [Star of the West](STAR-WEST-POWER-VERIFICATION.json).

Route evidence: ordinary coffin jump/distance<64 plate06bf96 open wall06bf95;
`next-content/normal-secret-wall-restart-passage-01` traverses after restart.
Ordinary swimming resolves the flooded route. These are existing behavior;
the older bedroll camp/wall exploration inputs remain historical branches.

Remaining acceptance gaps: expedition continuation/return; general M16 effects/UI;
spawn-root/child identity, grouped actors, respawn/all levels; full S5/M15, actor
arrow recovery, general XP, sound/reaction/view/device matrices and general bow tactics.
Zombie06c2c6 is deferred at historical `normal-stair-descent-01` (slab cleared by
jump; further descent stopped after25 units). Restoration chest04bb8b remains
behind an earlier route/ledge gap. Learned target-SPEL mastery still reads
Alteration AV20 rather than Destruction AV22: require an actual spell case.
Chameleon alpha0.75 is approximate. Earlier Flare provocation did not establish
a gap; healing and Torch02 already worked. Preserve linked evidence provenance.
