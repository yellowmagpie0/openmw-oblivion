# Full sewer escape — current handoff

## Target and remaining acceptance

Play the original opening continuously from a new game in prison through the
physical sewer exit and a fresh-process reload of that run's own outdoor save.
Include original dialogue/switch/doors, ordinary loot/equipment/healing, actual
combat/traps/traversal and Player shaman victory, escort reunion, birthsign,
Emperor death/Amulet, Baurus class/CharacterGen88 and distinct MQ01 exit88.
No health, quest, position or completion injection, skipped required encounters,
or spliced state qualifies. **Full escape remains incomplete.**

The genuine prison-origin checkpoint chain already reaches the exterior and
passes fresh outdoor reload. It is composite evidence. Remaining acceptance is
the uninterrupted new-game course plus its own outdoor save/reload, native-state
and input audit, and rendered inspection. Existing evidence keeps its provenance.

## Current chunk — uninterrupted original-new-game10

`full-escape-continuous-10` is live, game PID254562, runner254545,
supervisor254532, read-only save archiver254608. Preserve this process.
Actual argv has no load-save argument; config has new-game1. Native events
through original30 contain one run-start, no load and only the5 audited Peryite
startup deferrals. Opening19 and actual passage/descent saves match their native
snapshots. First ambush archive19/native match: all4 assassins killed by guards,
all3 escorts alive, Renault script-dead/null killer/nonessential, death counts1,
Player80HP/inventory unchanged, no bound gear on dead assassins and all combat
queues clear. Original26/27/28/29/30 observed. Departure archive20/native match:
CG30 retained despite later29 request, all3 escorts alive in022288, Glenroy
Complete/Low at the authored arrival marker with no progress deficit. Player80HP;
2 living early rats legitimately already engage Player. No supplemental inputs.
Prison/dialogue/passage/victory/departure captures inspected.

Scope remains opening chapter only. Same PID has completed segment3 and entered
segment4 (remaining rats/wall exit). Native Rat victories and later solo course
are not yet claimed. Current proofs: `full-escape-continuous-10/` files
`opening19-verification.json`, `first-ambush-victory-verification.json`,
`escort-departure30-verification.json`. Monitor's stage value is the last requested
SetStage, not the native maximum completed stage; repeated23/29 is expected.

Current binary SHA ff12ce92…c7f6d; full67 manifest SHA7c2e216a…9d0b85,
`equipment-active-page-full-assembly-01/full-escape.json`. All paths below are
relative to `build/oblivion-compat/sewer-escape/`. Incremental monitor reads logs
without input and verifies PID identity; `full-escape-continuous-10/incremental-watch.json`
is observation history, not sufficient alone to prove a currently live process.
Inspect actual process/launcher status. Current handles and prepared evidence:
`menu-admission-current-work.json`. No build/test helper is live at this checkpoint.

Next: same process through remaining solo combat, loot/healing and all later
segments; then fresh-load its own outdoor save. No other rendered game or binary/
resource overwrite while this UI is live. Do not restart on observation timeout.
On terminal failure retain raw evidence, fix the actual failed gate, verify a
bounded correction and commit it before another full original-new-game attempt.

Static later risk, not an observed failure: source10 leaves Inventory's Magic
filter selected; source40 assumes All before3 rightshoulder presses. Source
onOpen preserves the filter. `remaining-ui-filter-context-risk.json` records
this; `healing-filter-context-prepared-01/reproduce.json` is prepared, **not run**,
from native-exact real chest-loot save SHA f6327a68…3c84eb,26.14HP/3 potions.
It seeds the remembered category using normal inputs. Do not launch alongside10.

## Latest completed fixes and reusable evidence

Active page fix `afe0f46d22`: private Lua query returns the actual controller
window ID; source7 uses ordinary trigger inputs and fresh3-frame samples before
Club selection. Visibility is insufficient because inactive pages remain visible
off-screen.83 top-level source7 actions retain the full selected course. Inner
Key-phase expected-log prefixes corrected. Full2210 normal and2210 ASan/UBSan
engine cases pass, exact inventories,0fail/0skip, stable fingerprint29778055…1c939;
18 focused assembly/repeat tests pass. Full67/11559-action assembly compiles in
engine LuaJIT. Normal remembered-Magic and sanitized default-Inventory UI courses
pass (1/4 cycles): Club equipped, Key once, actual keyed destination022ff6,
native-exact70HP saves, all input bytes delivered, native run-end error0/pending0/
unsupported0. Whole entries differ only by7 body-velocity projections≤1e-5.
Engine leak1/runtime leak0; captures inspected. Failed full09, visibility candidate
and sanitizer timing attempts remain failed with original provenance.
[Active page proof](SEWER-EQUIPMENT-PAGE-VERIFICATION.json).

