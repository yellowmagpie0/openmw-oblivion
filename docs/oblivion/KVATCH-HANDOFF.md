# Kvatch main-quest course — current handoff

Target: outside Kvatch's main gates → original Savlian encounter → first Oblivion
Gate → original interior/road/tower progression. Full gate closure and main-quest
acceptance remain open. Choose the next bounded chunk from an observed gameplay
failure; keep unrelated dependencies and exact numeric parity outside its scope.

**Latest completed chunk: animated Sigil base collision offset.**
Ordinary traversal of The Blood Feast now passes original ACTI0c48e4/base0c48e3
and reaches the lower left Rending Halls doorway01d126. Its body32 remains solid
layer1; the authored body-local translation/rotation now survives animated node
updates instead of producing an invisible wall across the floor at y=-600.
Original normal movement independently crosses this plane (Y-760 to-461.99 at
X-475, Health80 unchanged); prefix/pristine restoration and clean exit pass.
Normal/SAN actual traversal and genuine own-save cold cases preserve Player/
Ilend life, Health94.54, earned inventory, MS48stage25, topics/reactions and the
unchanged Sigil reference. Normal/SAN components3,315 and engine2,317 pass;
Python623 reused with original provenance. No new schema; default Idle remains
ordinary automatic animation, without a new saved-phase claim.
[Contract](KVATCH-SIGIL-BASE-COLLISION-CONTRACT.json),
[native oracle](KVATCH-SIGIL-BASE-COLLISION-ORACLE.json),
[verification](KVATCH-SIGIL-BASE-COLLISION-VERIFICATION.json).
Evidence: `build/oblivion-compat/kvatch-blood-feast`.

Earlier accepted fixes: [occupied follower door arrival](KVATCH-FOLLOWER-DOOR-ARRIVAL-VERIFICATION.json)
keeps Ilend clear of Player after the original Tower Portal; [Harrada phantom
contacts](KVATCH-HARRADA-CONTACT-VERIFICATION.json) retain native geometry,
damage/push and save ownership. Their full original evidence remains linked.

**Next action:** use ordinary Space on the already faced, named original
Door to Rending Halls, then continue that original cell to the next observed
bounded gameplay failure. Healthy primary continuation:
`build/oblivion-compat/kvatch-blood-feast/normal-sigil-cold-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `418fa21bb6ebc3569676f73fef39efcc4b7737452239da46f674c906e7427115`.
Player[-1264.185669,3.386670,-248.341675], pitch0.546875/yaw-1.369141;
Ilend[-1077.366089,-16.018938,-228.365875], gap189, alive on follow1778d1.
Original door01d126 leads to01d118 in CELL01d0c7 (Rending Halls).
No Rending Halls entry, tower ascent, Sigil Stone or gate closure is claimed yet.
Earlier arrival/partial-floor checkpoints and diagnostic failures remain in the
linked contracts and evidence; partial path completion is not doorway arrival.

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
