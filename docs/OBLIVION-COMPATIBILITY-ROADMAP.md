# OpenMW Oblivion Compatibility Program

Status: accepted roadmap, implementation in progress  
Target: original English Oblivion 1.2.0416 GOTY data, followed by Shivering Isles,
Knights of the Nine, and every installed official DLC  
Compatibility requirement: preserve Morrowind behavior and OpenMW-native saves

## Implementation ledger

This ledger is the resume point. Detailed evidence and open gates live in
`docs/oblivion/` and generated artifacts live in `build/oblivion-compat/`.

| Milestone | State on 2026-08-08 | Durable report |
| --- | --- | --- |
| M0 | Accepted | `docs/oblivion/M0-BASELINE.md` |
| M1 | Accepted, with sanitizer toolchain unavailable on this host | `docs/oblivion/M1-TES4-PARSING.md` |
| M2 | Accepted: full official FormKey/reference graph and runtime identity gates passed | `docs/oblivion/M2-FORM-IDENTITY.md` |
| M3 | In progress: profile and standalone interior vertical slice delivered; final gates tracked | `docs/oblivion/M3-STANDALONE-BOOT.md` |
| M4 | In progress: versioned native save substrate and live shadow record delivered | `docs/oblivion/M4-RUNTIME-STATE.md` |

## 1. Goal and delivery model

Turn this fork into a clean-room-compatible implementation capable of running
the original Oblivion data files. The target is behavioral parity: correct
gameplay, progression, presentation, UI identity, and scene composition, while
retaining OpenMW improvements that do not alter intended behavior.

The current baseline is useful but early: the local build can parse every
installed official ESM/ESP and load Oblivion archives, yet runtime systems still
assume Morrowind in many places. Upstream likewise describes TES4 support as
basic "walking simulator" functionality with primitive actors and interaction:
<https://openmw.org/2025/openmw-0-49-0-released/>.

Every milestone will:

- Produce a bounded, reviewable set of source and test changes.
- End in a runnable vertical deliverable, not merely new data structures.
- Pass the universal verification contract below.
- Update a machine-readable compatibility matrix and milestone report.
- Preserve existing Morrowind configuration and OpenMW-native saves.
- Be implemented and tested by Codex; the user will not be asked to perform
  manual testing.

## 2. Architecture and interfaces

### Game separation

- Introduce `GameProfile::{Morrowind, Oblivion}` as an engine-level service
  selected automatically from the primary master, with
  `game-profile=auto|morrowind|oblivion` as an explicit override.
- Keep TES3 and TES4 readers separate. They feed shared runtime interfaces only
  after format-specific interpretation; TES4 records will never be converted
  into fake TES3 records.
- Move game-specific player creation, settings, calendar, statistics, UI
  resources, scripts, and startup behavior behind profile traits.
- Add an Oblivion resource layer and built-in script package parallel to the
  existing Morrowind-specific resources. Generic resources remain shared.
- Select profile-dependent behavior at subsystem boundaries rather than with
  scattered record-format checks.

### Record identity and runtime storage

- Add a stable `FormKey` identity containing source-plugin identity plus local
  FormID, with a separate namespace for runtime-created forms. Load-order
  indices will never become persistent identity.
- Resolve masters, overrides, deleted records, persistent references, and DLC
  load order through `FormKey`. This addresses the architectural mismatch
  tracked in <https://gitlab.com/OpenMW/openmw/-/issues/7002>.
- Add typed TES4 stores and runtime adapters for every official record family.
- Maintain a generated coverage registry with these states: `parsed`, `stored`,
  `resolved`, `instantiated`, `simulated`, `saved`, and `scenario-tested`.
- Unknown official subrecords may initially be preserved as opaque payloads,
  but may never be silently discarded. Every exception must be allowlisted
  with a reason and removal milestone.

### ObScript

- Transpile Oblivion's ObScript into the existing OpenMW Lua runtime through a
  data-driven command registry.
- Incorporate or independently reproduce the current upstream lexer/parser
  work, but validate it against all locally installed Oblivion and DLC scripts:
  <https://gitlab.com/OpenMW/openmw/-/merge_requests/5444>.
- Give object, global, quest, magic-effect, and dialogue-result scripts explicit
  execution contexts and persistent local-variable storage.
- Unsupported commands and conditions must emit structured failures containing
  script, line, command, context, and owning form. Final acceptance permits no
  reachable unsupported commands in official content.

### Persistence and developer interfaces

