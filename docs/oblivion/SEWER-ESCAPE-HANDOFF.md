# Full sewer escape — current handoff

## Target and remaining acceptance

Play the original opening continuously from prison through the physical sewer
exit and fresh-process outdoor save/load. Include ordinary loot/equipment,
combat/traversal and shaman victory, escort reunion, Emperor death/Amulet,
birthsign/class choices, Baurus CharacterGen88 and MQ01 exit88. No injected
health, quest, position or completion, skipped required encounters, or spliced
save state qualifies. **Full escape remains incomplete.**

Remaining: prove one uninterrupted original new-game prison→physical exterior
course and fresh outdoor reload from that new course. Current rendered sanitizer
coverage of recipient/fall fixes now passes as explicit composite evidence.
The genuine prison-origin checkpoint chain now reaches the exterior and passes
fresh outdoor reload. It remains composite evidence; full escape is not complete.

## Current chunk — occluded first-ambush descent

Original-new-game06 is terminal FAILED: original19/80HP, passage/descent and
ambush24 passed; three assassins died, but0178c3 stalled above Glenroy and the
stage26 gate timed out900seconds. Game0/launcher1, no engine findings. Preserve
`full-escape-continuous-06-failure-verification.json`, ordinary diagnosticF5 and
two read-only GDB snapshots. Its consumed two-point prefix ends beside the actor;
target is720units away/below and out of sight. Original PACK0178e2 flags0x483004
includeAllowFalls; original local PGRD node274 is in an8-node component without
node64 at the target. No exact original-game navigation parity is claimed.

New regression `CompletedPartialApproachCanFinishAnOccludedAllowedDrop` uses
actual snapshot coordinates/route and fails against production baseline (step
missing). Proposed movement fix: a consumed nonempty approach may finish an
allowed downward step around an occluding edge; empty unseen paths stay blocked.
Timer replans preserve that eligible approach; target/cell/movement changes keep
normal invalidation. No relocation, ownership or save schema change; permission
is reconstructed from the route after reload. First revision passes focused4,
full2210 normal and2210 ASan/UBSan engine cases, exact inventories/0fail/0skip,
sanitizer leak1; normal runtime SHA256ee06e0e9…57490. Actual fresh diagnostic-save
recovery01 PASSES original26: assassin0178c3 killed by Baurus, all3 escorts alive,
Player80HP/inventory unchanged, queues clear, actualF5 equals native snapshot,
whole fresh entry exact0differences and inspected victory capture. Preserve this
PASS; earlier sampling was not a terminal failure.

However, recovery takes237.659 simulation seconds with repeated partial descents
and returns to the platform. At actual intermediateZ-643.925, the remaining59.075
unit drop loses the old64-unit eligibility and permits an upper-prefix replan.
Standalone current-header red reproduces the missing step at that exact position.
The engine regression now includes it. Revised completion band is16 for consumed
nonempty approaches; empty routes retain64 and the visibility requirement. No
extra history or state/schema fields. Focused green02 passes all4 cases and builds
normal runtime225953a1…db552. Final full2210 normal and2210 ASan/UBSan engine gates
pass, exact inventories/0fail/0skip, same stable tested source, sanitizer leak1.
Actual normal02 and instrumented03 recovery both PASS original26 in4.316 simulation
seconds (first revision237.659): Baurus kill,80HP/unchanged inventory, all3 escorts
alive, all4 assassins dead, no dead bound owners, clear queues, parsed native save
exact, whole fresh entry0differences and inspected captures. Instrumented runtime
leak checking is disabled; engine leak coverage is separate.
[Current proof](SEWER-OCCLUDED-COMBAT-DESCENT-VERIFICATION.json).

Instrumented01 remains failed before gameplay (LD_PRELOAD was a linker script).
Instrumented02 remains failed only at the10second SDL receipt limit despite real
victory/F5 and eventual matching delivery; no whole-course pass inferred. Editable
probe now explicitly uses90seconds; corrected instrumented03 passes all actions.
Only this receipt timing changed after engine gates; C++/test hashes are unchanged.

