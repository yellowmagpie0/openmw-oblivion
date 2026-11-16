# Original Chorrol left-entry chair

Bounded M14/M11 course: normally reach and use original Great Hall chair `03fdd0`
(`UCChair04L`, base `02dd45`), cold-load seated, get up through empty-target Activate,
cold-load standing and walk. Right/back markers, NPC furniture packages and full
milestones remain open. [Contract](CHORROL-LEFT-SEAT-CONTRACT.json),
[independent original observation](CHORROL-LEFT-SEAT-ORACLE.json).

The Player furniture owner now accepts original left marker 11/orientation 1570,
loads the original left entry/exit clips, and rotates transition motion and
heading by a quarter-turn. Idle faces the chair; getup faces left. Existing
schema 106 saves retain marker/model/clip identity and phase/clock. Forward entry
keeps its previous resources and behavior.

Evidence root: `build/oblivion-compat/chorrol-left-stool`. It retains the historical
name of the rejected stool candidate behind a keyed guard-room door. The chosen
chair is behind unlocked dining door `02ba65`. Approach/checkpoint provenance is
composite: accepted MS45 reward shop save → ordinary shop exit → actual street
navigation (one manually saved progress checkpoint) → actual Castle door `00090e` →
Great Hall → unlocked dining door → close chair baseline. No fork teleport,
record injection, inventory grant or actor enabling. Inventories from tutorial,
Wellspring, shore and this independent Hackdirt reward branch stay separate.

Normal and sanitizer courses pass with semantic audits. MS45 stage 100,
Fame 1, inventory, native Player values, skills/advancement, chair reference and
allocator are preserved. Position/heading tolerances are declared in the oracle.
Normal forward-entry/getup regression, 2,310 engine tests per mode (exact SAN
union), and 621 Python tests also pass. Rendered SAN disables leak checks; instrumented units enable them.
[Verification and final checkpoint hashes](CHORROL-LEFT-SEAT-VERIFICATION.json).

Retained failures: locked stool approach; script-list fingerprint mismatch on an
exploratory load; manual route-progress checkpoint; initial rendering-ray focus
guard missed a real chair activation; first two cold getup drivers targeted
actual tableware because the first large mouse event was ignored. The corrected
driver observes camera pitch through small ordinary mouse motions before
Activate. These failures are not acceptance passes. SAN entry 01 also retained
a failed 180-second load guard; the corrected 900-second guard passes.
The first native sit exercised a neighboring chair and is excluded; the repeat
confirmed reference `03fdd0`.
Original qqq ended the attached wrapper with status 143; no owner processes remained and exact
isolated prefix inputs were manually restored/verified. No clean wrapper claim.

Both tested producer images have durable byte-verified Zstandard archives.
Original source fingerprints and failed attempts remain in the verification.

Next: select another reached original-content gap. Right/back marker families
and NPC furniture packages remain unimplemented; choose by actual acceptance need.