- Extend the OpenMW save header with game profile, content fingerprints, and
  TES4 save-schema version.
- Add TES4 save records for stable references, runtime-created forms, player
  state, actor state, inventories, script locals, quests, effects, world state,
  and UI state.
- Keep Morrowind save records and loading behavior unchanged; reject cross-game
  save loading with a clear diagnostic.
- Stabilize the preparatory handbook and MCP work, then extend the developer
  tooling with read-only ESM4 operations: `audit_content`, `inspect_form`,
  `inspect_cell`, `inspect_script`, `inspect_dialogue`, `inspect_quest`, and
  `compare_runtime_state`.
- Add a scenario runner accepting a declarative manifest containing profile,
  content list, seed, clock, start state, input actions, state assertions,
  expected logs, capture points, and performance budgets. It emits JSON,
  screenshots, video, and timing data and returns nonzero on failure.

## 3. Universal verification contract

These gates apply to every milestone in addition to its specific tests.

### Automated correctness

- Build Debug and RelWithDebInfo configurations with GCC and Clang when
  available.
- Run the complete existing CTest, Lua, OpenMW integration, and OpenCS suites.
- Run affected tests under ASan and UBSan; parsing and script frontends also run
  fuzz and malformed-input corpora.
- Add hermetic synthetic TES4 fixtures to the repository. Proprietary Bethesda
  data, saves, scripts, screenshots, and videos remain local.
- Run the full installed Oblivion/DLC content audit using a fingerprinted data
  manifest.
- Fail on crashes, assertion failures, unclassified parser omissions,
  unresolved references not present in the approved vanilla-data exception
  list, unsupported commands reached by a scenario, or unexpected error logs.

### Deterministic runtime inspection

- Run scenarios with fixed random seed, time, weather, resolution, graphics
  settings, and input timing.
- Record authoritative JSON state: profile, cell/worldspace, position, active
  forms, actor values, inventory, quest stages, globals, script locals, effects,
  AI packages, and relevant UI state.
- Compare save-before/save-after and reload state structurally, not only by file
  bytes.
- Acceptance campaign actions must use normal input, activation, dialogue,
  combat, and travel paths. Direct quest-stage manipulation is allowed only in
  isolated subsystem tests, never in campaign completion tests.

### Visual inspection

Each visual milestone must pass both automated and agent-performed inspection:

- Capture deterministic screenshots through Xvfb at fixed cameras and UI
  states.
- For OpenMW regression images, require:
  - UI/synthetic scenes: at most 0.1% unmasked pixel difference.
  - 3D scenes: SSIM at least 0.995 and perceptual-hash distance at most 4.
- Automatically detect missing textures, empty or black frames, NaNs, broken
  alpha, invalid bounds, missing expected forms, clipped required UI controls,
  unreadable text, and shader errors.
- Capture object-ID and depth buffers for geometry-presence and occlusion
  assertions.
- Run the original executable under an isolated Proton prefix as the behavioral
  and visual oracle. Compare paired captures, UI bounds, landmarks, animation
  phases, audio/video selection, and scene composition. Cross-engine pixel
  identity is not required.
- Codex must inspect each paired montage directly and record pass/fail notes. A
  changed golden image requires a reviewed before/after montage and an
  explanation in source-controlled test metadata.

### Morrowind regression gate

- Run the complete existing Morrowind integration suite.
- Run deterministic Morrowind scenarios covering new game/Seyda Neen, dialogue
  and quest progression, combat/magic, inventory/UI, exterior travel, and
  save/load.
- Compare Morrowind visual goldens and runtime-state JSON.
- Do not accept an unexplained regression above 5% in median frame time, p95
  frame time, loading time, or memory.
- Shared refactors require characterization tests before behavior is moved.

## 4. Iterative milestone roadmap

### Phase A - Engineering controls and standalone foundation

#### M0 - Reproducible baseline and acceptance harness

**Deliver:** Stabilize the handbook/MCP preparation, add the scenario runner,
original-game Proton capture driver, content fingerprinting, structured log
checker, image comparison pipeline, and compatibility report generator.

**Verify:** Capture current Morrowind and Oblivion baselines; scan every
installed official ESM/ESP/BSA; demonstrate an intentionally failing scenario,
visual diff, unsupported-record report, and Morrowind regression report.

**Success:** One command produces a complete JSON/HTML baseline with builds,
tests, record counts, logs, screenshots, timings, data hashes, and current
capability gaps.

#### M1 - Strict TES4 parsing and record census

