# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. Expedition and broader content acceptance remain open.

Latest accepted chunk: original crypt Skeleton06c2e4 (CreatureSkeleton1,
resolved024ddb) now equips its own rolled Iron War Axe000d81. The helper admits
native creatures with WeapAndShield bit4 and retains the NPC physical-stack
selector. One auto-equip changes slot0→65536, visibly holds the axe and uses
native onehand attacks. Normal and instrumented cold loads retain the same
112 condition/count/metadata and do not equip again. No item, repair or stat
grant. [Contract](CRYPT-SKELETON-WEAPON-CONTRACT.json),
[raw original data, runtime audits and source provenance](CRYPT-SKELETON-WEAPON-VERIFICATION.json).

Focused 8 and full normal/sanitizer 2,278 engine tests pass; Python 611 passes.
Two normal and two instrumented courses pass, plus a captured empty-handed
baseline. All 10 primary images inspected. Schema 99; existing inventory and
active-strike ownership retained. Engine sanitizer gates use leak checking;
rendered sanitizer courses disable it. Historical backend acceptance is unchanged.

This is equipment-only acceptance. The Skeleton still cannot publish physical
damage: original Undead Bone Resistances SPEL02b532 includes RSPA, absent from
the admitted permanent passive definitions. Keep the unresolved-contact guard;
the whole encounter and general M13/M15/M16 content matrices remain open.

Current expedition continuation:
`build/oblivion-compat/crypt-skeleton/normal-crypt-skeleton-weapon-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `db0a894a0dbcc3e7b0a71dba3a23c0030a26eb3aca14650150e5f9e52ebd5da6`.
Player alive with 53.0011 Health, Security 6 and six earned picks, near the active
Skeleton in Canosel through open Gate049bba. Original Skeleton alive with 20,
Iron War Axe at 112 equipped; no health damage or wear yet because the passive/
contact boundary remains. Prior XP branches keep their original provenance.

Next action: implement the bounded original Undead Bone Resistances ability,
starting with RSPA/permanent resistance admission and reviewing safe ownership
for legacy null passive state. Verify the actual Skeleton can then publish its
ordinary contact damage, retain innate values/ownership through cold loading,
and never double-apply grants. Do not bypass unresolved effects or silently
invent already-applied legacy ownership; use original rules/evidence as needed.

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
