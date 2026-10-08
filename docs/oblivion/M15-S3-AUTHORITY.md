# M15S3 authority and restore map

This maps the current implementation, not full S3 acceptance. The requirements
remain [S3 and section3 of the implementation plan](M15-COMBAT-STEALTH-CRIME-IMPLEMENTATION-PLAN.md).
Checkpoints, test fingerprints and runtime evidence are in
[the milestone ledger](M15-COMBAT-STEALTH-CRIME.md).

| Path | Live authority and mutations | Reads and persistence |
| --- | --- | --- |
| Player | `OblivionCombatService` owns native actor values, form inputs, shared-base overrides and life. World stat/reputation/crime-level requests prepare and publish the native transaction. | Shared `NpcStats`/`CreatureStats` are guarded projections. Lua stat requests use World adapters. `StatsWindow::onFrame` reads the shared reputation/bounty getters. T4ST captures native values; PLAY carries the compatible Player view. |
| Content NPC | Winning ESM4 NPC construction provides initial inputs; the service owns published values/life. `ESM4Npc::getCustomData` reprojects restored authority when lazy custom data is created. | Class, Lua and native script queries consume this authority. Owned bounty is raw float storage; the NPC compatibility getter truncates it. Native record types are skipped by shared CSTA writing, so T4ST is the native persistence path. |
| Creature | Winning ESM4 creature construction and the same service publication own values/life. `ESM4Creature::ensureCustomData` in `esm4interactive.cpp` reprojects restored values. | `CreatureStats` provides resource/attribute views; there is no NPC reputation facade. Legal bounty remains a native reference counter. T4ST stores the native actor and inventory state. |
| Dynamic native actor | T4ST retains a `native-reference` key and winning base/cell keys. World prepares native insertion after clear, publishes the registry binding and applies the native projection. | The dynamic registry resolves live references. Script actor-value queries use live enabled flags before saved fallback flags. The NPC/creature reconstruction matrix checks all72 channels and owned fields through two cycles in both stored process modes. |
| Unloaded reference | Native service state and retained reference metadata remain available by stable key without loading a cell. | `World::getOblivionScriptActorValue` uses live flags when resident, otherwise retained/authored flags, and delegates to the native service. Disabled GetAV reference counters use the reviewed base-form behavior; base queries remain reference-base queries. |
| Projected equipment | Live `InventoryStore`/item `CellRef` own counts, slots and instance condition/charge/ownership. Native preparation resolves definitions before staged inventory publication. | World captures native inventory descriptors. Shared generated definitions and compatible inventory records preserve the projected identities. Generated gear recovery, class/ammunition, weapon/cell and water controls are checked separately from actor-value tests. |

| Native player item hotkeys | T4ST inventory metadata owns item slots; `World::oblivionSetPlayerHotkey` is the mutation adapter. Live inventory determines surviving positive-count items. | `World::oblivionPlayerItemHotkeys` resolves eight shared GUI IDs without changing native state. Modern KEYS is a GUI projection rebuilt on restore, menu opening and UI save; spell bindings remain when a slot has no native item. Schemas before4 retain KEYS migration. |

The Player canonical key is `dynamic:player:0000000000000001`.
`components/esm4/runtimereferences.hpp` normalizes the Player reference alias.
Content keys retain plugin/local-form identity; dynamic keys retain namespace
and serial. Runtime pointers and registry numbers are reconstructed, not used
as persistent native identity.

Fame/Infamy raw signed counters are distinct from AV38/39 modifiers. Legal
bounty is distinct from AV37 composition and routes through its owned realm
state. Absence in old saves remains unowned; explicit adoption validates its
source. Legacy Infamy's original double is archived when native ownership is
established. These are separate contracts from integer/float GetAV composition.

Service-owned action/event IDs, FIFO death events, RNG, clocks, actor values,
life/breath, engagements and crime/arrest/jail contracts project into
`ESM4::RuntimeState`. Crime contracts are state/interface skeletons; their later
gameplay producers are outside S3. `takeNextDeathEvent` removes the front event
before dispatch so a callback save cannot replay that event.

