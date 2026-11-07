# Reusable sewer regression checks

This is a bounded regression artifact, **not full sewer acceptance**. It uses
`scripts/oblivion_compat.py scenario`, the existing sewer assembler/manifests,
genuine F5 saves, and the existing native/save audits. It never builds the game,
constructs checkpoint state, retries a failed course, or edits the installed game.
Use a completed build; do not rebuild or replace its resources during a run.
Only one rendered game runs at a time.

From the checkout root:

```bash
python3 scripts/sewer_regression.py focused \
  --output build/oblivion-compat/sewer-regression/focused-01
```

`--data /path/to/Oblivion/Data` overrides `OBLIVION_DATA`, then the normal Steam
location. `--openmw /path/to/openmw` and `--resources /path/to/resources` select
another existing build. `--library-path /path/to/lib` optionally reproduces a
retained dependency snapshot. Defaults are `build/openmw` and `build/resources`.
The output must be new. A single check can be selected with `--only combat`;
repeat `--only` to select several. `list` lists the four checks.

| Check | Selected original sections | What it exercises |
| --- | --- | --- |
| `floor` | 11–12 | Corridor overlap/floor recovery, authored runners, Player zombie victory, ordinary healing |
| `combat` | 41–45 | Patrol and nearby guards, ordinary attack/block/heal, eastern approach, full health/magicka recovery |
| `story` | 59–62 and own reload | Original Sanctum/Amulet/Emperor/final-assassin/class progression; faction replacement and door ownership persist |
| `exit` | 65–67 and own outdoor reload | Required western rat, physical final-sewer traversal, exterior exit, original story result and normal outdoor continuation |

The focused suite loads genuine checkpoints. A passing loaded course cannot
establish an uninterrupted new-game escape. Actor deaths retain their actual
killers: the eastern goblin may legitimately die to the shaman's friendly fire;
this is not relabeled as a Player kill. The required Player zombie, patrol,
nearby guard, and western-rat victories remain explicit native checks.

The existing full new-game course is a separate integration mode:

```bash
python3 scripts/sewer_regression.py integration \
  --output build/oblivion-compat/sewer-regression/integration-01
```

This runs all 67 original selected sections in one new-game process. Only after
that course passes does it launch a fresh process loading **that run's own**
outdoor save. It then applies the existing full escape audit. There is no retry
loop and no fallback to a different input save. Native success and manual
rendered review are recorded separately; this command alone does not silently
promote a course to full acceptance. During the closure delivery, at most one
final integration attempt is authorized after the focused checks are ready.
A failure is retained and classified, not followed by another tactics-tuning run.

## Results and diagnosis

Each invocation writes `RESULTS.md` and `results.json`; partial results are marked `running` with `complete=false`. The final result links to its retained
course directories. Courses retain `scenario.json`, process/driver logs,
screenshots, actual PID/start identity and executable hash, native event/live
snapshots, issued/delivered SDL input journals, and receipt-verified copies of
actual saves. `regression-audit.json` checks every retained save against its
native snapshot and checks the complete input checkpoint restoration. The only
floating tolerance is the existing animated-body velocity projection tolerance
of `1e-5`; health, quest, inventory, faction and ownership changes are not ignored.
Legacy schema89 checkpoints receive the documented schema90 absent-faction default.

| Result | Meaning |
| --- | --- |
| `passed` | All selected input gates and native/save checks passed; rendered review is separate |
| `failed / engine` | Crash, sanitizer finding, or native error/unfinished work |
| `failed / automated_player` | A native Player-death callback ended the course; no successful escape is claimed |
| `failed / unresolved_course_failure` | A route/input/observation gate failed; inspect the logs and manual checkpoint before attributing an engine defect |
| `failed / engine_or_persistence` | Native/save semantics differ; inspect the retained audit before assigning a root cause |
| `failed / infrastructure` or `checkpoint_provenance` | Runner, archive, dependency, or checkpoint-integrity problem |
| `could_not_run` | Required game assets, executable, resources, or genuine checkpoint are absent |

Exit status is 0 for passing checks, 1 for a failure, and 2 when checks could not
run. Missing checkpoints never cause synthetic state creation. The runner ends
a case early only after an actual native Player-death callback; ordinary
observation waits do not trigger healthy-game restarts. Keep failed directories.

