# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. **Normal retained expedition completed.** Broader
M13/M15/M16/M18 acceptance remains open; do not merge independent save branches.

Latest accepted chunk: original Mudcrab Swamp Fever098415, selected naturally
from LVSP181d18 by carrier06c92a. Compound DRAT Strength10/Endurance10 now
transfers as one owned disease. Attributes remain base55 and become modified45;
Health/Fatigue/carrying capacity change110/170/275→90/150/225. Cold process load
retains both effects once. Endurance life transitions and contact publication
are atomic, including low-Health and essential cases. The real case also required
fixing native Creature BNAM physics scale: Mudcrab0.5 previously scaled rendering
and reach16 but left its collision body full-size, preventing real bites.
[Contract](SWAMP-FEVER-CONTACT-CONTRACT.json), [original evidence](SWAMP-FEVER-CONTACT-ORACLE.json),
[verification](SWAMP-FEVER-CONTACT-VERIFICATION.json).

Normal/instrumented infection and cold restart pass; fresh normal2284 and complete
instrumented2284 engine inventories pass. Reused unchanged component/codec checks
retain original provenance. Engine sanitizer gates enable leak detection;
rendered courses disable it. Schema101 unchanged. Selection, transfer and VMOD
original evidence is reused with its declared boundaries; no new original launch
or exact RNG/Havok parity claim. Failed geometry/input/fixture attempts and the
interrupted build supervisor remain in the receipt. Scope stays attribute-only
recoverable self DRAT on supported singleton direct LVSP; general disease cure,
UI/statistics and NPC recipients remain open.

Real-contact continuation:
`build/oblivion-compat/swamp-fever-contact/normal-infected-retreat-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `3e3b8ff571d350b04dd73fd7f4cbf6f379b5ba1d7f59e6eb6c499f143f29e693`.
Player alive Health17.951/90, one Swamp Fever owner; carrier06c92a alive Health10
and still hostile nearby. Original race ability and selected source retained.
The healthier precontact checkpointa8519941… lives under
`normal-carrier-approach-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`;
its native actor source was selected naturally after genuine sewer-exitcb89…
without native authority for06c927/06c92a. No seed setters or unknown-owner repair.

Next bounded issue to investigate: held Block during actual carrier contacts
produced victim_action−1 and block_fraction0. Use the original Rusty Iron War Axe
and real Mudcrab to establish whether ordinary input reaches native block state
before choosing an implementation. Do not infer a formula bug from zero blocks.
The current compound-disease chunk is finished; avoid expanding it further.
Evidence root `build/oblivion-compat/swamp-fever-contact`.

Independent retained continuations:

- [Hand-to-Hand use](HANDTOHAND-USE-VERIFICATION.json): original Skeleton06c2ed,
  Hand35/XP0.6 after one3.02-damage punch; hit/reload/miss and Club regression pass.
  `build/oblivion-compat/handtohand-use/normal-completed-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
  SHA256 `a991290dd0727c1ee4be180c1f9337800e92fcb30387305d1784ef05070c5926`.
  Skeleton still alive16.97875. No pending-strike/level-crossing/blocked acceptance.
- [Destruction use](DESTRUCTION-USE-VERIFICATION.json): Flare0a97df against
  Skeleton04bb63 raises5/XP0→6/XP1.90621; saved in-flight impact awards once.
  `build/oblivion-compat/destruction-use/normal-flare-completed-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
  SHA256 `ef3dcbd5cdbd131f6bc6d5ae3c2a39a075166b3f0d6bd7bf1cb56a7fd2646cd2`.
  Health78.215, Skeleton dead with credit; Bonemeal unlooted. Earlier Canosel branch.
- [Nirnroot use](NIRNROOT-USE-VERIFICATION.json):
  `build/oblivion-compat/nirnroot-use/normal-completed-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
  SHA256 `6a4d5b649af28eee89d7caaf01425bea5470049fcf99ac129b028fb8f0be1165`.
  Health110, one Nirnroot owner1/Found1, Seeking Your Roots stage10/counters2,
  Blunt35/XP2, Restoration6/XP1.3062; Skeleton06c2ed dead/empty,17 bodies.
- [Immortal Blood](SKILL-BOOK-READ-VERIFICATION.json):
  `build/oblivion-compat/skill-book/normal-book-repeat-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
  SHA256 `fb073ae38596c6e476dcaf3aae4dad4fd65b394d6f4ae15e849132dd28b2613b`.
  Hand36/XP0, major progress1, one base award, Health83 from campfire approach.

Previous gameplay receipts: [Nirnroot pickup](NIRNROOT-PICKUP-VERIFICATION.json),
[settled mace push](SETTLED-MACE-PUSH-VERIFICATION.json),
[Skeleton death](SKELETON-DEATH-POSE-VERIFICATION.json),
[corpse loot](SKELETON-CORPSE-LOOT-VERIFICATION.json),
[retaliation](SKELETON-RETALIATION-VERIFICATION.json),
[axe](CRYPT-SKELETON-WEAPON-VERIFICATION.json),
[resistances](BONE-RESISTANCES-VERIFICATION.json),
[Blunt](BLUNT-USE-VERIFICATION.json), [Marksman](MARKSMAN-USE-VERIFICATION.json),
[Restoration](RESTORATION-USE-VERIFICATION.json),
[bedroll](BEDROLL-REST-VERIFICATION.json), [Amulet](AMULET-KINGS-EQUIP-VERIFICATION.json).

Remaining: general inventory transfer/removal lifetime and effects/UI; higher
Alchemy/resistance and general XP; spawn roots/children, grouped actors,
respawn/all levels; full S5/M15, arrow recovery, sound/reaction/view/device
matrices, bow tactics and other death models. Native death state is authoritative:
Skeleton Health+.597 while Dead is valid. Preserve unknown/null historical
passive ownership. Zombie06c2c6 and restoration chest04bb8b retain route gaps;
learned target-SPEL mastery still reads Alteration20 rather than Destruction22
and needs an actual spell case. Chameleon alpha0.75 is approximate. Exterior
missing-material/mesh markers remain adjacent rendering work. Healing ingredient
survey stays deferred without a local carried/loose sample.