`StateManager::loadGame` admits and prepares native state before cleanup.
`World::prepareOblivionSaveState` retains detached definitions, inventories and
native service plans; installation requires the expected clear boundary.
World capture uses the same live service state, not telemetry as an authority.
Prepared ManualRef type/base selection now uses incoming/static definitions
with the same override policy and signature precedence as actual publication;
outgoing types/projections are excluded, including native MISC dispatch.
ESMStore's public immutable restore lookup includes late facades and raw
incoming stores without indexes. Its20-family clear/commit matrix and actual
normal/instrumented generated-inventory two-process courses pass. Shared
projectile/magic restoration now uses that lookup for detached DTO/model and
Bullet collision preparation before native preparation/cleanup. Effect/school/
light definitions use static content. StateManager installs the guarded batch
once before saveLoaded/ActorId conversion and later rebinds casters; real scene/
Bullet tests cover cancellation, stale/copy/destruction, rollback and skips.
Normal/instrumented two-process physical PROJ courses preserve resources in
resaves; they precede the final magic-only lookup follow-up. Shared magic bolt
sound buffers now decode and remain pinned before cleanup against static
content; native directory preparation preserves global RNG. Lifetime/next-clear
one-shot callbacks publish after scene/collision installation and release failed
backend ownership. Actual null-backend tests cover cancellation, stale/copy,
source removal, playback recovery, queries and destruction in both builds.
Projectile publication and rollback now skip the next-clear generation,
preventing failed or empty restores from enabling another future-clear plan.
The actual scene/Bullet regression failed before the fix and now preserves
empty scene, collision and saved-record state. Sound/World reset counters only
advance on clear. Audible/full projectile runtime acceptance and broader
shared-resource failure boundaries remain open.
Admission errors preserve the outgoing world. Shared Player exterior/recall
coordinates and saved attribute/skill floats are checked before native
preparation; their live F9 rejection/resave courses preserve the outgoing world.
Shared consumed attribute/resource/skill floats, fall/drowning scalars and
draw-state enums are admitted before preparation, preserving ignored wire fields
and MissingACDT/custom-state skips. Actual normal/instrumented shared skill NaN
rejection/resave courses preserve the outgoing world and original shared stat.
Shared cell/actor timestamp hour domains are also admitted before preparation;
removed used-power definitions retain their skip behavior, and omitted active
effect worsening timestamps reconstruct as zero. GMAP PNG decoding and shape
validation now run before native preparation; the GUI retains the decoded image
and StateManager consumes its prepared restore once after cleanup. Shared spell
quickkey dependencies now resolve against incoming saved/static definitions
before native preparation; removed spells and the ignored tenth slot retain
their compatibility paths. Item quickkeys now validate non-item targets and
incoming inventory projections, preserving removed/discarded IDs and native
lockpick/repair dispatch. Oblivion GUI keys now bind after native inventory
replacement, Player setup and the load cell transition. The public hotkey setter
resolves generated shared-item keys and stages current live inventory metadata;
actual normal/instrumented two-process courses retain the GUI/native assignment.
Weather WTHR now prepares a fresh content region map and accepted overlays
before native preparation/cleanup, validating consumed indices against the
actual weather catalog and retaining removed-region/unreachable-tail behavior.
StateManager installs the lifetime/generation-guarded handle once after clear.
Normal/instrumented F9 rejection/resave and a normal fresh-process continuation
preserve current weather31 and declared native state. Oblivion admission now
rejects duplicate original weather-region identities before remapping, map
merging or preparation. Distinct removed regions are decoded then skipped,
while repeated original IDs and malformed payloads reject. The564-combination
schemas0–46/order/removal matrix checks rewind and callback suppression;
legacy first-entry decoding and ordinary reader remapping remain unchanged.
New native WTHR WXVR1/WXID persists stable weather content keys; RGIX/RGDF
preserves bucket order and fallback through catalog remapping. Current/next/
queued/regional choices validate against the loaded catalog before cleanup;
consumed missing definitions reject while unused/removed state skips. Legacy
catalog absence remains numeric. Native chance queries use the current-index
projection. Actual World RNG tests cover every roll through two reorderings;
normal legacy-to-WXVR1/fresh-process courses preserve current and regional
identities and compare all37 catalog keys with the original record walk.
Actual normal and instrumented two-plugin reversal now retain persistent
current weather A while its raw saved index changes37 to38; independent
content/catalog and native state comparisons pass. Additional normal and
instrumented two-process courses preserve nonzero A25/B75 and A25/B25-with-B-
fallback chance windows through B/A then A/B. Raw RGNW/RGDF/RGIX and stable
keys agree across both reversals, with every non-weather setup record exact.
Actual WeatherManager/save-codec tests now verify discarded, copied, stale,
extra-clear and destroyed-owner handles. The owner is immovable. Direct reads
and prepared publication skip the next-clear generation, preventing them from
enabling a callback awaiting a clear; both failing regressions now pass.
A state-only constructor exercises the actual manager while live frames still
require rendering. The final normal regional runtime course also passes.
Changed-content rejection runtime remains open. Quickkey texture preparation and complete native/shared hotkey authority/removal/
replacement reconciliation remain open. Later shared record/resource restoration
still has a cleanup-on-failure boundary: that is an open S3 gap,
not a proved all-or-nothing restore guarantee.

