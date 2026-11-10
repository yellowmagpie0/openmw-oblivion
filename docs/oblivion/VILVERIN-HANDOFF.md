# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. The expedition and full milestones remain open.

Latest closed chunk: original submerged Ayleid Cask066e79 lock choices.
[Contract](AYLEID-CASK-LOCK-CONTRACT.json),
[original-game UI oracle](AYLEID-CASK-LOCK-ORACLE.json),
[verification](AYLEID-CASK-LOCK-VERIFICATION.json).
Activation now pauses before offering Auto Attempt / Cancel. Cancel spends no
pick; explicit failure breaks one and offers another paused choice. Actual
normal attempts fail once, then unlock and take13 Gold, a Bronze Necklace and
a Weak Potion of Healing. Cancelled and emptied states survive cold reload.

Four normal and three linked instrumented courses pass live/wire/save audits.
Normal/SAN engine gates each pass2,276; current Python611 passes. Components
reuse the unchanged Bonemeal provenance. Schema98 is unchanged. Original menus
hold breath/Health and preserve picks on Close; fork menu holds8 wall seconds
with only0.166 simulation seconds across boundary frames. Manual tumblers,
Skeleton Key/XP and full lock/device matrices remain open; existing auto chance
is approximate. Original leveled lock1 versus fork0 is recorded separately.

Current genuine continuation, surfaced with air and retained actual loot:
`build/oblivion-compat/lock-dialog/normal-loot-surface-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 f98a69ceb53b487e35498819e84291df4ae30fbf525fc869c41014ee7728a81c.
Player53.0011 Health/42.2032 Magicka/170 Fatigue; breath20.5;
Canosel0463f5 at(-2204.043,-10014.338,-2860.883), looking straight up.
Cask066e79 is unlocked/empty. Picks7; new loot13 Gold, Bronze Necklace038027,
Weak Potion of Healing009310. Arrows24, looted Iron Bow109.12 condition,
Player-credited dead/emptied Skeleton04bb63, Amulet script owner1/locals and
arrow2 tombstone persist. Next dynamic serial3, next arrow14. Both powers remain
spent. The instrumented branch rolled different actual loot and is historical;
use the normal surfaced continuation above for expedition progress.

Next bounded piece: untouched unowned/unlocked Cask04bb85/base03767e at
(-5298.3,-10061.1,-2624.0), still holding8 authored LVLI roots. Implement its
native item-selection window: open/close without taking, take one item, reload
the remainder, take all, then reload empty. Current container activation takes
all stock immediately, and ESM4Container lacks a ContainerStore view. First
reach it through ordinary movement from the safe checkpoint and save before
activation; retain the real automatic-transfer baseline. Read-only candidates:
`build/oblivion-compat/next-content/canosel-item-candidates.json`.
The autonomous content iteration remains active; full expedition/milestones open.

Earlier accepted slices and reusable evidence:

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