**Deliver:** Complete official Oblivion parsing for settings, globals, classes,
factions, magic, dialogue, quests, scripts, AI packages, pathgrids, regions,
climates, weather, water, worldspaces, cells, references, actors, and items.
Preserve unknown data explicitly.

**Verify:** Synthetic fixture for every record/subrecord variant, compressed
records, extended-size subrecords, nested groups, malformed input, and string
encoding; ASan/UBSan and fuzz runs; full original-data census.

**Success:** Every record and subrecord in the installed official content is
parsed or explicitly allowlisted; repeated audits produce identical counts; no
silent skips or parser crashes.

#### M2 - Form identity, masters, and overrides

**Deliver:** Implement `FormKey`, master remapping, load-order resolution,
override/deletion semantics, persistent/temporary children, enable-parent
relationships, and runtime-created identities.

**Verify:** Synthetic multi-plugin override matrices plus all official DLC load
orders; randomized load-order tests; reference-graph validation before and
after save/load.

**Success:** Every official reference resolves to the intended winning record
or a documented vanilla exception; identities remain stable after load-order
index changes and save reloads.

#### M3 - Oblivion game profile and standalone boot

**Deliver:** Remove Morrowind-only startup assumptions; add Oblivion settings,
globals, time/calendar, player bootstrap, resource selection, archive defaults,
and direct-cell startup.

**Verify:** Launch with only Oblivion archives and `Oblivion.esm`; start
deterministic runs in `ImperialDungeon01` and an exterior worldspace; validate
profile auto-detection and wrong-profile diagnostics.

**Success:** The executable enters a TES4 cell without any Morrowind master or
Morrowind-specific resource package, with no missing-Morrowind-GMST/player
failures. Morrowind still auto-detects and boots unchanged.

#### M4 - TES4 runtime state and native saves

**Deliver:** Persist cell/reference enablement, transforms, ownership, locks,
inventories, globals, time, player position, dynamic forms, and per-reference
custom state.

**Verify:** Mutate each state through runtime APIs, save, restart, reload, and
compare canonical state JSON; test plugin reorder/removal diagnostics and
corrupt-save handling.

**Success:** State round-trips exactly across interior/exterior transitions and
process restarts; Morrowind saves retain their existing schema and behavior.

### Phase B - A complete traversable world

#### M5 - First interactive interior slice

**Deliver:** Make the prison cell a bounded playable slice with a placeholder
player, collision, activation, animated doors, teleport doors, containers,
books, flora, locks, ownership, and sounds.

**Verify:** Automated walk/activate/loot/read/open/transition scenario;
collision probes; paired original/OpenMW captures from fixed prison viewpoints.

**Success:** The scenario can traverse the selected prison cells without
console intervention, falling through geometry, missing required objects, or
incorrect activation state.

#### M6 - ObScript frontend

**Deliver:** Implement deterministic lexing, parsing, semantic analysis,
transpilation, diagnostics, and compilation caching. Keep execution disabled
except for synthetic tests.

**Verify:** Compare AST and emitted Lua against an independent test parser for
every base-game and DLC script; property-test whitespace, labels, expressions,
references, and control flow.

**Success:** Zero parse/transpile failures across installed official scripts;
identical output across repeated runs; every command and condition appears in
the coverage registry.

#### M7 - ObScript execution and event model

**Deliver:** Add object/global/quest/effect/dialogue contexts, persistent locals,
event dispatch, reference lookup, control flow, timing, and initial
world-manipulation commands. Implement the complete command subset reached by
the tutorial cells.

**Verify:** Synthetic event-order tests, save/reload of locals, re-entrancy
tests, and tutorial interaction traces compared with the original executable.

**Success:** No unsupported command or event occurs on the prison/tutorial
path; scripts enable, disable, move, activate, and transition objects in the
correct order.

#### M8 - Static rendering, animation, and collision

**Deliver:** Complete Oblivion static/item NIF materials, texture transforms,
normal/parallax/glow maps, alpha/refraction, particles, object controllers,
door/trap animation, and Bethesda collision-shape interpretation.

**Verify:** Batch-load every official static/item NIF; render a curated asset
gallery; collision-shape probes; image and depth-buffer comparisons for
interiors, ruins, caves, clutter, doors, and traps.

**Success:** No unsupported block affects a used static asset; gallery and
gameplay scenes contain no invisible geometry, missing materials, major
lighting errors, or collision mismatches.