Current evidence includes actual World/T4ST/host tests for native stat adapters,
legacy counter bounds, prepared crime restore and dynamic reconstruction;
normal/instrumented reputation courses prove actual save/quickload/quit/fresh
load/resave and pre-teardown conflicting shared-Fame rejection. Source tracing
of UI getters does not independently prove every rendered field. The populated World crime matrix now covers schemas42–46 and repeated
prepared restore; the Player/NPC inventory matrix explicitly preserves absent
crime state across schemas1–46. Remaining
semantic/resource staging, combined populated migration and profile/lifecycle matrices
must be audited against their full scope before S3 can close.

Native CSTA fog restoration now prepares optional PNG images before cleanup:
recognized dimensions are bounded before allocation, readable images require
32×32 RGBA bytes, and interior metadata must be finite. StateManager retains
admitted images, WorldModel consumes the plan once into CellStore without another
decode, and clear discards pending plans. TES3 admission remains structural;
ordinary rendering rejects unsafe decoded image layouts before pixel access.
Actual normal/instrumented F9 rejection followed by F5 proves live-state
preservation for the wrong-size image fault. A fresh normal PID reloads the
resaved output. Full C++ inventories and fixture-source Python checks pass;
see the local-fog evidence in the milestone. Finite-but-extreme geometry and
complete shared-resource/prior-World preservation remain open.

Native fog geometry admission additionally rejects inverted bounds and integer
map-grid overflow in coordinates, center and spans, using double subtraction.
All 47-schema fault coverage and adjacent-float domain boundaries pass, as do
full normal/sanitizer checks. The actual normal extreme-BOUN F9/Return/F5 course
preserves the prior scene and checked authoritative state. Representable oversized
maps and renderer arithmetic remain open; this is not complete geometry safety.

LocalMap now checks double-precision grid conversions and avoids integer
coordinate/neighbor overflow. Unknown position queries do not allocate empty
segments. Fog serialization visits actual retained segments in coordinate order,
avoiding theoretical-grid products and traversal. Three actual LocalMap cases,
including SDL/OSG GL context, texture creation, fog exploration and CellStore save,
pass in both full 1263-case engine inventories using SDL offscreen. Actual final
normal/instrumented rejection/resave and normal fresh-PID continuation preserve
the checked native/weather/fog state and inspected scene/HUD. Large representable
initial render-grid allocation and complete pre-teardown resource staging remain
open; these checks do not prove full prior-World preservation.

Native item hotkey removal now owns a GUI slot instead of clearing a possibly
moved item by ID. The same World mutation entry point supports empty-ID slot
clearing; spell replacement calls it to remove the native item assignment.
Actual generated Player/NPC inventory/capture/binary cases and both full 1264-case
engine inventories pass. Actual Xvfb mouse/F5 evidence proves item slots 0→7→7
and final GUI slot removal, but its scenario remains failed by generated weapon
legacy sOneHanded tooltip lookup and absent barter-frame resources. This is
metadata evidence, not complete GUI acceptance or full hotkey reconciliation.


