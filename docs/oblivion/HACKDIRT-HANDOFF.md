# A Shadow over Hackdirt — current handoff

The target is playable Chorrol before pickup, original discovery/acceptance,
overland travel, one original investigation route, cavern combat, Dar-Ma's
rescue/escort, original escape conversation, return/reward and save/reload.
**The full course remains open.** No original quest scripts or records changed.

## Launcher performance checkpoint — 2026-11-08

The Hackdirt shop tryout's repeated script scans and worldwide AI door searches
are repaired. The accepted desktop replay improves median frames from 103 ms to 29 ms
and the worst measured gameplay frame from 4.27 s to 172 ms; it remains CPU limited
below a steady 60 FPS. Full normal and ASan/UBSan engine gates each pass 2,254 tests,
with 598 Python tests. See [performance evidence](HACKDIRT-PERFORMANCE-VERIFICATION.json)
for binary/source identities, normal controls, save/restart checks and limitations.
Retained tryouts now use `~/.local/state/openmw-oblivion/tryouts` instead of tmpfs.
This closes the bounded launcher-performance repair; original return/reward and
the full quest course below remain open.

## Current checkpoint — 2026-11-08

**Original rescue, active/waiting follower restart, resumption and escort through
the cavern and inn doors are accepted.** See the [rescue/follower receipt](HACKDIRT-RESCUE-FOLLOWER-VERIFICATION.json)
for executable/content identities, ten editable normal-input manifests, actual
process/library hashes, save identities, semantic checks and retained failures.

The original key opened cage092c85; plea02809b and help02820f completed the
original stage80 result. Dar-Ma left the prison and followed. Original Wait
Here028695 paused her shared Follow0292f9 package. A fresh process verified that
she stayed stationary while Player walked 524 units away. Original Follow
Me028696 cleared waiting; another fresh process verified 194 units of actual
following and a saved distance 346. Dar-Ma then traversed ladder090cf1 into
Moslin's Inn and door0280fe onto Hackdirt street, alongside Player.

Latest accepted save:
`build/oblivion-compat/hackdirt/darma-street-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Player is around (-67450,64419,9811), outside the inn. Stage80,
untied1/unlockCell1/thankedPC0, Dar-Ma alive63, original key and inventory,
disposition modifiers, faction reaction and empty crime state persist. Her
original Follow package and companion link remain, with an explicit false
waiting override. Keep this immutable **pre-escape** checkpoint.

## Current chunk and next action

**Original escape conversation is accepted**, through requested topic
MS45DarMaGoHome028209 and INFO02820d. Its result is comment-only, with no choices
or stage change. See [escape evidence](HACKDIRT-ESCAPE-VERIFICATION.json).
Stage80 and untied1/unlockCell1/thankedPC1 persist, Dar-Ma alive at 34.2531.
Natch Pinder attacked her; she killed him and survived. His saved killer is
Dar-Ma and Player crime is empty. Original stage90 records Dar-Ma's death after
discovery; it is not an escape milestone.

Latest accepted post-escape save:
`build/oblivion-compat/hackdirt/darma-escape-crossing-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Player is around (-68772,65966,10008); Dar-Ma at (-68225,65539,9826).
The saved route was stalled at the exterior boundary: old PGRD0280cd,
new cell00f3a8, package0292f9. The repair retires consumed source-cell
navmesh segments, reevaluates interruptible Follow after a cell change, and
rebuilds restored stale sources while preserving mandatory selection.

The fresh-process retry selects original **MS45DarMaTravelHome0a2e59**, targeting
SeedNeeusRef0234fd in shop000807. It correctly releases the companion link.
The return is independent travel after the escape conversation, rather than a
continuing Follow package. Failed foreign planning previously fell through to
walking toward interior local coordinates; a regression now prevents that.

The next observed route failure was a missing street leg in the backward door
search: the shop and Chorrol gate occupy different exterior cells. The search
now includes resolved directed PGRI street links within a worldspace, at zero
door cost. Actual node routing still validates every approach. The search stops
at an exterior frontier already reachable from the source, avoiding unrelated
continent-wide door inspection. A four-cell/two-door regression checks both a
valid street link and a reverse-only link. The focused Door suite and boundary
regression pass in `exterior-street-focused-03` (exact inventory in its receipt).

