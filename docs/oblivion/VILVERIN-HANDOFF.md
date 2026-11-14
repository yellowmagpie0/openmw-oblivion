# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. **Normal retained expedition completed.** Broader
M13/M15/M16/M18 acceptance remains open. Keep independent save branches separate.

Latest accepted chunk: [original scripted Nirnroot drop/reacquisition](NIRNROOT-DROP-CONTRACT.json).
Normal and instrumented UI drop → cold world load → Space pickup → cold inventory
load pass. One item/Found1, exact metadata, MS39 stage10/counters2, Health110,
all earned skills/passives and33 credited deaths survive. Existing schema101
carries the typed world reference and transferred locals. Original UI preserves
Found1; command-drop resets it and is outside this scope. The
[oracle](NIRNROOT-DROP-ORACLE.json) and [receipt](NIRNROOT-DROP-VERIFICATION.json)
retain the failed legacy save, GUI-wrapper wiring failure, input attempts and
verifier corrections. Normal2291 engine/Python617 and full instrumented2291
engine pass; final lifetime guards have12 focused cases in both builds plus
fresh real drops. The final change checks inventory ownership before borrowing
item data; earlier cold-chain evidence retains its original binary provenance.
No generic stack/container/scripted-item, dynamic bounce or full M16/M18 claim.

Retained full-Health continuation, separate from the Cairn branch:
`build/oblivion-compat/nirnroot-drop/normal-inventory-cold-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `c05743f6dcbc781de8c6d8802fb61631d962c28276a0ce0a001e43593979fa07`.
One Nirnroot/owner4/Found1, MS39 counters2, Health110, Alchemy5/XP0.5,
Athletics30/XP17.009033, Acrobatics5/XP0.6, Hand35 and33 credited deaths.
Instrumented final inventory save has SHA256
`32aed4e7b0fc7c3588eb6b381ef49e20fd2c9ff1fbde71df95c1678ace53e1e8`.
Final guard-check drops are separate offshoots, not replacement continuations.
Coastal staging shows existing terrain/rock clipping; native cold loads are
expensive under instrumentation. Record these adjacent gaps for later.

Current chunk complete. Next action: choose another already carried/reached
original item that exposes a missing acceptance behavior, establish its baseline,
then implement that bounded behavior without merging independent save branches.

Separate Cairn continuation:
`build/oblivion-compat/cairn-bolete/normal-original-cairn-complete-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `0330a7448527955ca09680cb90ab3d785f8c091573a11d51004bea8cdd40fa6d`.
Wellspring Cave, Health70.48/110, cap0, Alchemy5/XP0.5, Athletics31/XP8.661195,
Acrobatics5/XP0, Hand36/book major progress2 and33 credited deaths. Original cap
heals1 Health/s for3s; normal/instrumented partial and completed cold loads pass.
[Contract](CAIRN-BOLETE-CONTRACT.json), [oracle](CAIRN-BOLETE-ORACLE.json),
[receipt](CAIRN-BOLETE-VERIFICATION.json) retain acquisition, genuine damage,
failed plants/staging and original3300 component/2289 engine/Python617 provenance.

Separate full-Health Wine continuation:
`build/oblivion-compat/cheap-wine/normal-complete-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `10e7ce315370f5eb984e4c9c33fb61f332eae0f93f8c23eacb87c9060cd3c527`.
Health110, Wine0/no pending owner, Athletics30/XP17.009033, Acrobatics5/XP0.6,
Alchemy5/XP0.5, one scripted Nirnroot/Found1, MS39stage10/counters2.
[Wine receipt](CHEAP-WINE-VERIFICATION.json) covers original mixed attributes,
Magicka/Fatigue derivation, instant Fatigue10, natural120s expiry/cold reloads and
real Mead regression. Its normal/instrumented2286 engine/Python616 evidence has
its own original provenance; component evidence was reused from Repair.

Separate Repair continuation:
`build/oblivion-compat/repair-hammer/normal-restart-06/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `3e0a7cecdfc70acbd758527f8e4f2f43ac35147c71369d853502599ad87dc258`.
Health80, Armorer5/XP1.5, Rusty Cuirass52/250, Hammer1, source chest04931b Hammer0.
Instrumented branch naturally broke its hammer; do not merge. Normal3298
components/2284 engine/616 Python and instrumented1119 native components/2284
engine pass. [Repair receipt](REPAIR-HAMMER-VERIFICATION.json) retains the original
rules, quest-flag checks, branch saves and tool-roll evidence. Expert125%,
enchanted/stacked/runtime-script-owner repair remain open.

Accepted historical evidence, with original sources and independent branch saves:

- [Alchemy](ALCHEMY-USE-VERIFICATION.json), [Acrobatics](ACROBATICS-USE-VERIFICATION.json),
  [Athletics](ATHLETICS-USE-VERIFICATION.json), [Blade](BLADE-USE-VERIFICATION.json),
  [taken Claymore focus](TAKEN-CLAYMORE-FOCUS-VERIFICATION.json).
- [Armor](ARMOR-USE-VERIFICATION.json), [Block](BLOCK-USE-VERIFICATION.json),
  [Swamp Fever](SWAMP-FEVER-CONTACT-VERIFICATION.json),
  [Hand-to-Hand](HANDTOHAND-USE-VERIFICATION.json),
  [Destruction](DESTRUCTION-USE-VERIFICATION.json).
- [Nirnroot use](NIRNROOT-USE-VERIFICATION.json), [pickup](NIRNROOT-PICKUP-VERIFICATION.json),
  [Immortal Blood](SKILL-BOOK-READ-VERIFICATION.json),
  [settled mace push](SETTLED-MACE-PUSH-VERIFICATION.json),
  [Skeleton death](SKELETON-DEATH-POSE-VERIFICATION.json),
  [corpse loot](SKELETON-CORPSE-LOOT-VERIFICATION.json),
  [retaliation](SKELETON-RETALIATION-VERIFICATION.json),
  [axe](CRYPT-SKELETON-WEAPON-VERIFICATION.json), [resistances](BONE-RESISTANCES-VERIFICATION.json).
- [Blunt](BLUNT-USE-VERIFICATION.json), [Marksman](MARKSMAN-USE-VERIFICATION.json),
  [Restoration](RESTORATION-USE-VERIFICATION.json),
  [bedroll](BEDROLL-REST-VERIFICATION.json), [Amulet](AMULET-KINGS-EQUIP-VERIFICATION.json).

Remaining acceptance gaps: general inventory transfer/removal lifetime and
effects/UI; broader Alchemy, resistance and skill progression; spawn roots/children,
grouped actors, respawn/all levels; full S5/M15, arrow recovery,
sound/reaction/view/device matrices, bow tactics and other death models. Native
death state is authoritative; positive Health on a Dead actor is valid. Preserve
unknown/null historical passive ownership. Zombie06c2c6 and restoration chest04bb8b
retain route gaps. Learned target-SPEL mastery still reads Alteration20 rather
than Destruction22 and needs an actual acquired spell case. Chameleon alpha0.75
is approximate. Cure Disease remains deferred: no carried/directly placed Vilverin
source found, container loot unverified, nearest found outdoor Mandrake70k away.

Checkpoint: bounded progression, potion ownership and first-effect ingredient
healing gaps are shrinking; broader milestone gates remain open. The Cairn healing and
Nirnroot round trips are complete. Choose the next behavior from already carried/
reached content and record adjacent rendering/navigation issues for later.