Native weapon hover now uses Interface labels for missing legacy string settings;
QuickKeysMenu captions use localized native labels. ItemWidget retains available
legacy frames or uses the existing shipped DDS at full size with state tint.
The final normal Xvfb mouse course passes all three saves, independent slot
0→7→7/KEYS checks and the error-log audit; inspected captions/tooltips/frame are
readable. Lua menu setup is disclosed because the seed blocks F1. A fresh normal
PID preserves all raw KEYS and the generated binding through load/F5, with all38
initial native groups exact and ten stable post-save groups equal. Both full
1264-case normal/instrumented engine inventories pass; evidence and fingerprint
are in the milestone. This closes the observed hover/frame defects, leaving
full hotkey reconciliation, spell replacement, texture preflight and full S3
restore guarantees open.


Moved native items now clear duplicate GUI Item/MagicItem buttons after a
successful native assignment, without another native mutation from stale slots.
Pending activation of a removed button is cancelled. Normal real mouse/F5
checks prove automatic removal at phase2 and exact six-entry inventory metadata
apart from the intended hotkey. A retained duplicate-binding save normalizes its
old GUI TYPE/ID through a fresh normal PID load/resave, with all38 initial native
groups exact, ten stable post-save groups equal and remaining KEYS exact. Both
full1264-case normal/instrumented engine inventories pass. The milestone records
fingerprint/evidence and retained failed build attempts. This does not close
MagicItem/spell runtime coverage, conflicting nonduplicate assignments or full
pre-teardown resource staging.


Modern item hotkey restoration now treats T4ST inventory as authority rather than
letting KEYS reassign it. The public World eight-slot query resolves native and
generated IDs; actual schemas1–46 prove read immutability and pre4 legacy absence.
A scoped GUI restore suppresses native mutation callbacks. Stale, empty and missing
GUI-record fresh-PID normal courses preserve all38 initial native groups, ten
stable post-save groups and reconstruct the expected KEYS; an instrumented stale
course passes the same comparisons. The clean pre-fix course changes native
hotkey7 to0 as its only initial native difference. Actual mouse/F5 checks also
pass. Both full1265-case normal/instrumented engine inventories pass. The milestone
records fingerprints, binary hashes, failed build/startup attempts and evidence.
MagicItem/spell and legacy GUI runtime, resource preflight and full S3 restore
acceptance remain open.


LocalMap now retains actual saved interior fog fragments and creates render
cameras/map textures only for requested or player-explored tiles. Clear/interior
unload cancel queued cameras and cached segments. The actual SDL/OSG case covers
9×9 and over-trillion-cell representable grids without initial camera allocation,
repeated/invalid requests, remote saved fog preservation and bounded exploration.
Both full1265-case engine inventories pass. Normal/instrumented ordinary-size
fresh-PID courses preserve declared native/weather/KEYS state and independently
compared fog metadata/RGBA pixels; inspected scene/HUD remain intact. The milestone
records exact fingerprints and evidence. Full-grid GUI widget pairs and integer
canvas arithmetic remain open, so this does not close large-map runtime acceptance
or the broader pre-teardown resource guarantee.


Interior GUI map widgets now use a sliding viewport over full logical bounds.
Pan/zoom preserve logical points; current renderer revisions refresh reload bindings
without resetting ordinary per-frame views. Coordinate/canvas arithmetic is widened
and off-canvas markers keep their original logical identity. Both full1269-case
engine inventories pass. Normal/instrumented actual Xvfb courses load an unchanged-
native-state save with trillion-cell BOUN, open/pan/return/zoom/F5/quit, prove changed
pan/zoom pixels and pixel-exact return, retain original fog pixels and metadata,
and preserve declared native/weather/KEYS state. A fresh normal PID preserves all
five resulting fog fragments. The milestone discloses private Lua menu setup and
failed/metadata-only probes. This addresses initial whole-grid renderer/widget
allocation for the tested large interiors; broader resource staging, full marker/
exterior/profile/lifecycle and prior-World acceptance remain open.
