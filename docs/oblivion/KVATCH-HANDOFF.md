# Kvatch main-quest course — current handoff

Target: outside Kvatch's main gates → original Savlian encounter → first Oblivion
Gate → original interior/road/tower progression. Full gate closure and main-quest
acceptance remain open. Choose the next bounded chunk from an observed gameplay
failure; keep unrelated dependencies and exact numeric parity outside its scope.

**Latest completed chunk: optional path-point commands on unlinked Hall doors.**
Normal Space entered original Rending Halls01d0c7 through door01d126. Original
script005162 on refs01d119/093ee0 now finishes Open0/init1, matching independent
native SV observations. Raw master scan confirms neither door has a PGRL link.
Missing links cause no changes through the existing authority; native AI/null
context checks remain and real linked-node mutation code is unchanged.
Normal/SAN entry each records two initialization no-ops, unchanged path overlays,
one Ilend transition and clear arrival; genuine own-save cold processes retain
both initialized locals with no repeated no-op/transition/clearance. Health94.54,
Player/Ilend life and earned ownership, quests25, topics/reactions persist.
Full normal/SAN engine2,317 pass; unchanged components3,315/Python623 are reused
with original provenance. Unit leak1, rendered leak0. No schema change.
[Contract](KVATCH-UNLINKED-DOOR-PATHPOINTS-CONTRACT.json),
[native oracle](KVATCH-UNLINKED-DOOR-PATHPOINTS-ORACLE.json),
[verification](KVATCH-UNLINKED-DOOR-PATHPOINTS-VERIFICATION.json).
Evidence: `build/oblivion-compat/kvatch-rending-halls`.

Earlier accepted fixes: [animated Sigil base collision offset](KVATCH-SIGIL-BASE-COLLISION-VERIFICATION.json)
clears the original Blood Feast floor while retaining its solid body;
[occupied follower door arrival](KVATCH-FOLLOWER-DOOR-ARRIVAL-VERIFICATION.json)
and [Harrada phantom contacts](KVATCH-HARRADA-CONTACT-VERIFICATION.json)
retain their original linked evidence and bounded acceptance.

**Forward preparation:** normal movement reached the first physical Hall door093ee0;
Space opened it with original script Open1/init1. Its own-save cold process
restored active animation phase0.344333 and advanced to1.0 with the passage clear.
[Door preparation](KVATCH-RENDING-HALLS-DOOR-PREPARATION.json) records normal-only
evidence and editable recipes; no new engine implementation or SAN opening claim.

**Next action:** exercise the original Dremora093ede encounter after ordinary movement
through that open door and choose the next chunk from an observed blocker. Current verified
continuation:
`build/oblivion-compat/kvatch-rending-halls/normal-after-first-door-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `9adaafb125b6dff4264ccf6e71f9bbb87bcff0ae95d374b1618bd71cc0813928`.
Player[1273.624512,1318.130737,522.225403], pitch0.34375/yaw1.403126;
Ilend[1023.820007,1290.287354,521.749390], alive on follow1778d1, health94.54.
Enemy093ede approached from[1878.60327,1803.11877,523.48566]; stopped within650. The first door record
pivot[1024,1276,768] is offset from its physical leaf near X780; do not use pivot
distance as the physical approach requirement. Use actual actor pathfinding bounds.
Full Hall traversal, tower ascent, Sigil Stone and gate closure remain open.

For healthy road replays, the earlier earned source remains:
`build/oblivion-compat/kvatch-ilend/normal-original-ilend-follow-cold-road-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `becc73807be4b5e52f63579085380745c9c12d1ef6d1f2d7fe90e1d265890b93`,
health104.54. [Healing/route preparation](KVATCH-INTERIOR-ROUTE-PREPARATION.json).
Ilend UI replay requires controller menus and virtual gamepad; use
`kvatch-ilend/ilend-follow-agreement.json` then `ilend-cold-follow-road.json`.
Do not use absolute-pointer SDL actions or save while a forced greeting is open.

**Windows performance integration is complete.**
Fetched `c8fdfc22a8` from `origin/codex/windows-native-dev`, merged as `d4a1665868`,
verification committed as `9886ce4475`. Combined normal/SAN gates and six original
courses passed before this contact chunk. [Merge receipt](WINDOWS-PERFORMANCE-MERGE-VERIFICATION.json),
[upstream Windows timing provenance](KVATCH-PERFORMANCE-HANDOFF.md).
The Linux timing is one descriptive course comparison, not a Windows FPS claim.
Do not repeat this merge request. Keep all work local; never push without request.

Earlier accepted course dependencies and their original provenance:

- [Harrada flora/morph playback and saved phase](KVATCH-HARRADA-ANIMATION-VERIFICATION.json).
- [Native TrapUpdate no-op, real gate entry and own-save continuation](KVATCH-TRAP-UPDATE-VERIFICATION.json).
- [Weather commands](KVATCH-WEATHER-VERIFICATION.json).
- [Original gate creature births/lifetime](KVATCH-GATE-SPAWN-VERIFICATION.json).
- [Savlian INFO-added topics](KVATCH-TOPIC-LEARNING-VERIFICATION.json).
- [MS48 agreement faction delta](KVATCH-FACTION-REACTION-VERIFICATION.json).
- [Original guard defense of Player](KVATCH-ALLY-DEFENSE-VERIFICATION.json).

The course inherits a separate earned-Amulet source: MQ01stage100, MQ02stage20;
MQ03 was not artificially advanced. One declared Player MoveTo to original marker
00cef7 staged the requested Kvatch location. No cross-country travel or full quest
prerequisite claim. Never use diagnostic spawn-count/missing-creature saves as
legitimate continuations. Preserve all earlier durable producer archive anchors.

Run one rendered game at a time. Use observed state to end movement steps in slow
sanitizer runs; retain timing/preload failures. Serialize heavy sanitizer rendering
and large test compilation. [Chorrol throne](CHORROL-THRONE-SEAT-HANDOFF.md) is
complete; Countess work remains deferred. The older [Windows merge](WINDOWS-MERGE-VERIFICATION.json)
is historical, separate from the latest performance integration.
