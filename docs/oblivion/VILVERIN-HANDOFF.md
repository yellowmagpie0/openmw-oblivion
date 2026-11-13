# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. **Normal retained expedition completed.** Broader
M13/M15/M16/M18 acceptance remains open; do not merge independent save branches.

Latest accepted chunk: native Acrobatics primary use on a real grounded jump
at original Vilverin entrance066ac8. One jump rises65.918 units, spends30 Fatigue,
lands alive Health110 and now earns0.3 XP rather than0. Normal/instrumented jump,
idle and cold-load courses pass; idle/cold add0. Existing Athletics4.066545,
other26 skills and inventory/script ownership persist. Normal2284/instrumented2284
engine inventories and focused9 each pass. [Contract](ACROBATICS-USE-CONTRACT.json),
[oracle](ACROBATICS-USE-ORACLE.json), [verification](ACROBATICS-USE-VERIFICATION.json).
Physics eligibility uses captured frame movement: a second consuming velocity()
read rejected actual jumps despite passing focused tests; that failure is retained.
Damaged falls, rejected-request fatigue, steep slopes and midair saves remain open.

Acrobatics continuation (healthy retained expedition at entrance):
`build/oblivion-compat/acrobatics-use/normal-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `603acf165c4ce24977990c727a4039b8b06c1e6ee94ed0cf361d96a936f32443`.
Player Health110, Acrobatics5/XP0.3, Athletics30/XP4.066545; earned Bonemeal1,
Lettuce1/Rice1, original loot and Nirnroot ownership retained.

Previous accepted chunk: native Athletics on the original inlet crossing to
Vilverin entrance066ac8. Winning SKIL03e rates0.03 run/0.04 swim now grant frame-
scaled experience: normal27.121/81.323 seconds→4.066545XP; instrumented42.496/
73.870→4.229675XP. Both arrive alive Health110, Athletics base30 and all other
skills/inventory retained. Grounded idle/walking/cold load add0 and retain earned
progress exactly; final instrumented walking moves36units. Raw PLAY skill8 owns
fractional progress, with no new unsaved timer or schema. [Contract](ATHLETICS-USE-CONTRACT.json),
[oracle](ATHLETICS-USE-ORACLE.json), [verification](ATHLETICS-USE-VERIFICATION.json).
Normal2284/instrumented2284 engine inventories, focused9 each and full616 Python
checks pass. The necessary replay lookup64KiB→1MiB fixes original exterior AI
log bursts. Final observers latch arrival, emit neutral terminal steering and wait
for measured walking distance. Retained failures and fixture variants are recorded.
No exact original movement-caller, level-crossing/cap or full movement-policy claim.

Athletics continuation (healthy retained Nirnroot-use branch at entrance):
`build/oblivion-compat/athletics-use/normal-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `b49b0a431aadd4d34b677c6e0460e17b25cbb2ab668da457dc8ca2e478f9e2e0`.
Player Health110, Athletics30/XP4.066545, original expedition loot and one remaining
Nirnroot owner/Found1 retained. Distinct from first-zone Blade/focus branches.

Previous accepted chunk: original taken Iron Claymore0c56f9 no longer receives a
zero-count world label after cold load. Oblivion fallback focus now excludes
deleted references while retaining content scripting accessibility. Before pickup,
the same original still shows Iron Claymore with one scene owner; after pickup it
is absent from the scene and owned once. Normal/instrumented taken, untaken and
restart courses pass;23 affected engine tests pass per build. Full world HUD
captures are reviewed separately from state audits. [Contract](TAKEN-CLAYMORE-FOCUS-CONTRACT.json),
[verification](TAKEN-CLAYMORE-FOCUS-VERIFICATION.json). No resurrected model or
duplicate transfer was established. Broad focus cone/occlusion remains open.

Focus continuation (earlier first-zone pickup branch, axe still equipped):
`build/oblivion-compat/taken-claymore-focus/normal-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `d4ecc9d1bfc7e4d3fcef35499ee2bd006602cebbc2eb8c28f432049ff3071fb6`.
Original claymore owned once/deleted, Player alive; separate from Blade combat.

Previous accepted chunk: ordinary Blade use with original loose Iron Claymore
0c56f9/base01c6cd, acquired and equipped normally in Vilverin. One4.325-damage
hit on Bandit Bowman04bb73 now grants Blade35/XP0.5 instead of0. Normal and
instrumented hit/miss/cold-load courses pass; miss grants no use/wear, cold load
retains0.5 without replay. Original winning SKIL03f and dispatcher18cases establish
primary0.5 with declared boundaries. Normal2284 and full instrumented2284 engine
tests pass. Blade level crossing, power/zero/immune/fully blocked contacts remain
open. [Contract](BLADE-USE-CONTRACT.json), [oracle](BLADE-USE-ORACLE.json),
[verification](BLADE-USE-VERIFICATION.json). Actual incoming dagger hits legitimately
advance Heavy Armor; do not erase that progress.

Blade continuation (independent first-zone branch):
`build/oblivion-compat/blade-use/normal-blade-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `c0424880f85c83b54ad6f857cb252446487fc9d260acb90f443ee2ae50871909`.
Player alive Health23.825, Blade35/XP0.5, Heavy41/XP6.78445; bandit alive9.675,
claymore equipped once/condition167.28. Source deleted/taken/count0. Normal input
and observer preparation failures are retained in the verification receipt.

Previous accepted chunk: Heavy Armor use on original Rusty Iron body armor;
unblocked hits grant1.25 per selected body hit, weapon-blocked hits grant0 while
retaining Block use. Normal/instrumented hit/block/cold-load courses and engine
inventories pass. [Verification](ARMOR-USE-VERIFICATION.json). Light Armor,
projectiles and level crossing remain outside actual acceptance.

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

Next chunk: native Alchemy secondary use from genuinely looted Bonemeal01ebff,
using the healthy Acrobatics continuation. Existing successful ingestion consumes
one item and applies supported Fatigue damage but grants no XP. Original SKIL044
has primary5/secondary0.5; original ingredient caller passes19/usage1 only for
successful non-food use. Food bit2 (Lettuce flags3) bypasses XP; use actual carried
Lettuce as the negative control. Ignored original caller24cases and dispatcher30
pass with explicit boundaries; no implementation or gameplay acceptance yet.
Evidence: `build/oblivion-compat/alchemy-use`. Cure Disease remains deferred:
no carried/directly placed Vilverin source found; container loot unverified,
nearest outdoor Mandrake found70k away.

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
