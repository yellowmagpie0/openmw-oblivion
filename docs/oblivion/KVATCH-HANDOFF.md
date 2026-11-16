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

**Next action: approach Ilend Vonius through normal movement.**
Healthy earned-state source:
`build/oblivion-compat/kvatch-ilend/normal-earned-healing-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `83961f3f2c4a43c52f3947085f2e7bc31725ee431a02a96a40b8dbc6414b2294`.
Eleven ordinary Heal Minor Wounds casts raised health22.7→104.54, with inventory,
topics, reactions and quest stages unchanged. The replay helper now supports the
default next-spell key;18 focused helper tests pass. This is route preparation,
not new engine spell-rule acceptance.
[Preparation evidence](KVATCH-INTERIOR-ROUTE-PREPARATION.json).
Ilend098138/base09812f was alive near[-311,2135,53] in the entry checkpoint;
reinspect his current state during approach. Private next course:
`kvatch-ilend/ilend-approach.json`. It stops near him or at an observed modal.
Use actual movement/dialogue, without health/quest/target writes. Harrada attack
damage and further encounters remain open.

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
