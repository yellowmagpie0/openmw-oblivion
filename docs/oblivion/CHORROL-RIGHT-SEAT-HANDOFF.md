# Original Chorrol right-entry chair

Bounded M14/M11 course: original Great Hall chair `03fdce` (`UCChair04R`, base
`02dd43`) now supports ordinary Player entry, seated cold reload, empty-target
Activate getup, standing cold reload and horizontal movement. Back-entry markers,
NPC furniture packages and full milestone acceptance remain open.
[Contract](CHORROL-RIGHT-SEAT-CONTRACT.json),
[original-game observations](CHORROL-RIGHT-SEAT-ORACLE.json),
[verification](CHORROL-RIGHT-SEAT-VERIFICATION.json).

The existing furniture owner now accepts right marker 12/orientation 4712 and
uses the original right entry/exit clips. Transition motion and heading use a
minus-quarter-turn; idle faces the chair and standing after exit faces right.
The existing schema 106 marker, asset hashes, phase and clock retain save identity.

Normal and sanitizer three-process courses and semantic audits pass. Original
MS45 stage 100, Fame 1, inventory, Player values, skills/advancement, allocator
and chair reference are preserved. Positions and headings match the independent
native observations within the declared tolerances. Current normal left-entry/
cold getup regression passes; prior forward coverage keeps its original provenance.
Full engine coverage is 2,310 tests per mode (exact disjoint SAN union), plus 621
Python tests. Rendered SAN uses leak checking disabled; instrumented units enable it.

Evidence root: `build/oblivion-compat/chorrol-right-seat`. The source is the
accepted independent left-chair standing checkpoint from the genuine MS45 reward
branch. Ordinary navigation reaches this right chair, verifies actual focus and
records the unsupported-marker baseline before implementation. No fork teleport,
grants, added records or actor enabling. The native diagnostic uses declared
original-only travel; selected chair identity is verified before activation.
Its detached launcher and separate Proton session finish with wrapper status 0,
exact input restoration and the pristine save hash intact, with no owned game
processes remaining.

Both tested producers have byte-verified Zstandard archives. Seven historical
SAN executable caches were also recompressed losslessly; their new paths and
exact decoded identities are recorded in the verification's cache map and the
adjacent `openmw-recompressed.json` files. All original content, saves and runtime
evidence remain retained.

Next: select the next reached content gap. Back-entry markers and NPC furniture
packages remain unimplemented; keep this branch separate from other inventories.
