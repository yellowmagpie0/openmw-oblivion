# Native runtime/restart checks

Select the current acceptance case from the stage's delivery audit and latest
passing verification receipt. Reuse its editable manifest and fixture builder;
check their current inputs and schema before running. Historical S1 recipes are
not a default: some lack the native Player form now required for construction.

## Before an expensive run

- Identify the behavior and acceptance gap this run checks. Reuse a compatible
  course instead of duplicating the harness. Apply AGENTS.md's combat fidelity
  policy; exact historical numbers are not automatically new acceptance gates.
- Check manifest syntax, required variables/files, fixture content and Player/
  actor bindings, current save schema and expected action/completion signals.
  Run available focused fixture checks first; do not launch after a failed
  preflight. Separate fixture failures from engine defects.
- Build the required game binary, using `run_checks.py --mode engine
  --with-runtime` when also running engine tests. Test-only engine mode does not
  refresh `openmw`. Record game binary and relevant loaded-library identities.
- Use fresh ignored output directories and private copies of input saves;
  preserve pristine hashes. Keep editable manifests as source. Inspect the
  current command's `--help` rather than copying an obsolete invocation.

## Runtime assertions

Verify actual process epochs, simulation ticks, pause state and action
acknowledgements. Save, quit, start a fresh process, load and resave for restart
coverage; another in-process load is not equivalent. Assert gameplay/state
semantics, not merely file existence or generic startup logs. Use justified,
predeclared tolerances for approximate combat math; retain strict checks for
identity, state ownership, duplicate effects and save/load integrity.

Inspect the required captures and retained failures. A passing isolated course
covers only its declared behavior, not the full gameplay acceptance matrix.
Reuse applicable evidence with its original source/binary/fixture provenance;
rerun when relevant implementation, dependencies or inputs change.

## Specific historical cases

Search [runtime-checks-history.md](runtime-checks-history.md) for the relevant
schema migration, legacy action service, actor binding, script query, lifecycle,
or loading failure. It preserves prior recipes and evidence paths; validate
compatibility before reuse. The S3 acceptance report is a starting index for
service/persistence evidence, not proof of current S4 gameplay acceptance.
