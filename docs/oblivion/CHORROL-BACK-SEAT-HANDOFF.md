# Original Chorrol back-entry chair

Bounded M14/M11 course: original Great Hall chair `03fdd6` (`UCChair01F`, base
`02ed35`) now supports ordinary Player entry, seated cold reload, empty-target
Activate getup, standing cold reload and horizontal movement.
[Contract](CHORROL-BACK-SEAT-CONTRACT.json),
[original-game observations](CHORROL-BACK-SEAT-ORACLE.json),
[verification](CHORROL-BACK-SEAT-VERIFICATION.json).

The owner now also accepts the verified marker 14/orientation 3141 pair and
loads the original back entry/exit clips with their authored durations. Transition
motion and heading use a half-turn; idle and standing after getup face placement.
The existing schema 106 marker, asset identity, phase and clock retain cold saves.
Marker 14/orientation 0 on the nearby throne remains a separate unimplemented
variant. NPC furniture packages and full M11/M14 acceptance remain open.

Normal and sanitizer three-process courses and semantic audits pass. Inventory,
MS45 stage 100, Fame 1, Player values, skills/advancement, allocator and chair
reference are preserved. Pose/heading tolerances were declared before the native
probe. Current normal right-entry/cold getup regression passes; earlier left and
forward evidence keeps its original provenance. Full engine coverage is 2,310
tests per mode (exact disjoint SAN union), plus 621 Python tests. Rendered SAN
uses leak checking disabled; instrumented units enable it.

Evidence root: `build/oblivion-compat/chorrol-back-seat`. Ordinary navigation from
the accepted independent right-chair checkpoint reaches this actual chair and
records the selected unsupported-marker baseline. No fork teleport, grants,
added records or actor enabling. The native diagnostic uses declared original-only
travel. A first centre click selected the wall through a backrest opening;
selected-ID evidence comes from the solid wood repeat. The game exits with
wrapper status 0 and exact original documents/plugins/pristine save restoration.

The post-build identity guard also retained a premature sanitizer snapshot from
a still-running link and refused it before any runtime launch. Only the completed
verified image supplies accepted courses. Both final producers have byte-verified
Zstandard archives. Twelve historical normal executable caches were recompressed
losslessly, with new paths/decoded identities in the verification's cache map and
adjacent metadata. One interrupted packing attempt retained its source gzip and
resume receipt; all twelve final conversions passed. Three more historical SAN
archives were migrated with a separate cache map. An attached coordinator ended
with status 143 during an initial cold load; the partial case is retained. The
accepted getup retry and remaining phases use a detached coordinator. Original
content, saves and runtime evidence remain retained.

Next: select another reached original-content gap. Noncanonical marker/orientation
pairs and NPC furniture packages remain unimplemented; keep independent branches
and save ownership separate.
