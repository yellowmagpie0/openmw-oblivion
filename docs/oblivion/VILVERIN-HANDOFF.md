# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. **Normal retained expedition completed.** Broader
M13/M15/M16/M18 acceptance remains open. Keep independent save branches separate.

Latest bounded chunk: [worn Rusty Iron War Axe drop](WORN-AXE-DROP-CONTRACT.json).
The genuine carried axe090613 previously became a legacy world object: a cold
re-save failed because its standing contact had no native identity. Native
ordinary inventory weapon singleton admission now preserves the reference,
exact worn condition22.494663, ownership and authored loose-body state. Actual
outdoor testing also exposed missing loose-object/terrain collision; heightfields
now admit loose objects. No suppression of standing-contact validation.
[Oracle](WORN-AXE-DROP-ORACLE.json) separates original full-condition ordinary
pickup/drop behavior from genuine fork worn-item preservation.
[Receipt](WORN-AXE-DROP-VERIFICATION.json) records baseline failures, normal and
instrumented drop → cold world re-save → ordinary pickup/save → cold inventory
courses, regression gates and independent asset provenance.

Healthy continuation, separate from Cairn and infected branches:
`build/oblivion-compat/weapon-drop/normal-worn-axe-native-inventory-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `e38b5b86444feac911d7516fc4b00b4907f273fcf30b197ac39010a4629bb305`.
One unequipped worn Rusty Iron War Axe, equipped Club59.5000458, Health110,
Endurance55, no Swamp Fever, Crab0/Rice1, Nirnroot owner4/Found1, MS39stage10/
counters2,33 credited deaths and all earned skills retained. Allocator7;
retired dropped key6 remains deleted/count0. Schema102 unchanged.
Instrumented final inventory SHA256:
`b590bc49fcb92548c3bfdd4d7bf612710a483f5e523830ed6a11ad7366faedb4`.
Normal2292/full disjoint instrumented2292 engine, focused138 each and Python618
pass. Unchanged components reuse the Crab receipt with original provenance.

Remaining current acceptance gaps: equipped/scripted/bound/enchanted/stacked
weapon inventory drops; broader inventory lifetime/effects/UI; coastal rendering/
rock clipping (terrain collision correction does not fix rendering); natural
positive Endurance-damage healing gameplay. Choose the next bounded chunk by
observed unsupported behavior in genuinely carried or reached content.

Reusable evidence and independent continuations:

- [Crab Meat](CRAB-MEAT-VERIFICATION.json): original repair5/no disease cure,
  healthy and genuine Swamp Fever pending/completed cold-load courses; accepted
  component3301/native sanitizer1122, engine2292 both modes, Python618 provenance.
  Its receipt retains separate healthy and infected save hashes. Infected normal
  branch has Health25.83499/90 and a nearby bandit; respond promptly. Never merge
  that branch with the healthy continuation.
- [Plain Rice](PLAIN-INGREDIENT-DROP-VERIFICATION.json),
  [Nirnroot drop](NIRNROOT-DROP-VERIFICATION.json) and
  [Nirnroot pickup](NIRNROOT-PICKUP-VERIFICATION.json): scriptless identity and
  scripted owner-transfer round trips with retained original failures. Their
  recipes, earlier source saves and component/Python gates retain their original
  provenance; new weapon receipt includes a fresh Rice dispatch regression.
- [Cairn healing](CAIRN-BOLETE-VERIFICATION.json),
  [Wine](CHEAP-WINE-VERIFICATION.json), [Repair](REPAIR-HAMMER-VERIFICATION.json):
  separate continuation hashes and exact retained progression are in each receipt.
- [Vilverin containers](VILVERIN-CONTAINER-LOOT-VERIFICATION.json),
  [Skeleton](SKELETON-DEATH-POSE-VERIFICATION.json),
  [loose mace push](SETTLED-MACE-PUSH-VERIFICATION.json),
  [skills](HANDTOHAND-USE-VERIFICATION.json),
  [Swamp Fever](SWAMP-FEVER-CONTACT-VERIFICATION.json) and
  [weapon assets](M15-WEAPON-SCABBARD-VERIFICATION.json).

Broader gaps: Alchemy/resistance/skill progression; spawn roots/children/grouped
actors/respawn; full S5/M15, arrow recovery and sound/reaction/view/device matrices;
bow tactics and other death models. Native Dead state owns life even at positive
Health; retain unknown historical passive ownership. Zombie06c2c6 and restoration
chest04bb8b retain route gaps. Learned target-SPEL mastery needs a real acquired
spell case (reads Alteration20 rather than Destruction22). Cure Disease remains
without a verified reached source; container loot is unverified and nearest found
outdoor Mandrake is70k away. Chameleon alpha0.75 is approximate.

Checkpoint: original ingredient use, scripted/plain identity round trips, and
ordinary worn weapon transfer now have distinct real-content evidence. Broad
milestones remain open. Current chunk complete; next action is select another
observed real-content failure, preserving the explicit gaps above.
