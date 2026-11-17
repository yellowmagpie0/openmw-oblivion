# Kvatch main-quest course — current handoff

Target: outside Kvatch's main gates → original Savlian encounter → first Oblivion
Gate → original interior/road/tower progression. Full gate closure and main-quest
acceptance remain open. Choose the next bounded chunk from an observed gameplay
failure; keep unrelated dependencies and exact numeric parity outside its scope.

**Latest completed chunk: original Harrada phantom contacts.**
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

**Next action:** continue normal-input road/tower exploration from the genuine
normal own-save continuation:
`build/oblivion-compat/kvatch-harrada-contact/normal-contact-cold-03/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `86b90aaa38f97666732d0566b963df50647ca9f9fba5b5ff1c794fab9c4da54c`.
Player health94.54, Ilend alive, quest25. Pending push is legitimate saved state.
Find the next discrete missing behavior through this original course. Continuous
hazards, static ACTI phantoms, exact corpse impulse and other entry angles remain
outside the Harrada acceptance; do not expand them without a course requirement.

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