#### M9 - Exterior worldspaces, terrain, LOD, and vegetation

**Deliver:** Complete LAND blending, worldspace inheritance, roads, distant
terrain/objects, paging, water placement, trees, grass, SpeedTree-style
billboards, cell borders, and map markers. Explicitly resolve known foliage and
paging limitations tracked in <https://gitlab.com/OpenMW/openmw/-/issues/6388>.

**Verify:** Deterministic routes from the sewer exit around Imperial City and
through multiple terrain biomes; seam and LOD-transition detectors; aerial
depth-buffer sweeps; original/OpenMW landscape montages.

**Success:** Continuous travel crosses cells without holes or incorrect world
transforms; required vegetation appears and transitions correctly; no
persistent terrain seams or severe paging pop-in.

#### M10 - Environment, weather, audio, and video

**Deliver:** Implement climates, weather transitions, sky, sun/moons, fog,
interior lighting, water appearance, precipitation, ambient regions, music
states, positional sounds, voice playback, and intro/menu/loading video
playback.

**Verify:** Accelerated deterministic 24-hour cycles in representative
interiors and exteriors; weather transition matrix; archive-to-playback
audio/video tests; visual and waveform captures.

**Success:** Every official climate/weather/media asset used by base content is
selected and played correctly; scene lighting and visibility follow cell and
weather data without missing resources.

### Phase C - Player, actors, and gameplay systems

#### M11 - Actor appearance, skeletons, and animation

**Deliver:** Add race/sex FaceGen, head/hair/eyes/skin, biped slots, equipment
attachment, first/third-person skeletons, skinning, KF animation groups,
facial/lip animation, creatures, death poses, and ragdolls.

**Verify:** Race-by-sex appearance gallery, armor-slot matrix,
animation-transition suite, representative creature roster, voice/lip captures,
and save/load during animations.

**Success:** Every player race and all actor/creature skeleton families used by
official content render and animate without missing body parts, invalid
transforms, or animation deadlocks.

#### M12 - Player statistics, controls, movement, and character creation backend

**Deliver:** Implement Oblivion attributes, skills, derived values,
race/class/birthsign data, movement modes, fatigue, encumbrance, swimming,
camera behavior, activation reach, controls, and character-generation state.

**Verify:** Formula/property tests against original-game probes;
keyboard/mouse and gamepad input playback; movement courses;
first/third-person visual captures.

**Success:** A configured character can move, jump, swim, activate, and
transition through the tutorial and exterior world while exposing correct
stable stats across save/load.

#### M13 - Items, inventory, equipment, locks, and economy

**Deliver:** Implement TES4 inventory stores, stacks, equipment slots, hotkeys,
condition, ammunition, books/scrolls/keys, containers, harvesting, locks/traps,
ownership, barter, repair, recharge, and training backends.

**Verify:** Item-type matrix, equip/unequip visuals, transfer and stack property
tests, lock/key/trap scenarios, economic formula probes, and save/load after
every operation.

**Success:** Tutorial and market scenarios can acquire, equip, use, sell,
repair, and persist every official item category without duplication, loss, or
TES3 behavior leakage.

#### M14 - Navigation, detection, and AI packages

**Deliver:** Interpret TES4 pathgrids and package data; implement high/low actor
processing, schedules, wander, travel, follow, escort, eat, sleep, use-item,
flee, pursue, dialogue approach, companions, and horses.

**Verify:** Synthetic pathgrid maps, door/cell transition navigation, 24-hour
NPC schedule replay, obstruction recovery, companion travel, and deterministic
detection probes.

**Success:** Tutorial escorts and representative city populations complete
their expected schedules and transitions without stalls, teleport loops, or
package-order errors.

#### M15 - Combat, stealth, crime, and jail

**Deliver:** Implement melee, ranged combat, blocking, armor/damage, stagger,
knockdown, weapon condition, projectiles, creature attacks,
death/ragdoll/loot, sneak, pickpocket, trespass, theft, assault, murder, bounty,
guards, surrender, and jail.

**Verify:** Formula matrices, controlled duels, ranged trajectories, dungeon
combat, stealth visibility/noise tests, crime witness permutations,
arrest/jail scenarios, and combat visual/audio capture.

**Success:** The tutorial combat, a representative dungeon, an Arena match,
and every crime-resolution path complete through normal gameplay with expected
state and persistence.

#### M16 - Magic, alchemy, enchantment, disease, and progression

