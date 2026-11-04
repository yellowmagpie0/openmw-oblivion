# Shortest path to a playable tutorial chunk

## Current handoff — completed 2026-11-04

The scoped melee tutorial slice is implemented and verified: normal prison
progression through the first assassin ambush and escort departure into the
tunnels, with Player melee/block participation and fresh combat save/load.
Audio, projectiles and general loot remain deferred. Full M15/S7/E1 and the
complete tutorial through the sewer exit retain their separate acceptance gates.

The course covers original dialogue/switch/doors and control release, actual
owned Blades weapons and bound assassin gear, Player punch and native retaliation,
novice unarmed blocking, Renault's authored mortal death, four assassin deaths,
original stages 27–29, the physical gate, authorized locked XTEL traversal,
noncombat low-process continuation and Glenroy's original arrival callback for
stage 30. All three escorts remain alive in the next tunnel cell; the Player
is alive with combat/block released. Pending action, death and package queues
are empty. Renault's death count is one, his essential override is false, and
dead assassins retain no bound equipment. Inspected captures show real actors,
Player combat pose, persistent corpses and the rendered final room/Player/HUD.

Acceptance and exact provenance are recorded in
[TUTORIAL-MELEE-SLICE-VERIFICATION.json](TUTORIAL-MELEE-SLICE-VERIFICATION.json).
All 11 scoped requirements are proven; no acceptance work remains for this chunk.
The current Player position can obstruct Uriel during the test course; ordinary
forward movement clears it. The editable course observes an 80-unit Player step.
Mechanical linked NPC conversation is synchronous; audio pacing was excluded.

Evidence under `build/oblivion-compat/tutorial-melee/`:

- `find-conversation-range-integration-01`: all 2,142 normal engine tests pass,
  exact inventory, no failures/skips. Source fingerprint
  `b35ed0dcec748fe142b7355cc5b2d80acd58a1d17e6d952610ae63db4ea71649`.
- `player-course-sanitize-engine-02`: all 2,142 ASan/UBSan engine tests pass,
  exact inventory, no failures/skips; engine leak checking enabled. Fingerprint
  `26c30cb6332836d01bfa411250f7b80ca5629d443cff1747f992421d336e660e`.
  Component 3,241 and Python 549 passes are reused with their original provenance
  in the verification artifact; they are not relabeled as current-source runs.
- `player-intervention-live-12`: real Player punch, native retaliation and novice
  unarmed block. The block has zero absorption, a 20-point fatigue debit and
  `process_action=6`; advanced `blocked_dispatch` is false at novice skill.
  Midcombat save SHA256 `d4a0e70047e2f0389c14570e6a66b1badd348cf9b82534a92be9475ea98db01f`.
- `composite-player-departure-verification.json`: normal course to original
  stage 30, including exact whole-live-state comparisons across fresh processes,
  no consumed hit/death replay, actual marker completion and callback exactly once.
  Final save SHA256 `e233d69cba63b35e7b38125eb8da833d094d366005737395f7486f05706f388a`.
- `sanitized-composite-departure-verification.json`: fresh sanitized combat
  continuation through the same fight, movement, gate and all three tunnel
  entries, then exact fresh low-process save/load and original stage 30.
  `player-sanitized-low-continuation-01` passes, exit 0, no unreviewed errors;
  its final capture was inspected. Final save SHA256
  `987bb1c811fa031230793c51de232282f1eadaa2b6f0e9522289b43d8039f7a0`.
  Runtime leak checking was disabled; this is separate from the engine leak gate.
- `opening-course-audit.json`, `read-only-course-events-audit.json` and
  `lua-observer-source-audit.json` retain legitimate progression/input provenance.
  The earlier `escort-stage30-restart-01` also passes a fresh stage-30 reload
  without old conversation/result/departure callback replay.

These are explicit composite proofs. Normal run 04 failed an extra path-node
arrival assertion; sanitizer run 01 exceeded its startup allowance and run 02
exceeded its 2,400-second stage-30 wait while Glenroy was still making healthy
progress. All failures remain failed in their original directories. Genuine F5
saves supplied the successful fresh continuations; no health, quest, position or
completion state was injected. The reusable full sanitizer manifest now permits
3,600 seconds for stage 30, a timing-only allowance based on observed completion;
that revised timeout was not rerun as a single process. C++/Python implementation
is unchanged after the recorded engine gates; later edits are docs/manifests only.

### Try the normal checkpoint

From the checkout root, this uses the normal binary and a separate save directory,
so F5 does not overwrite the retained evidence saves. The input is the genuine
stage-24 checkpoint: Renault has died through his original script and all four
assassins remain alive. The configuration is the one used by the passing normal
continuation; its additional Player observer has no world-changing handlers.

```bash
TUTORIAL_PREVIEW_ROOT="$PWD/build/oblivion-compat/tutorial-melee"
build/openmw --replace=config \
  --config "$TUTORIAL_PREVIEW_ROOT/player-stage27-moving-continuation-01/config" \
  --user-data "$TUTORIAL_PREVIEW_ROOT/playable-userdata" \
  --load-savegame "$TUTORIAL_PREVIEW_ROOT/quest-stage-renault-death-01/userdata/saves/Bendu_Olo/Quicksave.omwsave" \
  --no-sound
```