Editable recovery probe:
`scripts/data/oblivion_compat/oblivion_sewer_occluded_combat_descent_resume.json`.
Only focus/F5 controls: all movement, descent and victory belong to native NPCs.
This bounded fix is ready to commit. No UI or verification helper live at this
checkpoint. Next active course is `full-escape-continuous-07`: original new game
through physical exterior, then fresh-load its own outdoor save. Inspect its
actual process/status before starting another course. Full escape remains
incomplete; loaded recovery is not uninterrupted-course acceptance.

### Reusable early-solo verification

Original new-game `full-escape-continuous-03` failed during the first ambush:
Glenroy killed0178c4, then bound-gear cleanup threw `Invalid or dangling disposition
actor identity` each frame. Original24/Renault death passed;26 did not. The driver
waited900seconds and ended with scenarioFAIL/launcher1/game0. Preserve raw logs,
ordinary diagnosticF5 and read-only GDB; no ambush victory inferred.

GDB locates the failed validation in bound-equipment publication. Its copied
cached snapshot lacked a later registered disposition actor0280da, while live
whole-world capture remained valid. Transactions now stage a fresh world capture
before updating inventory/ability ownership and validating; strict validation and
atomic inventory/registry publication remain. No save schema change. Focused red
reproduces the exact error; all9 bound-item tests and full2209 normal plus2209
ASan/UBSan engine tests pass, exact inventories,0fail/0skip, sanitizer leak1.
Runtime binaries built in focused normal and full sanitizer gates.
[Actor-state cleanup proof](SEWER-BOUND-CLEANUP-ACTOR-STATE-VERIFICATION.json).

Original-new-game `full-escape-continuous-04` is now terminal/failed. It passed
opening/ambush/escort30; then its first rat-facing observation timed out before
Player attacks. Game0/launcher1, no engine findings. Preserve the raw failure;
no whole-course pass inferred. Its triple-yaw early rat observer overshot in the
preserved first-person view. Two early rat observers now use camera-aware gain;
all actions/order/counts unchanged. Four assembly regressions pass.
Focused `wall-rat-camera-aware-first-01` and `wall-rat-camera-aware-finish-01`
both pass cleanly from04's genuine stage30 save and the first victory's actualF5.
Player kills both rats;80→66→36HP, actual wall crossing, final combat/death/action
queues clear. Actual saves parse exactly to their receipt snapshots. Both fresh
entries restore the whole state with only6 tiny animated-body velocity projection
differences, abs<=1e-5. Captures inspected and input receipts match. Source changes
are only the2 observer yaw gains; actions/order/counts unchanged. Four assembly
regressions pass; rebuilt67-segment/11957-action Lua compiles and4340 gates compile.
[Early rat input proof](SEWER-WALL-RAT-CAMERA-VERIFICATION.json).
These are separate loaded checks; uninterrupted full escape remains unproven.
Original-new-game05 is terminal/failed: it passed original30 and the first
Player wall-rat kill, then timed out at the second course's wall-crossing gate.
No engine findings; actual first-ratF5/72HP retained unchanged. Camera sampling
could precede application of previous motion at low frame rates; the one-shot
heading also missed the opening. Raw failure is preserved in05.

Current changes: assembled camera motions wait for actual frames (later adaptive
recipes keep their existing barriers); wall movement uses feedback through a
pre-gap point and yields to a rat within melee reach before completing the exit.
Room-rat first-person selection now tolerates an already first-person save.
Focused selections are explicitly loaded checks and do not qualify as full escape.
Harness support is committedef32f3b33e: bounded nested normal-input feedback,
parent deadlines/control cleanup, frame barriers and explicit focused selections.
Full Python02 passes578/0fail/0skip;12 loop and6 assembly cases are included.
Failed Python01 retains its one Xvfb startup error; isolated case passes.
[Harness proof](SEWER-INPUT-FEEDBACK-HARNESS-VERIFICATION.json).

