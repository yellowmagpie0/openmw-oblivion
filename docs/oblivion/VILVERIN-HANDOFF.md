# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. Expedition and broader content acceptance remain open.

Latest accepted chunk: supported dead native actors enter the existing owned
physical scene/body path. Actual Skeleton06c2ed now collapses using its original
17-body/13-joint NIF. Both retained-corpse admission and fresh Club kills are
verified; cold loads retain body identities and filters, native death state,
Health/XP/inventory and one innate grant. No GetUp owner is created for a corpse.
Existing recovery death cleanup rebinds actor pointers after publication.
[Contract](SKELETON-DEATH-POSE-CONTRACT.json),
[original asset facts](SKELETON-DEATH-POSE-ORACLE.json),
[verification](SKELETON-DEATH-POSE-VERIFICATION.json).

Seven accepted actual-content courses, nine primary images; old death/bind-pose
baseline has three reviewed images. Focused336 and full2,278 engine checks in
each build, Python612 pass. Engine sanitizer gates include leak checking;
rendered courses disable it. A sanitizer input-loop failure after native death
is retained separately: its combat aim logger stopped when the actor died.
Retry keeps aim observations across that transition; C++ was unchanged.

This reuses the current Down profile, not an instruction-certified original
death controller/impulse sequence. Unsupported nonphysical models retain legacy
death handling. Other Creature/NPC death courses, resurrection and independent
loose-weapon death drops remain separate acceptance work. Schema99 unchanged.

Latest integration checkpoint: ordinary activation and Take All transfer the
physical Skeleton's Bonemeal1 and Steel War Axe1 exactly once. Normal and
sanitized cold loads keep its inventory explicitly empty, Player items and axe
condition149.7999 intact, and the same17-body corpse owner. Four accepted courses,
eight inspected images and fresh Python612 checks; unchanged C++ reuses the
engine evidence above with its original provenance. No engine fix was needed.
[Corpse loot verification](SKELETON-CORPSE-LOOT-VERIFICATION.json).

