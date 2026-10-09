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


S3 hotkey-resource follow-up (2026-10-08): KEYS and the migrated native snapshot
now feed a read-only GUI resource preparation callback before native-plan ownership
transfer and World cleanup. Detached incoming/static ManualRefs and static spell
effects select cached MyGUI icons/frames; modern native item slots cover absent
KEYS. Shared icon/frame resolution also supplies the native fallback for spell/
MagicItem frames. The final normal and instrumented inventories each pass1273
engine tests at source fingerprint71453cb314d426ccbb6b0156378adf52c1a71b643afa81e74aeee23012baa714.
Actual authored/generated/MagicItem and absent-KEYS courses pass in both builds;
a shared saved-spell course deliberately verifies existing empty-icon warning-image
compatibility, exact SPEL bytes and pre-restore decode order. A distinct-PID normal
continuation passes too. See the milestone's exact directories, failures and scope.
This establishes hotkey image preparation and the declared display-kind restores;
it does not establish native spell-icon translation, enchanted-item activation,
all hotkey UI/legacy paths or complete all-or-nothing S3 restoration.


S3 actual content-rejection follow-up (2026-10-08): the normal executable rejects
an actually changed selected script-content file by its independently checked
saved/current SHA-256 and an actually removed selected file by name. The editable
changed/missing-content source manifests are byte-identical to tested manifests;
private `content-changed-runtime-normal-01` / `content-missing-runtime-normal-01`
courses pass action/error/exit audits, retain untouched input/save bytes and emit
no native application or load/save completion. This supersedes the initial-load
content-rejection gap for that family. Prior running-world preservation, other
content families and complete all-or-nothing restoration remain separate gates.


S3 shared effect-scalar follow-up (2026-10-08): all five consumed float channels
of Player/NPC/creature active and queued effects reject NaN/infinity before native
preparation and cleanup, preserving signed finite/permanent values. A7050-case
matrix fails before the fix; final normal/instrumented inventories each pass1274
engine tests. Actual F5/injected shared LEFT NaN/F9/Return/F5 courses in both builds
prove rejection without teardown and preservation of declared owned state and
preceding native references/script instances. The incorrectly ordered first
fixture remains a failed framing course. Full input/publication transaction
staging and other S3 gates remain open; see the milestone's hashes and scope.


S3 immutable admitted-input follow-up (2026-10-08): StateManager's native loader
now snapshots input bytes before admission and reads every subsequent record from
that owned stream, retaining the original path only as identity/metadata. Source
in-place overwrite/truncation can no longer invalidate the admitted second pass;
read/allocation failures precede cleanup. The Morrowind path is unchanged. Two
file-backed regressions fail before the fix; final normal/instrumented inventories
each pass1277 tests at fingerprintf2dfe8ef39a2e6a0e740252b1d64e0cbbf4ae3a66ed49cbe39416c2b0cb4b60e.
Actual normal/instrumented workers truncate the10MB slot to46 bytes during
preparation, before observed restore logging; native restore, GUI state, exact
shared SPEL and real F5 resave remain correct. This closes admitted-input stability,
not the remaining later publication/resource failure boundary or full S3 gate.


S3 shared AI/magic/legacy attribute follow-up (2026-10-08): aggregate magic
modifiers and consumed AI coordinates/durations reject nonfinite values before
native preparation; Player permanent attribute effects validate only existing
incoming/static spells and consumed effect IDs. Finite signed and intentional
skip paths remain accepted. Retained red regressions fail; normal/instrumented
full inventories each pass1279 tests at fingerprint
1b683338799544861330c47596f0a7cc4eb9060346166cb4bfa56a3ace9e69a1.
The13536-case all-version/all-actor float matrix and720-case legacy dependency
matrix preserve reader contexts and suppress preparation on rejection. Six
successful actual F5/fault/F9/Return/F5 courses prove scoped prior-World state
preservation; the first concurrent instrumented magic input timeout remains
failed evidence and a fresh solo course passes. See the milestone's exact
paths, hashes, warning scope and remaining publication/resource boundary.