`darma-travel-home-diagnostic-04` used the first street-link repair before the
search bound: it planned the independently audited 76-node route to original
Chorrol gate00092f and physically moved Dar-Ma244 units. Its save caught only
about46 units of that movement, with active TravelHome, gate00092f, no repath
failures and health34.2531. Retain its planning-pause stack diagnostic; the
bounded version still needs runtime verification. The failed fixture in
`exterior-street-focused-01` placed two doors in one cell; corrected02 passes.

**Next:** load that original post-escape return prefix with the bounded planner,
walk the previously verified road back to Chorrol, verify Dar-Ma's original gate
and shop arrival, receive the original reward, and verify final restart. The
prepared normal-input road manifest is ignored evidence at
`build/oblivion-compat/hackdirt/return-player-road-manifest.json`.
The bounded version rebuilt the gate route on reload in `return-player-road-01`.
Its immutable `prefix-15` save has Dar-Ma3503.56 units from her escape position,
health34.2531, active route/node36, no retries, stage80 and empty crime. The full
road attempt failed its waypoint22 steering timeout; it is not a return pass.
A frame sample identified repeated rebuilding of the street index for other
actors. The derived index is now cached by graph keys/generations, invalidated
on overlays, clear, restore and retained-runtime swaps. The focused Door suite
and boundary case pass in `exterior-street-cache-focused-01`, including disabling
and reenabling the street link on the same service. The fresh continuation uses
`return-player-road-reload-manifest.json`, with gentler normal mouse corrections.

Full normal engine gate `exterior-return-normal-01` passed2250 tests before the
cache change. Sanitizer `exterior-return-asan-01` was deliberately terminated
during build after discovering the repeated-index cost; retain that failure.
The cached version passes2250 full normal tests in `exterior-return-normal-02`.
`exterior-return-asan-02` was stopped during build for the following observed
physics dependency; it is retained, not a passing gate.

The player road continuation `return-player-road-03` structurally passed all74
waypoints, but its prefix30 save exposed Dar-Ma stalled at (-68858,74229,10766),
health34.2531, stage80, packageTravelHome, reason bounded-repath-exhausted. This
is **not accepted NPC return**. The normal player detour around the obstruction
only closed the player-road prefix. `return-player-road-02` and manual Jump/E,
strafe/D receipts are retained failed attempts; no state commands were used.

The actual obstruction is original Viper's Bugloss flora: its body is authored
on Havok layer15, OL_NONCOLLIDABLE, yet was baked into solid world collision.
The original asset hash, parsed filter, pinned Niftools schema and limitations
are in ignored `flora-collision-audit-01/audit.json`. The loader now filters these
Oblivion bodies alongside existing portal bodies. A mixed-shape component
regression preserves solid siblings, triggers and later-version behavior;
`flora-layer-focused-01` passes. Fresh normal/component and sanitizer gates plus
runtime plant crossing remain pending. Replay from immutable road-prefix15,
where Dar-Ma is active, so success demonstrates removing the actual blockage
rather than restarting an already-stalled intent or changing its saved state. Do not inject
EvaluatePackage, positions, quest variables or package state to close this gap.

The earlier street save remains the immutable pre-escape checkpoint.
`darma-escape-01` displayed the right response but failed its camera observation
before saving. The accepted retry splits saved town-edge approach
`darma-escape-edge-01` from short crossing `darma-escape-crossing-01`, holding
walking only until dialogue opens. After the boundary repair, return and
receive the original reward, then verify final restart. The full course is open.

## Implementation and checks

RemoveScriptPackage releases the mandatory override. GetFactionRankDifference
supports the original rescue result's HELLO conditions. Follow holds a reachable
position within range and routes toward the leader beyond it. Wait/StopWaiting
use shared PACK flag0x10000 with schema91 persistence, including explicit false;
older saves default to no overrides. The original master has no initially
waiting PACKs. Original-instruction evidence covers 147 faction-rank evaluator
cases and 12 waiting-flag mutator cases, with boundary stubs disclosed.

A return-walk crash exposed a borrowed saved-reference index invalidated by
package actions growing the saved vector. The fix refreshes it at actor/step
boundaries and rejects stale views in World enable queries. The same return
route subsequently grew the vector from 141345 to 141346 without crashing.
The freed-storage regression passes normally and under ASan/UBSan.

