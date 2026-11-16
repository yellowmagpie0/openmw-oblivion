# Original Chorrol empty throne

Original Great Hall throne `02b1f5` (`CastleThroneChorrolF`, base `02f17f`)
now supports ordinary Player sitting, seated cold reload, empty-target getup,
and standing cold reload and horizontal movement.
[Contract](CHORROL-THRONE-SEAT-CONTRACT.json),
[original observations](CHORROL-THRONE-SEAT-ORACLE.json),
[verification](CHORROL-THRONE-SEAT-VERIFICATION.json).

The verified marker 14/orientation 0 uses the back entry/exit clips in the
placement frame and a half-turn seated idle frame, facing the hall. Existing
schema 106 retains the owner, exact asset identities and animation clock.
Canonical marker 14/orientation 3141 remains covered by a current entry/cold-getup
regression. Other orientations, NPC furniture packages, occupied-seat handling
and full M11/M14 acceptance remain open.

Normal and sanitizer three-process courses pass with semantic audits preserving
MS45 stage 100, Fame 1, inventory, Player values, skills/advancement, allocator and
original reference. Independent native observations establish 0/2/3/4/0 and sleep 0,
with predeclared seated 4/standing 12-unit and .02rad heading tolerances. Ordinary
fork navigation reaches this actual empty throne; no fork teleport, grants,
actor enabling or clock writes. Original-only travel and normal Wait are declared;
original wrapper 0 and exact profile/plugins/pristine-save restoration pass.

All 2310 engine tests per mode and 621 Python checks pass. SAN units enable leak
checking; rendered SAN disables it. Unchanged component evidence retains the
Windows integration receipt's original provenance. Failed focus/stamp/storage/
codec attempts remain linked in the receipt, with corrected acceptance runs.
The verified producer archives use explicit independently decoded 512MiB delta
chunks against retained full Back anchors. Historical identical live snapshots
share storage with every original path and hash preserved.

Next: follow the user-directed main quest course outside Kvatch's main gates,
enter the first Oblivion Gate and choose bounded chunks from observed failures
along that route. The Countess arrival/world-realization discrepancy is recorded
under `build/oblivion-compat/next-npc-throne` for later.