Focused `wall-frame-feedback-slow-06` now PASSES (game/launcher0), from05's
immutable genuine stage30 save with a one-FPS cap. All3 rats, including the early
joining room rat037e40, have native Player kill credit. Actual wall exit passes;
ordinary healing48→72HP and actual room traversal tox1687 pass. Final Player72HP,
CharacterGen30, combat/death/action/cast/projectile queues clear. Actual finalF5
parses exactly to its hashed receipt snapshot;365 issued/delivered inputs match.
Fresh entry differs only in12 tiny animated-body velocity projection values.
Wall-exit and room captures inspected; upright creature corpses remain a known
rendering limitation. This loaded check is not uninterrupted escape acceptance.
[Early solo feedback proof](SEWER-EARLY-RAT-FRAME-FEEDBACK-VERIFICATION.json).

Raw focused failures remain separate:01 deliberate/-2 for insufficient walking
budget;02 Player death against a rat just short ofx560;03 Player death during
redundant fixed aiming;04 untracked room rat037e40 killed Player while the recipe
aimed at a different/farther rat;05 preflight rejection of nested loops before
launch. Do not infer passes. No C++ changes: code392bd5d472/runtimeef71da25…7292a
and2209 normal/sanitized gates retain their provenance. Recipe/proof committed
03455a1358. Fresh default assembly `frame-feedback-full-assembly-01` contains67
segments/11732 actions; Lua and4297 regex gates compile, no load flag.

Original-new-game06 used supervisor192502/runner192503/game192526, original new
game/no load, recipe03455a1358/harnessef32f3b33e and runtimeef71da25…7292a. It is now
terminal/failed as recorded above. Its ordinary stage19F5/80HP parses exactly to
its native receipt snapshot, and the Emperor conversation was rendered/inspected.
No whole-course acceptance inferred. Full uninterrupted escape/reload remains due.

Accepted04 prefix used manifest `full-escape-bound-cleanup-fixed.json`.
Same course/ordinary controls, actual no-load argv and new-game1. Original26
first-ambush victory now passes: actualF5 `live-3-19.json`,80HP/unchanged Player
inventory, Renault scripteddead/essentialfalse, all4 assassins dead (three Baurus,
one Glenroy), surviving3 escorts alive. All5 deathcallbacks drain; bound ownership,
combat/death/action/cast/projectile queues clear. ActualF5 copied unchanged to
`full-escape-continuous-04/first-ambush-victory.omwsave`; parsed native state exactly
matches the receipt snapshot. Inspected victory capture and133 matching issued/
delivered inputs. No loads or frame errors. Original27/departure movement/28/29/30 passed. Actual30 F5 `live-3-20.json`
retains80HP and all3 escorts alive in022288; immutable stage30 save parses exactly
to its receipt snapshot. The solo section/rest of escape remain pending.
This closes the new-game runtime reproduction of the bound-cleanup failure;
it does not close the full uninterrupted escape. Launch provenance records
normal runtime SHA256ef71da25d612636f79b97bf50d2d17643aef49549d044483d5a1a0ad36b7292a,
built from7db0440e9a plus the tested actor-state patch/code392bd5d472. Keep this
binary provenance separate from the focused check above. All build helpers are
terminal. Next: run a fresh original new game through the physical exterior
and fresh-load its own outdoorF5. Full escape remains incomplete until that
uninterrupted course and fresh reload pass.

## Connected exterior and fresh outdoor reload evidence