Menu admission fix `aee30a8b22`: Morrowind's new-game menu lock had also blocked
Oblivion; original TES4 scripts never issue those menu-unlock commands. Oblivion
now admits its windows, preserving Morrowind's lock and both profiles' character
creation reset. Normal/instrumented cold-new-game Tab/menu probes pass; actual
loaded07 wall-exit save heals36→60 through3 native releases and traverses room
throughx1695.613. Inventory and3 Player rat kills retained, queues clear, native
save exact; whole loaded entry differs only by2 tiny body-velocity projections.
Focused9, full2210 normal and2210 ASan/UBSan engine cases pass, exact inventories,
0fail/0skip, identical stable full-gate source; engine leak1/runtime leak0.
[Menu admission proof](SEWER-NEW-GAME-MENU-VERIFICATION.json).

07 remains terminal FAILED, game0/launcher1/no engine findings: original19/26/
27/28/29/30, all4 assassins dead, all3 early rats killed by Player and physical
wall exit pass, then no healing menu/casts and36HP/80MP unchanged; segment5 fails.
Its immutable recovery save is
`full-escape-continuous-07/retained-saves/3-0023-Quicksave.omwsave`, SHA0e90362e…b3491.
Failure proof: `full-escape-continuous-07-failure-verification.json`.

Other reusable evidence:

- [Uninterrupted07 first ambush](SEWER-UNINTERRUPTED-AMBUSH-VERIFICATION.json).
- [Occluded NPC descent](SEWER-OCCLUDED-COMBAT-DESCENT-VERIFICATION.json): native
  normal/instrumented26 recovery, all escorts alive, exact restart boundaries.
- [Bound cleanup actor state](SEWER-BOUND-CLEANUP-ACTOR-STATE-VERIFICATION.json).
- [Early solo feedback](SEWER-EARLY-RAT-FRAME-FEEDBACK-VERIFICATION.json) and
  [input harness](SEWER-INPUT-FEEDBACK-HARNESS-VERIFICATION.json).
- [Joined shaman victory](SEWER-JOINED-SHAMAN-VICTORY-VERIFICATION.json),
  [recipient/escort fix](SEWER-ESCORT-RECIPIENT-VERIFICATION.json), and
  [reunion/birthsign](SEWER-JOINED-REUNION-BIRTHSIGN-VERIFICATION.json).
- [Composite exterior/reload](SEWER-JOINED-EXTERIOR-VERIFICATION.json).

## Next action and completion audit

Preserve the current original-new-game process; inspect milestones, actual saves/native receipts and rendered captures.
On terminal failure, retain raw evidence, diagnose the actual failed gate and
verify a bounded fix with focused/runtime checks before broad integration gates.
Commit completed self-contained changes; no push. Do not edit source/docs or
commit while any `run_checks.py` helper is live. Reuse unchanged passing checks.

On a full exterior pass, fresh-load that run's own outdoor save in another process.
Prepared ignored `build/oblivion-compat/sewer-escape/verify-full-escape.py` audits
all67 ordered segments, native quest/death/equipment state, selected loot/healing/
traversal gates, ordinary input receipts, exact save/native snapshots and fresh
whole-state restoration (tiny body-velocity projection tolerance only). Its
positive whole-course audit remains unproven. Review actual captures and the
original requirements separately before claiming completion.

## Known limits

Five original tick1 SetUnconscious deferrals belong to distant Peryite shrine
followers, independently audited outside this course; no general script/game
acceptance is implied. General NPC spell fallback and Player Flare remain
unsupported; shaman coverage uses staff/unarmed combat. Upright creature corpses,
blank birthsign descriptions and take-all-only loot remain gaps. Other character
choices and full M15/S7/E1 retain separate acceptance. UI courses use silent
configuration; linked NPC conversation is synchronous.

Detailed prior checkpoints, failures and evidence are retained in
[SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md).
