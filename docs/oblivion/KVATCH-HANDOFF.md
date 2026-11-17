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

**Current chunk: let an idle follower yield to its followed Player on the bridge.**
Ordinary west exit085fa5 now reaches original world01d0bc, physical cell021072
(grid0,3), with Ilend safely following. New real bridge crossing stalls at
Player[1260.15,15542.00,4388.96] against Ilend[1271.46,15484.54,4390.02].
Ilend peacefully waits on Follow1778d1 targeting Player; the collision loop
unconditionally excludes that target even when idle yielding is admitted.
[Bounded contract](KVATCH-BRIDGE-FOLLOWER-YIELD-CONTRACT.json).
Accepted: existing idle-yield policy now considers the followed target.
Normal/SAN ordinary bridge passage and own-save fresh-process continuation pass;
each advances over500 units with both lives/health and Follow1778d1 preserved.
Player remains on bridge; earned kills/inventory/quests/topics/reactions and
inactive corpse shader state persist. Full engine2,324 in each build and Python623
pass. [Verification](KVATCH-BRIDGE-FOLLOWER-YIELD-VERIFICATION.json).
Next required action: freshly fetch/merge Windows branch after this local chunk,
verify integration, then continue bridge toward Reapers Sprawl. Primary normal
continuation is `build/oblivion-compat/kvatch-bridge-yield/normal-cold-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `471a051ed339039f33e8d253a2129b0157adbad3f61c3196a559f576eeaea234`.
Player[2026.40,14842.78,4373.65]; Ilend[1839.58,15004.74,4373.65].
Remaining: rest of bridge, guardian/key encounter, locked hall, tower/Sigil/gate.

Bridge source (actual input/codec/live/earned-state audit passed):
`build/oblivion-compat/kvatch-dark-salvation/normal-route-survey-05/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `0ffb72a796bb60c9204e529985958e3355c1245f565f091917aa0d991868c792`.
Player[1176,15580,4386.40], Ilend[1271.46,15484.54,4390.02], health91.168.
This is normal forward preparation; next case provides genuine own-save cold.
Required Sigil Keep Key093ef5 is on guardian0878d5 in Reapers Sprawl033b5e;
current level100 door085f69 is correctly keyed, not a lock-bypass defect.
Windows branch refreshed after4fda: pinned c8fdfc22a8 already ancestor; no delta.
Survey observer identity/protocol failures are retained and excluded from primary
continuations. Record audits distinguish persistent storage cell01e897 from
actual exterior cell021072/world01d0bc.

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
