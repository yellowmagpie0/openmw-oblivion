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

GetAV helper `004F6060` branches on actor flags +8 bit **0x800 Disabled**, not
Dead. Verify flag meaning against the independent GetDisabled `004F60E0` and
GetDead `00502870` paths before assigning names. GetDisabled also checks an
enable-parent predicate; GetAV only checks the direct bit. Its disabled form
getter can retain exact int32 precision through a double result store, unlike
GetBaseAV's float-store/floor path. Use `actor-value-script-queries.py` and
`script-disabled-flag.py` in the ignored M15 oracle directory as the known
examples; do not conflate command return type with the underlying float getter.

For actor ModAV wrappers, distinguish eligibility input from converted storage:
Player checks god mode on the original signed input before the integer-to-float
round trip. INT_MAX can pass that gate, round to 2^31, then become INT_MIN and
trigger a negative-Health callback. NPC negative Fatigue additionally checks
virtual +278. `command-modifier-wrappers.py` exercises both BAABE0 CPU branches;
both agree for typed int32 command inputs. This does not establish arbitrary
out-of-range float conversion behavior. The Unicorn virtualenv does not include
Capstone; use the installed `objdump -d -M intel --start-address=... --stop-address=...`
for bounded disassembly, without adding a dependency just for instruction reads.

## Native death-count storage and queries

For the pinned 1.2.0416 executable, use the retained
`S2/oracle-emulator/dead-count.py` / `dead-count-insert.py` probes. `440FA0`
updates a per-base uint16 entry; `440F70` reads it; script helper `4F5010`
sign-extends before producing a double. `600C95` increments by one on terminal
entry, using base returned by +170 (`4D9B40`, actor+1C) when +190 (`977C50`)
returns true. Those targets match Player/Character/Creature vtables.
The other direct updater call at `4413BA` restores saved counters. The probes
cover all 65,536 increments, 131,074 signed/null/missing queries, and 32 new-entry
insertions. Only allocation supplies synthetic fresh 8-byte objects; actual
list insertion and queries execute. Do not replace this historical counter
with a scan of resident dead actors or invent old-save history from life flags.

## Essential wake timer

The retained `essential-wake.py` probe starts at `603E97` and stops at `603F5D`:
state 6 and knocked byte 1/3 gate subtraction. Process virtual +98 is `6439C0`
(float process+88 minus global B33E9C); +9C is `629290`. High/MiddleHigh knocked
getter +2E4 is `64B080` (signed process+11C); Low/MiddleLow return zero. On <=0,
state changes to 0 and the entry fraction-based Health adjustment is issued.
Track ESP carefully: `603F2A` pushes before the product store, so differently
spelled stack offsets refer to the same target slot. Do not infer full-base
healing from an isolated later load. The 27,648-case table and C++ comparison
cover both x87 words; actual Damage application and frame-time production are
boundary fixtures, not proved by this probe. Native raw knocked states are not
interchangeable with the existing TES3 animation-state enum.

## Permanent Magicka/Fatigue slots and resurrection

Do not apply the generic sparse-add probe to all native AVs. ActorValues
constructor `65BE10` allocates permanent AV9/10 nodes (+8/+C); actual lookup
`65C010` and zero-removal `65C9B0` keep them at +0. The generic sparse probe
stubbed those boundaries and missed this distinction. Use
`actor-dedicated-modifier-add.py` (2,312 exact-bit cases, no hooked game functions)
and `actor-container-reset.py` (432 all-AV checks) in the ignored oracle folder.
Container clear `65C6A0` preserves AV9/10 while clearing other slots.

`resurrection-reset.py` covers 3,840 common/NPC reset and keep-state branches;
its second boolean is false and visual/base/inventory/Health application remains
at declared boundaries. The preserve branch needs all body/cell/attached gates.
`player-resurrection-reset.py` covers 192 Player paths: only AV8–10 Damage zeros;
other Player modifier arrays survive. Common actor Script-container ownership
is not the Player's separate Script array. Consult each probe's boundary list
before using these results for a world resurrection implementation.

## Actor subobject and rest dispatch distinctions

In the pinned executable, Creature `00A710A4` and Player `00A739BC` are
MagicCaster subobject vtables (the +30 active-item getter). Their actor vtables
are `00A710F4` / `00A73A0C`; Character's actor table is `00A6FC9C`.
Use the correct subobject address point before interpreting virtual slots.
`S2/sources-01/actor-value-mutation-vtables.json` and
`actor-physical-vtables.json` identify actor tables independently via RTTI.

Do not apply the Player's hourly3,600-second restoration argument to NPCs
without tracing their dispatcher. The actor-manager slice `00678006..0067804E`
uses2 seconds when an actor was selected and Player remaining-hours is positive.
`S3/authority-draft/rest-npc-dispatch.py` executes20 original cases with the real
hours predicate; restoration callees are stubs. Eligibility/effect advancement
precede that slice, and other process tiers remain separate. This proves a
specific dispatch, not a full rest implementation or an NPC hourly-rate policy.

For IsInCombat, do not use Player's virtual+334 (006FE080, always false) as
its script result. Command00505FC0 and condition004F8F30 share dispatch, then
replace the Player result using006605A0(false). NPC/Creature005E6110 checks
process package0C/0D and has an additional true-argument gate. The8,422-case
`S3/authority-draft/combat-query-dispatch.py` probe supplies actor/process gate
returns and the Player list result; it does not prove list/pursuit maintenance.
Use native archive menus to check UI actions before adapting shared dialogs:
original sleep_wait_menu.xml has Rest/Wait and Cancel, no Until Healed button.
Its prompt/hour traits come from menus/strings.xml, not TES3 sRestMenu GMSTs.

## Actor manager clock

For the pinned executable, `673B10` stores the manager time at B3BCF0, resets
strictly above100000 or nonfinite values to+0, and preserves finite negatives
and signed zero. The full setter probe `manager-clock-setter.py` uses actual
CRT checks with no stubs. All Player/Character/Creature +368 virtuals resolve
to5FAAE0; `actor-clock-delta.py` runs its clock prefix to5FAB4A. Reset/negative
previous time produces zero elapsed except new time strictly inside(0,.3f).
The threshold is binary32; equality is excluded. Keep prefix arithmetic evidence
separate from full scheduler/resource execution. `actor-clock-compare.cpp` and
its186-row recorded TSV provide exact-bit comparison examples.
