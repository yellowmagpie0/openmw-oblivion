# M6 ObScript corpus, frontend, and native IR

Status: accepted offline on 2026-08-10. The accepted working tree is based on
revision `cdd9860bd098cea9beaec94a853fe2ce398e5fce`; M6 changes are represented
by the accompanying dirty-tree manifest in the generated acceptance report.

M6 provides the complete non-executing ObScript substrate. It retains official
source and compiled payloads byte-for-byte, assigns a stable address to every
standalone and embedded result unit, parses all official source through a
dedicated frontend, and emits deterministic native `ObScript::Program` IR.
Runtime dispatch and world mutation remain disabled and are owned by M7.

## Delivered source

### Lossless typed records and stable corpus

`ESM4::ScriptDefinition` now retains independent optional byte arrays for
`SCTX` and `SCDA`, the decoded compiler view, the complete SCRO table as stable
`FormKey` values, compiled locals, header metadata, and optional quest stage
and result-entry provenance. The SCPT, INFO, and QUST loaders no longer discard
SCDA or flatten embedded scripts:

- consecutive INFO results become separate ordered units even when their
  source and compiled bytes are identical;
- every QUST result records its `INDX` stage and QSDT entry ordinal;
- SCPT header type distinguishes object, quest, and effect contexts;
- the owning form is a load-order-independent `FormKey`, while the revision
  plugin remains part of corpus identity so overrides are not conflated;
- raw source bytes remain distinct from Windows-1252-to-UTF-8 decoding.

`components/obscript/corpus.*` creates and sorts a `ScriptUnitId` containing
owner, revision plugin, execution context, stage, stage entry, and unit ordinal.
Duplicate identities fail the audit. Header-only metadata is preserved by the
record loader but is not miscounted as a source/compiled script unit.

### Deterministic frontend and native IR

`components/obscript/` contains a standalone lexer, recursive-descent parser,
value AST, semantic compiler, canonical serializers, coverage registry,
compilation cache, and SCDA decoder. The frontend supports the syntax and
vanilla compiler behavior reached by the official corpus, including:

- CR, LF, and CRLF input; comments; case-insensitive keywords; digit-led names;
- block/event labels and arguments, declarations both outside and inside
  blocks, references and member access, and space/comma command arguments;
- precedence, grouping, missing comparison operands, numeric coercion, and
  local/external/member assignment;
- nested if/elseif/else, split `else if`, missing endif, stray terminators,
  separator lines, returns, and multiple event blocks;
- vanilla duplicate declarations, retained in source order with deterministic
  first-declaration name lookup.

The native Program is a typed stack/control-flow IR with explicit event entry
points, locals, stable SCRO `FormKey` tables, loads/stores, calls, conversions,
branches, returns, and discards. Canonical AST and Program encodings exclude
source-location noise while preserving semantic case where it matters. Their
FNV-1a fingerprints are pinned across the C++ and independent Python tests.

The cache is keyed by the complete stable unit identity and verifies both raw
source and reference-table fingerprints. Reusing an identity with different
compiler input is an error. Diagnostics carry severity, stable code, source
location, owning unit/context, and therefore owning form. The coverage registry
is deterministically sorted and records command versus condition use plus all
execution contexts in which each name occurs.

No ObScript Program is registered with, dispatched by, or executed in the
runtime in M6. In particular, this work does not introduce a Lua execution
path or the later M7 native VM/event model.

### SCDA validation oracle

`components/obscript/scda.*` decodes every official instruction stream using
the ordinary `[opcode, payload-size, payload]` framing and the reference-
qualified `0x1c` wrapper with its SCRO index and nested instruction. It rejects
truncation and overrun, compares declared SCHR byte size, inventories literal,
local, and reference atoms, and compares the nine understood source/compiled
control opcodes.

The decoder is deliberately a validation oracle, not a runtime. Native source
IR remains the portable representation.

### Reproducible offline audit

`esmtool obscript report.json plugin...` performs the lossless scan, compiles
each unit twice through the cache, decodes SCDA, and emits per-unit payload,
AST, Program, reference, diagnostic, and provenance evidence. The report is
byte-deterministic for identical inputs.

