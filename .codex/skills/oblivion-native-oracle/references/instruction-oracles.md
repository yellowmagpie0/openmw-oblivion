# Original instruction execution

Use the local venv at `build/oblivion-compat/m15/S2/oracle-emulator/venv/bin/python`
when present; it contains Unicorn. Do not assume system Python has Unicorn.
Consult nearby successful harnesses before writing another loader. Relevant
examples are listed in `evidence-map.md`; they are ignored local artifacts, not
portable dependencies of this skill. If absent, construct a small independent
harness using inspected PE metadata and an available x86 emulator.

## Proven layout and conventions

For the known executable, existing harnesses map `[0x400000, 0xd00000)` and
fixtures/stack at `[0x2000000, 0x2010000)`. Copy each raw section to its declared
virtual address. `original-exe.json` supplies `(name, VA, raw offset, raw size)`.
Verify the image SHA before every experiment. Inspect alignment/SizeOfImage when
preparing another mapping; the constants are not universal PE parameters.

Set FPCW `0x27f`, FPTAG `0xffff`, FPSW zero; compare with FPCW `0x37f` only when
precision is relevant to the investigated gameplay discrepancy. The original truncation helper is `009828C0`.
Writing zero to `00BAABE0` selects its real non-SSE branch. Keep this choice in
the evidence report rather than silently replacing conversion with host casts.

At a hook boundary, emulate `ret N` by reading `[ESP]`, setting EIP to that
return address, advancing ESP by `4+N`, and placing the result in the correct
register/FPU stack. A cdecl caller removes its own arguments. Retain callee-saved
registers. Restore caller-live EAX/ECX/ESI etc when entering in the middle of a
function; several past faults were harness setup errors, not game behavior.

Do not rewrite instruction stubs on a reused Unicorn instance without
invalidating its translated blocks. Prefer a fixed `UC_HOOK_CODE` handler whose
return values read the current case, or a fresh machine. Change fixture data
freely. Put explicit success/reject stop addresses on **every** exit, including
null and early-return paths. Reaching an instruction budget is not a return.

## Scope and comparison

Use original instructions to answer the bounded question selected in SKILL.md.
Record expected behavior independently of production code and disclose stubs.
Choose representative cases and relevant branch boundaries. Exact float-bit or
random-sequence comparisons are optional diagnostics, not default combat gates;
use justified tolerances when behavior is intentionally approximate. Preserve
failures and distinguish harness faults from a falsified rule.

For known pitfalls or prior actor-value, death, rest, clock and dispatch findings,
search [instruction-oracles-history.md](instruction-oracles-history.md). Consult
only the relevant section rather than loading the entire historical catalog.