S3 canonical Player metadata follow-up (2026-10-08): prepared native saves now
select and allocate the canonical name/race/class/sex record in their detached
shared definition plan before cleanup. Install publishes that record; native
apply retains its identity and consumes the staged birthsign. Dynamic classes
resolve against incoming/static definitions in direct World preparation too;
the loader's existing shared-class admission error remains unchanged. The88-case
schemas3–46 regression fails before the change; full normal/instrumented
inventories each pass1280 tests at fingerprint
62263573b68e1fe1b861c553aeaec51e63c8ecfd261ba79b60302968aaaf9ae9.
Actual normal/instrumented class-removal F9 rejection/resave courses pass;
the instrumented PID loads the exact normal final save, independently checked
by hash and38-group initial native readback. The first normal course's wrong
expected error remains failed evidence. This stages Player metadata allocation,
not all later shared actor/scene/resource/publication work; S3 remains open.

Shared compatible inventories now prepare item references and equipment against
incoming/static definitions during admission, retaining definition ownership for
both T4ST and shared-only native-profile saves. WorldModel keys normalize Player
and converted legacy ActorIds, preserve unset-ID identities, and consume plans
once during actual class restoration. Legacy restocking and spell-created RefNums
retain their compatibility behavior. Item registry publication still allocates
after clear; shared stats/custom data and scene resources remain unstaged.
The shared-inventory normal/instrumented1285-test reports and two actual populated
save/reload/resave courses pass. A separate normal shared-only v7 course creates
and reloads schema46 deliberately, with empty Player inventory and no claim of
retaining its removed T4ST payload. See the milestone ledger for exact source,
executable and continuation hashes and retained failed attempts. S3 remains open.

Admission's shared Lua validation collection now copies owner/item payloads so
prepared inventories keep saved scripts and timers. The four-case preparation
regression failed before this fix; normal/instrumented full inventories now pass
1286 tests at the fingerprint recorded in the ledger. Existing runtime courses
precede this follow-up and do not prove scripted-item callback execution.

Additional shared NPC fields now prepare during admission with inventories.
Prepared skill/faction/expelled/used-ID nodes publish through the actual NPC class
reader once, preserving old overlays and the native skill assignment guard.
Factions use static content; used IDs follow accepted incoming/static publication
policy. CreatureStats and other actor/scene allocations remain separate and open.
Normal/instrumented full1288-test inventories, populated World fixtures and actual
two-process save/reload/resave courses pass. A normal no-T4ST declared-v7 course
also creates/reloads current state deliberately. Exact fingerprints, runtime
hashes, fixture mistakes and verifier-only negative controls are in the ledger.
This is not a complete transactional restore or S3 acceptance declaration.

Core compatible CreatureStats now prepare attributes, resources, aggregate effects,
summons, AI settings and consumed scalars before teardown. Owner-key plans publish
once through actual NPC/creature class reads, retaining MissingACDT and overlays.
Every relevant native guard is checked before core mutation; the former partial
resource failure is reproduced and fixed. Normal/instrumented full1291-test reports
and actual fresh-process resave courses pass, including shared core/AI subrecords.
Spells, active effects, AI packages, custom data and later scene/registry work remain
outside this core preparation. See the ledger for scope, fingerprints and failed
fixture attempts; S3 is not closed.

S3 active-effect preparation follow-up (2026-10-08): the existing shared
creature plan now owns `ActiveSpells::PreparedState` before teardown, including
active nodes, strings, effect vectors and queued storage. Class NPC/creature
restoration consumes it with the runtime reader's converter, avoiding borrowed
admission converter lifetime. Missing legacy active IDs are generated during
installation; queued missing IDs and item-reference behavior are preserved.
Normal and instrumented suites pass all1293 tests, and separately launched
idle-effect F5/F9/F5 courses pass exact native/shared persistence comparisons.
Populated active/queued payloads are covered by direct/class tests; these
courses do not prove effect execution in gameplay. Existing nonempty queues
and legacy converter bookkeeping may allocate during installation. Spell-list,
AI-package, custom-data, registry and scene preparation plus late-load rollback
remain open. AI construction currently draws World PRNG through its reaction
timer and assigns global Follow indices; both require deferred initialization.