Latest fully inspected main save:
`build/oblivion-compat/sewer-escape/joined-final-outdoor-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Player outside/Tamriel005e3d,(47073.230,82958.086,297.828),84.511HP/110,80MP,
Warrior class/birthsign, one Amulet/no sewer key. Original04bed7 exit review
Finished and one real exterior teleport, followed by clean separate-process
reload/F5/capture. Both scenarios pass, all actions/receipts match, no unexpected
errors; both fresh boundaries exact0differences. Health/inventory/death counts
unchanged from upper gate, no combat/pending death/action. Character creation
tracking cleared; MQ01 exit88 completed. Original5-second tutorial timer then
runs90→100/stops MQ01 and CharacterGen during outdoor continuation; independent
original records verify this is authored progression, not replay.
[Connected exterior proof](SEWER-JOINED-EXTERIOR-VERIFICATION.json).

Clean27-point bridge/lower-room/stair course includes actual upper rat066a04
Player victory, ending at(352.452,1601.702,6878.335),84.511HP. Goblins remain alive
away from route, no actor relocation or combat suppression. Actual upper gate
activation/jump and final corridor use ordinary inputs.
[Upper passage proof](SEWER-JOINED-UPPER-TUNNEL-VERIFICATION.json).

[Key door/final sewer entry](SEWER-JOINED-FINAL-SEWERS-ENTRY-VERIFICATION.json):
only original Baurus key consumed, Amulet/health preserved, actual manhole teleport.
[Western rat](SEWER-JOINED-FINAL-WESTERN-RAT-VERIFICATION.json): two axe contacts,
rat death+1, health unchanged; actual victoryF5/capture before SIGINT during runner
shutdown. Raw launcher remains interrupted/-2; no whole-scenario pass inferred.
All following main continuations use that actual F5 with exact native entry.

Empty combat-route AllowFalls case is implemented and independently verified:
red regression exposes the old empty-route restriction; focused green and full2207
normal/ASan+UBSan engine tests pass, leak checking enabled for engine sanitizer.
Actual stalled-save normal replay physically drops01774a fromZ-991.999 to-1183.027,
Glenroy kills it, original52/54 fire, Player100HP/inventory unchanged, no pending
combat/death/action. Exact fresh0differences and inspected stage54 capture.
[Empty-route fall proof](SEWER-EMPTY-COMBAT-ROUTE-FALLS-VERIFICATION.json).

Completed-route descent preservation is now verified and committeda9ab59a13e.
Standalone red uses live GDB02 projected-prefix data; focused2 normal+ASan/UBSan
and full2208 normal+sanitized engine cases pass, exact inventories/0fail/0skip,
engine leak checking. Actor/cell/large destination changes and loss of permitted
visible downward movement retain normal replanning. Transient routes only;
no save schema change.
[Descent progress proof](SEWER-COMBAT-DESCENT-PROGRESS-VERIFICATION.json).

Rendered `completed-descent-rendered-sanitized-01` passes cleanly from the original
failed sanitizer route's real diagnosticF5, exact fresh0differences. Third assassin
01774a physically descends, Emperor032a18 kills it (normal empty-route proof has
Glenroy killer); all3 escorts alive, Player100HP/inventory unchanged. Only generic
assassin base death count+1, queues/combat clear. Original52/54 and actualF5/capture
pass; SDL receipts match. Runtime leaks disabled, engine leak gate separate.
Old raw route fails54; later first retry remains intentionally interrupted/-2.
No failures relabeled. Read-only GDB02 and ordinary diagnosticF5 receipts retained.
This closes rendered recipient/fall coverage as explicit composite evidence.

First full new-game attempt `full-escape-continuous-01` is terminal/-2, deliberately
stopped after an impossible driver gate: real engine SayTo was incorrectly given
an outer Lua prefix. No gameplay failure/pass inferred. Corrected assembler keeps
engine SayTo and allM-numbered events unwrapped; only Lua observations are scoped.
Actual speech/door log regressions plus wrong-segment exclusion pass, combined
observer compiles. Exactly2 wait patterns changed; all11957 input actions/order
otherwise identical. [Producer audit](SEWER-CONTINUOUS-COURSE-ASSEMBLY-VERIFICATION.json).

Second original new-game attempt `full-escape-continuous-02` is also terminal/-2,
intentionally stopped at actual14: mixed Lua/engine alternatives were wrapped as
one producer, so original14 could not satisfy the South movement fallback. East
waypoint really passed; original9/10/12/13/14 and complete guard conversation
passed. Ordinary diagnosticF5 at10 is documented; no state injection. Preserve raw
status. Corrected all3 mixed conditions, including later Emperor70 fallback;
actual native14/70 and Lua/wrong-segment checks pass. All4340 regex gates compile;
11957 actions/control order unchanged. QA source only, runtime C++ unchanged.

Earlier03 opening-prefix evidence below remains valid for its recorded prefix;
its later terminal ambush failure is recorded above. No whole-course pass inferred.

[Ongoing uninterrupted opening prefix](SEWER-UNINTERRUPTED-OPENING-PREFIX-VERIFICATION.json):
new-game→original19,80HP, no load events/samePID171106, original Emperor dialogue,
switch, inspected captures, immutable native snapshot/F5 receipt. Mixed native14
fallback really released strafe; F7+F8 outer transition retains intended inner4.
This is only a prefix; first ambush/escort/rest of escape are not inferred complete.

Four hermetic regression cases now protect producer classification, mixed native
callbacks, wrong Lua-segment rejection, nested regex groups/all gate compilation,
and original-new-game/readonly-query constraints. Focused4pass/0fail/0skip, source
hashes in producer audit. This is not a fresh full Python-suite claim; runtime
C++ unchanged, full2208 normal/sanitized and rendered descent54 retain provenance.

Earlier03 prefix used normal runtime73861667d0252ae7260ddb3c954ad2ced059e820edaa54028060d5b74cc33bf6,
built at ef2c3ef348+final descent patch/codea9ab59a13e. Keep that original
provenance separate from the bound-cleanup fix and tests above.
Prepared ignored `verify-full-escape.py` draft rejects composite exterior, partial
opening and a still-running course; positive whole-course/reload audit is pending.

Adjacent known new-game gap: five tick1 deferred SetUnconscious events belong to
original DAPeryiteScript01ecbd/shrine followers outside the tutorial. Same events
exist in accepted earlier opening baseline; original source independently audited.
Record for later Peryite work, do not claim general script/game acceptance.

Uninterrupted course editable sources now prepared: selection
`scripts/data/oblivion_compat/oblivion_sewer_continuous_selection.json`, assembler
`scripts/assemble_oblivion_sewer_course.py --output build/oblivion-compat/sewer-escape/full-escape-continuous-versioned.json`.
67 segments,11732 ordinary actions, original new-game/no load flag (current assembly). F7+F8 changes
only outer observer locals; plainF8 retains compound inner phases. Every Lua
movement/aim observation is segment-scoped. Actual interrupted prefixes are cut
at saved evidence, notably fifth patrol save and fourth Subterrane save; no later
planned actions inferred. Versioned source recipes preserve provenance and genuine
input course, prepared recipe is not gameplay acceptance. Lua syntax compiles.
The next original-new-game course will use the verified early-solo feedback recipes;
physical exterior/reload acceptance remains due for a single new-game process. Main composite
physical exterior/fresh reload retains its separate provenance.

[Final battle/class proof](SEWER-JOINED-CLASS-CHOICE-VERIFICATION.json): Baurus032a17
kills final assassin01e703 with three genuine sword contacts while Player blocks,
taking two dagger hits. Original Warrior class023787/birthsign01fd9d,Strength55/
Endurance55,101.971HP/max110,80MP; original CharacterGen88, **MQ01 remains80**.
Amulet and Baurus key granted by original dialogue; bound foe cleanup and all
combat/death/action queues clear. Four-point normal secret-passage traversal
preserves health/inventory. Exact native fresh boundaries; captures inspected.
Committed7d263cf859. Binary b2a22b98ba3baf33ef21323ca03b518773ffa3c0686fe44a2618b12a3d4640d1
built94370948f1 plus fixture-only IDs; production unchanged04152787f5.

[Sanctum/Amulet/death proof](SEWER-JOINED-SANCTUM-AMULET-VERIFICATION.json): clean
physical door0226a2→022282/stage58, original59/65 scene, actual approach/side-step
clearance and original Emperor70/73/74 Amulet dialogue, then scripted death76/80.
Each fresh boundary exact; raw failed overshoot and blocked approaches retained.
Only Amulet+1 inventory delta before final battle. No splicing.

[Recipient/escort56 proof](SEWER-ESCORT-RECIPIENT-VERIFICATION.json): hold paused
escort recipients and allow existing yielding without new wire/phase/destination
writes.42 focused and full2206 normal+ASan/UBSan engine/leak1 pass with original
source provenance. A later fixture-only ID cleanup separates NPC bases from PACK
keys; both affected cases pass normal and sanitized checks with runtime rebuilt.
Later rendered recipient/fall coverage is closed by the completed-descent
sanitizer proof above; older NPC victory retains its original provenance. Normal landing backstep permits
original52/54/56; every genuine continuation restores exactly.
[NPC acquisition proof](SEWER-THIRD-AMBUSH-NPC-ACQUISITION-VERIFICATION.json).

Pre-combat source for continuous battle/escort replay:
`build/oblivion-compat/sewer-escape/joined-third-ambush-approach-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
[Approach](SEWER-JOINED-THIRD-AMBUSH-APPROACH-VERIFICATION.json),
[reunion/birthsign50](SEWER-JOINED-REUNION-BIRTHSIGN-VERIFICATION.json).

