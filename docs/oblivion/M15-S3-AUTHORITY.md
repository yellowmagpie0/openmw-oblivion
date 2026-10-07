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
Admission errors preserve the outgoing world. Shared Player exterior/recall
coordinates and saved attribute/skill floats are checked before native
preparation; their live F9 rejection/resave courses preserve the outgoing world.
Later shared record/resource
restoration still has a cleanup-on-failure boundary: that is an open S3 gap,
not a proved all-or-nothing restore guarantee.

Current evidence includes actual World/T4ST/host tests for native stat adapters,
legacy counter bounds, prepared crime restore and dynamic reconstruction;
normal/instrumented reputation courses prove actual save/quickload/quit/fresh
load/resave and pre-teardown conflicting shared-Fame rejection. Source tracing
of UI getters does not independently prove every rendered field. Remaining
semantic/resource staging, populated migration and profile/lifecycle matrices
must be audited against their full scope before S3 can close.
