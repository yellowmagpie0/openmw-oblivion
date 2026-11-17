# Kvatch main-quest course — current handoff

Target: outside Kvatch's main gates → original Savlian encounter → first Oblivion
Gate → original interior progression. Entry and cold interior continuity now
pass; further interior progression and full main-quest acceptance remain open.
Choose subsequent chunks from observed gameplay failures.

**Latest accepted chunk: native TrapUpdate compatibility and actual gate entry.**
The original command0x110c points to a complete success no-op at00977c50.
Recognizing it removes Harrada script066707's unsupported-command abort without
adding damage, state or a save schema. Actual normal/SAN movement and activation
enter original cell01e895/world01d0bc. The original activation sets
`MS48.inOblivion=1` and releases the weather override; both are observed. Five
loaded Harrada instances initialize and run without diagnostics. Own-save cold
processes retain entrance pose, flag, locals, inventory, topics, stage10 and
reaction40. Four primary courses pass; engine2,310 per mode and Python621 pass
with their recorded source provenance. Rendered SAN uses leak0; unit SAN leak1.
[Contract](KVATCH-TRAP-UPDATE-CONTRACT.json),
[independent original handler](KVATCH-TRAP-UPDATE-ORACLE.json),
[verification](KVATCH-TRAP-UPDATE-VERIFICATION.json).

**Latest self-contained feature: scripted Harrada flora/morph animation.**
Native Flora now joins the mechanics animation manager. Named embedded float
tracks bind independently to their matching morph slots and use the existing
sequence clock. Original plant068733 visibly attacks. Four real normal/SAN
attack/save and own-save cold courses pass: normal phase0.096→0.345; SAN
phase0.249→0.498, with no scripted attack replay before resave. Inventory,
quest stage25, Player health and Ilend life state persist. No new save schema.
Normal components3,310/engine2,310/Python621 and SANcomponents3,310 pass.
[Contract](KVATCH-HARRADA-ANIMATION-CONTRACT.json),
[original asset inspection](KVATCH-HARRADA-ANIMATION-ORACLE.json),
[feature checkpoint](KVATCH-HARRADA-ANIMATION-VERIFICATION.json).

**Immediate next action: NEW Windows performance merge requested by the user.**
After this feature commit, fetch latest GitHub `origin/codex/windows-native-dev`
and merge its new work. The earlier53509962 integration does not satisfy this
new request. Full sanitizer engine integration was deliberately deferred to the
combined-source checkpoint; run fresh combined-source engine/component/Python
checks and affected real runtime courses before continuing any content work.
Do not report that deferred gate as passed. Keep all changes local; no push.

Evidence: `build/oblivion-compat/kvatch-harrada`. Final rendered producer04 and
resources03 cover14 relevant source files; class-only01/02 are not animation
acceptance. SANcold01 was kernel-OOM-killed during concurrent large test-file
compilation; cold02 passed alone. Preserve the failure and sequencing records.
Serialize heavy sanitizer rendering and large unit compilation.

Normal active-animation continuation:
`build/oblivion-compat/kvatch-harrada/normal-original-harrada-morph-cold-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `3229196a195cd3fb407a96cb4e9bb87ea8c807a8c381cdbfc0ac7bb18320e873`.
For replaying approach from a healthy earned source, use the road checkpoint
below. Phantom actor contact damage/pushback remains separate: the model uses
bhkSimpleShapePhantom/SPCollisionObject, excluded from the existing multibody
ACTI trap path. Verify original actual contact behavior before implementing it;
never substitute proximity damage. Do not start that chunk before the merge and
combined integration are verified.

**Latest earned route checkpoint (normal baseline only):** original Ilend reply
1778d0 advances MS48 to25, evaluates package1778d1 and saves. A fresh process then
walks the Player>1,000units; Ilend physically moves>500units and closes within500.
No new engine mechanic or SAN acceptance is attributed to these baseline courses.
Continue from:
`build/oblivion-compat/kvatch-ilend/normal-original-ilend-follow-cold-road-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `becc73807be4b5e52f63579085380745c9c12d1ef6d1f2d7fe90e1d265890b93`.
Player health104.54; Ilend alive; original inventory and quest progress are earned.
The Harrada baseline uses this source without stat/quest/target writes. It reaches
87units from the plant and records failed playback every2seconds. A new candidate
must preserve that same route provenance.

