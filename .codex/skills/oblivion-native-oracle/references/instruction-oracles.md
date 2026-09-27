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

Set FPCW `0x27f`, FPTAG `0xffff`, FPSW zero; compare with FPCW `0x37f` where
precision could affect a rule. The original truncation helper is `009828C0`.
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

## Interpretation pitfalls caught in this session

- On x87, `fcom` compares ST(0) with its operand. Track push/pop order before
  interpreting `fnstsw ax; test ah, mask; jp/jnp`. In fight scoring, two plausible
  preliminary interpretations were wrong: distance uses `min(0, term)` and the
  friend bonus requires responsibility product **less than** friend disposition.
  Independent execution exposed both errors.
- A stored float term and an x87 sum without a final float store differ at
  integer boundaries. Do not insert an extra cast just because inputs are float.
- Integer/float comparisons may load the integer exactly on x87. Converting
  integer radius 2147483647 to float incorrectly admits distance 2147483648.
- Equality is specific to each path: trespass timer expires strictly `> limit`;
  pickpocket transfer uses `< chance`, untouched exit uses `<= chance`.
- Native helper labels in cached xOBSE headers or filenames are hints. Verify
  code and callers: `005E32F0` returns race, `005E32D0` tests NPC base type,
  `005E0F30` tests sleeping state 9, and `005E6BA0` tests an alarm package.
  An attack-alarm method's `this` can be the victim despite an external label.
- Resolve live aliases/ownership layers before the pure rule. Missing resolution
  is not equivalent to a valid null record or an absent offender.

Record expected values from original instruction execution before comparing
C++. Include zero/one, adjacent representable thresholds, signed/extreme valid
inputs, invalid-domain cases and both outcomes for each branch. For stochastic
rules separate exact forced-draw tests from fixed-seed distributions with a
predeclared tolerance. Save failing cases/logs; distinguish bad setup from a
falsified rule. Avoid shipping an oracle script that imports production logic.

- A function advertised as returning float can leave an exact integer in x87.
  NPC/Creature form getter `0051E790` does FILD and RET without FSTP. The low
  process adds Script/Damage before rounding; a C++ float base parameter loses
  Health precision after a large SetAV. Use `base-health-float.py` and
  `base-health-query.py` as local examples of full form/process composition.
- Original `009828C0` has CPU-dependent overflow behavior. Its non-SSE path
  uses FISTP int64 and returns low bits; do not assume CVTTSD2SI's int32 sentinel.
  Keep unsupported conversion domains explicit until both paths are modeled.