S3 shared AI preparation follow-up (2026-10-08): `PreparedCreatureStats`
now owns reconstructed `AiSequence::PreparedState` packages/vector before
cleanup. Saved package constructors defer World PRNG reaction-timer draws and
Follow global indices until installation, in saved order; cancellation consumes
neither. Installation swaps package ownership, preserves empty-source retention
and unknown-only clearing, rebuilds Combat/Pursue counts, and uses the runtime
reader's actor converter. Direct/class tests cover all eight saved package
paths, payload lifetime, cancellation, exact draws, timer guards and replay.
All1296 normal/instrumented engine tests pass; native idle-service continuations
and the declaredv7 shared-only noT4ST course pass. Populated shared AI coverage
is from unit/class tests, not an AI execution gameplay claim. Legacy converter
bookkeeping still allocates after cleanup. Spell-store/cache listeners, actor
custom data, registry/scene resources, full load-failure transaction and broader
adapter/migration acceptance remain open.

S3 runtime evidence correction (2026-10-08): previous shared-core runtime
reports incorrectly called the first11 STBA blocks creature stats and next4 AI.
PLAY actually saves NPC27 skill blocks, then creature11, then AI4. An immutable
post-hoc audit of all10 completed shared runtime courses now compares all42
blocks and passes; prior labels are retained but superseded. The old changed
health control mutated NPC skill index8. Fresh changed/missing health controls
at the correct STBAindex35 are rejected without success reports. Direct health
unit evidence is unaffected. The milestone ledger records the corrected scope.

S3 shared spell-payload preparation follow-up (2026-10-08): the creature
plan now owns resolved incoming saved/base spell vectors in both first/cached
orders, used-power timestamps, saved selection and legacy permanent effect
payload before cleanup. Installation swaps prepared vectors; unavailable and
base-only selection retain the old selection. Ordinary readers preserve prior
spell/power overlays. Actual first/cached NPC instance ordering and actual
Player-only legacy attribute conversion are tested. All1300 engine tests pass
in both builds, with normal/instrumented native continuations and shared-only
legacy noT4ST acceptance. The corrected runtime decoder compares actual NPC27,
creature11 andAI4 stat blocks plus spell fields. Runtime spell fields are empty;
populated payload/legacy conversion evidence comes from unit/class tests.
Shared SpellList cache/listener attachment, actor custom-data construction,
legacy attribute mutation, registry/scene resources and full late-failure
transaction remain outside this chunk's detached preparation.

Shared spell-list restoration now prepares list/cache ownership and admitted
listener capacity before cleanup. The store-identity/next-clear guarded batch
installs once after definitions, preserves first/cached class ordering, and
releases unused pins on success or clear. Live listeners keep lists alive.
NPC/creature class spell attachment transfers prepared base vector storage.
All1303 normal/instrumented tests and separate-process native continuation
pass; declaredv7 shared-only/noT4ST normal course passes. Evidence and tested
fingerprint are in the newest spell-attachment ledger entry. Actual runtime
spell fields are empty; populated ordering/powers coverage uses actual class
unit instances. Actor custom-data construction, missing-ACDT base initialization,
additional unplanned bindings and full late-failure preservation remain open.

Complete-stat shared NPC/creature custom data now constructs detached before
cleanup. Owner-keyed WorldModel plans preserve incoming creature inventory
kind, transfer the exact object once and release unused objects on successful
restore or clear. Actual class tests check pointer identity and preparation
leaving outgoing Player/registry unchanged. All1305 normal/instrumented tests
and separate-process native continuation pass; see the newest custom-data
ledger entry for fingerprint and evidence. Legacy missing-ACDT initialization
still follows the existing base path. Registry binding, scene resources and
whole late-failure rollback remain open; S3 is not complete.

Authored base AI for missing-ACDT actors now prepares detached package/list
storage before cleanup, deferring reaction RNG and Follow IDs to the existing
base-fill location. Empty saved overlays retain authored packages; nonempty
overlays replace them after consuming the same base draws. Actual NPC/creature
class tests compare ordinary fill/wire/RNG and both overlay paths. All1307
normal/instrumented tests and fresh native continuation pass. Normal and
instrumented declared NOAC Player courses preserve native/shared state and
authored base definition AI; Player::readRecord deliberately clears instance
AI, so those courses do not prove Player instance package persistence. The
first wrong Player persistence expectation and an SDL receipt timeout remain
failed attempts. Recipe reproduction and verifier-only changed-authored-AI
control pass. See the newest base-AI ledger entry for exact scope/fingerprints.
Remaining base initialization, registry/scene resources and whole late-failure
preservation keep S3 open.

