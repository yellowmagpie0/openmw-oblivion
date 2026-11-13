# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. **Normal retained expedition completed.** Broader
M13/M15/M16/M18 acceptance remains open.

Latest accepted chunk: harvested singleton ingredients dispatch original OnAdd
with item-owned script identity. Ordinary picks of original Nirnroot06f184 and
06f182 start Seeking Your Roots at10 and independently count both plants.
Each retained ingredient owns Found1; quest counters and misc-stat27 reach2.
Inventory normalization now preserves these owners rather than merging them
into an invalid scripted count-2 stack. No schema change (99).
[Contract](NIRNROOT-PICKUP-CONTRACT.json),
[original record audit](NIRNROOT-PICKUP-ORACLE.json),
[verification](NIRNROOT-PICKUP-VERIFICATION.json).

Seven accepted pickup/reload courses, ten primary images plus two Amulet
regression images. Normal and sanitizer reloads retain locals/counters without
OnAdd replay. Exact loot/progression, Health110, dead/empty Skeleton06c2ed with
17-body owner and one innate grant survive. New merge regression fails before
the fix and passes after; focused64 engine/17 inventory cases, full3,291 normal
components,1,112 native sanitizer components,2,279 engine tests in each build,
and Python612 pass. Sanitizer engine inventory is audited across two processes
because Bullet has a cumulative thread-ID limit. Engine gates check leaks;
component/rendered sanitizer courses disable leak checking. Reused evidence
retains its original source/binary provenance in the verification.

Primary continuation:
`build/oblivion-compat/nirnroot-pickup/normal-second-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `1408e448b4b04a930fc2e2ccb208d2e549f0f04f9b72be93bc157f1ac7d72335`.
Player is beside the second plant, with two owned Nirnroots, Blunt35/XP2.0,
Restoration6/XP1.3062 and earned Bonemeal/Steel War Axe retained. No item/stat/
position/save edits. Failed navigation, rock-top aim, shoreline and second-save
attempts remain explicit in the receipt. Ordinary X11 F5 recovered actual swim
progress; its parent automatic course remains failed. The later valid
pre-activation checkpoint is independently audited before reuse.

Next action: test ordinary inventory use of an acquired Nirnroot. Original
INGR04e940 has ENIT flags3 and authored DRHE/DRFA/DRAT/DRAT effects; the current
ingredient consumer admits fatigue profiles. Establish the actual missing
behavior before implementation. Keep script ownership and save correctness
when an item leaves inventory; generic transfer/removal is not yet admitted.

Previous accepted implementation: characters push owned released native
scenery. Ordinary movement clears Vilverin mace0c4986 with fixed anchors and
8-body/7-joint ownership retained through normal/sanitizer passage and cold
load. Mass80/response0.1s are approximations, not original force constants.
[Mace verification](SETTLED-MACE-PUSH-VERIFICATION.json).
Original outside return save954b4a… and pre-fix reproduction sourcefec79d…
retain their exact paths/hashes in that receipt.

Reusable evidence (retain original provenance):

- [Physical Skeleton death](SKELETON-DEATH-POSE-VERIFICATION.json),
  [corpse Take All](SKELETON-CORPSE-LOOT-VERIFICATION.json),
  [retaliation](SKELETON-RETALIATION-VERIFICATION.json),
  [owned axe](CRYPT-SKELETON-WEAPON-VERIFICATION.json),
  [Bone Resistances](BONE-RESISTANCES-VERIFICATION.json).
- [Blunt](BLUNT-USE-VERIFICATION.json), [Marksman](MARKSMAN-USE-VERIFICATION.json),
  [Restoration](RESTORATION-USE-VERIFICATION.json),
  [bedroll](BEDROLL-REST-VERIFICATION.json),
  [Amulet OnEquip](AMULET-KINGS-EQUIP-VERIFICATION.json).
- Earlier actual gates, casks, consumables, powers, bow and expedition branches
  retain their receipts in this directory. Reuse them rather than repeating
  unchanged gates. Original mace-oracle saves/config/Plugins.txt were restored;
  no original game is left running.

Remaining acceptance: general effects/UI; spawn roots/children, grouped actors,
respawn/all levels; full S5/M15, actor arrow recovery, general XP,
sound/reaction/view/device matrices, bow tactics and other death models.
Native death state is authoritative: Skeleton Health+.597 while Dead is valid.
Do not recreate passive grants for old unknown/null ownership.
Zombie06c2c6 and restoration chest04bb8b retain historical route gaps;
learned target-SPEL mastery still reads Alteration20 rather than Destruction22
and needs an actual spell case. Chameleon alpha0.75 is approximate. Exterior
missing-material/mesh markers remain adjacent rendering work.