**Deliver:** Implement MGEF/SPEL/ENCH semantics, delivery ranges, projectiles,
area effects, resistance/reflect/absorb, active effects, scripted effects,
poisons, diseases, vampirism, soul gems, alchemy, enchanting, skill use,
leveling, and level-up choices.

**Verify:** Generated effect matrix covering target/range/stacking/resistance
combinations; original formula probes; visual captures for every effect family;
long-duration effect save/load tests.

**Success:** Every official magic effect is implemented or proven unused;
representative Mage Guild, alchemy, enchanting, disease, and leveling scenarios
match intended outcomes.

#### M17 - Dialogue, voice, persuasion, and services

**Deliver:** Implement DIAL/INFO ordering and conditions, greetings, topics,
choices, rumors, disposition, voice/lip/subtitles, persuasion, and all
service-menu integrations. Execute dialogue result scripts.

**Verify:** Evaluate every official INFO condition through unit or
content-driven tests; compare representative dialogue traces with the original;
capture every dialogue/service UI state.

**Success:** Tutorial conversations and representative quest/service dialogues
select the correct responses, play correct voice assets, mutate state correctly,
and expose no unsupported condition.

#### M18 - Quests, objectives, journal, and markers

**Deliver:** Implement QUST stages, objectives, result scripts, quest variables,
branching, journal history, active quest selection, target markers,
failure/completion, and script-driven scene progression.

**Verify:** Synthetic branching quest; a compact official side quest from
discovery through alternative endings; save/reload at each stage; quest-state
graph comparison with original traces.

**Success:** The selected official quest completes through every consequential
branch without console commands, skipped scenes, duplicate journal entries, or
stale markers.

#### M19 - Complete Oblivion UI and new-game experience

**Deliver:** Implement Oblivion-profile main/pause/loading menus, HUD,
subtitles, character creation, inventory, stats, magic, map, journal, dialogue,
barter, containers, repair, recharge, alchemy, enchanting, lockpicking,
persuasion, wait/sleep, level-up, controls, and gamepad navigation.

**Verify:** Screenshot every UI state at multiple aspect ratios and UI scales;
automated focus/navigation and text-overflow checks; paired original/OpenMW
montages; complete new-game input replay.

**Success:** A fresh launch proceeds through intro, menus, character creation,
prison tutorial, sewer exit, save, quit, and reload using only normal user
input, with all required UI accessible and visually accepted.

### Phase D - Base-game completion

#### M20 - Base-game engine coverage closure

**Deliver:** Use full-content traces to implement every remaining record field,
command, condition, event, animation, mechanic, and UI path reached by base
Oblivion.

**Verify:** Run the complete coverage registry, all official scripts, dialogue
conditions, quest graphs, asset census, and exploratory scenario sweeps.

**Success:** Base-game audit has zero unclassified omissions and zero reachable
stubs; every exception is either demonstrated unreachable or assigned to a
later DLC milestone.

#### M21 - Main quest campaign

**Deliver:** Fix compatibility gaps uncovered from the prison through Kvatch,
Cloud Ruler Temple, Oblivion gates, Great Sigil Stone, Paradise, and the Temple
of the One finale.

**Verify:** Chained normal-input campaign scenarios with save checkpoints; fast
pre-merge segment runs plus a nightly reconstruction from fresh new game;
original quest/state/visual comparisons at every major stage.

**Success:** The main quest completes end-to-end without cheats, stage
injection, deadlocks, missing scenes, or unsupported functionality; ending
state survives reload.

#### M22 - Guilds and Arena

**Deliver:** Close compatibility gaps for Fighters Guild, Mages Guild, Thieves
Guild, Dark Brotherhood, and Arena quest lines, including membership, rank,
expulsion, rewards, and branching outcomes.

**Verify:** One complete canonical campaign per faction plus separate scenarios
for consequential branches, failures, expulsions, and re-entry where supported.

**Success:** Every faction and Arena quest reaches a valid terminal state
through normal gameplay and awards the expected rank, items, spells,
reputation, and world changes.

#### M23 - Daedric, city, and miscellaneous base quests

**Deliver:** Close all remaining base-game quest, world encounter, settlement,
shrine, house, training, and miscellaneous mechanic gaps.

**Verify:** Generate a manifest from all base QUST records; classify
internal/non-player quests; run at least one normal completion path for every
playable quest and every consequential alternative branch.

**Success:** Every playable base-game quest has a passing completion trace; all
base QUST records are classified and covered; no unresolved base-game
compatibility entry remains.