Historical checkpoint sequence continued through key door/manhole, final sewers
and exterior/reload as linked above. Current action is the next uninterrupted original-new-game attempt after the focused early-solo pass.

[Connected Subterrane entry](SEWER-JOINED-SUBTERRANE-ENTRY-VERIFICATION.json):
clean978-action post-shaman10heal/26originalPGRD/Space03816c→022288 course,
80HP/80MP, CharacterGen30, inventory/death counts unchanged.

[Connected shaman victory](SEWER-JOINED-SHAMAN-VICTORY-VERIFICATION.json): original
469-action staff/approach F5 prefix (raw later fails melee distance),11 real shock
impacts5 each, staff150→7, Player80→25; clean fresh four-heal/sprint/17axe-contact
victory saves15.33HP with only base03b52d count0→1 and all queues clear. Slow
sprint02 Player death/no checkpoint; lowercase sprint01 preflight/no gameplay.
Shorter control pauses in successful264-action continuation. Upright corpse limit;
no state splicing, full uninterrupted course still unproven.

[Pre-shaman recovery](SEWER-JOINED-PRE-SHAMAN-RECOVERY-VERIFICATION.json) proves
clean80HP/80MP,11 normalC releases, actual Sorcery/Respite use, unchanged life/counts
and FIRS1. This precedes the successful connected shaman encounter.