Earlier [healing preparation](KVATCH-INTERIOR-ROUTE-PREPARATION.json) used11 normal
Heal Minor Wounds casts (22.7→104.54), preserving inventory, topics and quest state.
The default next-spell replay key is supported;18 focused helper tests pass.
For Ilend UI replay, use `kvatch-ilend/ilend-follow-agreement.json` with controller
menus=true and virtual_gamepad=true. Its explicit INFO checks select09811c →
098154 →1778d0. Do not use absolute-pointer SDL actions, disable controller menus,
or attempt F5 while the forced greeting remains open; retained failures document
those fixture errors. The cold road course is `ilend-cold-follow-road.json`.

Reusable exact rendered producers, loaded-section proofs, standalone verified
archives and all primary/failed cases are under
`build/oblivion-compat/kvatch-trap-update`. Normal entry uses short movement
pulses. The failed first SAN entry timed out at low frame rate; its accepted
retry holds normal forward/run while steering, releases before final aiming,
and meets the same landing/state requirements. Do not repeat the failed timing
recipe or run two rendered games simultaneously.

Earlier accepted dependencies:

- [Weather commands](KVATCH-WEATHER-VERIFICATION.json).
- [Real gate creature births and lifetime](KVATCH-GATE-SPAWN-VERIFICATION.json),
  `58f203eedb`: three genuine timed Scamps with original template/script state.
- [Savlian INFO-added topics](KVATCH-TOPIC-LEARNING-VERIFICATION.json),
  `e3f540969a`: original reply teaches Help and Oblivion Gate; cold selection works.
- [MS48 agreement faction delta](KVATCH-FACTION-REACTION-VERIFICATION.json),
  `2081d3b3a9`: guard→Player reaction20+20=40, reverse0, own-save persistence.
- [NPC Player defense](KVATCH-ALLY-DEFENSE-VERIFICATION.json), `edf3d84627`:
  original guards acquire actual Scamps; Savlian deals physical damage, and cold
  continuation preserves damage/controller/intent without replaying the saved
  hit. Only detected Creature-to-Player threats are accepted, with no full
  crime/alarm or complete encounter-clearing claim. Original live evidence is
  aggregate guard response/count3/deadCount3, without individual killer attribution.

Gate01e8a3/base1778c8/script1778c5 pairs with01e8a5 in cell01e897/world01d0bc.
Authored XTEL landing[1372.577,-1088.240,661.154] settles toZ671.825 on terrain;
portal pose[1375.229,-1281.649,765.794] is not the incoming landing. Savlian's
original Ambush force-greeting already works. Earlier ordinary combat deaths
and fixture failures remain in their ignored evidence directories.

The separate earned-Amulet source and declared Kvatch travel setup are recorded
in the spawn contract/receipt: MQ01stage100, MQ02stage20; MQ03 was not artificially
advanced. One declared Player MoveTo to original marker00cef7 established the
requested location, rather than cross-country travel or full quest prerequisites.
Keep this inventory separate from Chorrol/Vilverin; never use diagnostic
count3/missing-creature saves as legitimate spawn continuations.

[Chorrol throne work](CHORROL-THRONE-SEAT-HANDOFF.md) is complete at4d63f80e20;
Countess work is deferred. The requested Windows integration is already merged:
[receipt](WINDOWS-MERGE-VERIFICATION.json), including unchanged3,310-per-mode
component provenance. Do not repeat that merge or call these slices complete
Kvatch/main-quest acceptance. Preserve earlier durable producer archive anchors.
