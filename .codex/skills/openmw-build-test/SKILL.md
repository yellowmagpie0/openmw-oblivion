---
name: openmw-build-test
description: Build and test this OpenMW Oblivion fork with existing CMake configurations, sanitizer runs, exact GoogleTest inventories, and revision-linked evidence. Use for C++ or Python implementation verification and M15 regression checks.
---

Use the repository root containing `components/esm4` and its local `AGENTS.md`.
Resolve this skill directory before running scripts; personal installation may
be a symlink into the checkout. Do not assume the shell is at the repo root.

## Choose the check

Use [scripts/run_checks.py](scripts/run_checks.py) for fresh build/test evidence:

```bash
python3 SKILL/scripts/run_checks.py --repo REPO --output build/oblivion-compat/m15/S2/CHUNK-01 --mode components --mode sanitize --mode python
```

Replace `SKILL`, `REPO` and `CHUNK-01` with actual paths/name. The output directory
must not exist. The helper builds first, records the source fingerprint and
binary/cache hashes, verifies inventory against XML, and refuses source drift.
Do not edit sources or commit during a run. Run via a yielding command tool;
follow progress in its logs and keep the user updated during long builds.

- `components`: `components-tests`, full inventory by default.
- `sanitize`: the separate `build/m15-sanitize` configuration, all `ESM4*`
  suites by default so newly added native suites are not silently omitted.
  ASan leak checks are disabled; UBSan halts on errors. This is not leak coverage.
- `engine`: builds `openmw`, `openmw-tests`, `esmtool`; runs engine tests.
- `python`: `python -m unittest discover -s scripts/tests`, requiring a nonempty,
  clean result without skips.
- `--filter 'Suite.*:Other.*'` deliberately narrows C++ selection. The report
  records this; never describe a filtered run as all component/engine tests.

Build and test only the relevant modes; engine integration is appropriate for
world/service wiring or an accumulated integration checkpoint. Do not repeat
unchanged passing checks just to accumulate evidence. For configuration changes
or a missing build directory, read [references/build-config.md](references/build-config.md).
The helper does not configure or download dependencies automatically.

For existing evidence, use `scripts/verify_gtest.py --inventory LIST --xml XML
--filter 'EXPRESSION'`. The list must come from the same binary's unfiltered
`--gtest_list_tests`. Empty filters, duplicate/missing/unexpected cases,
nonexecuted/skipped/failed cases and corrupt XML fail verification. A process
exit code of zero is not sufficient.

## Close a chunk accurately

Inspect `verification.json` and relevant warnings/failures. Keep failed attempts
in their original evidence directory; rerun a corrected change in a new one.
Use `git diff --check`, inspect/stage only the intended files, and commit when
authorized. Do not push. Record the commit or tested dirty-source fingerprint,
selection, counts and evidence path in the milestone document where relevant.

Build, parser/unit, independent-oracle and normal-input runtime evidence are
different layers. None alone closes gameplay/restart gates. Follow the current
implementation plan's gates, not old counts or status remembered by the skill.
For real engine save/restart fixtures, consult
[references/runtime-checks.md](references/runtime-checks.md).

When adding or changing a typed TES4 record, read
[references/native-records.md](references/native-records.md) for engine registration
and independent record validation requirements.
