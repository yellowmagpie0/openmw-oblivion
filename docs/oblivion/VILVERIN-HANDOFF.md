# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. **Normal retained expedition completed.** Broader
M13/M15/M16/M18 acceptance remains open; do not merge independent save branches.

Latest accepted chunk: ordinary armor skill use on original Rusty Iron Cuirass
0661c1/Greaves0661c3. Unblocked bites now grant1.25 Heavy XP per selected body
hit: normal3hits→3.75, instrumented2hits→2.5, instead of baseline0. Cold loads
retain earned use without replay; weapon-blocked hits still wear armor but grant
no armor XP and retain their Block1.25. Heavy base40, other skill bases/counters,
original inventory/ability/source ownership and schema101 remain intact.
[Contract](ARMOR-USE-CONTRACT.json), [oracle](ARMOR-USE-ORACLE.json),
[verification](ARMOR-USE-VERIFICATION.json). Normal2284 and instrumented2284 engine
inventories pass; six actual courses plus reused real baseline, seven primary
captures reviewed. Original armor-tail16cases plus Heavy/Light dispatcher18cases
each establish the rule with declared boundaries. Light Armor/projectiles and
level crossing remain outside actual acceptance.

Armor continuation (disease-free shore branch):
`build/oblivion-compat/armor-use/normal-armor-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `e2cc058c132543438caa236715582808b29e0d80427011562085fa12c91af097`.
Health61.231/110, Heavy40/XP3.75, Block30/XP0; Mudcrab06c927 alive8.69796,
known-none and WABR retained. Separate from the older Block and infected branches.

Previous accepted chunk: ordinary melee Block experience using original Mudcrab
06c927 and Rusty Iron War Axe090613. Two actual15%-absorbed bites now grant
2.5XP (1.25 each) instead of0, with identical damage/wear. Native Block30 remains
base30; other skills, advancement counters and original ability/source ownership
are unchanged. Held-block cold load retains2.5 without replay; unblocked bites
and held-only controls grant0. Normal and instrumented four-case courses pass;
normal2284 and full audited instrumented2284 engine inventories pass. Original
caller36cases and use-dispatch18cases independently establish the award rule.
[Contract](BLOCK-USE-CONTRACT.json), [oracle](BLOCK-USE-ORACLE.json),
[verification](BLOCK-USE-VERIFICATION.json). Projectile Block XP, level crossing,
shield and zero-absorption gameplay remain outside this bounded acceptance.

Block continuation (earlier disease-free shore branch):
`build/oblivion-compat/block-use/normal-block-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `c3510f8b3600bd229a3fca2ea6289659ee5e590df0a625ea63bc5f49619b333d`.
Player alive Health71.319/110, Block30/XP2.5, Mudcrab06c927 still alive8.69796,
known-none source retained. This is distinct from the infected carrier branch.

Previous accepted chunk: original Mudcrab Swamp Fever098415, selected naturally
by06c92a from LVSP181d18. One disease owns ordered Strength10/Endurance10 drains:
base55→modified45; Health/Fatigue/capacity110/170/275→90/150/225. Cold load keeps
both once. The real case also required applying BNAM0.5 to the physical body,
which previously remained full-size and prevented bites within scaled reach16.
[Contract](SWAMP-FEVER-CONTACT-CONTRACT.json), [original evidence](SWAMP-FEVER-CONTACT-ORACLE.json),
[verification](SWAMP-FEVER-CONTACT-VERIFICATION.json). Attribute-only recoverable
self DRAT on supported singleton LVSP; general cure/UI/statistics and NPC
recipients remain open. Schema101 unchanged. Original evidence retains declared
boundaries; no new original launch or exact RNG/Havok claim. Rendered sanitizer
courses disable leak detection; engine gates enable it. Unchanged component and
codec checks retain their original provenance. Retained failures are in receipts.

Real-contact continuation:
`build/oblivion-compat/swamp-fever-contact/normal-infected-retreat-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `3e3b8ff571d350b04dd73fd7f4cbf6f379b5ba1d7f59e6eb6c499f143f29e693`.
Player alive Health17.951/90, one Swamp Fever owner; carrier06c92a alive Health10
and still hostile nearby. Original race ability and selected source retained.
The healthier precontact checkpointa8519941… lives under
`normal-carrier-approach-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`;
its native actor source was selected naturally after genuine sewer-exitcb89…
without native authority for06c927/06c92a. No seed setters or unknown-owner repair.

Next action: audit the carried Blade weapons and their currently missing contact
use, then choose a real damaging-hit case. Armor use is finished; avoid expanding
it into projectile or mixed-armor work without a selected real-content case.
Evidence roots `build/oblivion-compat/armor-use` and `build/oblivion-compat/block-use`.

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