Prior engine gates before this boundary repair pass **2,247 normal and 2,247 ASan/UBSan tests**, exact
inventories, zero failures/skips; engine leak checking is enabled. Selected
sanitized component coverage passes 1,090 ESM4 tests with leak checking disabled.
Both final engine gates and the sanitized component gate ran at
`ada83e7a18882cec3cd3108fb932b53f3dad453733c2dc09938d770d42b7bcb6`, before
manifest/document publication. Unchanged codec evidence is reused with its
original fingerprint: 3,268 normal component tests and 598 Python tests. The
receipt distinguishes those provenances and retains the interrupted sanitizer
build and all runtime/fixture failures.

Earlier accepted provenance remains in [pickup](HACKDIRT-PICKUP-VERIFICATION.json),
[road/restart](HACKDIRT-OVERLAND-VERIFICATION.json),
[invitation](HACKDIRT-JIV-VERIFICATION.json),
[meeting/package doors](HACKDIRT-PACKAGE-DOORS-VERIFICATION.json), and
[cavern entry/one hostile guard fight/restart](HACKDIRT-CAVERN-ENTRY-VERIFICATION.json).
Gaps are shrinking: the remaining deliverable is the corrected return escort
and original return/reward, with restart. These prefixes do not complete that course.

## Latest return checkpoint — abstract handoff

The flower fix passes3269 full normal component and2250 engine tests at
`f16697662b7b0e3c04086273d693d2caa21b683db1903a4db67b91c720576a28`.
`return-player-road-04` structurally passes the complete road and verifies both
actors crossing the original flowers directly. Its prefix30 save has active
Dar-Ma at (-68521,75685,11622), health34.2531, no retries. At the stables she later
stalls against another physical obstruction. Player entered original gate00092f
in `return-gate-01`, but the physical stall remained terminal after Tamriel
became inactive. This still did not close NPC return.

The bounded repair now retires only a physical `bounded-repath-exhausted` stall
with positive no-progress time when the actual abstract executor takes over.
It resolves the same intent against native pathgrids and door permissions,
without changing the package or selection generation. Semantic route failures
stay blocked. `abstract-handoff-focused-01` passes8 shifted-load-order, physical/
semantic, locked/unlocked cases, including a real XTEL completion exactly once.
The final source has not yet passed fresh broad gates. `flora-return-asan-01`
was intentionally stopped during tests for this new observed dependency; its
partial inventory is not a passing gate.

**Current action:** `return-shop-03` replays the unchanged inside-Chorrol gate
save with the following repairs. Actual shop arrival, original reward100 and
final restart remain open. Do not mark the full course complete yet.

The first city handoff committed gate00092f but then rejected the intermediate
XTEL landing because there was no remaining source route or final destination
there. The bounded validator now accepts a fresh original door landing only
when the paired marker cell matches and position is within128 units. The
corrected `door-landing-focused-02` passes8 valid/expired/reverse/distant cases;
01's test used10ms without reaching the50ms AI tick. `return-shop-02` confirms
actual resident transfer into Chorrol, but still crashed during guard cleanup.

Two normal crash traces, the targeted GDB watch in `city-guard-watch-01`, and a
standalone compiler reproduction identify a separate optimized GCC16 bug:
`(condition ? routeA : routeB) = {};` nulls an empty FormKey string.
Explicit `TargetRoute{}` avoids it. The minimal six-case matrix is ignored at
`repro-route-reset.cpp`; case0 fails and cases1–5 pass at-O2. The production
conditional now uses the explicit type. `route-reset-focused-01` passes the
public-API reset/cleanup regression plus both handoff/landing tests,3 GoogleTests
and18 shifted-load-order cases. The ASan/O1 city diagnostic did not reproduce
the normal/O2 crash; its extended driver timeout is retained and is not a
passing runtime gate. `city-handoff-asan-focused-01` passed both prior tests.

Full normal engine count is now2253. Fresh broad normal/sanitizer gates and
sanitized NIF coverage remain pending until this runtime chunk succeeds. The
3269 normal component receipt from `flora-return-normal-01` remains applicable:
subsequent changes affect only AI.cpp and engine integration tests. All failed
or deliberately interrupted sanitizer receipts remain in their original paths.

## Requested Git checkpoint

The user requested committing the current workspace state. The current normal
retry `return-shop-03` has passed the former guard crash and all9 city walking
waypoints with the explicit route-reset workaround. Shop arrival, the original
reward100, final restart and final broad engine/sanitizer gates are still open.
The focused three-test regression gate passes; this checkpoint is not full-course
completion. Ignored saves, proprietary extracted assets and bulky diagnostics
remain outside Git. No push is authorized.
