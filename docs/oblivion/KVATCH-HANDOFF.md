# Kvatch main-quest course — current handoff

Target: outside Kvatch's main gates → original Savlian encounter → first Oblivion
Gate → Blood Feast/Rending Halls → original tower ascent. Full Sigil Stone,
gate closure and main-quest acceptance remain open. Choose work from actual
content failures; keep the current bounded deliverable ahead of adjacent gaps.

**Current gameplay blocker: original corpse flame playback in Dark Salvation.**
Entry through Blood Feast door01d580 reaches Corridors of Dark Salvation085e4f
and Ilend follows, but original FlamingCorpseSCRIPT0c47ae calls unsupported PMS
on decorative corpse06bc4e when its original roll reaches90. Failed entry01 is
diagnostic; its initializer already ran and cannot establish successful playback.
[Contract](KVATCH-FLAMING-CORPSE-CONTRACT.json).

**Accepted dependency: typed original effect-shader data.**
EFSH now has typed engine-store/esmtool support for original96/224-byte layouts,
raw payload preservation and explicit absent legacy particle data. All102
original shaders match independent raw decoding across2,116 field groups in
normal/SAN tools. All official files parse without unsupported record families.
Full components3,319 and engine2,318 pass in both builds; Python623 passes.
SAN engine coverage is2066+84+84+84 disjoint cases, checked against the complete
inventory. Normal/SAN earned-save boots and genuine own-save cold processes
retain health91.168, Player/Ilend life, both Dremora corpses and Ilend killers,
follow1778d1, inventory/ownership, quests/topics/reactions and codec/live state.
Schema106 unchanged. Unit leak1, rendered leak0. Immutable producer archives,
loaded-section proofs and exact evidence are linked in
[verification](KVATCH-EFFECT-SHADER-DATA-VERIFICATION.json).
This accepts ingestion; actual fork flame playback remains unimplemented.
Evidence root: `build/oblivion-compat/kvatch-flaming-corpse`.

**Next action:** implement actual corpse-owned PMS surface particles using the
authored fire shader0852fe, then verify original entry and own-save continuation
in normal/SAN games. The data dependency is finished; keep sequencing on this
visible effect before selecting another content failure.
Independent [native checks](KVATCH-FLAMING-CORPSE-ORACLE.json) establish the caller:
the uninitialized `myself` local falls back to the executing corpse. Omitted
duration is-1. Original surface flames persist in a fresh original process with
init1/lucky52 and no shader reissue. The actual fire shader disables membrane
and authors particle texture/motion/color/lifetime. Original probes restored the
isolated prefix exactly; their direct shader commands are visual diagnostics,
not original main-quest completion or natural-roll acceptance.

Healthy primary entry replay source:
`build/oblivion-compat/kvatch-rending-halls/normal-upper-dremora-defense-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `4088d33c8172b40a798fc877b1aca3e5b3d2fead7c8db00a1aed98b56a84a429`.
Player[-735.799011,-21.811663,619.107971], pitch-0.263672/yaw2.803508;
Ilend[-657.051819,-46.695877,642.995117], alive on follow1778d1.
Player health91.168: one recorded3.372-damage hit before Ilend killed093ee1;
earlier093ede is also dead with Ilend as killer. Player has weapon ready.
Use ordinary movement/Space recipe `original_kvatch_dark_salvation_entry.json`.
Keep the failed entry01 and interrupted surveys out of primary continuations.
The accepted loader-only cold saves are linked in the verification receipt;
they preserve this same earned location and do not exercise PMS.

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
