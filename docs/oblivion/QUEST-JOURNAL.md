# TES4 quest journal

The Oblivion profile uses a dedicated window for the Journal action. The Active
and Finished tabs display authored notes from winning quest records. Selecting
a row browses its history. Track quest changes the current HUD and map objective
for the loaded game, and Automatic restores the existing priority-based choice.
The debug toggle opens a separate spoiler pane with stage state, unreached
entries, and discoverable dialogue and script sources.

## Save state

Stage membership alone cannot recover earned history. The installed
`Oblivion.esm` with SHA-256
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`
has two conditional completion entries at Hackdirt stage 100. The branch uses
a script variable, and both outcomes share the saved stage number.
The TES4 runtime therefore stores ordered stage updates with the earned entry
ordinals and an unresolved-condition flag when needed. The journal fields in
version 107 contain no text or UI preferences. Opening or browsing the journal does not
execute quest scripts. Guidance reads live target conditions as before, while
earned notes are resolved from their captured identities.

Older saves recover unconditional notes in stage order. The journal marks
uncertain history and omits conditional notes whose original branch is unknown.
For a legacy conditional completion entry, current conditions can supply a
completion hint. That result is provisional because the original condition
outcome was never saved. Static debug sources are possible transitions.
Computed or indirect script calls and unsupported conditions can remain
unresolved.

## Verification

The [original-content event fixture](../../scripts/data/oblivion_compat/quest_journal_events.txt)
starts from an earned post-sewer save and isolates quest updates with
`Oblivion.esm` and `DLCBattlehornCastle.esp`. It exercises Hackdirt's original
successful turn-in result, multiple entries at Kvatch stage 10, and Battlehorn's
nested stage 90 alongside ordinary stages. Tutorial and Deliver the Amulet
exercise legacy Finished and Active views. These checks exercise the journal
at the selected beats. Full normal-playthrough evidence remains in the
[Hackdirt handoff](HACKDIRT-HANDOFF.md),
[Kvatch handoff](KVATCH-HANDOFF.md), and
[main quest guidance plan](SEWER-TO-KVATCH-QUEST-GUIDANCE-PLAN.md).

Local rendered and save evidence is under `build-windows/quest-journal-live-01`.
The first save produced version 107 and matched the Python encoder byte for
byte. A fresh process loaded and resaved it with all five selected quest
records identical. The rendered checks used 100% and 125% GUI scale, including
mouse tab switching, the debug toggle, and switching the HUD objective to
Battlehorn. The same Journal action opened the original Morrowind book in a
Seyda Neen smoke test. The action was invoked by an in-game probe. Hardware
gamepad input remains unverified.

The alternate-branch engine test changes a condition after capture and checks
the original note and event order across encoding and restore. It also checks
conservative reconstruction and the winning record's text.

The Windows integration run verified 1,134 TES4 component tests, 2,324 engine
tests, and 210 Python runtime-state and crime-contract tests. The unfiltered
engine run reproduced the four documented particle failures caused by the
missing PPM reader. The final journal-specific run verified both test cases
against the updated game sources. The full Python discovery also encountered a
fixture RGB decoding failure outside the journal suites.

The C++ inventory receipts are in `build-windows/checks-7a8adb5d01784239afb0f755b20e18b6`
for components, `build-windows/checks-1710759ce9b84b6e928de86e5a2d0e67` for
engine integration, and `build-windows/checks-b7aff80d26304223a61fe35da85487ef` for
the final focused run.