[Eastern defenders proof](SEWER-JOINED-EASTERN-DEFENDERS-VERIFICATION.json).

Early reinforcing handoff completes but defender52f joins, leaving8HP/active
combat. Four realC casts recover to21.7; immediate settled-aim axe fight kills52f.
Original missed-waypoint followup is stopped/no checkpoint; incorrectly launched
8HP eastern navigation dies/no checkpoint. Both failures retained. No state splice.
Northern patrol real finalF5/fresh native victory accepted separately; launcher143
prevents a whole-course pass claim:
[patrol evidence](SEWER-JOINED-NORTHERN-PATROL-VERIFICATION.json).

[Recovery/chest proof](SEWER-JOINED-CAVERN-RECOVERY-VERIFICATION.json) records
three normal casts5.02→26.14, six original downhill points, original chest051d72
Space take-all and three actual inventory healing-potion uses→80.
[Log encounter proof](SEWER-JOINED-LOG-ENCOUNTER-VERIFICATION.json): original
Space releases6 log bodies; neither environmental kill claimed; Player kills
03b518/03b51a via genuine first-kill F5 prefix and clean final continuation.
Failed/death attempts remain failed; upright corpse limit. General combat casting
not accepted; dedicated ordinary recovery casts proven.

Earlier cavern joins:
[first goblin](SEWER-JOINED-FIRST-GOBLIN-VERIFICATION.json),
[western passage/second goblin](SEWER-JOINED-SECOND-GOBLIN-VERIFICATION.json),
[lower route/Caverns entry](SEWER-JOINED-CAVERN-ENTRY-VERIFICATION.json).
Their raw failures and reviewed prefix/expectation acceptances remain separate.