`scripts/obscript_reference.py` is a separate Python lexer, parser, semantic
emitter, and canonicalizer. It does not import or invoke the C++ frontend and
compares every AST and Program fingerprint. The checked-in
`m6_obscript_count_lock.json` locks the canonical official plugin order, every
file size and SHA-256, per-plugin/context unit counts, raw payload byte totals,
SCRO count, SCDA progress, coverage count, and complete corpus fingerprint.

`scripts/oblivion_compat.py m6-acceptance` owns the repeatable gate. It is
explicitly offline-only: it runs audit tools and test executables but never
starts OpenMW, original Oblivion, or a Morrowind scenario.

## Official corpus result

The canonical English official set produced exactly 11,098 source-bearing
units and no bytecode-only units:

| Context | Units |
| --- | ---: |
| Object SCPT | 2,645 |
| Quest SCPT | 285 |
| Effect SCPT | 116 |
| INFO dialogue result | 5,977 |
| QUST stage/result | 2,075 |
| Total | 11,098 |

The retained source payload is 3,236,024 bytes. SCDA is present for 10,720
units (1,306,043 bytes); 378 official result sources have no compiled payload,
and none has compiled data without source. The corpus retains 29,484 stable
SCRO references. Its count-locked fingerprint is
`fnv1a64:0a9bd43a8a6fc49a`.

All 11,098 units parsed and lowered with zero diagnostics or failures. A second
independent implementation reproduced 11,098/11,098 AST fingerprints and
11,098/11,098 native Program fingerprints exactly. The coverage registry
contains 287 names, each with at least one command or condition use and one
execution context.

All 10,720 SCDA streams decoded fully, all 10,720 SCHR compiled-size fields
matched the exact retained payload, and the decoder walked 94,260 instructions.
Control structure matches exactly for 10,665 units. The other 55 are retained
and reported per unit; their differences are limited to known vanilla
else/elseif/endif source-after-compile or tolerated-terminator behavior. They
are progressive bytecode-oracle differences, not source parse or IR failures.

## Offline verification

The acceptance command is:

```sh
python3 scripts/oblivion_compat.py m6-acceptance \
  --source "$PWD" \
  --build "$PWD/build" \
  --sanitized-build "$PWD/build-oblivion-sanitized" \
  --oblivion-data "$HOME/.local/share/Steam/steamapps/common/Oblivion/Data" \
  --output "$PWD/build/oblivion-compat/m6-acceptance"
```

Durable local evidence is written to:

- `build/oblivion-compat/m6-acceptance/acceptance.json`
- `build/oblivion-compat/m6-acceptance/acceptance.html`
- both complete deterministic corpus reports and the independent-reference
  report in the same directory
- focused, sanitizer, full component, full engine-unit, and Python harness logs
  below `build/oblivion-compat/m6-acceptance/tests/`

The nine focused synthetic/property tests cover raw INFO/QUST repetition,
stage/entry identity, exact SCTX/SCDA/SCRO retention, whitespace and line
ending invariance, labels and references, precedence and coercion, duplicate
declarations, control-flow quirks, cache separation/invalidation, diagnostics,
coverage roles, ordinary/reference-qualified SCDA, truncation, and bounded
source/bytecode mutation corpora. The same focused suite passes under ASan and
UBSan in a GCC Debug build. The canonical RelWithDebInfo build uses GCC; Clang
is not installed in the test environment. The final offline gate passed all
1,492 component tests, all 495 engine unit tests, all 19 compatibility-harness
tests, and all nine focused tests in both the normal and ASan/UBSan builds.

No game executable was run during this milestone's acceptance, as required
for this task. Visual, campaign, live runtime, and Morrowind gameplay scenarios
are consequently not claimed as M6 evidence. The complete component and
engine unit suites provide the available offline shared-code regression gate;
M6 changes do not add runtime hooks or alter the Morrowind script path.

## Success assessment

M6's delivery and offline success criteria are satisfied. Every official unit
is represented once by stable identity, both payload types and reference tables
are retained, all source compiles to deterministic native IR, independent AST
and IR results agree, diagnostics and cache results repeat exactly, every
compiled payload decodes within its exact bounds, repeated INFO/QUST payloads
remain separately addressable, and the coverage registry spans the complete
compiled corpus.

There are no open M6 gates. Native execution, persistent script instances,
event dispatch, and world-manipulation commands remain intentionally disabled
for M7; no later-milestone behavior is implemented here.
