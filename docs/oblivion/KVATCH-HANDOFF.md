# Kvatch main-quest course — current handoff

Target: outside Kvatch's main gates → original Savlian encounter → first Oblivion
Gate → Blood Feast/Rending Halls → original tower ascent. Full Sigil Stone,
gate closure and main-quest acceptance remain open. Choose work from actual
content failures; keep the current bounded deliverable ahead of adjacent gaps.

**Accepted chunk: persistent original corpse flames at Dark Salvation entry.**
Normal/SAN ordinary ramp/Space through Blood Feast01d580 and Ilend arrival pass.
Original FlamingCorpseSCRIPT0c47ae keeps myself null/init1/lucky90 and issues one
PMS on corpse06bc4e. Authored fire particles emit from2,812 actual mesh triangles.
Fresh own-save processes restore one live emitter and visibly animated flames
without another PMS, preserving health91.168, Player/Ilend life, both earned
Dremora kills, inventory/ownership, quests/topics/reactions and codec106.
[Playback verification](KVATCH-FLAMING-CORPSE-PLAYBACK-VERIFICATION.json),
[original expectations](KVATCH-FLAMING-CORPSE-ORACLE.json),
[typed EFSH dependency](KVATCH-EFFECT-SHADER-DATA-VERIFICATION.json).

Full engine2,324 passes normal/SAN; SAN exact union2072+84+84+84. Components3,319
in each build and Python623 pass/reuse with exact test-only source proof. Six new
renderer/save-validation checks pass. A flaky pool-slot clock assertion was fixed
without changing production behavior; its failed run is retained. Unit leak1,
rendered leak0. Immutable producer archives/loaded-section proofs are recorded.
Limits: particle-only rigid surfaces; membrane and rig/morph effects rejected.
Cosmetic density/RNG and individual cold particles approximate native appearance.
Static upstairs cameras are declared visual diagnostics; Player remains at the
entrance. Upper-corridor traversal and full gate/quest completion remain open.

**Next action:** resume ordinary movement through original Corridors of Dark
Salvation from the earned entrance; choose the next discrete implementation
from an actual course failure. Do not repeat resolved native/data/shader checks.
Evidence/controller state: `build/oblivion-compat/kvatch-flaming-corpse/playback`.

Primary normal continuation (no static-camera staging):
`build/oblivion-compat/kvatch-flaming-corpse/playback/normal-entry-dev-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `a10212f11a65f9b33ee395d96e545f4f68b8a481857bd12895207d585ee1475f`.
Player[1521.658936,-1574.094238,1514.027344], yaw3.112392/pitch0;
Ilend[1386.658936,-1574.094238,1514.074829], alive on follow1778d1.
Cell085e4f, last transition01d580, shader0852fe indefinite on corpse06bc4e.
SAN entry source/own-save cold and earlier4088 pre-entry fallback are linked in
the receipt/contract. Keep failed entry/case attempts out of primary continuations.

Course progression and earlier accepted fixes:

- [Rending Halls defense and upper exit preparation](KVATCH-RENDING-HALLS-UPPER-EXIT-PREPARATION.json).
- [Physical Hall door opening and saved animation continuation](KVATCH-RENDING-HALLS-DOOR-PREPARATION.json).
- [Unlinked Hall path-point commands](KVATCH-UNLINKED-DOOR-PATHPOINTS-VERIFICATION.json).
- [Animated Sigil base collision offset](KVATCH-SIGIL-BASE-COLLISION-VERIFICATION.json).
- [Occupied follower door arrival](KVATCH-FOLLOWER-DOOR-ARRIVAL-VERIFICATION.json).
- [Harrada phantom contacts](KVATCH-HARRADA-CONTACT-VERIFICATION.json) and [morph playback](KVATCH-HARRADA-ANIMATION-VERIFICATION.json).
- [Native TrapUpdate and real gate entry](KVATCH-TRAP-UPDATE-VERIFICATION.json), [weather](KVATCH-WEATHER-VERIFICATION.json), [gate births](KVATCH-GATE-SPAWN-VERIFICATION.json).
- [Savlian topics](KVATCH-TOPIC-LEARNING-VERIFICATION.json), [MS48 faction delta](KVATCH-FACTION-REACTION-VERIFICATION.json), [guard defense](KVATCH-ALLY-DEFENSE-VERIFICATION.json).
- [Earlier healing/route source](KVATCH-INTERIOR-ROUTE-PREPARATION.json).

The course inherits an earned-Amulet source: MQ01stage100, MQ02stage20; MQ03 was
not artificially advanced. One declared Player MoveTo to original marker00cef7
staged the requested Kvatch location. Cross-country travel and full prerequisites
are not claimed. Ilend UI uses controller menus/virtual gamepad; preserve the
accepted UI recipe and avoid saving during a forced greeting.

**Windows performance integration is complete.**
Upstream `c8fdfc22a8` merged as `d4a1665868`, verification `9886ce4475`.
[Merge receipt](WINDOWS-PERFORMANCE-MERGE-VERIFICATION.json),
[Windows timing provenance](KVATCH-PERFORMANCE-HANDOFF.md).
Keep all work local; never push without request. Preserve prior producer archives.

Run one rendered game at a time; serialize heavy SAN rendering and compilation.
Use actual actor bounds and physical checkpoints when following paths; keep the
final path point active until the checkpoint. Navmesh endpoint tolerance and
record-root distance can differ. The old foreground wrapper interruption is
retained; detached controllers completed all current checks. Poll actual process
and exit evidence before restarting work. [Chorrol throne](CHORROL-THRONE-SEAT-HANDOFF.md)
is complete; Countess work remains deferred.