Earlier accepted joins:
[root entrance/combat](SEWER-JOINED-ROOT-ENTRANCE-VERIFICATION.json),
[17-point root descent](SEWER-JOINED-ROOT-DESCENT-VERIFICATION.json),
[western encounters](SEWER-JOINED-WEST-VERIFICATION.json),
[actual root attacker](SEWER-JOINED-ROOT-ATTACKER-VERIFICATION.json),
[north passage](SEWER-JOINED-NORTH-VERIFICATION.json),
[zombie](SEWER-JOINED-ZOMBIE-VERIFICATION.json),
[recovery](SEWER-JOINED-ZOMBIE-HEALING-VERIFICATION.json),
[key loot/door](SEWER-JOINED-KEY-LOOT-VERIFICATION.json),
[first chest/gear](SEWER-JOINED-SUBSTRUCTURE-START-VERIFICATION.json).
Failed-prefix acceptances, exact fresh boundaries and all raw failures remain explicit.

## Verification and reusable opening evidence

Camera preference save/load is fixed and accepted: temporary Vanity/Preview retain
the selected view; old explicit saves remain authoritative. Red regression,
all4 camera/recovery cases and2203 normal engine cases pass. Real idle FP→Vanity
save writesFIRS1 and fresh-loads FirstPerson with exact native state at both
boundaries. [Camera proof](SEWER-IDLE-VIEW-SAVE-VERIFICATION.json).

Current camera-fix ASan/UBSan engine integration PASSES all2203 cases, exact
inventory, zero failures/skips, leak checking enabled and runtime built:
`idle-view-sanitizer-integration-02/verification.json`, source d697f176f8 clean.
First01 also has independently verified2203 passing XML cases, but its supervisor
lost its session before final report; it is not the accepted integration gate.
02 repeats only to resolve that missing terminal evidence. Current rendered
sanitizer fresh reload/save also PASSES: FP observed, exact native entry and actual
FIRS1 save, clean exit0, capture inspected. The source idle FP→Vanity F5 prefix
really writesFIRS1 but its raw scenario fails a10-second SDL receipt timeout
because the wrong key left the default active. Raw failure retained; independent
prefix observation and clean fresh continuation are explicit composite evidence.
Correct key `sdl_input_timeout_seconds=90` is verified in fresh receipt provenance.
Rendered leak checking is disabled; engine leak coverage stays separate.
[Current sanitizer proof](SEWER-IDLE-VIEW-SANITIZER-VERIFICATION.json).
Component3262/Python560 retain their original provenance; no fresh pass inferred.

The genuine opening chain remains linked without state splicing:
[new game→19](SEWER-CONTINUOUS-OPENING-VERIFICATION.json),
[original19→30 escort](SEWER-CONTINUOUS-ESCORT-VERIFICATION.json),
[wall-rat victories/passage](SEWER-JOINED-WALL-RATS-VERIFICATION.json),
[normal healing38→62](SEWER-JOINED-HEALING-VERIFICATION.json),
[first-person room rat](SEWER-JOINED-ROOM-RAT-VERIFICATION.json).
Escort raw failure is solely the inherited MQ01wall24 exclusion: independent
original-record/trace audit accepts the authored wall activation separately.
Wall proof accepts genuine failed03 first-kill F5 followed by exact06 continuation;
all raw failures remain failed. No full single-process acceptance is inferred.

## Reusable gameplay evidence