Primary continuation — actual return obstruction below the upper Vilverin maces:
`build/oblivion-compat/expedition-return/normal-outside-door-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `fec79d98e5222b121d406e9d2bc8e8ad3908914f381f9be638d7794c95945aab`.
Player110 Health after an ordinary hour at bedroll06bf97; Blunt35/XP2.0,
Restoration6/XP1.3062, retained Bonemeal and Steel War Axe. Normal route audit
preserves exact inventory, progression, character history and native death state.
The original Skeleton remains Dead/empty with its17-body physical owner.

The return crossed the opened secret wall in reverse, reached the bedroll and
activated door049b4a into upper Vilverin. Images were inspected. At approximately
(-4988,-2105,-1102), original short swinging maces06c605/0c4986 obstruct the
stair return: center approach, left side-step, Jump and Sneak did not clear it;
Health stays110. Cold read-only inspection and a three-second W attempt both completed. Existing
`OPENMW_TRACE_PLAYER_OBSTACLES` identifies actual blocker0c4986 at upper-body
height109.96 (capsule half-height66.5), grounded, no step. Its eight-body dynamic
chain/head remains owned; body54 is settled. Rays alone miss these shapes and
were not sufficient to identify the blocker. Independent original-game comparison confirms passage from near the same
position: Y-2109.75 to-1949.94 after W3sec, then-1708.22 after another2sec. Both
maces remain enabled/triggered3. Their placement differs visually, so a missing
Player-push rule is not established. NIF inspection rules out an Unequip root-transform change: its two interpolators
are constant identity, no key data, duration0.03333sec; text keys are start/sound/end.
Next: resolve original settled physical body placement (including the native
probe frame-time/settling boundary) against the owned fixed anchor and seven
constraints before choosing a fix. Do not assume a Player-push rule or reset
saved bodies merely to pass the stair.
Original Script053259 only lowers potency after5sec; it does not reset/disable
spent maces. Oracle evidence and retained native ESS are under
`build/oblivion-compat/settled-mace-push/original-02/observation.json`; the original
prefix was restored exactly, pristine67d927… unchanged. No games remain live.
No production change or outside acceptance claimed. Both inspector/trace courses
are terminal; evidence names end `-inspect-01` and `-trace-01`.

Evidence root `build/oblivion-compat/expedition-return` retains editable manifests,
manual ordinary-input receipts and `route-state-audit.json`. First long path
ended early at a wall; shorter routes succeeded. A preflight Caps Lock name was
unsupported, so ordinary Shift Run is used. The outside-door manifest also had
its target height below the observed landing; corrected
`outside-door-floor.json` uses path endpoint(-5415.79,74.8,517.6).
The long outside attempt is a retained failed diagnostic with actual F5 saves,
not an accepted outside course. No item/actor-stat/save edits or effect bypass.

Next content candidate after the return obstruction: original Nirnroot06f184
at(53306.80,91007.98,82.94), plant base066a26/066538 and ingredient04e940.
Its original OnAdd starts MS39(04e936), increments quest counters and miscstat27;
current generic harvesting appears to omit that event, but this is not yet a
runtime-established failure. Read-only original-record surveys and an editable
ordinary pickup manifest are under the return root and
`build/oblivion-compat/nirnroot-pickup/first-plant.json`. Do not implement a
speculative fix before the actual pickup. Unknown innate ownership stays unknown.

Loot baseline01 selected floor STAT049cc7 beside the thin bones. Tighter
camera-relative aim at a rendering-ray-confirmed bone point succeeded, so this
was an input-observer failure rather than an inventory/engine picking defect.
Evidence root: `build/oblivion-compat/skeleton-corpse-loot`.

Reusable pre-kill source:
`build/oblivion-compat/skeleton-retaliation/normal-retaliation-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `16f23ad74d5b7fde691e9aa6d011096155957c0049251528ec22faccd4c39fb9`.
Player61.6072, Skeleton10.2595, owned Steel War Axe151.5999, Club60.3400,
Blunt35/XP1.0. Native Actor.isDead/life is authoritative; float Health<=0 is not
an adequate death observer. Old defeat01/02 were harness failures after actual
native death, not proof of missing pursuit. Their records remain preserved.

Recent independent branches retain their progress:

- [Undead Bone Resistances](BONE-RESISTANCES-VERIFICATION.json): RSPA fixedAV66
  enables original SPEL02b532's complete five-effect innate grant. Normal and
  instrumented reloads retain one owner. Fresh unsupported creatures keep known
  empty ownership; existing null state stays unknown. Hostile Paralysis immunity
  and actual physical-contact encounter acceptance remain separate runtime gates.
- [Marksman use](MARKSMAN-USE-VERIFICATION.json): actual Rusty Iron Bow047aca/
  Iron Arrow017829 hits bandit06bfc2 for about1.84 and grants0.8 XP once. Two cold
  loads retain it; static ARNHallTurn01/06bf42 miss and incoming Skeleton arrows
  grant no Player Marksman. Incoming-arrow continuation supplies the heal wound.
- [Creature owned axe](CRYPT-SKELETON-WEAPON-VERIFICATION.json): old crypt source
  `crypt-skeleton/normal-crypt-skeleton-weapon-restart-01` retains Security6/six
  earned picks/Player53.0011 Health and IronWarAxe112. Its null innate ownership
  stays unresolved; actual compatibility check confirms no grant/repair/re-equip.

Restoration branch:
`build/oblivion-compat/restoration-use/normal-heal-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `2899425e4b21ae020b908da6abbda20dfddc5485ebb4ca54dd252d93efb957e2`.
Player85.6551 under active Skeleton arrows, Restoration5/XP0.6 and selected heal.
This is a separate genuine earlier combat branch, not the primary innate-owned
route. All source histories remain linked in their receipts.

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
