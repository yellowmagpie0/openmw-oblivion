# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. **Normal retained expedition completed.** Broader
milestone matrices remain open; this does not close M13/M15/M16.

Latest accepted implementation: characters push owned released native scenery.
Ordinary movement clears original Vilverin mace0c4986 without resetting,
disabling or teleporting it. Workers collect intents; the serial barrier checks
membership and applies impulses through the owning physics API. Fixed/foreign
owners and legacy movement retain handling; incoming hazards skip voluntary
pushes. Mass80/response0.1s are bounded approximations, not original-certified
force constants. No schema change (99).
[Contract](SETTLED-MACE-PUSH-CONTRACT.json),
[original-game observation](SETTLED-MACE-PUSH-ORACLE.json),
[verification](SETTLED-MACE-PUSH-VERIFICATION.json).

Four accepted normal/sanitized passage and cold-load courses, eight inspected
primary images. Both passages retain Health110, exact inventory/progression,
original mace anchors/body/joint/script identities, and dead/empty Skeleton06c2ed
with17-body owner and one innate grant. Focused262, normal engine2,279/Python612,
and sanitizer engine2,279 pass. Sanitizer inventory is audited across two
processes: the single-process attempt exhausted Bullet's cumulative thread-ID
counter. Engine gates use leak checking; rendered courses disable it. Failed
harness/unit attempts and timing changes are retained in the verification.

Normal outside return: original door016b61 reaches
(52034.625,87420.484375,334.92218), Health110. Loot and Blunt35/XP2.0,
Restoration6/XP1.3062 remain intact; no item/stat/save edits.
Outside save:
`build/oblivion-compat/settled-mace-push/normal-outside-after-push-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `954b4ae564af4cfba777bfb4a703f7ec3c72fe88941f8e38f44ad16bd2f696fb`.
Mace pre-fix reproduction source:
`build/oblivion-compat/expedition-return/normal-outside-door-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `fec79d98e5222b121d406e9d2bc8e8ad3908914f381f9be638d7794c95945aab`.

Next bounded chunk (confirmed real failure): original Nirnroot06f184 near
(53306.80,91007.98,82.94). Ordinary pickup in
`nirnroot-pickup/normal-near-plant-03` gives ingredient04e940 and disables the
plant, but MS39 stays at0, counters stay0, and no ingredient Found instance is
created. Generic harvest omits ingredient OnAdd. Implement item-owned dispatch,
first-pickup quest/counters, retained Found on cold load, and independent counting
for a second original plant. Do not substitute base identity for item identity.
[Failing state audit](../../build/oblivion-compat/nirnroot-pickup/normal-near-plant-03/missing-onadd-audit.json).
Reusable pre-pickup save (primary continuation):
`build/oblivion-compat/nirnroot-pickup/normal-first-plant-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `2a9d328c54ebb8c5da559af0461d08fe5ae35b683d2a5dce578bdf76ce8aa393`.
The first approach stopped at a partial nav endpoint; the next activation was
out of reach atop a rock. Ordinary W0.7s from this checkpoint reaches the plant.
The successful pickup recipe is `nirnroot-pickup/near-plant-03.json`; observers
only read state and steer ordinary input. Second plant candidate06f182 is near
(48425,94541); re-audit its winning records before admission.

Reusable evidence (retain original provenance):

- [Physical Skeleton death](SKELETON-DEATH-POSE-VERIFICATION.json),
  [corpse Take All](SKELETON-CORPSE-LOOT-VERIFICATION.json),
  [retaliation](SKELETON-RETALIATION-VERIFICATION.json),
  [owned axe](CRYPT-SKELETON-WEAPON-VERIFICATION.json),
  [Bone Resistances](BONE-RESISTANCES-VERIFICATION.json).
- [Blunt](BLUNT-USE-VERIFICATION.json),
  [Marksman](MARKSMAN-USE-VERIFICATION.json),
  [Restoration](RESTORATION-USE-VERIFICATION.json),
  [bedroll](BEDROLL-REST-VERIFICATION.json),
  [Amulet item-owned OnEquip](AMULET-KINGS-EQUIP-VERIFICATION.json).
- Earlier actual gates, casks, consumables, powers, bow and expedition branches
  retain their linked receipts in this directory. Reuse those records rather
  than repeating unchanged gates. Original backed-up saves/config/Plugins.txt
  from the mace oracle were restored; no original game is left running.

Remaining acceptance: general effects/UI; spawn roots/children, grouped actors,
respawn/all levels; full S5/M15, actor arrow recovery, general XP,
sound/reaction/view/device matrices, general bow tactics and other death models.
Native life/death is authoritative: raw Skeleton Health+.597 after death is
valid. Do not recreate passive grants for old unknown/null ownership.
Zombie06c2c6 and restoration chest04bb8b remain behind historical route gaps;
learned target-SPEL mastery still reads Alteration20 rather than Destruction22
and needs an actual spell case. Chameleon alpha0.75 is approximate. Exterior
missing-material/mesh markers observed on return remain adjacent rendering work.