- [Shaman victory/reload](SEWER-SHAMAN-VICTORY-VERIFICATION.json):18 normal axe
  contacts,11 actual shock releases, charge150→7, unarmed retaliation, Player
  killer and death count1. Player3.780029 HP; two fresh-reload healing casts reach
  17.860031. Legacy87→89 defaults and13 independently audited ONAM poses are
  explicitly reviewed; only tiny animated-body velocity projection differences
  remain. Native death below1 HP explains remaining0.840938 on the dead shaman.
  Post-victory input: `build/oblivion-compat/sewer-escape/shaman-victory-reload-heal-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
  CharacterGen30, Natural Caverns; no active engagements/actions/projectiles or
  pending death events. This victory is separate from the later outdoor chain.
- [Exterior exit/reload](SEWER-EXTERIOR-EXIT-VERIFICATION.json): original review
  Finished, MQ01 exit88, cleared chargen history, weather/crime reset, native
  regenerating-rat resurrection, physical Tamriel entry and exact fresh reload.
  Native reset preserves position/death history; Player, attached preserve-state
  and owned-paralysis resets remain guarded. General leveled inventory and broad
  active-actor visual resurrection are unverified. Outdoor input:
  `build/oblivion-compat/sewer-escape/sewers03-outdoor-reload-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
  Health85.971672, Warrior class/birthsign, one Amulet, no key. Three failed
  encounter probes contributed genuine pre-failure F5 prefixes only; all failed
  statuses and eight exact composite boundaries remain explicit in the artifact.
- [Exit review ownership](SEWER-EXIT-MESSAGEBOX-VERIFICATION.json),
  [weekday/PC achievement](SEWER-EXIT-CALENDAR-VERIFICATION.json),
  [class/Baurus88](SEWER-CLASS-CHOICE-VERIFICATION.json),
  [key door/final sewer entry](SEWER-FINAL-SEWERS-ENTRY-VERIFICATION.json),
  [western rat](SEWER-FINAL-WESTERN-RAT-VERIFICATION.json).
- [Emperor/Amulet/final assassin](SEWER-EMPEROR-DEATH-VERIFICATION.json),
  [friendly greeting](SEWER-FRIENDLY-GREETING-VERIFICATION.json),
  [default-open Sanctum gate](SEWER-SANCTUM-GATE-VERIFICATION.json),
  [Sanctum/bound dagger](SEWER-SANCTUM-ENTRY-VERIFICATION.json),
  [daily packages](SEWER-DAILY-PACKAGE-VERIFICATION.json),
  [reunion](SEWER-REUNION-SEX-CONDITION-VERIFICATION.json),
  [birthsign](SEWER-BIRTHSIGN-VERIFICATION.json).
- [Opening melee slice](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md),
  [traps](SEWER-TRAP-HAZARD-VERIFICATION.json),
  [logs](SEWER-ROLLING-LOG-VERIFICATION.json),
  [loot/healing](SEWER-CAVERN-HEALING-VERIFICATION.json),
  [eastern goblin](SEWER-EASTERN-CAVERN-VERIFICATION.json),
  [healing casts](SEWER-HEAL-CAST-VERIFICATION.json),
  [staff projectiles](SEWER-STAFF-PROJECTILE-VERIFICATION.json),
  [Subterrane entry](SEWER-SUBTERRANE-ENTRY-VERIFICATION.json),
  [second ambush](SEWER-SECOND-AMBUSH-VERIFICATION.json).

## Known limits and operation

General NPC spell fallback and Player Flare remain unsupported; the shaman proof
covers staff/unarmed combat. Upright creature corpses, blank birthsign descriptions
and take-all-only loot remain gaps. Other birthsign choices lack this runtime
coverage. No full M15/S7/E1 or general magic/dialogue acceptance is implied.

Run UI courses singly. Use dpad_y hat axes: virtual-pad EV_KEY D-pad buttons do
not reach SDL here. Linked choices begin unselected; Down then A chooses first.
F5 needs a closed conversation. Keep failed/death attempts and original evidence
provenance. Historical detail: [SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md)
and the linked verification artifacts.