Missing-ACDT actors now use the same detached custom-data constructor and
owner-keyed plan map as complete-stat actors. NPC/creature ensureCustomData
consumes that exact object before performing its existing base initialization;
creatures keep the prepared container. Ordinary fallback retains its original
container-allocation point. All1307 normal/instrumented tests and separate
process native continuation pass, as do normal/instrumented declared NOAC
Player courses and recipe reproduction. Direct actual class tests check
constructor identity, consumption and base/AI overlay behavior. See the newest
legacy custom-data entry for exact scope and fingerprint. Base setters,
recalculation, race/faction/spells/autocalculation, inventory fill/autoequip,
registry/scene resources and full late-failure preservation remain open.

Missing-ACDT shared creature base stats now compute detached from winning
incoming/static definitions before cleanup. Existing setters resolve consumed
NPC magicka GMSTs through a transient read-only context without live Player
queries; it clears before publication. Zero-health death timestamps defer to
the restore clock. AI/resource/gold/persistent flags retain captured values
through the one-shot class initialization. Eight actual class cases preserve
ordinary wire/RNG behavior after base changes, and missing consumed GMSTs fail
without outgoing mutation. All1309 normal/instrumented tests and common
CreatureStats/Player native+NOAC regression courses pass. Those engine courses
do not prove shared creature runtime acceptance; the direct class matrix
proves the creature path. See the newest ledger entry for exact evidence.
NPC base computation/canonical metadata, remaining spells/inventory/registry/
scene resources and full late-failure preservation keep S3 open.

Missing-ACDT shared NPC/Player base stats now compute before cleanup from the
winning NPC and incoming/static class, using common ordinary setters and a
transient PC/NPC magicka context without live Player-stat queries. Canonical
native Player race/class/sex comes from the same metadata helper used for World
publication; old native versions and shared-only admission retain their intended
metadata contracts. A 16-case actual Player/shared NPC wire/RNG matrix and
missing consumed-setting tests pass within all1312 normal/instrumented engine
tests. Normal/instrumented native continuation and declared Player NOAC courses
also pass; these cover the Player NPC factory, while the direct class matrix
proves shared NPC behavior, not ESM4 NPC adapter initialization. See the newest
NPC ledger entry for exact fingerprints and retained failed/interrupted attempts.
Autocalculated/base/race spells, inventory fill/autoequip, other future dependency
views, registry/scene resources and full late-failure outgoing-World preservation
remain open; S3 is not complete.

Detached legacy NPC race and NPC/creature PC/NPC magicka settings now resolve
against definitions that survive restoration. Unsupported outgoing/incoming-only
race/GMST records cannot conceal a missing static dependency. A 64-case actual
Player/shared NPC wire/RNG matrix covers independent conflicting outgoing race
and multiplier overrides, and negative cases reject outgoing-only dependencies
before mutation. All1313 normal/instrumented engine cases and fresh normal/
instrumented declared Player NOAC courses pass. See the newest base-dependency
ledger entry for exact fingerprint and runtime scope. Other descriptor/faction
dependencies, base/race/autocalculated spells, inventory fill/autoequip,
registry/scene resources and full late-failure preservation still keep S3 open.

Detached complete-stat and missing-ACDT shared NPC/creature constructors now use
surviving static attribute/skill descriptors, without retaining a store pointer.
Existing ordinary constructors keep their behavior. Detached NPC base attribute/
skill autocalculation uses the same static descriptor set. A 128-case actual
Player/shared NPC matrix compares base values before the saved NPC overlay can
mask an error, final wire/RNG behavior and outgoing World/registry preservation;
constructor tests compare with the ordinary post-removal descriptor set.
All1314 normal/instrumented engine tests and four final-source native/NOAC
runtime courses pass. See the newest descriptor ledger entry for exact source,
process continuation and runtime scope. Faction dependencies, base/race/
autocalculated spell initialization, base inventory fill/autoequip, registry/
scene resources and full late-failure preservation remain open; S3 is incomplete.

