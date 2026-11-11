# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. The expedition and full milestones remain open.

Latest chunk is native item selection for unowned Ayleid Cask04bb85 in
Canosel. The player reached the cask by swimming through the flooded tunnel,
surfacing and defeating Skeleton06c2e6. Opening resolves the eight authored
leveled lists once. Closing preserves all rolled entries. One click takes the
entire gold stack. The remaining entries survive same-process quickload and
a fresh process, and Take All leaves an empty cask after another cold load.
Keyboard and gamepad Take All agree. The submerged Cask066e79 and the earlier
food sack retain their verified lock and leveled-loot paths.
[Contract](AYLEID-CASK-SELECTION-CONTRACT.json),
[original-game UI oracle](AYLEID-CASK-SELECTION-ORACLE.json),
[verification](AYLEID-CASK-SELECTION-VERIFICATION.json).

The normal and instrumented engine gates each execute 2,276 tests. Python
executes 611 checks. Reused component evidence and real gameplay artifacts are
linked in the verification receipt. Schema 98 is unchanged.

The expedition continuation is
`build/oblivion-compat/container-window/normal-empty-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `b23acab4f165351d71e81185fd985fa4bedca5c15120ca825eac78de2db78637`.
The player is alive with 53.0011 Health and the cask is empty.

Next chunk is the original Weak Potion of Light earned on the independent
locked-cask branch. The candidate evidence identifies the actual bottle, save
and remaining breath. Surface by ordinary swimming, record the existing use
behavior in inventory, and verify the light in original Oblivion. The main
expedition save above remains available.
`build/oblivion-compat/potion-light/earned-potion-candidate.json`

Earlier accepted slices and reusable evidence:

- [Submerged Ayleid cask lock](AYLEID-CASK-LOCK-VERIFICATION.json): a paused
  choice preserves picks on Cancel. Explicit auto failure spends one pick and
  success unlocks the cask. Cold-load state and held breath are verified.
  Manual tumblers, Skeleton Key and skill experience remain open.

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
arrow recovery, XP, sound/reaction/view/device matrices and general bow tactics.
Zombie06c2c6 is deferred at historical `normal-stair-descent-01` (slab cleared by
jump; further descent stopped after25 units). Restoration chest04bb8b remains
behind an earlier route/ledge gap. Learned target-SPEL mastery still reads
Alteration AV20 rather than Destruction AV22: require an actual spell case.
Chameleon alpha0.75 is approximate. Earlier Flare provocation did not establish
a gap; healing and Torch02 already worked. Preserve linked evidence provenance.