F draws fists, left mouse punches, right mouse blocks, and Shift+W runs. Follow
the escorts after combat and leave room for Uriel to walk past. Novice unarmed
blocking of an armed hit does not absorb weapon damage in the accepted course.
The stage-30 end save is retained in
`player-stage27-moving-continuation-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.

## Historical implementation evidence — 2026-11-03/04

### Player combat and restart checkpoint — 2026-11-04

The Player course exposed a directed-controller bookkeeping gap after another
actor's StopCombat. Melee AI re-establishes its validated current-target edge
before publishing intent; strict RuntimeState validation is unchanged. The
actual-contact/AI continuation regression covers 20 scenarios across two load
layouts, including peaceful-NPC exclusion and authored OnHit StopCombat priority.
`directed-combat-bookkeeping-integration-01` passes 2,141 engine cases at its
original fingerprint 818d82e1f26ee7a3075a8cebbc1969c5f1a114cc585c580774d51add0a6005ce.
The later ranged-Find change is covered by 2,142 normal/SAN tests above.

`player-combat-restart-departure-02` loads the genuine live12 midcombat save,
restores the entire independent live snapshot exactly and completes four new
deaths, but stops at stage 26 on Uriel's post-fight route. The ranged-Find fix
ends conversation approach at authored visible range instead of walking into a
moving partner. Run 03 passes original stages 27/28/29 and both guard cell transitions, but
its 400-second departure wait is too short and Player remains in Uriel's path.
Neither course is whole-course acceptance. Original normal F5 diagnostics and
intentional-stop receipts are retained. `player-course-sanitize-engine-01` was
stopped before changing source for this observed failure, not counted as a pass.

Run 04 uses only ordinary mouse/keyboard events after victory; Player moves 90 units
and Uriel advances. Its unnecessary specific path-node arrival gate fails and
it is stopped with exit 130. The genuine normal F5 checkpoint (SHA256
`e4f009a4536399cf294c953922e509577e9fe2fb69d8c709072a8d20d53832f9`)
continues in `player-stage27-moving-continuation-01`, which passes original
stage 30, exit 0, no forbidden/unreviewed errors. The composite verifier checks
both distinct-process whole-live-state boundaries, new callbacks 2–5, no old
contact/death replay, Player release/alive state, all three escorts in the next cell, bound cleanup and
Renault's death count of one. Final capture was inspected. The corrected whole course uses
normal W+Shift with a read-only 80-unit displacement gate and a 900-second stage-30
wait; no camera calibration, quest, completion, position or health injection.

### NPC conversation and escort departure checkpoint — 2026-11-04

Reusable evidence under `build/oblivion-compat/tutorial-melee/`:

- `essential-base-integration-01`: all2133 engine +3241 component cases pass;
  `essential-base-python-01`: all549 Python cases pass (original provenance below).
- `quest-stage-progress-integration-01`: all2134 engine cases pass. Original
  executable probes establish SetEssential (24 cases) and max-stage/repeat
  semantics (10 requests); see the historical records below.
- `quest-stage-renault-death-01` and `renault-corpse-restart-01` pass normal authored
  death and fresh GetStage24/GetDead1, with visible persistent corpse, one death
  count and no old result/hit/death replay.
- `hostile-hit-response-integration-01`: all2134 engine cases pass, fingerprint
  `b7aca9491190efc3e8bfea49dc7a4998ca9059ebb8cf6bf78cd25b55444748f7`.
  `escort-retaliation-02` passes actual Glenroy/Baurus hit responses and melee
  contacts. They were unarmed despite owned swords. Assassin OnHit legitimately
  requests completed stage24; the corrected gate forbids result replay rather
  than that request. Failed01 and its original manifest are retained.
- `combat-equipment-exit-focused-01` passes4/5 cases, including terminal-target
  cleanup, essential retention, prior AI and disarm policy. Its new weapon case
  passes selection/metadata but exposes unregistered low-level fixture contents.
  Focused02 retains a protected-helper compile error. Focused03 passes the
  corrected weapon case with public WorldModel registration and ownership
  assertions intact. No production registry workaround was added.

- `combat-equipment-exit-integration-01`: all2136 engine cases pass, fingerprint
  `dd34d1cc4eea5036d3d205a35c781e673ca6c2b383991c1ea5f23133c904bc3c`.
  Runtime game SHA256:
  `40a8db14b9a5b04b31a65107d40b0f62347221b9088bb04288057fc7fcbb5d4d`.
- `ambush-victory-01` passes normal sword combat, all four original death callbacks,
  stage26 and controller release. Inspected screenshot shows drawn swords and
  dead assassins with their bound equipment removed.
- `escort-resolution-01` is terminal: stage28 times out after300 seconds; fight
  gates pass and engine exits0. `post-fight-wait-01` passes a separate-process
  stage26/corpse/controller checkpoint plus60 seconds passive observation. Its
  motion comparison proves Baurus Find phase Complete without conversation.
  No quest or position mutation. Glenroy/Uriel travel remains unaccepted.

- `npc-conversation-focused-03`:2 linked-conversation tests pass (both load
  layouts, speaker0/1/2, ordered conditional links, result side effects, history
  serde, self/Player exclusion and cycle termination). Focused01/02 retain
  compile failures. Focused04 retains a headless Find test crash: the fixture
  lacked MechanicsManager for rendered approach, confirmed by GDB. Focused05
  passes the corrected Find low-process/LOS pending-action restore test; no
  production null-service bypass was added.
- `npc-conversation-departure-01`: currently running from `ambush-victory-01`.
  Original conversation27 and original SayTo/quest stages28/29 observed;30 is
  unaccepted until the course/save finishes. A declared normal F5 diagnostic
  checkpoint at27 proves further actor movement. Current game SHA256:
  `269718be7dc9686b31c29406c56515d483e3d78367a1ec96f8769c52775cd788`.

- `npc-conversation-integration-01`: all2139 engine tests pass, fingerprint
  `b816bde043dc977f87c6f513b50c5dc49ea4dc4dfe92e090a6bea4be08d5f91d`.
- `npc-conversation-departure-01/02` are terminal failures only at stage30.
  First01's normal stage29 save still has Glenroy Path/no-progress0.133s;
  second02's final save records Stalled/bounded-repath-exhausted from the old
  door bug. All original conversation27/28/29 results observed, no runtime errors.
  Independent installed door026c11 originZ-684.448 versus floor around-832
  demonstrates the elevated-origin problem. Physical gate03f1b0 lies in front.
- `player-intervention-focused-02`: existing acquired-contact test passes20
  scenarios (both layouts), including Player response, peaceful-NPC exclusion,
  scripted StopCombat suppression and consumed-hit restore. Focused01 retained
  a fixture-only nonplayer-value exception; GDB proved Player was accidentally
  included in a native-NPC confidence update, corrected without production bypass.
- `escort-door-approach-focused-01`:8 focused engine cases pass, including new
  cross-cell elevated-door/floor approach and post-door restore/completion test,
  prior same-cell disconnection handling, conversations and actual contact cases.
- `escort-door-departure-01` deliberately stopped (game SIGTERM, waiting launcher
  SIGINT, terminal130; reason receipt retained): input was already terminally
  stalled from old bug. Baurus visibly selected/opened blocking gate03f1b0, but
  no Glenroy departure acceptance. Do not mutate/restart its terminal saved AI.

- `escort-door-integration-01`: all2140 engine +549 Python tests pass,
  fingerprint `c1d68f7cd0d94932d4e6da6fcfc157ed6b1db24f7fbbfec481299c6c31865782`.
- `escort-door-departure-02` terminal1: actual gate03f1b0 opens and Glenroy
  reaches XTEL026c11, but ordinary activation rejects its lock despite AI owner
  authorization. Final save remains stage29/Path, not terminally stalled.
- `escort-owned-door-focused-01` retains a headless fixture Scene-null crash;
  GDB proves that fixture cannot test resident World cell movement. Focused02's
  renamed bounded test passes owner authorization, ordinary activation denial,
  abstract cell transition, retained lock and no early completion. Actual
  resident movement is required from the rendered course.
- `escort-owned-door-departure-01` confirms both guards' actual references and AI
  state move to cell022288, but remain High/Path at the landing. Normal F5 saved
  `after-exit-cell-checkpoint.omwsave`. The registry wrongly prevented abstract
  execution after removal from Actors. Known-failing wait deliberately stopped
  (game SIGTERM/launcher SIGINT, terminal130), reason receipt retained.
- `escort-inactive-continuation-focused-01`:43 engine cases pass across AI,
  doors, conversations and actual melee contacts. Latest production binary is
  built by this check; hash it when recording runtime provenance.

- `escort-inactive-departure-01` passes (270.66s), no unreviewed runtime errors.
  Normal save SHA256:
  `596a10eafdd3162f048813916070768a5c4975c221f929fc46a33ce7d17f87ce`.
  All3 escorts in022288; Glenroy Complete/Low at[1547.67,4492.43,-863.99],
  inside the authored Travel marker region. Original stage30 result runs once.
  Screenshot inspected: Player remains in emptied ambush room, corpses persist.
  Game SHA256 `1619ef8101be7665de970cb76b45e32ca2ca170fe4688a524414958a5fc5f9e7`.
- `escort-inactive-integration-01`: all2141 engine tests pass. Original source
  fingerprint is retained in its verification.json; Python549 unchanged from
  `escort-door-integration-01` (new declarative courses validate at execution).

At this checkpoint the remaining work was a fresh stage30 restart and normal
Player participation. Subsequent outcomes are in the current handoff above.

Original dialogue audit is ignored `post-fight-dialogue-audit.py/json`; xEdit
commit9fb016884bec138ea6c7b872cec831537d464c3e defines next-speaker0/1/2 as
Target/Self/Either. Mechanical conversation progression is synchronous; audio
pacing is excluded. Historical hashes, retained failures and details follow.


### Earlier tutorial implementation

The user has authorized implementation of a significant melee-focused tutorial
slice. Target normal prison progression through the first assassin ambush and
the authored post-encounter departure into the tunnels, with functional visuals,
Player participation, escort combat and save/restart continuation. Audio,
projectiles and general corpse-loot functionality are outside this slice;
implement normal item interaction only where the actual route requires it.
This is an explicit focused departure from full M15 stage order, not acceptance
of S4, S5, S6, S7 or full E1. The original review below is historical.

Current chunk: enable escort retaliation after a genuine directed hostile hit,
then verify actual guard contacts and post-combat resumption. Persistent base
SetEssential, Renault's true death, maximum quest-stage/repeat semantics and
fresh-process corpse/combat continuation now pass. The melee contact bridge now dispatches real
filtered OnHit scripts after damage. Focused `melee-hit-script-focused-04` passes
6 engine cases; `melee-hit-script-engine-integration-01` passes all2132 engine
cases. The subsequent diagnostic-only renderer change passes16 focused engine
cases in `melee-hit-scene-diagnostic-01`.

Actual `renault-hit-continuation-02` reaches Renault's original filtered OnHit
and CharacterGen stage24, but fails runtime acceptance: the save shows essential
knockout rather than death because stage20 SetEssential remained deferred.
The later renderer guard identifies Renault's invalid scene traversal while
actor/root/generation/callback/binding remain valid. Do not weaken that guard.
Retest the intended death after SetEssential works; dynamic torch attachment is
an unproven possible contributor to the traversal failure.

Schema84 extends existing shared actor-base overrides with an optional essential
flag. `essential-base-focused-02` passes1 engine regression (both content layouts):
cached/new references, signed command flag boundaries, base/reference scope,
wire roundtrip, restore, static-content fallback and real scripted death. The
first attempt passes6 existing focused cases and fails only the new test
fixture's missing cell identity; the fixture now uses its real CELL.
`essential-base-python-01` passes all549 Python cases at source fingerprint
`c03fc10427678c442ed4563410d59a140a23cb3faccb6cce60dba28206e07925`
(before the test-only fixture correction). Current game SHA256:
`6e00c81ca8a9b050466a3f820c098e33d0666ff6eabc675bfd005a11ad389d4b`.
Normal stage19 replay input `staircase-input.omwsave` still matches its recorded
SHA256 `ca941ca1bfc1ed0a0b4eb342a0bb82899976adec290b4bab4d66334cfa42dd83`;
`oblivion_m15_tutorial_essential_stage20.json` checks the original command.
`essential-base-integration-01` passes all2133 engine and all3241 component
cases, exact inventories, zero failures/skips, source fingerprint
`4c6c90a6a05928dad07ce73e2bc35bf13d2040ab4eaa8daad84d249efa119af8`.
`essential-stage20-01` passes normal stage19 movement and original package
completion to stage20; its F5 schema84 save contains Renault base02349f,
values empty, essential=false. Save SHA256
`4a2b5ee32cb6ebc2e90db54ab87905bf718ea4bd142f83190961cedb91a7b99c`.
No forbidden/unreviewed errors. A read-only screenshot and extra normal F5 were
recorded while waiting on package completion; the course subsequently reached
its original command and own F5/checkpoint normally. The first diagnostic
xdotool option was rejected before sending input; the corrected invocation is
recorded in `live-diagnostic-actions.json`. Full encounter acceptance remains
open. `essential-stage22-01` passes its fresh-process original stage22 course with
zero forbidden/unreviewed errors. Its schema84 F5 retains Renault essential=false
alongside authored aggression10 in the same base override. Save SHA256
`de7e142d51d974da042f2ddcdd390e2abde63647690bfb89c62061682f098773`.
`essential-renault-death-01` passes from that save through all four original
StartCombat callbacks, real Renault OnHit/Kill/stage24, a death animation and one
native death callback. F5 life phase is Dead with recovery0, essential=false;
corpse visible in `renault-death.png`. No forbidden/unreviewed errors or prior
renderer traversal failure. Save SHA256
`0d802f8f0428c02ce0c8a536ad9e7bf7eae550cc5b73a78ccc40fdc9bf34f440`.
Its quest current-stage23 despite completed24 exposed a separate real bug; do
not reuse it as accepted post24 progression. The A4 controller correctly puts
Baurus first at priority220, with dead Renault second at200; it is not stuck
on Renault at its front. Guards still lack their own retaliating controllers.

Original `quest-stage-oracle-01.json` passes10 request cases: Quest529AC0 keeps
the highest stage, Getter529750 reads it, and52B080 skips completed results
unless Quest flags8 allows repetition. Installed CharacterGen DATA flags0.
Quest modified virtual and individual result-entry execution are supplied.
Editable inputs: `m15_quest_stage_progress_oracle_cases.json`. SetStage now
preserves maximum current-stage and respects repeat permission. New nested
30->20->10/save/restore regression passes both layouts and repeat modes.
`quest-stage-progress-focused-01` passes16/17 cases; its sole failure was a
menu-ID fixture expecting stage30->20 rewind. Focused02 passes both new stage
and repaired menu cases; that fixture explicitly resets its quest between
independent menu runs and retains all ID/stale-callback assertions. `quest-stage-progress-integration-01` passes all2134 engine cases, zero
failures/skips and exact inventory, fingerprint
`f67c78f695f59dece88d3f815a82afd5ef1a9d00ae38d2d442130de84136238d`.
Game SHA256 `4344ca16ec8d11fa69bae0db29ebe86b5f7081119c28ce6c74d0bcdd4b240c79`.
`quest-stage-renault-death-01` passes the same normal stage22->ambush course:
Renault Dead/recovery0/essentialfalse, current stage24 and completed24, exactly2
initial stage23 result dispatches, 1 stage24 result, 1 Renault death callback.
No forbidden/unreviewed errors. F5 SHA256
`f8225e81fde4411c5cae83b8c1b23ac05a64376e4528c8b2e0dd852f3febc384`.
`renault-corpse-restart-01` passes fresh GetStage24/GetDead1 and normal F5
without old hit/death/stage results replay. Corpse screenshot inspected. Resave
SHA256 `9d744a1d98add987d8f2b467502c82630fc57ce829b4d9fb766d5b64e185e98a`;
lifeDead/recovery0/essentialfalse/current24 and native death count1 persist.

The next observed blocker is missing escort combat controllers. The committed
hit bridge now lets an alive, positive-confidence native NPC/creature acquire
its actual deliberate attacker through the existing StartCombat controller,
after OnHit script effects. Player input remains Player-owned. A graph edge or
incidental hit does not authorize this response, and a dead defender cannot
respond. This is bounded reactive melee policy, not full S7 hostility/flee AI.
`escort-confidence-content-audit.json` verifies Renault/Glenroy/Baurus confidence100
from hashed installed ESM AIDT; Uriel has80 but is not an authored attack target.
`hostile-hit-response-focused-03` passes the physical hit/script/ledger case
across7 scenarios and2 layouts: ordinary/filter/miss, hostile response, graph
only, scripted death and directed miss. Attempts01/02 retain a missing-include
compile failure and an incomplete Player profile in the enlarged test fixture;
focused03 supplies real race/class data for World capture. Live production
verification: `escort-retaliation-01`; inspect its live handle before starting
another game. Remaining: actual Player melee/block, guard fight completion and
escort departure, mid-combat restart with new guard controllers, fresh sanitizer
coverage. Ordinary corpse pose is visible; no new dynamic ragdoll gate is claimed. Manifest:
`oblivion_m15_tutorial_renault_death_from_stage22.json`. Original executable instruction probe
`essential-command-oracle-01.json` passes24 cases, hash
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`:
handler50B780 sets ACBS bit2 for positive int32, clears for zero/negative,
preserves other flags and marks base modified16. Argument extraction and modified
virtual are supplied boundaries; no runtime acceptance follows from this probe.
Editable inputs: `m15_set_essential_oracle_cases.json`. Pending: current schema
codec/service checks, normal stage20 replay, true death/renderer continuation,
Player melee/block, escort retaliation and departure, fresh-process encounter
restart, accumulated sanitizer coverage. Older stage23 saves must not be
silently repaired from quest stage; use an input before original stage20.