The full verifier also enforces one original new-game PID/no intermediate loads,
all 67 sections, original story changes, required deaths, class/birthsign/items,
physical exterior entry, clear native queues, and own-save fresh reload. Only
the five already-reviewed distant Peryite tick1 deferrals are allowed in a new
base-game run; loaded focused checks require zero unsupported commands.

## Checkpoints and provenance

The editable catalog is
[`sewer_regression_checkpoints.json`](../../scripts/data/oblivion_compat/sewer_regression_checkpoints.json).
It records exact SHA256, original retained path, source evidence, and regeneration
boundary. Saves, generated plugins, original assets and build outputs are not
committed. Failed parent runs can supply genuine **earlier** F5 checkpoints;
they never become continuous acceptance evidence.

The resolver first checks `build/oblivion-compat/sewer-checkpoints/NAME.omwsave`,
then the retained original path in the catalog. Canonical bytes must match the
catalog hash. To transport these inputs, copy those exact saves into that named
checkpoint directory. `--checkpoint-dir` changes the directory. `checkpoint-path
--checkpoint NAME` prints the resolved, verified path.

To generate a replacement through ordinary original new-game play:

```bash
python3 scripts/sewer_regression.py regenerate --checkpoint floor \
  --checkpoint-dir build/oblivion-compat/sewer-checkpoints-new \
  --output build/oblivion-compat/sewer-regression/regenerate-floor-01
```

The same command accepts `combat`, `story`, and `exit`. It runs the existing
new-game prefix through sections 10, 40, 58, or 64 respectively, checks native
receipts/input/save state, then copies the actual final save without alteration.
A sidecar binds its new hash to the regeneration result. Use that directory with
`--checkpoint-dir` for the focused suite. Failed regeneration creates no accepted
checkpoint; it may expose the same automation limitations as integration mode.
When moving a regenerated checkpoint, retain its sidecar, source report and
course diagnostics together. Update only the sidecar's `source_report` path to
the retained report's new location; preserve the save bytes and recorded hashes.
It is not a shortcut that sets health, position, faction, or quest stages.
For a manually played replacement, retain the original F5, runtime/config/asset
provenance and stage evidence, then explicitly review a catalog update. Do not
hex-edit a save or manufacture JSON state to satisfy a regression.

## Manual investigation

Use the existing isolated tryout tooling:

```bash
./scripts/try-oblivion/tutorial.sh
./scripts/try-oblivion/checkpoint.sh floor
./scripts/try-oblivion/checkpoint.sh combat
./scripts/try-oblivion/checkpoint.sh story
./scripts/try-oblivion/checkpoint.sh exit
```

Each accepts an optional Data directory. `SEWER_CHECKPOINT_DIR` selects transported
or regenerated checkpoints. `OPENMW_BIN` and `OPENMW_RESOURCES` select a completed
build. The launcher copies the checkpoint and executable/resources to its private
`/tmp` directory, preserves the original bytes, and uses normal controls with no
automated input or event injection. A no-op compatibility descriptor preserves
the saved read-only observer's content identity; it does not run its controller
or mutate game state. Your F5/F9 saves stay in the private tryout directory.

## Reused implementation evidence

Do not rerun broad checks solely to collect another green result. The unchanged
engine implementation has 2,223 normal and 2,223 ASan/UBSan engine passes, with
original revision/fingerprint and leak-check scope retained in
[the floor-recovery verification](SEWER-DESCENDING-RECOVERY-VERIFICATION.json).
The unchanged scenario runner has the recorded 587 Python passes in
[the reinforcement verification](SEWER-REINFORCEMENT-RESPONSE-VERIFICATION.json).
New wrapper, provenance/classification, branch and assembly tests are focused.
Those old results are historical evidence, not mislabeled current-tree runs.
For an actual collision implementation change, rerun the relevant ActorBounds
cases with `.codex/skills/openmw-build-test/scripts/run_checks.py`, then broaden
only as required by the change.

Current delivery status, retained failures, gameplay limitations, and the next
playable recommendation are in [the concise handoff](SEWER-ESCAPE-HANDOFF.md).