Detached legacy NPC faction reputation no longer primes the ordinary global
setting caches. It captures surviving settings or already cached values plus
base rank/level. Only live restoration at the old faction phase primes unset
cache entries, preserving ordinary first-use/partial-failure behavior and
existing cache lifetime. Five isolated actual class probes verify failed/
cancelled preparation, captured installation, existing-cache dependency skipping
and no-faction skipping without cache-reset test APIs. All1319 normal/
instrumented engine cases and four native/NOAC compatibility runtime courses
pass. Direct class probes establish faction-cache semantics; rendered courses
are compatibility evidence. See the newest faction ledger entry for exact
source/runtime hashes. Base/race/autocalculated spells, inventory fill/autoequip,
registry/scene resources and full late-failure preservation remain open.

Legacy Missing-ACDT base spell binding and NPC race-power additions are now
prepared as owned resolved pointer vectors, missing-ID warning lists and reserved
merge buffers. Lookup uses incoming/static spell definitions; outgoing-only
spells cannot supply missing definitions. First/cached list behavior, pointer
deduplication, prior instance order and repeated authored warning phases remain
ordinary-compatible. Prepared clones preserve reserved capacity. Full-stat NPC
race powers now require the surviving static race before cleanup, like autocalc
NPCs. All 1322 normal/instrumented engine cases and four actual native/NOAC
compatibility courses pass; see the newest authored-spell ledger entry for
attempt history, exact source/binary hashes and fresh-process continuation.
Autocalculation/cache preparation, actual first-binding restoration ordering,
base inventory fill/autoequip, registry/scene and full late-failure preservation
remain open. This does not add native spell gameplay acceptance.

Admission retains original serialized encounter order across Player and cell
owners for detached actor preparation. The DTO ownership and definition transfer
boundary are unchanged; callback failure restores the input reader before any
incoming definition publication. A six-permutation/all-native-versions/shared-only
failure-position matrix and all1323 normal/instrumented engine cases pass.
This supplies ordering information for future first/cached spell prediction;
that prediction remains open, including skipped-owner/cell behavior. See the
latest ordering ledger entry for source fingerprint and evidence. No new game
runtime acceptance is claimed by this callback ordering change.

Detached explicit legacy NPC restoration now prepares autocalculated spell
selection against static/incoming spell traversal, preserving duplicate-ID
traversal and incoming winning lookup. Independent ordinary first-use caches
remain unprimed during preparation; only first live binding installs consumed
values and resolved selection at the old phase. Cached actors skip unused
dependencies and arithmetic. Consumed integer sums/products and rounded costs
reject out-of-domain values before cleanup. Eleven isolated actual class/cache
probes, twelve actual-publication traversal cases, skipped-cell admission and
static-cell availability tests pass alongside all1337 normal/instrumented cases.
Four final native/NOAC compatibility courses pass with exact normal-save fresh
instrumented continuation; see the newest ledger entry for fingerprints, binary
hashes and preserved failed/interrupted attempts. This does not close implicit
Player initialization from leveled initial inventory, base fill/autoequip,
registry/scene resources or full prior-world late-failure preservation. The
existing saved-owner prediction covers explicit record bindings; pending
inventory work must preserve the earlier implicit Player level query.

The verified accumulated restore checkpoint is committed locally as
517f6ec93a9f6cce310c07f21c72a17df1c2e5de in the independent writable checkout
/tmp/openmw-m15-s3-verified-checkpoint. A verified bundle lives under
build/oblivion-compat/m15/S3/verified-restore-commits/. The original checkout's
Git metadata remains read-only at c29816aec81fc177d52fdfe736d86d75d29b0a87;
no push/publication occurred and the unrelated handoff is excluded.

On 2026-10-09, both recovery commits (517f6ec93a and a5a21a195a) were imported
into the main repository's master branch with their history preserved. Git
metadata writes succeeded in this session. The recovery follow-up also includes
the tutorial handoff. No new build or runtime acceptance was performed, and
the remaining S3 scope is unchanged. Nothing was pushed or published.