Last verified game (schema83) SHA256:
`622fd87650c4ccef9e8ec575e02b8ec9c3d3fbbd8066dca7babed9885c39900e`.
`bound-live-focused-02` builds the engine, game and esmtool and passes5 engine
cases with no failures/skips, fingerprint
`6a9ec88eca3e1f29f016f6fd12652bea62145d88bcefbfbf6d673847cdb2df33`
(before subsequent manifest/docs changes). The prior same-cell-door game was
schema82 SHA256 `062c298f06861081000ab80ec5504a29763ab536a098d3fc52753755ae2a7dd3`.
Current broad engine and fresh engine/runtime sanitizer coverage remain pending.
`same-cell-door-focused-03` passes8 engine cases, including a disconnected
same-cell XTEL route, rejection of a useless exit, save/reload before the door,
correct floor landing and no early/duplicate package completion in both content
layouts. `same-cell-door-engine-integration-01` passes all2126 engine cases,
zero failures/skips, source fingerprint
`c5dfd50b70159e5ec6213925019ee4fd4815d90cd2a7d7f62d52e9f4af876d3e`.
Focused01 retains a compile failure from same-line test-trace macros; focused02
retains the negative case that exposed straight abstract movement after route
failure. The fixed movement guard preserves that assertion. Earlier3232
component,544 Python and183 component sanitizer results retain their
[original provenance](#prior-faction-and-alert-checkpoint). Fresh sanitizer
engine/runtime coverage for accumulated changes remains required.

The missing route was a real door: `same-cell-door-content-audit.json` identifies
the installed ESM hash and paired XTEL references04f7a2/04f795, both in CELL01fbb9.
The former leads from the five-node holding component to the ledge above the
room. The AI had rejected every door route whose destination CELL matched its
source. Same-cell routing now checks a reachable source approach and a useful
landing, uses existing permissions and normal door activation, and replans after
XTEL even when the cell key stays unchanged. Low process uses the authored XTEL
landing and no longer moves straight after a failed route. No reference-specific
production rule, injected stage, direct package completion or marker warp was
added. No partial-navmesh/drop-policy change was needed: the actual continued
course reached the floor and all four callbacks with this bounded door fix.

Latest evidence under `build/oblivion-compat/tutorial-melee/`:

- `same-cell-door-ambush-01` passes the original ambush-entry course from the
  legitimate stage22 continuation. All four use door04f7a2 and finish their
  original packages, stage23 runs, four escort SetAlert1 calls apply, and all
  four original StartCombat callbacks succeed. Screenshot inspected; full
  checkpoint validation passes. Four controllers and four native engagements
  exist. No forbidden or unreviewed errors. Equipment/melee outcome is not
  accepted by this entry gate.
- `ambush-combat-restart-01` passes a fresh process, four IsInCombat1 queries,
  four escort GetIsAlerted1 queries, normal F5 resave and checkpoint validation.
  No StartCombat, SetAlert or stage23 result replay. `comparison.json` checks
  exact authored directed targets (A1/A2→Glenroy, A3→Baurus, A4→Renault), all
  four engagements and all four authored faction overrides before/after load.
- `ambush-combat-entry-input.omwsave` SHA256
  `96b7651c1dc7ff0dc2f524d4f5181c520ae0bc42f07d48f204b8293800ac8ddf`
  and `ambush-combat-restart-input.omwsave` SHA256
  `a8a9f8ad82dd88bcb6630ae37999eec2899d860a359d53cd5fd8d388e66e340a`
  have provenance JSON. Continue equipment/combat testing from these; retain
  earlier stage20/stage22 inputs for the eventual original-route integration.

Source schema83 now retains explicit bound ownership: inventory items carry
optional `BoundItemSource{spell,effect_index}`, and
`RuntimeState::mNativeBoundAbilities` maps each actor to owned spells and
ordinary displaced equipment. Both C++ and Python preserve those fields,
reject orphan/duplicate/nested ownership, invalid presence/counts and downgrade
loss, and agree with an independent literal wire recipe. Bound instances do
not merge with ordinary stock during native inventory normalization.
`bound-state-components-integration-02` passes all3234 component cases and
`bound-state-python-integration-03` passes all547 Python cases, zero failures or
skips, fingerprint
`5a7a7ef3726cb50acc600dbbaf0044e677b13a3e8006095007442b4a5c5b03b4`.
Focused02 passes3 codec/JSON cases. Failed attempts remain: component-integration01
found the old ordinary-inventory JSON expectation; Python preflight01 found
fixture upgrade defaults. Exact whole-state comparisons remain in place after
adding explicit empty ownership/null source defaults. Fixture recipes now target83.

Live projection and the World bound lifecycle are now built. Admission resolves
installed NPC/SPEL/MGEF/item inputs, prepares tagged inventory and its ownership
baseline together, stages pointer registration and publishes both before the
equipment observer. Repeat admission retains worn instances. Terminal death
cleanup precedes script update/death callbacks; essential knockout retains gear.
Save admission checks the installed graph and displaced ordinary definitions.
Pending save restoration suppresses grants until commit; the restore commit
refreshes inventory-owner SafePtrs and native armor/clothing caches.

The5 focused engine cases include the two definition cases and three live
integration cases: coherent owner/inventory/cache/equipment notification in both
content-index layouts, ordinary same-base separation and worn repeat admission,
terminal death versus essential knockout, and source removal/restoration plus
pre-mutation rejection of generic removal. This is headless lifecycle coverage;
actual encounter death and recovery remain open. `bound-live-focused-01` was
intentionally interrupted (build exit-2, no tests) to fix restore ordering before
finishing the broad build; compiled objects were reused in02.

`bound-live-legacy-migration-01` passes the original authored combat-state course
from preserved schema82 input, normal F5, schema83 checkpoint and all four combat/
four escort-alert queries, with no forbidden or unreviewed error findings.
`bound-equipment.json` additionally verifies all four real ability03e91b grants,
effect indices0/1/2, actual bases/slots/count1, distinct source ownership, ordinary
inventory conservation and displaced hood snapshots. `bound-live-restart-01`
passes a separate process from that schema83 save and the same gear/state checks
without duplicated grants, repairs or changed displacement. It also captures
`bound-equipment-restart.png`; the Emperor/escorts are visible, but the assassins
are outside that camera view. Their rendered gear is NOT accepted by that image.
`bound-assassins-view-01` passes normal forward movement to y2709, but its
image still misses the assassins. View02 retains a failed lateral waypoint at a
solid boundary (stopped near x0); no position mutation or collision bypass.
View03 passes a read-only Lua camera-heading gate driven by the normal right
stick. Its inspected `bound-assassins-view.png` shows black bound body armor on
the assassins. Held-mace presentation and encounter outcomes remain open; committed NPC melee is verified below.
Prior01/02 source manifests are retained beside their evidence. View02 also
records actual loaded MyGUI/SDL library paths and hashes in
`runtime-libraries.json`.

New immutable continuation: `ambush-bound-equipment-input.omwsave` SHA256
`5206e22e611b992af539ff8f8c3deff91dca249f6642b0c29d15e437f60220aa`, with
provenance JSON. Gear comparison helper/report are retained in the ignored
`check-bound-runtime.py` and each run's `bound-equipment.json`. Its restart wear
check permits combat wear but rejects an increase from a saved worn instance.
This is initial equipment/combat-state evidence, not positive Player melee or
encounter completion. The source resolver does not reinterpret these associated
items as VMODs or implement general M16 casting.

`bound-record-audit.json` identifies the installed ESM and confirms ability03e91b
AbBoundArmorMaceMD on bases014a27/014a28/017617/017619: MYTH→armor033525,
MYHL→helmet051b47 and BWMA→mace026274, each authored duration1000 and self range.
`bound-lifetime-oracle.json` passes79 hash-identified original-instruction cases;
editable outcomes are `m15_bound_ability_lifecycle_oracle_cases.json`. Already
applied Ability4 survives beyond duration1000 while timed Spell0 expires;
target death invokes removal. Initial bound updates apply once when their
materialization delay is reached. Target access, RTTI, enchantment exemption,
dead query and removal/materialization bodies are supplied boundaries. This
proves the lifetime/dispatch rules, **not inventory removal/restoration or actual
rendered equipment**. Vtable identity and disassembly stay in ignored evidence.

The pure inventory update plan now exists in
`components/esm4/boundequipment.{hpp,cpp}`. `prepareBoundEquipmentUpdate` takes
installed source-tagged grants and existing inventory/ability ownership, then
prepares replacement inventory and owners without live mutation. Repeat
admission preserves saved condition, charge and equipment state; missing or
changed owned effects reject instead of regenerating gear. Removal/death erases
only tagged instances, restores matching existing ordinary units, splits ordinary
stacks for equipment and never recreates a removed ordinary item. Multi-slot
ordinary equipment blocked by another remaining bound ability keeps its pending
restoration descriptor until that ability is removed too.

`bound-plan-focused-01` passes6 new component cases;
`bound-plan-components-01` passes all3240 component cases;
`bound-plan-sanitize-01` passes24 selected ASan/UBSan cases, zero failures/skips,
filter `ESM4BoundEquipment.*:ESM4RuntimeState.BoundEquipment83*:ESM4InventoryMechanics.*`.
All three use source fingerprint
`36a7575e1b9822d189cc2f05df1fd937833bd3f1ada517bce93a41473b407870`.
Sanitizer leak checking is disabled. This covers the pure inventory plan and
selected ownership/ordinary-inventory codecs, not engine or runtime behavior.
The prior547 Python results still cover the unchanged Python implementation.
Existing sanitizer build warnings remain; no sanitizer fault was reported.

`bound-record-header-audit.json` independently checks installed ESM
SHA256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`:
the bound armor, helmet and mace have zero record flags. The existing base
quest-item disarm guard therefore cannot by itself protect source ownership.
Supplementary hash-checked original disassembly shows materialization adding
extra types47/50 to the instance; the bounded equipped-drop guard is verified
below. Full contact integration remains pending. Do not infer blanket immunity
from their names. Detailed addresses/files remain in the ignored notes.

Independent equipped-drop guard evidence now passes4 original-instruction cases:
`bound-disarm-guard-oracle-05.py` and `bound-disarm-guard-oracle.json` execute
49108B..4911C5, native unequip5F2E70 admission/rejection, and actual count/worn
queries. Alive equipped CannotWear47/BoundArmor50 remains worn/count1 and makes
no recursive DropItem request. Controls without47 or with a dead actor reach
the ordinary body and recursive request. Actor virtual queries, RTTI, entry
lookup and uncontended locks are supplied; the admitted ordinary body is a
supplied control. No actual loose-reference allocation or mastery roll is
claimed. Editable source: `m15_bound_equipped_drop_guard_oracle_cases.json`.
Failed01/02 missed the lock boundary,03 mis-unwound the supplied control stack;
04 passed and05 made the absent47 control's linked list consistent.

Newest normal-input continuation: `ambush-bound-visible-input.omwsave`, with
provenance JSON. View03 actor-state summary confirms all seven combat actors
are alive/Active; all four assassins are drawn and approach their targets, with
A4 processAction3. The escorts remain undrawn. View03 also records actual autonomous mace
contacts: A3→Baurus health damage3.08366 and A4→Renault damage2.59863/2.59822,
fatigue debits7, native mace-condition writes4198.68/4192.08/4190.76, and an
armor wear write on Renault. These are committed native contacts, not synthetic
packets. Player contact, blocking and escort retaliation remain open. `actor-state-summary.json` retains
selected source fields. No scripted Kill or new quest stage was injected.

Observed next blocker: committed hits on Renault produce no native OnHit
script callback or Kill/stage24 trace. View03 script report has zero diagnostics
and no Kill/OnHit command counts; source search finds no native OnHit dispatch
in the melee/contact path. Wire the native callback after successful contact
publication, with correct attacker identity and once-only/save ordering, then
verify Renault's original script and escort retaliation/Player participation.
This takes priority over speculative movement work: all four assassins already
approach and make real contacts. The authored SetEssential requirement may then
become the next concrete dependency.

The protected bound-item disarm consequence is also pending before final combat
acceptance. Keep mastery RNG and action
interruption, but skip loose-reference materialization and empty/spare-weapon
reaction projection when the bound item stays equipped. Existing generic
removal/world-drop guards only reject before mutation; they are NOT the final
native disarm implementation. Do not label bound weapons globally immune or
suppress the entire disarm attempt. Detailed current notes and live handles are
`build/oblivion-compat/tutorial-melee/bound-live-followups.txt`.
Then test Player melee, Renault's authored death, essential recovery and escort
departure, choosing dependencies from the actual encounter. Stage20's required
SetEssential remains in the deferred set and needs checking for Renault. Fresh
broad engine/sanitizer integration remains required after the pending combat
changes; no engine/runtime sanitizer claim is made from the24 pure cases.
No full M15 stage or E1 is accepted.

## Historical implementation evidence

### Prior enable-parent checkpoint

Prior chunk: unblock Assassin4's authored stage22 Travel package, then admit
real bound equipment and verify the melee encounter. The parent-enable blocker
is fixed and verified in a fresh game process; original stage23 remains pending.

Prior game SHA256:
`bb8514d1d062d9eee40401cc41b8bdcd5c794eeed1331b7e18f71b8269f00724`.
`enable-parent-focused-02` passes9 engine cases, including native bit0/inversion,
chains, explicit cycle rejection, resident root enable/disable, script queries,
saved flags, disabled low-process suspension and both content-index layouts.
`enable-parent-engine-integration-01` passes all2125 engine cases, zero failures
or skips, source fingerprint
`f6914d5120aff5907d8fcc480e3772b3ace1ec71f6414ed1deaed08edd316492`.
Focused01 is retained as a failed missing-include build; it ran no tests.
The earlier3232 component,544 Python and183 component ASan/UBSan results remain
applicable with their [original provenance](#prior-faction-and-alert-checkpoint).
Fresh sanitizer engine/runtime coverage for the accumulated changes remains
required; prior component results do not cover the new World integration.

Implementation: `oblivionenablestate.{hpp,cpp}` resolves XESP chains against
static winning records, saved root flags and actual resident state. Only bit0
inverts the parent. Scene admission synchronizes RefData after respawn and
before insertion; live root edits propagate through actual scene membership.
Incoming save flags are normalized before restore; save capture and both
GetDisabled condition adapters share the rule. Low-process AI skips disabled
actors and uses effective enablement for actor destinations. No StartCombat
guard was relaxed and no tutorial reference is special-cased.

Latest gameplay evidence under `build/oblivion-compat/tutorial-melee/`:

- `ambush-enablement-01`: failed original stage23 wait, retained with zero
  forbidden/unreviewed errors. Normal movement from legitimate `stage22-input`
  reaches Player `[29.78,2121.80,-639]`. All four assassins query enabled; a
  normal manual F5 diagnostic save proves enabled flags plus native value/life
  owners. Screenshot `ambush-waiting.png` inspected. No quest/position edits.
- `enable-parent-restart-01`: passes separate-process four GetDisabled0 queries,
  normal F5 resave and full checkpoint validation. All four keep enabled flags,
  value/life owners and real native movement bodies with collision enabled and
  on-ground state. No enable/disable or SetAlert replay; stage22 retained.
- `ambush-waiting-input.omwsave` SHA256
  `550675a2674d636e842e444ea8ed183c407931c7ac0d44ab88f10d6634f13f3d`
  and `enable-parent-restart-input.omwsave` have matching provenance JSON.
  They are diagnostic normal-input continuations of the failed ambush course,
  not evidence that the encounter or full route passed. Earlier legitimate
  stage20/stage22 inputs remain in the prior checkpoint below.

Next action: resolve the observed Assassin4 route. Independent stock-record
inspection `ambush-packages-conditions.json` shows package0178cd is eligible
at stage>=22; the other three travel packages require stage>=23 and correctly
wait. Assassin4 remains near `[-2462,3177,-592]`, targeting marker0178c2 near
`[-246,3129,-832]`; the continuous navigator repeatedly returns a partial path.
`ambush-pathgrid.json` audits PGRD0150c6: nearest start268 lies in a five-node
component268–272, disconnected from destination78 even without saved overlays.
The only saved disabled node151 is outside that component. Determine how the
original authored course leaves this area before choosing a movement fix;
do not fabricate package completion, teleport to the marker or inject stage23.
No relocation for these four assassins was found in the inspected source
scripts/results; broad source search is not proof that no native relocation
policy exists. The actual bound armor/helmet/mace ability, Player melee,
Renault death, essential recovery, escort departure and combat restart still
need acceptance. No full M15 stage or E1 is accepted.


### Prior faction and alert checkpoint

Prior chunk: fix the linked assassins' native enablement, then admit their
real bound equipment and verify the melee encounter. SetFactionReaction executes
and persists through original stage22. SetAlert/GetIsAlerted now share saved
native process state and pass an isolated separate-process game probe; original
stage23 as a whole remains unaccepted.

Prior game SHA256:
`fc3b09f385e931f509cd62ddd0fd73ed0fdf06ae4ca01c7cf0d93e49e0ee2282`.
Normal checks pass3232 component,2123 engine and544 Python cases. Component
results are the successful mode of `alert-normal-integration-02` (overall
receipt retained with the then-stale dialogue test expectation). Component and
183-case ASan/UBSan runtime-state/chargen results in
`alert-sanitized-components-01` use source fingerprint
`40cbaf9483a662f44029151358f273b3b413b0d9297c293214eb51bce8346f4a`.
Only the dialogue test changed afterward: preserve unsupported-command failure
with SetForceRun, and separately verify SetAlert followed by SetStage through
both Say and SayTo. `alert-dialogue-regression-01` passes2 focused cases;
`alert-engine-integration-01` passes all2123 and
`alert-python-integration-01` passes544, fingerprint
`921f4d38e98883dcb3fb7a3c9fb14c69e64b5d5f86906088796effe1b74e2745`.
All verified selections have zero failures/skips. Failed broad attempts are
retained: alert-normal01 caught two independent wire offsets missing the new
absent flag; normal02 caught the old unsupported-SetAlert expectation. Their
corruption/failure coverage is retained in the corrected tests. Sanitizer leak
checks are disabled. Fresh sanitizer engine/runtime coverage remains required;
the183-case result covers components only.

Reusable inputs under `build/oblivion-compat/tutorial-melee/` (each has provenance):

- `stage22-input.omwsave`, SHA256
  `45d3cea49c83c6ee141379ad6cf6538917c0fc2ea0ff6796b37d4ec135fc4be6`:
  passing `faction-stage22-01`, original stage22 completion from the legitimate
  stage20 save under controller movement. Faction BladesCG01ee1e ->
  PlayerFaction01dbcd becomes30, then original EVP, wall Backward and Autosave
  run. No stage23 or script errors; screenshot/save inspected.
- `faction-restart-input.omwsave`, SHA256
  `94be1c58313ce30b4f3a6fef7a3b0533678072362254149bde35d72c7318b1ce`:
  passing `faction-restart-02`, separate-process GetFactionReaction30 and normal
  F5 resave. No setter replay, no stage23, no errors. Stage22 and positive
  Athletics2.639998 persist. Restart01 proved the query but pressed F5 too early;
  retained as failed missing-save evidence. A brief post-load wait fixes the
  input course without an engine change.
- `before-ambush-input.omwsave` remains the legitimate original stage20 fallback,
  SHA256 `24a65f1abca23e552c18a1476b22517515ab22d103367c485d950646daa898d7`.

The faction owner is a directed stable FACT-key map in RuntimeState v81.
Explicit zero shadows a removed authored relationship; getters and native
NPC disposition consume the same owner. Static winning FACT identity is checked
at save admission, including through detached incoming shared definitions.
The C++ and Python codecs preserve direction, int32 values and zero; reject
malformed order, duplicates, truncation and downgrade loss. Actor fixtures now
use a real native cell and both content-index layouts. No general ModFactionReaction
or full crime functionality is claimed.

The alert owner is optional `RuntimeActorValues::mProcessAlerted` in RuntimeState
v82. Signed-positive SetAlert writes an existing Low/Active owner; a missing
owner stays absent. Both GetIsAlerted condition adapters and the script query
read that owner, independently of TES3 alarm state. C++/Python codecs preserve
absent/false/true and reject downgrade loss and malformed flags. No automatic
perception/combat alert timer policy is claimed.

`alert-command-probe-01` and `alert-command-restart-01` both pass in the current
game: controlled SetAlert1/GetIsAlerted1 on Renault, normal F5 save, then a fresh
process query1 and resave without setter replay. Both checkpoint validators
pass schema82 with3663 actor AI rows; Renault's saved flag istrue, original
quest stage20 stays unchanged, and no unreviewed errors occur. Editable
manifests are `oblivion_m15_tutorial_alert_command_{probe,restart}.json`.
`alert-command-{probe,restart}-input.omwsave` and matching provenance JSON are
controlled command-state fixtures, **not canonical tutorial gameplay inputs**.
Continue the actual course from `stage22-input` or `faction-restart-input` above.

Reusable independent rule evidence:52 original command cases and48 parent-state
application cases pass; editable expectations are
`m15_tutorial_ambush_command_oracle_cases.json` and
`m15_enable_parent_application_oracle_cases.json`. Original SetAlert converts
signed-positive input and writes an existing process byte31; absent process
is not allocated. The original parent-state prefix replaces the child's own
disabled flag with parentDisabled XOR inversion-bit0 (including actual XESP
padding flags0xc6e000). Extra-data lookup, change notification, extraction,
RTTI and heap/list boundaries are explicit in the evidence. Full live propagation,
cycles and save/restoration semantics still need implementation/verification.

At this prior checkpoint, assassins014a2c/0178c3/0178c4 were still disabled despite
enabled parent014a30. Their missing native life/value owners explain the observed StartCombat
admission failures; fix actual enablement rather than relaxing that guard.
Original stage23 completion, the real armor/helmet/mace ability, Player melee,
Renault's authored death, essential recovery, escort departure into tunnels and
separate-process combat restart remain unaccepted. General loot, projectiles
and audio remain deferred. No full M15 stage or E1 is accepted.


Reusable evidence under `build/oblivion-compat/tutorial-melee/`:

- `default-combat-focused-01`: component intent-wire test passes; engine build
  caught an incorrect test-fixture removal API. Retained failure.
- `default-combat-focused-02`: seven focused engine cases pass, including
  default-style attack ownership/restore and actual Bullet reach/control release.
  Source fingerprint: `bbee842321044cd3deeb8131c929384b0657893864b244f2cb59edf709873b6d`.
- `default-combat-python-01`: all 539 Python cases pass. Its earlier source
  fingerprint differs only by the subsequent C++ test-fixture API repair.
- `opening-baseline-01`: the existing M14 course on the pre-change executable
  fails stage16 and Renault route checks after stage15; retain its legitimate
  normal-input saves. It also diagnoses MQ01's unsupported current Encumbrance query.
- `opening-clear-space-01`: diagnostic setup failure (copied executable lacked
  adjacent defaults). `opening-clear-space-02` successfully reloads the baseline
  save, but attempted movement cannot clear the scene because the authored
  script has disabled Player movement. Its pass is diagnostic action coverage.
- `opening-avoidance-01`: the same legitimate saved encounter reaches authored
  stages16/17 with collision avoidance enabled, identifying the default-off
  legacy avoidance switch as a tutorial blocker. This is an isolated diagnostic,
  not fresh-build tutorial or combat acceptance.
- `opening-blockers-focused-01`: all nine selected engine cases pass after
  native steering is enabled independently of the legacy setting, combat
  avoidance uses the directed enemy rather than the parked package target,
  and current Encumbrance reads native inventory weight. The existing armor
  burden matrix now exercises script queries for both Player and NPC.
- `default-combat-components-01` and `opening-blockers-engine-01`: full
  unfiltered normal checks pass 3,224 component and 2,103 engine cases, with
  exact inventories and no failures/skips. Tested source fingerprint:
  `c172758b67570860e893d23f4a05ed18c76b375a03e74fbc27465b019f9a56a0`.
- `opening-current-01`: the fresh built executable passes the unchanged M14
  opening course under default settings, including named escort routes and
  in-process save/reload checks. Authored stages16/17 are reached, with no
  unreviewed error-log findings. Its screenshot was inspected. This closes
  the reproduced opening regression, not dialogue or combat acceptance.
- `opening-blockers-sanitized-01`: the selected component intent-wire case
  and nine selected engine cases pass ASan/UBSan, with exact inventories and
  no failures/skips. Leak checking is disabled. Tested source fingerprint:
  `4bd090fd661dbdf7896c1e6f58be1483065913df8752fa0445b277780f3cf455`.

Current greeting evidence under the same directory:

- `dialogue-focused-02`: five selected engine cases pass, covering actual native
  linked choices/results, menu retention, stale callback rejection and callback
  clearing. `dialogue-raw-links-01` independently matches all 19,278 shipped INFOs
  (9,698 choice, 4,141 link-from and 1,342 added-topic links) to typed output.
- `emperor-greeting-01` reaches force-greeting but exposes unsupported IsTurnArrest;
  `emperor-greeting-02` gets past that query and exposes CanPayCrimeGold during
  higher-priority generic greeting selection. Both failures remain retained.
- `turn-arrest-oracle-01` runs original evaluator4F5D60 and non-console command506570:
  eight cases / sixteen calls prove a global consume-on-read signal, independent
  of actor identity. Runtime owns that signal, but the actual arrest producer is
  still outside this tutorial slice. `dialogue-focused-03` passes fourteen selected
  component and six engine cases; fingerprint
  `9feb3ccc16e74763c71cb6e1579b9b2fed58577573f26a59e4d2e53c82b7a5ea`.
- `can-pay-oracle-01`: eight original evaluator4F5290 cases pass null/nonactor,
  zero/equal/insufficient/sufficient payment boundaries. Actor predicate, actual
  gold count and legal bounty getter are explicit supplied boundaries; the native
  comparison executes. Both query oracles identify original executable SHA-256
  `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
  Editable inputs: `m15_turn_arrest_oracle_cases.json` and
  `m15_can_pay_crime_gold_oracle_cases.json` under `scripts/data/oblivion_compat/`.
- `dialogue-focused-04`: build succeeds and fourteen component cases pass, but
  the new crime-query test supplies invalid alternate-realm NPC bounty. Retained
  fixture failure. `dialogue-focused-05` then rejects a duplicate scalar AV37
  base. Corrected to the existing NPC storage contract and modifier-only AV view
  used by prior crime tests before rerun.

- `dialogue-focused-06`: the corrected crime-query case passes; `dialogue-focused-07`
  passes fourteen component and seven engine cases, fingerprint
  `ffe197ca63aeb1a3e34e2c285b02bfe0bef0bc865a7cd535d37fdacd39811cfb`.
  These check subject-owned legal bounty (independent of AV modifiers), live gold,
  zero/equal/insufficient payment and read-only behavior.
- `emperor-greeting-03` gets past CanPayCrimeGold, then stops on IsGuard125.
  `is-guard-oracle-01` runs original4F7B90 and class accessor51BEF0 for eight cases;
  only the subject base getter is supplied. Class bit1, NPC type and missing-class
  behavior are verified. The condition now uses the native class, including the
  shared Player's current class. `dialogue-focused-08` passes fourteen component
  and eight engine cases; fingerprint
  `a915b2e99023658aaa2f5a6fd1e99a11077d63cf47ba9019134f502901fd9697`.
- `emperor-greeting-04` gets past IsGuard, then stops on GetCrime122.
  `empty-crime-oracle-01` executes original4F7AF0 and675C40 with explicitly empty
  manager lists for eight offense inputs. Only the subject actor predicate is
  supplied. The runtime query now rejects a matching offense when its owned
  incident ledger is empty; populated-ledger observer/type matching remains
  explicitly unavailable, rather than deriving crime from bounty. This is a
  bounded empty-state query, not crime gameplay implementation.
- `dialogue-focused-09` passes all nine selected engine cases, including the
  empty-crime query; fingerprint
  `8703892cebef58d074ab45d0d468de7f6b41eb2967030c5cab480a4847cb8abb`.
- `emperor-greeting-05` gets past those queries and stops on IsInMyOwnedCell146
  in generic INFO02A34D. The executable identity is retained in
  `emperor-greeting-05-executable.sha256`. The source course now stops after a
  failed required wait, avoiding dependent menu inputs on subsequent attempts.
  The pristine input save still hashes to
  `33ec1241eeaa196f1b0d20fa18ac8bf5e2b3b2bc3ce285a41e5e1c629084a435`.

- `dialogue-sanitized-01`: fourteen selected component cases pass, but the
  engine loader case reports an ASan overflow. `stale-object-audit.json` identifies
  thirteen objects built after the draft header's preserved mtime but before its
  actual copy ctime. Its caller allocated456 bytes for the now528-byte DialogInfo.
  The current layout probe and object hashes are retained there. The normal build
  has no stale objects in the same audit. Invalidating only the thirteen stale
  sanitizer objects and rebuilding makes the exact failing case pass in
  `dialogue-sanitized-02` (ASan/UBSan, leak checking disabled). This is a corrected
  build-artifact mismatch, not a waived memory error. Both runs use fingerprint
  `dc3d327606fae2f53f3ecae4ba9b870e38e6ea21046a9339c467f203b8c5edec`.
- `owned-cell-oracle-01`: eleven original4F53D0/6ECC80/4CAAC0 cases pass, with
  declared actor/base/extra-data/RTTI/faction-rank boundaries. The implementation
  now delegates resident dialogue queries to the existing
  `oblivionActorHasCellOwnershipClaim`; its stronger original-helper corpus and
 3376-case engine comparison remain reusable from
  `S5/native-drop-cell-access-oracle-01` and `NativeCellClaim*`.
  Truly unloaded actors remain explicitly unavailable in this new adapter.
  `owned-cell-prison-records.json` independently verifies that the shipped
  ImperialDungeon01 cell has no owner/rank, and INFO02A34D requires ownership.
  `dialogue-focused-10` and `dialogue-sanitized-03` pass all fifteen selected
  engine cases, including owning-NPC/faction-rank/content-index condition tests
  and existing ownership conformance cases. Fingerprint:
  `a8f130a09354e6e5dc51f10d4769b284f419dd24a4f6f24e7e941243092d5166`.
  ASan/UBSan leak checking remains disabled.
- `emperor-greeting-06` gets past ownership but stops on IsTrespassing145 in
  generic INFO029204. Native command evaluator4F5370 dispatches actor virtual354:
  NPC/Player60E320 asks current-cell4CABC0; creature69D990 returns false. Reuse
  the existing272-case original helper evidence at
  `S2/oracle-emulator/cell-trespass*` and pure `cellTreatsActorAsTrespasser`.
  The new bridge resolves actual cell flags/owner/global, guard class and native
  static faction ranks, including Player raw-flag8 filtering. It does not advance
  warning timers or report crimes. Focused normal checks are next.

- `dialogue-focused-11` and `dialogue-sanitized-04`: all16 selected engine
  cases pass, including the new trespass adapter's owning-NPC, Public/HandChanged,
  zero-valued permission-global, guard and NPC/Player faction-mask cases. These
  share the current fingerprint above. `emperor-greeting-07` then gets past
  IsTrespassing but stops on GetClassDefaultMatch229 in INFO01E8B2. The executable
  hash is retained in `emperor-greeting-07-executable.sha256`.

Next-query investigation: original command table entry for opcode10E5 dispatches
4F5880, which calls Player recommendation662750 and current class5E3320. It returns
2 for identical class pointers,1 for matching specializations, otherwise0.
662750 recomputes while the current class FormID equals winning
`iClassCharactergenClass` (storageB37D00). It groups positive tutorial skill uses
by SKIL specialization, normalizes each to seven rounded points and applies
ordered class branches. Independent rounding can select multiple branches; later
branches replace earlier selections. No new selection retains the cached class;
only an empty cache falls back to winning iClassAcrobat. Non-temporary current
classes return the prior cached recommendation without recomputation.

`class-default-oracle-01` executes the complete original recommendation routine
for71 empty-cache and six cached/non-temporary cases, plus twelve original
4F5880 match cases. It uses actual x87 conversion instructions, supplied class
and SKIL lookup boundaries, pinned original executable and installed master
hashes. The first harness setup incorrectly assumed SKIL index0..20 rather than
AV12..32 and used the wrong DATA specialization offset; the correction precedes
native execution. A later register observation was corrected to read EAX before
its move into ESI; class lookup/output observations were unaffected.
`components/esm4/charactergen.hpp` now implements the pure selection proposal and
match rules. Null proposal explicitly means retain the caller's cached identity;
a concrete Acrobat proposal is distinct from no proposal. The runtime must still
resolve winning GMSTs/CLAS and own history/cache. `class-policy-focused-01` passes
three initial empty-cache tests; corrected cache-aware API and independent match
cases pass all three tests in `class-policy-focused-02`, fingerprint
`79587408a3a818fb65ff222c2bed777bbd0626a2ebce2c1922b2ff74b4d1903e`.
`class-policy-sanitized-01` also passes all three policy tests under ASan/UBSan,
with leak checking disabled. Its source fingerprint (including the updated
handoff) is `e0e29c95c4340ad881d1fff955c6ccb7980dad5fcb89f2d977d702d01c746b53`.
Editable expectations: `scripts/data/oblivion_compat/m15_class_recommendation_cases.json`.
This is policy evidence, not a working GetClassDefaultMatch bridge or UI gate.

Next owner work: Player+5B0 is the21-float tutorial skill-use array and+650 caches
actual class identity. The native usage writer668B30 updates it only with
tracking flag+6E5 enabled and its suppression argument false, before the shared
notification tail. SetInCharGen's original65D620 clears/frees the use array when
disabled but retains the cached class. The fork's existing SetInCharGen handler
currently updates only the shared chargen-state global. Connect that command,
actual gameplay skill-use events and persisted Player state to a dedicated owner;
do not reuse current skill levels/progress as invented use history or put this
state in a TES3 faction/stat map. The actual prior runtime codec is v79 (the earlier v53 handoff note was stale).
The pending implementation adds explicit v80 state rather than hiding a FormKey
in an unrelated numeric actor-value slot. Reuse current class metadata via store
identity lookup, accounting for shifted content indices; GMST fallback IDs are
not already managed runtime IDs. Resolve the pristine older save's adoption
policy explicitly, then test query/save/reload before retrying the greeting.

Query adapters do not claim a complete script-host command surface. Cold
gold/ownership queries still have explicit unavailable paths; native faction
mutation/persistence is pending (the new read adapter uses the existing static
native authority, not a TES3 faction map). Adjacent later-game aliases in the
condition-name table (117/118/123) remain recorded for later. Avoid preserving
old mtimes when copying staged source.

Remaining route gaps include real dialogue choices/results, the assassins'
bound armor/helmet/mace, autonomous combat and retargeting, visible death/essential
recovery, escort resumption, and a separate-process combat restart course. Do not
replace the bound gear with unarmed assassins or mistake the opening course for
completed melee acceptance.

## Historical review

Prepared: 2026-10-07.

## Target

Deliver an early playable checkpoint: start in the Imperial Prison, follow the
Emperor and Blades, survive the first ambush, loot, and continue past it through
normal gameplay. This is a focused subset of M15, not full M15 acceptance or a
promise of a complete tutorial through the sewer exit.

This document records a read-only review of the existing plans and reports.
No build, test, or runtime scenario was run for that review. The user requested
this handoff document; implementation has not been requested by that action.

## Existing foundation

M0–M14 are marked accepted in the roadmap and implementation ledger. They
provide parsing, identity, startup, saves, scripts, rendering, actors, player
movement and character creation, inventory/equipment, navigation, and tutorial
escort behavior.

The M14 tutorial escort manifest demonstrates opening prison progression,
named Emperor/Blades movement and door transitions, and save/reload using
normal inputs. It does not establish successful tutorial combat. If the user
only wants to play the opening before combat, recheck that existing course on
the current build before assuming more implementation is necessary.

## Shortest reasonable work list

1. **Finish necessary services and persistence — M15 S3.** Close the state,
   restore, and action-continuation boundaries needed by this route. Actors,
   equipment, and ongoing actions must remain consistent across save/load.
2. **Finish melee, blocking, damage, and reactions — M15 S4.** Ordinary
   Player/NPC contacts already have bounded verified implementation. Complete
   the route's required actor, reaction, control, and restart paths. Include
   creature attacks if the chosen endpoint reaches them. S5 bow combat can be
   deferred for a melee-focused checkpoint if the actual route does not require
   it; this is not a waiver of S5 for full M15 acceptance.
3. **Implement death, essential recovery, and corpse loot — M15 S6.** Mortal
   enemies must die correctly, essential escorts must recover correctly, and
   loot must come from actual corpses. Preserve Renault's authored death as a
   distinct scripted requirement.
4. **Implement autonomous combat and escort resumption — M15 S7.** Assassins
   and Blades must acquire targets and fight through production combat paths;
   escort behavior and authored progression must continue after the encounter.
5. **Close tutorial dependencies and verify the route — tutorial portions of
   M15 S12/S13.** Execute original dialogue conditions/results and quest events.
   Implement the first assassins' real bound armor/helmet/mace ability, including
   creation, equipment, removal, save, and death behavior. This is an identified
   narrow magic prerequisite, not completion of M16. Verify normal-input entry,
   combat, loot, post-encounter progression, and save/reload continuation.

This focused dependency list is a recommendation for an earlier checkpoint.
The existing full-M15 implementation plan uses a linear S0–S14 execution order,
including S5 and S8–S11 before S12/S13. Do not silently revise that plan or mark
its stages accepted based on this smaller route. If implementation of this
checkpoint is later authorized, make the bounded scope and any departure from
that order explicit in its implementation handoff.

## Work that can wait for this checkpoint

- Bow/projectile gameplay when not required by the selected route.
- Stealth/pickpocket and general crime, arrest, and jail gameplay.
- Arena and representative dungeon acceptance campaigns.
- Full magic, alchemy, enchanting, disease, and progression (M16).
- General dialogue, persuasion, and services (M17), and full quest/journal/marker
  support (M18), beyond the real paths required by this tutorial chunk.
- Complete Oblivion UI presentation (M19). Functional input, dialogue choices,
  inventory/equipment, combat feedback, and save/load must still be accessible.

The roadmap's full fresh-launch/new-game/tutorial/sewer-exit checkpoint belongs
to M19. Reaching this smaller endpoint does not demonstrate that larger flow.

## Acceptance for the smaller checkpoint

Use the M15 E1 tutorial combat contract as the basis, scoped to an explicitly
named post-encounter endpoint:

- Start from normal prison progression, or a checkpoint with proven legitimate
  progression provenance; no fabricated quest completion or already-dead targets.
- Acquire and equip actual available equipment through normal interaction.
- Follow the authored escort/door sequence and execute real dialogue results.
- Demonstrate actual autonomous attacks, contacts, damage, death, escort
  survival/recovery, and exact corpse-loot changes.
- Reach the endpoint through original scripts/conditions without console stage
  injection, forced outcomes, or unsupported commands masquerading as success.
- Verify appropriate legal consequences, including no inappropriate bounty.
- Save/restart during combat or its nearest valid save boundary, and after loot;
  continue to the same endpoint and compare against uninterrupted progression.
- Report implementation/test results separately from actual runtime acceptance.
  Run relevant build, sanitizer, and Morrowind regression gates for changed code.

## Resume instructions and sources

First read the current M15 report and recheck its stage ledger. At this review,
S0–S2 were passed; S3, S4, and S5 were in progress; S6–S14 were pending. Later
reports and current source take precedence over this dated snapshot.

Read local AGENTS.md and the relevant repository skills before implementation
or verification. Keep proprietary game data and generated evidence local;
preserve unrelated changes and do not push or publish without authorization.

- [Compatibility roadmap](../OBLIVION-COMPATIBILITY-ROADMAP.md)
- [Implementation status](IMPLEMENTATION-STATUS.json)
- [Current M15 report and stage ledger](M15-COMBAT-STEALTH-CRIME.md)
- [M15 implementation plan, scope boundaries, S3–S7, S12–S13, and E1](M15-COMBAT-STEALTH-CRIME-IMPLEMENTATION-PLAN.md)
- [Selected campaign inputs and first-ambush bound equipment](M15-CAMPAIGN-COMBAT-POLICIES.md)
- [M14 navigation and tutorial escort acceptance](M14-NAVIGATION-DETECTION-AI.md)
- [Editable M14 tutorial escort manifest](../../scripts/data/oblivion_compat/oblivion_m14_tutorial_escort.json)


## Current class-owner implementation and remaining checks

`CharacterGenerationState` owns tracking, optional21-float history, and stable
recommended CLAS identity. Player clears it with its own lifecycle. The v80
C++/Python codecs preserve presence, signed finite history and class identity;
older envelopes omit it. On first adoption of an older fork save, the existing
chargen global supplies tracking and history starts at zero; historic use is
not inferred from levels or XP. Disabling tracking releases history but retains
cached identity. Safe explicit re-entry allocates a fresh history.

The query resolves current classes through the managed store and GMST integer
IDs through `resolveEsm4RuntimeFormId`, preserving shifted file indices. It
reuses the independently checked proposal/match rules, including positive-use
filtering and retained-cache behavior. SetInCharGen updates the same owner.
Existing gameplay skill-use events record winning native SKIL use values;
ordinary melee/block contacts prepare the history change with their first
contact publication, require the prior owner to remain unchanged, and publish
it without allocation alongside contact state. Continuation phases do not add
another use. This is recommendation history, not a complete native XP system.

New tests cover owner enable/disable/clear, version80 round trips and downgrade
rejection, class/GMST/SKIL bindings with both content-index layouts, real query
and command adapters, and skill-use records. Python codec preflight passes all161
module cases; an initial new test used incorrect helper names and omitted its
AI RNG fixture field, corrected before the pinned check. C++ syntax preflight
passes helper/world/state/Lua event code and the corrected menu fixture. A new
fixture argument-container mismatch was caught and corrected before the build.
Fresh normal checks now pass as recorded above. Actual greeting, result-script
execution and v80 save serialization are proven through stage18. Positive
contact/restart history coverage and fresh sanitizer checks remain required.

## Greeting UI and course evidence — 2026-11-03

Course08 passes class selection and resolves the intended greeting but exposes
a TES3 companion-local lookup of the native script. The native dialogue window
now avoids TES3 companion locals, merchant restocking and TES3 disposition
calculation. Course09 removes the exception but captures before the first menu
frame. Course10's render delay proves the menu visible;11 enables controller
menus, and12 adds normal D-pad navigation to leave mouse-cursor mode before A.
Course12 successfully traverses the first two responses, then exposes the
course's incomplete choice sequence. The retained original graph requires
What's going on -> Who are you -> Why am I in jail -> What should I do ->
Goodbye. Course13/14 execute all of these; inspected screenshots show the
actual dialogue text/choices. Course14 saves the stage18 checkpoint before
waiting for stage19. Both fail only that next gate, with no unreviewed errors.

Next implementation references: `reserveAction`/`completeAction` and resident
Act/Wait processing in `apps/openmw/mwmechanics/oblivionai.cpp`. Original
UseItemAt package04D84D has PLDT near marker032B00 and PTDT reference04E90D,
not an inventory item. Local original source inspections are retained as
`secret-door-script-04e90e.txt` and `opening-script-02480b.txt` in the ignored
evidence root. Review callback invalidation/reacquisition when activation can
change quest/package state; retain consumable UseItemAt behavior and test
actor identity, single activation, stale/missing target and save continuation.

## Placed switch delivery and current gate probe — 2026-11-03

Resident UseItemAt now reserves the explicit placed ACTI reference when it is
loaded, enabled, in the actor's cell and within activation range. The existing
stable action-item field and timed reservation preserve intent across saves.
At the post-iteration commit boundary, matching live state/target/range are
rechecked, the reservation is consumed, then actual OnActivate dispatch runs
with the NPC as action reference. No actor-map iterator/reference is used after
the callback. Consumable behavior retains its existing path; unloaded switch
activation is not newly implemented or accepted. Terminal unresolved packages
can retry when that same placed target is actually available. Older phase code
collapsed the specific error to generic "interrupted"; both saved reasons are
covered. Do not infer that other legacy interruptions are migrated.

The new headless tests load real native CELL/ACHR/REFR/ACTI/SCPT/PACK records,
execute IsActionRef against the actual user, check one activation, state restore
before/after commit, disabled/moved targets, and retry selection. Initial
fixture/syntax/expected-phase failures are retained in `switch-focused-01`..06.
The headless retry's attempted locomotion had no MechanicsManager; its retained
GDB backtrace identifies that fixture limit. The focused retry check now stops
at actual reselection, while real runtime courses cover locomotion/activation.
`switch-focused-07` passes15 cases cleanly;08 adds existing AI coverage and the
observed generic interrupted save case, passing48. No ASan claim for this chunk.

Runtime evidence:

- `secret-passage-01` retains the diagnostic failure that exposed the actual
  old save's generic interrupted reason.
- `secret-passage-02` passes actual activation of04E90D by Renault032A15,
  runs source script04E90E, plays switch/wall Forward and reaches stage19 after
  its authored five-second timer. Native v80 save reinspection passes.
- `secret-passage-03` passes the same course plus normal held forward input.
  Its later inspected screenshot clearly shows the open passage and escorts;
  the player position is unchanged, proving an outstanding control/input gate,
  not player-traversal acceptance.
- `emperor-greeting-15` passes the unchanged full greeting-to-stage19 requirement
  on the current binary. Its intermediate real F5 save captures a pending timed
  switch reservation, not already-completed activation.
- `secret-passage-04` loads that pending reservation in another process, commits
  exactly once and reaches19. Zero-use class-history identity survives too.
- `prison-gate-query-01` used an incorrect environment manifest key and never
  scheduled the queries.02 gets the decisive read-only results but its premature
  F5 did not save.03 removes that unrelated save step, waits for rendering and
  passes both actual read-only query assertions with no unreviewed errors.

The editable `oblivion_m15_tutorial_secret_passage.json` retains the stage19 and
actual activation-event gates. Its forward-input suffix is diagnostic until a
position/control-release requirement is added. The read-only gate query is
`oblivion_m15_tutorial_prison_gate_query.json`; it accepts observed query output,
not a prescribed closed-state expectation. Source BaurusScript022B1B and the
scoped native control inspection remain local ignored evidence.

The wall screenshot at stage19 initially showed partial opening. Local original
mesh SHA256 `bff6eca3eb06d8b5e0ba610f7c791b128ffee4b09c05dcb5e81f2f01cd8c1dfb`
has Forward duration31.6667, frequency1, and mechanical translation keys through
about5.0667; the later screenshot shows the open wall. No production animation
change was made from that early screenshot. `secret-wall-inspection-01` contains
the scoped inspection. Its first helper lacked NDEBUG and therefore read a
wrong RecordPtr layout; `invalid-layout-sequences.txt` is invalid. The corrected
helper uses matching NDEBUG and agrees with independent raw sequence-field
reads. Do not reuse the initial zero frequency/duration as asset evidence.

## Passage and navigation records

Historical checkpoint details from the prison/passage navigation work follow.
The current target and next action are above.

Current chunk: follow the escorts from the legitimate passage checkpoint to
the first ambush. The prison dialogue, Renault's placed-switch action, animated
gate closure and authored control release now work. Actual autonomous ambush,
Player melee participation and post-encounter continuation remain unaccepted.

`gate-release-04` passes normal controller traversal of the opened secret
passage. Baurus closes the cell gate and restores all Player controls through
his original script. The save has opaque INPU/CFLG0, gate Close progress1 and
unchanged reference yaw, plus naturally accumulated Athletics history0.24.
`passage-restart-01` passes a separate-process reload: GetOpenState3, no replayed
EnablePlayerControls, further normal movement to y372.66, and preserved positive
Athletics history/cached Acrobat recommendation. Both scenario receipts have
no forbidden or unreviewed error findings; screenshots were inspected.

The animated-door helper uses the live native Open/Close clip or its existing
saved scripted-animation owner; requests use the actual embedded clip instead
of legacy hinge rotation. Transitional/completed Open map to2/1 and Close to4/3.
Hinge fallback remains for other doors. No new save schema is needed.

Latest checks under `build/oblivion-compat/tutorial-melee/`:

- `gate-focused-02`:50 selected engine cases, runtime rebuilt. Fingerprint
  `6266dba07b6e72a72ea26bc8587483e241ba5d1826615c58325263e959d6faab`.
- `gate-regression-01`:all2120 engine cases pass. Fingerprint
  `30cae3802e9ad380c85e1001ac258881b6434e70e779526f036d49c5a70baf10`.
- `gate-components-python-01`:all3230 component and540 Python cases pass.
  Fingerprint `e6e2e46d431a3b6da71ded7c9888b912d99c724afbc5c761c22fd04b8d8cfb11`.
- Exact C++ inventories match; no failures/skips. Game SHA256:
  `a7d6a960b12f58436ac4fd98e4e4900d5c309e3b64c85d899fb0e819cbe3d956`.
  Fresh sanitizer coverage for the v80 character-generation owner, switch and
  gate changes is still required at the next integration checkpoint.

Reusable pristine inputs (each has a provenance JSON):

- `passage-restart-input.omwsave`: stage19, actual further passage traversal,
  controls enabled, positive Athletics history. SHA256
  `3a11207b5cdd0c8561dfd311a50df9331d0e4d587610d1f38c794f801c4dcd41`.
- `inside-passage-input.omwsave`: original gate-release04 passage save,
  SHA256 `6969d71b447ef32ea8955982a90f7e66de90c8704c50e449e887967ec7f670fa`.
- Earlier stage18/19 inputs and original provenance remain in the evidence
  directory; see the historical switch/dialogue records below.

Retained gate attempts: focused01 had a NotNullPtr deduction compile failure;
release01 used a wall-time query that did not fire in slow simulation;02 used
an unsuitable onUpdate observer;03 walked into the right doorway edge. Proper
alignment in04 traverses without a collider change. `gate-asset-inspection-01`
records original mesh Open1.66667s/Close1.8s sequences.

Current route evidence: `descent-01` failed to load because the observer Lua
was accidentally written into the omwscripts descriptor; corrected in source.
`descent-02` loads and traverses to x661.375,y1181.547,z-427.635, then its straight
forward approach times out. A normal F5 diagnostic snapshot places Uriel at
710.832,1239.049 and Renault already across the landing. No collider defect is
established. `descent-doorway-input.omwsave` preserves that legitimate stage19
checkpoint (SHA256
`cdc5d7ebe4eceb7695c48b93b41a41e7bec4038e52ec08d80f1139f1977f68bb`).
The next normal-input approach course first sidesteps, then follows the stock
pathgrid's landing/staircase turns toward roomC. See its editable manifest;
ambush acceptance still requires original triggers and actual combat.

`ambush-approach-01` sidesteps to x629 then advances to x632,y1239 before
stopping against the landing edge. A normal F5 snapshot (`landing-edge-input`,
SHA256 `a40f723fd1331d71bffff6122c314efcb63a1282f5dc989d3546eddfc4cb2b27`)
shows Uriel still at710.921,1238.907 while Renault reaches1316.757,1628.860.
The game was deliberately terminated after the repeated obstruction; the
retained runner later reports its unmet waypoint. Uriel's navmesh attempts lack
a start polygon or return partial paths; the native PGRD fallback skips its
nearest entry node and accepts intermediate nodes within64 units. Stock node0
is658.137,1274.193,-466, linked to4 at735.715,1384.994,-448. This suggests corner
cutting; it is not proof of an asset collision defect.

Candidate fix is now applied: resident PGRD fallback retains its entry node and
uses16-unit XY/64-unit vertical intermediate waypoint arrival. Final destination
and low-process policy remain as before. `landing-focused-01` passes51 selected
engine cases, including the doorway-coordinate regression; source fingerprint
`88cf8a15e3af288edee96d15bb229f3e8cf27bba8a89426997971d8284d2aa63`.
Rebuilt game SHA256
`1c78ef520c30685c4527f501a4996ee74d3e9e55bad6e736ceccb55a6cb80a08`.
`landing-01` now passes from the original stuck save under ordinary forward
input. Player reaches661.077,1417.135,-447.023; Uriel clears the doorway to
826.684,1428.919,-447 while Renault moves to1384.749,1504.644. Stage19 remains
original and untouched. The screenshot and save were inspected; no forbidden
or unreviewed error findings. `landing-input.omwsave` preserves this actual
checkpoint, SHA256
`24448d1cd0461908e1b6806a935516bd504f361d17be4276056d387d70e251a4`.
Baurus and Glenroy are still following through the passage; their later arrival
and the next staircase remain to verify. Continue with the roomC normal-input
course; this is doorway repair, not ambush acceptance. `passage-sanitized-01` was deliberately stopped
at build returncode-15 so the observed fix can join the next check batch; no
sanitizer tests ran. Completed objects can be reused, not acceptance results.

Remaining course: reach the first ambush under normal input, execute assassins'
actual bound armor/helmet/mace, prove autonomous combat and Player participation,
death/essential recovery, escort departure into tunnels and separate-process
combat restart. Positive melee/block history remains unverified. General loot,
projectiles and audio remain deferred. Do not call any full combat milestone or
E1 accepted from these passage gates. Continue by the next observed tutorial
blocker, without expanding general crime or unrelated package behavior.

## Faction-command implementation checkpoint — 2026-11-03

`faction-focused-01` retained a real missing-cell fixture failure (39/40 passed).
`faction-focused-02` fixes the fixture and passes one component/four engine
cases, including prepared restore with detached incoming definitions. Fingerprint:
`46da71d319c22e1a9ebf6c85f7e2f92a5ef69c74237c0cad08a35f35a958e826`.
`faction-normal-integration-01` caught an old clock-wire corruption test whose
suffix calculation omitted v81's empty map. Only its offset was corrected;
`faction-clock-focused-01` passes both relevant protocol cases.
The next full C++ gates pass as recorded above. Its Python stage caught three
current-version recipe pins and missing reviewed empty-map upgrade defaults.
The strict whole-state fixture comparisons remain in place; the corrected
15-case focused retry and all542 Python tests pass. These were fixture/upgrade
repairs, not permission to ignore other differences. No fresh sanitizer result
is claimed from the earlier cancelled `passage-sanitized-01` build.

The previous pre-faction checkpoint remains historical:

Current chunk: implement the tutorial's native stage22/23 command paths and
verify linked assassin enablement before admitting the actual melee encounter.
The original stage20 pre-ambush checkpoint now passes without script errors.
Do not reuse the stage23 diagnostic save as an accepted combat checkpoint.

Latest game SHA256:
`1c78ef520c30685c4527f501a4996ee74d3e9e55bad6e736ceccb55a6cb80a08`.
`landing-focused-01` passes51 selected engine cases; full
`landing-engine-regression-01` passes all2121 engine cases, exact inventories,
no failures/skips. Full-engine source fingerprint:
`b49e695e6187f4fb456ee3bb4d968d536c96d15ebb53194c28cde19831be4666`.
Earlier applicable3230 component/540 Python gates remain linked below. Fresh
ASan/UBSan for v80/dialogue/switch/gate/navigation remains required:
`passage-sanitized-01` was deliberately cancelled during compilation to batch
the observed navigation repair; it executed no tests. Reuse compiled objects,
not its incomplete receipt, at the next integration check.

Current reusable input under `build/oblivion-compat/tutorial-melee/`:
`before-ambush-input.omwsave`, SHA256
`24a65f1abca23e552c18a1476b22517515ab22d103367c485d950646daa898d7`.
Its provenance JSON points to passing `before-ambush-01`: original Renault
OnPackageDone reaches20 while Player remains1045.157,1665.022,-561.382,
outside trigger range. No stage22 execution, forbidden or unreviewed errors.
Tracking=true, Athletics2.3099983 and cached Acrobat survive this fresh-process
continuation. The screenshot/save were inspected.

Confirmed progress: prison dialogue -> placed switch -> animated gate closure
and authored control release -> actual Player traversal/restart -> lower
passage doorway -> Renault's original roomC arrival. The doorway repair keeps
the resident fallback's entry node and uses tighter XY arrival at PGRD corners.
`landing-01` proves the old stuck Emperor and Player both clear it. The later
stair-edge obstruction clears through ordinary retreat/recentering; it did
not require another collision change. See [navigation records](#passage-and-navigation-records).

Observed next failures (`staircase-recovery-01` diagnostic): all four normal
input waypoints reach the ambush room; original stages20,22,23 run, but the
course correctly fails its script-error checks. SetFactionReaction aborts
stage22/23 result units; SetAlert aborts the second stage23 unit. Three disabled
assassins subsequently produce StartCombat initialization errors. In that save
refs014a2c/0178c3/0178c4 are still disabled and have no native values/life owners;
all three original XESP records name enabled parent014a30. Check parent-derived
initial/restore enablement and disabled AI progression before changing the
StartCombat admission guard. Parent semantics are not yet independently proven.

Independent next-command evidence:52 original-instruction cases pass in
`ambush-command-oracle.json`; editable expectations are
`m15_tutorial_ambush_command_oracle_cases.json`. Original executable SHA was
freshly verified. Faction writes are directed, retain int32 values, and remove
zero entries; facade marks source modified8. SetAlert's script conversion is
signed-positive, routes through the existing process, and does not allocate an
absent process; all four process types share the byte31 setter. Heap/list
operations, argument extraction and RTTI are disclosed harness boundaries.
These are rule checks, not live command/save acceptance. Raw scripts, package
inputs and XESP inspections are retained in the ignored evidence root.

Remaining acceptance: execute original ambush commands, enable/initialize the
actual four assassins, implement their real bound armor/helmet/mace lifecycle,
prove autonomous combat and Player participation, Renault's authored death and
essential recovery, escort departure into tunnels, and separate-process combat
restart. Positive melee/block history remains unverified. General loot,
projectiles and audio remain deferred. No full M15 stage or E1 is accepted.
