# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. **Normal retained expedition completed.** Broader
M13/M15/M16/M18 acceptance remains open.

Latest accepted chunk: M16 post-class Destruction experience from original
Flare0a97df against Skeleton04bb63/base084b1a. Four actual5.58-damage hits
raise5/XP0→6/XP1.90621, Willpower/Magic advances+1 and misc2+1, major progress
unchanged. Saved in-flight projectile26 hasXP0; cold impact awards1.2 once.
Completed reload retains6/XP1.90621 with no release/impact replay. Static miss
and real Flash Bolt20-damage scroll grant no Destruction XP. Only newly earned
ordinary-use level counts are added; historical missing misc2 is not invented.
[Contract](DESTRUCTION-USE-CONTRACT.json), [original oracle](DESTRUCTION-USE-ORACLE.json),
[verification](DESTRUCTION-USE-VERIFICATION.json).

Six normal/four instrumented actual courses pass. Eighteen primary captures
reviewed; separate read-only UI shows Destruction6/Warrior/level1/Health78/110.
Fresh full normal2,282 engine and complete audited instrumented2,282 engine
inventory pass. Reuse original book3,294 normal/1,115 native sanitizer components
and Python616 provenance: those implementations/tests are unchanged. Engine
sanitizer gates check leaks; rendered/component gates disable leak checks.
Fixed-wall-time sanitizer cast attempts, scroll observation/schema92 comparison
failures, UI marker mistakes and unproven outer143 interruptions remain explicit.
All actual gate subprocesses exited0; independent XML union verifies11+2,271
without missing/duplicate/failed/skipped cases. Post-gate changes only correct
input recipes and acceptance docs; production C++/tests unchanged. Original
Docs/Plugins restored exactly after interrupted cleanup; pristine unchanged,
no owned original process remains.

Destruction test continuation (earlier genuine Canosel branch):
`build/oblivion-compat/destruction-use/normal-flare-completed-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `ef3dcbd5cdbd131f6bc6d5ae3c2a39a075166b3f0d6bd7bf1cb56a7fd2646cd2`.
Health78.215/Fatigue170, Destruction6/XP1.90621; Skeleton04bb63 dead with Player
credit and17 bodies. Its authored Bonemeal remains unlooted. This branch
predates the later Crypt/harvest/book progress; do not merge its save state.

Independent retained outside continuations:

- Nirnroot: `build/oblivion-compat/nirnroot-use/normal-completed-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
  SHA256 `6a4d5b649af28eee89d7caaf01425bea5470049fcf99ac129b028fb8f0be1165`.
  Health110, one Nirnroot owner1/Found1, Seeking Your Roots stage10/counters2,
  Blunt35/XP2, Restoration6/XP1.3062, Bonemeal/Steel War Axe/Club retained;
  Skeleton06c2ed dead/empty with17 bodies and one innate grant.
  [Receipt](NIRNROOT-USE-VERIFICATION.json).
- Immortal Blood: `build/oblivion-compat/skill-book/normal-book-repeat-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
  SHA256 `fb073ae38596c6e476dcaf3aae4dad4fd65b394d6f4ae15e849132dd28b2613b`.
  Hand-to-Hand36/XP0, major progress1, one base award, Health83 from actual
  campfire approach. [Receipt](SKILL-BOOK-READ-VERIFICATION.json).

Accepted latest chunk: [ordinary Hand-to-Hand use](HANDTOHAND-USE-VERIFICATION.json)
against original Skeleton06c2ed/Creature024ddb. Genuine Crypt checkpoint plus
ordinary Club unequip gives Hand35/XP0; one3.02-damage punch now grants0.6XP.
Normal and instrumented hit/reload/miss pass, Club still grants Blunt0.5,
shared advancement and original ability ownership remain unchanged. Normal2282
engine tests and sanitizer2282 disjoint inventory union pass; all17 primary
captures reviewed. Schema101 unchanged. Reused original Rat0.6 observation and
SKIL000042, with original provenance; no new original launch.

Hand-to-Hand test continuation (earlier Crypt branch, Skeleton still alive):
`build/oblivion-compat/handtohand-use/normal-completed-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `a991290dd0727c1ee4be180c1f9337800e92fcb30387305d1784ef05070c5926`.
Hand35/XP0.6, target Health16.97875; do not merge with later death/harvest/book
branches. Retained timeouts, Bullet test-process partition and capture-wrapper
recovery are explicit in the receipt. No pending-strike, level-crossing, blocked
or immune Hand acceptance. Next action: address the observed Mudcrab disease
selection/contact-admission gap below, starting before actor initialization.

Adjacent discovery: original Mudcrab06c927 has LVSP181d18 selecting Swamp
Fever098415 (Strength/Endurance drains). Current contact admission supports
only a single Intelligence drain, so its otherwise complete WABR ownership
cannot admit a punch. Retained approach/empty-hand checkpoints and failed
contact in `build/oblivion-compat/handtohand-use/mudcrab-contact-blocker.json`.
No damage committed, so this is not evidence of missing Hand XP. Defer disease
admission to a separate content chunk; start before actor initialization rather
than repairing cached unknown disease ownership. Healing ingredient candidate
also deferred: no local loose/carried example, flora11k/33%.

Previous accepted chunks:

- [Nirnroot harvest OnAdd](NIRNROOT-PICKUP-VERIFICATION.json): original plants
  06f184/06f182, distinct Found1 owners, counts2, no replay or physical merging.
- [Settled mace push](SETTLED-MACE-PUSH-VERIFICATION.json): ordinary movement
  clears original Vilverin mace0c4986, retaining8 bodies/7 joints through
  passage and cold load. Mass80/response0.1s are approximations.
- [Skeleton death](SKELETON-DEATH-POSE-VERIFICATION.json),
  [corpse loot](SKELETON-CORPSE-LOOT-VERIFICATION.json),
  [retaliation](SKELETON-RETALIATION-VERIFICATION.json),
  [axe](CRYPT-SKELETON-WEAPON-VERIFICATION.json),
  [resistances](BONE-RESISTANCES-VERIFICATION.json),
  [Blunt](BLUNT-USE-VERIFICATION.json), [Marksman](MARKSMAN-USE-VERIFICATION.json),
  [Restoration](RESTORATION-USE-VERIFICATION.json),
  [bedroll](BEDROLL-REST-VERIFICATION.json), [Amulet](AMULET-KINGS-EQUIP-VERIFICATION.json).
- Earlier actual gates, casks, consumables, powers, bow and expedition branches
  retain receipts here; reuse original provenance rather than repeating them.

Remaining: general inventory transfer/removal lifetime and effects/UI; higher
Alchemy/resistance and general XP; spawn roots/children, grouped actors,
respawn/all levels; full S5/M15, actor arrow recovery, sound/reaction/view/device
matrices, bow tactics and other death models. Native death state is authoritative:
Skeleton Health+.597 while Dead is valid. Preserve unknown/null historical
passive ownership. Zombie06c2c6 and restoration chest04bb8b retain route gaps;
learned target-SPEL mastery still reads Alteration20 rather than Destruction22
and needs an actual spell case. Chameleon alpha0.75 is approximate. Exterior
missing-material/mesh markers remain adjacent rendering work.
