# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. **Normal retained expedition completed.** Broader
M13/M15/M16/M18 acceptance remains open. Keep independent save branches separate.

Latest completed bounded chunk: [worn Rusty Iron Cuirass](CUIRASS-DROP-CONTRACT.json),
original ARMO0661c1/CGIronCuirass. Ordinary drag unequips and drops1->0 as native
key8 with its original convex ground body and earned condition19.6576748.
Cold world, Space pickup, a separate cold paperdoll equip and final cold equipped
inventory pass in normal and instrumented games. CGHeavyArmorScript ownership
moves world8->inventory9; its original OnAdd/OnEquip handlers each fire once,
setting MQ01heavyArmor/heavyArmorEquip0/0->1/0->1/1 without replay on reload.
Only stateless OnAdd/OnEquip armor programs admit explicit missing-owner
initialization; existing script owners transfer intact. All other inventory,
27skills/book progress, health, quest stages/counters and33credits survive.
[Oracle](CUIRASS-DROP-ORACLE.json) and [receipt](CUIRASS-DROP-VERIFICATION.json)
separate structural, compiler, semantic and actual runtime evidence. Fresh342
focused/2292 full engine tests per mode/Python618 pass; unchanged3301 component
evidence per mode retains original provenance. Real Torch/Pants and stateful
Nirnroot drop/cold/pickup regressions pass; the latter adapts only historical
schema101 expectations to the current103 migration. Failed development input
and compile attempts remain retained. Broader M15/M18 remain open.
Historical [Torch receipt](TORCH-DROP-VERIFICATION.json) records fresh inactive
LIGH ownership; [Iron Key receipt](IRON-KEY-DROP-VERIFICATION.json) records the
10.2.0.0 mutable-root dependency.

Current **separate Cairn/Wellspring Cave continuation**:
`build/oblivion-compat/cuirass-drop/normal-cuirass-native-inventory-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `56fdd64c3d18190376e0a2d08eb66edbbc27b8f342a00b4de428909ccb516d73`.
Cuirass equipped/condition19.6576748/scriptowner9, fresh Torch3/lifetime1000,
Iron Key1/Lockpick8/Iron Arrow24/Pants1/Bone1, Club equipped, MQ01flags1/1,
allocator10; dropped keys1..8 retired/count0/no bodies. Health70.480003/110,
Endurance55, all27 skills/book history/33 credited deaths preserved. MS39stage0
and original absence of Nirnroot/counter27 retained. Instrumented continuation
is in the receipt.

Healthy shore continuation remains separate:
`build/oblivion-compat/weapon-drop/normal-worn-axe-native-inventory-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `e38b5b86444feac911d7516fc4b00b4907f273fcf30b197ac39010a4629bb305`.
Unequipped Rusty Iron War Axe22.494663, Club59.5000458 equipped, Health110,
Endurance55/no Swamp Fever, Crab0/Rice1, Nirnroot owner4/Found1, MS39stage10/
counters2,33 credited deaths/all earned skills. Allocator7, dropped key6 retired.

Current gaps: stateful-scripted loose armor, bound/enchanted and other stacked
item categories; active/spent Torch fuel and broader
inventory lifetime/effects/UI; unsupported multi-body MISC, nonconvex triangle proxies
and general prismatic
motion; coastal rendering; natural positive Endurance-damage healing gameplay.
Shore Bone drop/world passed, but rock occlusion blocked retrieval: partial evidence.
Read-only coastal probes found original067530 placement/collision intact and a
grounded camera above water; enabling refraction did not resolve the view. Details:
`build/oblivion-compat/coastal-rock/DIAGNOSTIC-RESULT.json`. No coastal fix or old
legacy-object migration claim. Choose the next bounded chunk by genuinely carried
or reached content and the acceptance gap it unblocks.

Reusable evidence:

- [Lockpick stack/convex strip proxies](LOCKPICK-DROP-VERIFICATION.json):
  original8→0→8 and separate1-from8, closed convex one-geometry unit-scale only.
- [Whole-stack ammunition](AMMO-STACK-DROP-VERIFICATION.json),
  [one-arrow split/locked prismatic](IRON-ARROW-DROP-VERIFICATION.json).
- [Original static-surface fired-arrow recovery](IRON-ARROW-RECOVERY-VERIFICATION.json):
  historical ordinary shot/pickup/restart/expiry acceptance; broader S5 remains open.
- [Pants](PANTS-DROP-VERIFICATION.json): three-body/two-hinge physical ownership,
  independent normal/instrumented persistence; earlier cave continuation hashes.
- [Bone](BONE-DROP-VERIFICATION.json), [worn axe](WORN-AXE-DROP-VERIFICATION.json):
  singleton native transfer and the earlier loose-object/terrain mask correction.
- [Crab Meat](CRAB-MEAT-VERIFICATION.json): repair5/no disease cure; separate
  healthy and genuine Swamp Fever continuations. Infected normal branch retains
  Health25.83499/90 and a nearby bandit; respond promptly and never merge branches.
- [Plain Rice](PLAIN-INGREDIENT-DROP-VERIFICATION.json),
  [Nirnroot drop](NIRNROOT-DROP-VERIFICATION.json),
  [Nirnroot pickup](NIRNROOT-PICKUP-VERIFICATION.json): plain/scripted owner transfer.
- [Cairn healing](CAIRN-BOLETE-VERIFICATION.json), [Wine](CHEAP-WINE-VERIFICATION.json),
  [Repair](REPAIR-HAMMER-VERIFICATION.json): independent continuation hashes/state.
- [Containers](VILVERIN-CONTAINER-LOOT-VERIFICATION.json),
  [Skeleton](SKELETON-DEATH-POSE-VERIFICATION.json),
  [mace push](SETTLED-MACE-PUSH-VERIFICATION.json),
  [skills](HANDTOHAND-USE-VERIFICATION.json),
  [Swamp Fever](SWAMP-FEVER-CONTACT-VERIFICATION.json),
  [weapon assets](M15-WEAPON-SCABBARD-VERIFICATION.json).

Broader gaps: Alchemy/resistance/skill progression; spawn roots/children/grouped
actors/respawn; full S5/M15, broader fired-arrow recovery and sound/reaction/view/device
matrices; bow tactics and other death models. Native Dead owns life even at positive
Health; retain unknown historical passive ownership. Zombie06c2c6/restoration
chest04bb8b retain route gaps. Target-SPEL mastery needs a real acquired spell
case (reads Alteration20 rather than Destruction22). Cure Disease lacks a verified
reached source; container loot remains unverified and the nearest found outdoor
Mandrake is70k away. Chameleon alpha0.75 is approximate.

Checkpoint: native singleton ingredient/weapon/MISC/KEYM, articulated clothing,
ammunition quantities and the closed-convex Lockpick stack now have separate
real-content evidence. Key chunk complete; choose the next observed gameplay gap
without closing broader milestones.
