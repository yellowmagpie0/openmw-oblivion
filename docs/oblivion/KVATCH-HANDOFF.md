# Kvatch main-quest course — current handoff

Target: outside Kvatch's main gates → original Savlian encounter → first Oblivion
Gate → original interior/road/tower progression. Full gate closure and main-quest
acceptance remain open. Choose the next bounded chunk from an observed gameplay
failure; keep unrelated dependencies and exact numeric parity outside its scope.

**Latest completed chunk: occupied follower door arrival.**
The earned Player entered original Tower Portal0220a0 into The Blood Feast01d0c8.
Ilend's native follow package1778d1 now clears the occupied arrival using bounded
floor-supported capsule/scene queries. Original-game idle separation was about144
units; the fork's safe placement here is135units. No exact search/offset claim.
Normal/SAN real crossing and genuine own-save cold continuations pass: one logical
transition on entry, none on restart; Player/Ilend remain alive and inventory,
quests, topics, reactions and position ownership persist. Schema106 is unchanged.
Full normal/SAN engine2,317 pass; unchanged components3,314/Python623 results are
reused with their original provenance. Rendered sanitizer leak0; unit leak1.
[Contract](KVATCH-FOLLOWER-DOOR-ARRIVAL-CONTRACT.json),
[native oracle](KVATCH-FOLLOWER-DOOR-ARRIVAL-ORACLE.json),
[verification](KVATCH-FOLLOWER-DOOR-ARRIVAL-VERIFICATION.json).
Evidence: `build/oblivion-compat/kvatch-follower-door`.

**Previous completed chunk: original Harrada phantom contacts.**
Original plant068733 now damages through its actual non-solid phantom body182,
using the shared native trap Health/life/push authority. Authored settings are
5damage/750pushback, noncontinuous, no rigid-body minimum-speed gate. No proximity
damage or new save schema; schema106 retains typed FLOR contact ownership and
validates the saved body against its winning model. TrapUpdate stays a no-op.
Push uses actor collision center minus contact point as a reasonable approximation
to the original model-height midpoint; no exact native trajectory claim.

Normal approach and separation/re-entry pass. Genuine F5 captures an active latch
and pending push; normal/SAN own-save cold processes preserve both, advance the
push timer, clear the latch on separation and never replay damage. Earned
inventory, quest25, topics, reactions and Player/Ilend life persist. Invalid saved
body181 is rejected before world publication. All required captures were reviewed.
Normal components3,314/engine2,313/Python623 and sanitizer components3,314/
engine2,313 pass; sanitizer engine coverage is the full disjoint inventory.
Unit sanitizer leak detection is enabled; rendered sanitizer cases use leak0.
[Contract](KVATCH-HARRADA-CONTACT-CONTRACT.json),
[original-game/executable/model oracle](KVATCH-HARRADA-CONTACT-ORACLE.json),
[verification and source/artifact provenance](KVATCH-HARRADA-CONTACT-VERIFICATION.json).
Evidence: `build/oblivion-compat/kvatch-harrada-contact`.

**Next action:** continue ordinary input through The Blood Feast and choose the
next bounded implementation from an observed gameplay blocker. Start from:
`build/oblivion-compat/kvatch-follower-door/normal-cold-03/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `0ef83548fc42e0af066380a7a45ea87d7c413fc823d58ce8087fe5dcfaca7a2a`.
Player health94.54, Ilend alive on original follow, MS48stage25. Player at
[0,-1480,-156.043976], Ilend[-95.459412,-1384.540527,-186.001907].
Original preportal/entry/overlap checkpoints and provenance remain in the arrival
contract; the old completed-overlap diagnostic save is not a repaired-save claim.
No tower-interior completion, Sigil Stone or gate closure is claimed.

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