### Phase E - Official expansions and final release

#### M24 - Shivering Isles

**Deliver:** Implement Shivering Isles-specific world, weather, creatures,
materials, scripts, mechanics, UI, main quest, and side content.

**Verify:** Full expansion audit; gate-entry scenario; main campaign;
Mania/Dementia branch coverage; every playable expansion quest; representative
realm-wide visual routes.

**Success:** Shivering Isles main and side content completes normally from a
base-game save and persists correctly after returning to Cyrodiil.

#### M25 - Knights of the Nine

**Deliver:** Close pilgrimage, infamy, shrine, relic, equipment, companion,
quest, and final-battle compatibility gaps.

**Verify:** Clean and high-infamy entry cases, shrine sequence, all relic
acquisition paths, final battle, post-completion state, and save/load checks.

**Success:** The complete Knights campaign functions through normal gameplay
with correct world, faction, equipment, and quest state.

#### M26 - Remaining official DLC

**Deliver:** Complete Horse Armor, Orrery, Wizard's Tower, Thieves Den,
Mehrunes' Razor, Vile Lair, Spell Tomes, and Battlehorn Castle.

**Verify:** Per-plugin load-order audit, acquisition/upgrade/quest scenarios,
asset and UI captures, cross-DLC interaction tests, and a combined all-plugins
campaign profile.

**Success:** Every installed official plugin's playable content and upgrades
are accessible and completable both individually and in the canonical combined
load order.

#### M27 - Hardening and release candidate

**Deliver:** Remove temporary compatibility switches and completed allowlists;
optimize startup, streaming, rendering, scripts, AI, and saves; finalize
launcher detection, packaging, documentation, diagnostics, and upgrade
handling.

**Verify:**

- Full clean build and test matrix.
- Entire base game and DLC campaign suite.
- Eight-hour automated travel/combat/quest soak.
- One hundred alternating save/load cycles.
- ASan/UBSan long runs and parser fuzz corpus.
- Corrupt/missing/reordered content tests.
- Complete visual review catalog.
- Morrowind campaign and performance regression suite.
- Fixed-route 1080p High benchmark on the current host.

**Success:**

- The executable detects the installed Oblivion data and starts without
  Morrowind files.
- New game, main quest, guilds, Arena, all base quests, Shivering Isles,
  Knights, and every installed official DLC pass their canonical campaigns.
- No reachable official record, script command, condition, mechanic, UI state,
  or media path remains unsupported.
- No unexplained error logs, crashes, sanitizer findings, save corruption, or
  memory growth above 5% after soak warm-up.
- Fixed-route Oblivion performance reaches a median of at least 60 FPS and 1%
  low of at least 45 FPS at 1080p High on the GTX 1080 Ti host.
- Morrowind behavior, saves, visuals, and performance pass the universal
  regression gate.

## 5. Assumptions and boundaries

- Target content is the locally installed English Oblivion 1.2.0416 GOTY data,
  then Shivering Isles, Knights, and all installed smaller official plugins.
- Behavioral parity is required; pixel-perfect rendering and reproduction of
  original-engine bugs are not.
- Saves are OpenMW-native. Importing original `.ess` saves is outside scope.
- Third-party Oblivion mod compatibility and OpenMW-CS TES4 editing are outside
  the initial program; architecture must not deliberately prevent later
  support.
- Oblivion Remastered is outside scope.
- Linux is the primary runtime/release target; existing OpenMW cross-platform
  compilation must remain healthy.
- Original proprietary content and reference captures remain local and are
  addressed by paths and hashes supplied at test time.
- The fork may periodically rebase or cherry-pick relevant upstream work, but
  no milestone depends on an upstream merge.
- Existing preparatory documentation and MCP changes are preserved and
  completed in M0.

## 6. Progress log

| Milestone | State | Evidence |
| --- | --- | --- |
| M0 | Complete | `docs/oblivion/M0-BASELINE.md` |
| M1 | Complete, with documented sanitizer-host exception | `docs/oblivion/M1-TES4-PARSING.md` |
| M2 | Complete | `docs/oblivion/M2-FORM-IDENTITY.md` |
| M3 | In progress | `docs/oblivion/M3-STANDALONE-BOOT.md` |
| M4 | In progress | `docs/oblivion/M4-RUNTIME-STATE.md` |

Update this table only after the milestone's deliver, verification, and success
criteria have all passed. Detailed command output and visual review records
belong in source-controlled milestone reports under `docs/oblivion/`.
