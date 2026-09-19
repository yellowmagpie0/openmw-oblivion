# M15 native data and rule provenance

Status: **in progress**. Record decoding is not proof of runtime policy or
physical/crime formulas. Original-game behavioral probes and independent rule
expected values remain required before the S2 gate can close.

## Combat-style binary layout

Target: original TES4/Oblivion, header versions 0.8 and 1.0, 20-byte record
headers. Later-game CSTY layouts are not interpreted as Oblivion policy.

Primary implementation references, pinned during this investigation:

- [xEdit TES4 definitions at 9fb016884bec138ea6c7b872cec831537d464c3e](https://github.com/TES5Edit/TES5Edit/blob/9fb016884bec138ea6c7b872cec831537d464c3e/Core/wbDefinitionsTES4.pas).
  Download SHA-256: `76a649e7791c47dba17a54e55049f87fba76c776ff4eac5b61d61ae92fba98f0`.
- [Wrye Bash TES4 reader at 7469ce4daea0e3f32e0029097a0e28d5abef885f](https://github.com/wrye-bash/wrye-bash/blob/7469ce4daea0e3f32e0029097a0e28d5abef885f/Mopy/bash/game/oblivion/records.py).
  Download SHA-256: `e111e80fcbbc7919e38e551a1f7e0e25985269580fc30c74bef601f05af1a0cf`.

The six CSTD sizes below are independently present in the installed, hashed
winning official records and listed as layouts in Wrye Bash. xEdit's current
optional-tail declaration alone does not cover the shortest installed record.

| CSTD bytes | Present tail | Winning count |
| ---: | --- | ---: |
| 84 | Core attack/dodge/block policy and flags only | 1 |
| 92 | Range multipliers | 7 |
| 104 | Melee/ranged switch distances and buff standoff | 1 |
| 112 | Ranged/group standoff | 6 |
| 120 | Rushing chance and distance multiplier | 14 |
| 124 | Do-not-acquire flag | 100 |

CSAD is 84 bytes (21 finite floats); 71 winning styles have it, exactly those
with the Advanced flag. Padding contains nonzero editor-memory bytes in native
files and must be preserved without treating it as gameplay flags. Probabilities
are byte percentages; directional entries are weights and are not required to
sum to 100. Timer intervals are ordered and nonnegative. Distances/range
multipliers are finite and nonnegative. Additive attack bonuses and advanced
modifiers remain signed; decoding does not invent clamping rules.

Legacy omitted fields are represented as absent values. The editor's displayed
creation defaults are **not yet accepted as original-game runtime defaults**.
Likewise, actors without ZNAM are not silently assigned a zeroed style. The
initial inventory contains 2,218 such actors; policy resolution remains open.
All eight core flag bits are named; the trailing 32-bit flag currently has only
its documented low bit. Unknown subrecords and padding remain in the lossless
payload alongside typed fields.

## Rule/probe gate

No damage, block, armor, mastery, projectile, stealth, pickpocket, crime, fine,
jail, or identity formula has passed its independent oracle gate yet. The data
inventory command reports these open gates and returns failure overall even
when every structural content check passes. Future entries must state units,
rounding/clamping order, thresholds, supported version, independent expected
values and exact retained original-game probe evidence. Do not infer gameplay
acceptance from this document or the inventory counts.

## Creature and equipment input layouts

The same pinned xEdit definitions independently describe CREA DATA as 20 bytes:
creature type, three skills, one-byte soul plus padding, health, padding,
attack damage and eight attributes. RNAM is one-byte reach. CSDT selects one
of ten sound families; each following CSDI/CSDC pair is an individual sound and
byte percentage. A family can contain multiple pairs; later families must not
overwrite earlier entries. Native readback matches independent decoding of all
1,001 installed physical creature records, including all 771 sound entries.
This establishes record semantics, not animation timing or audible acceptance.

WEAP DATA is 30 bytes, AMMO DATA 18 bytes, and ARMO DATA 14 bytes. Equipment
weight/speed/reach are finite floats; weapon/ammo damage and armor hundredths
are unsigned 16-bit integers. The audit treats reserved padding as padding,
preserves all native equipment, and marks enchantment/staff execution as M16.
Crime ownership and faction values are inventoried without inferring their
runtime formula or access policy.

## Original combat-style initialization and legacy loading

Read-only inspection of the installed original 1.2.0416 executable, SHA-256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`,
provides an independent static oracle for the first pure style-policy helper.
Disassembly and extraction scripts remain local in `S2/sources-01`; the
[63 named initializer facts](M15-COMBAT-STYLE-DEFAULTS.json) contain numeric
inputs/provenance only. Winning content GMST overrides still apply; this is not
a list of final installed values or a behavioral-probe pass.

The original DefaultCombatStyle vtable at VA `0xa4109c`, slots 55–89, reads the
named default settings. Slots 90/91 implement secondary/core flag queries.
The normal style initializer at `0x4a9a00` and CSTD loader at `0x4abbc0` establish
two distinct policies. The loader clears the 124-byte standard block, initializes
selected tails, copies the available subrecord, then applies zero-value fixes.

| Input case | Independently observed original behavior |
| --- | --- |
| No explicit style | Use DefaultCombatStyle setting-backed values and flags |
| Historical CSTD lacks range multipliers | Literal `1, 1`, regardless of default range GMSTs |
| Historical CSTD lacks switch/buff/ranged/group fields | Corresponding default setting values |
| Historical CSTD lacks rush chance/distance | Corresponding default setting values |
| Historical CSTD lacks secondary acquisition flag | False, even if the default-style GMST is true |
| Stored switch distance is zero | Replace that individual component with its default setting |
| Stored rush chance or rush distance is zero | Replace with its respective default setting |
| Other stored zero, or smallest positive switch/rush distance | Preserve exactly |

`resolveCombatStyleStandard` applies those distinctions to immutable inputs;
it leaves raw/source fields untouched and diagnoses incomplete/invalid defaults.
It deliberately rejects nonfinite/out-of-domain authored inputs consistently
with the decoder. The test oracle uses distinct default and record values,
zero and the immediately adjacent positive float, all six historical sizes,
invalid inputs and signed additive bonuses. These tests establish pure input
resolution; actor selection, settings-store integration, advanced-modifier use
and independent original gameplay remain separate open gates.

A useful discrepancy: original compiled additive attack/power bonuses are 5,
where current editor creation defaults differ. Original attack-skill base is 0
and multiplier 20. xOBSE was used only for navigation; its command table has
mappings that disagree with inspected original addresses and is not copied as
an authoritative formula table.

The original advanced-data initializer (`0x4a9bf0`) maps all 21 named GMSTs
to their CSAD fields. Its getters (starting `0x4a9cb0`) use the record modifiers
only with the Advanced flag and a valid modifier block; otherwise they read
native settings. The pure advanced resolver preserves this selection and signed
modifiers. Malformed required record data is explicitly rejected, as in the
semantic audit, rather than silently pretending that authored data was applied.

## First live original-setting probes

[Seven inspected live observations](M15-ORIGINAL-SETTING-PROBES.json) now confirm
numeric lookup in the original 1.2.0416 game. The isolated reference character
was created through the normal new-game screens. Read-only console `getgs`
queries observed recoil bonus 30, idle maximum 1.5, attack-skill base 0,
attack-skill multiplier 20, difficulty multiplier 5, damage-skill multiplier
1.5 and damage-strength multiplier 0.5. The recoil bonus is the master's override
of compiled fallback 5. This distinction validates the required override order;
it does not turn the fallback catalogue into final installed settings.

The original run uses a task-local Proton prefix and windowed 1280x720 settings
with background keyboard/mouse input enabled for Xvfb. Steam was started
normally; no executable/DRM modification was used. Input logs, INI files,
inspected captures, executable hash and a normal prison-start quicksave remain
under `S2/original-04`. These are setting probes only. Damage/contact/crime,
style-choice behavior, animation and audio probes remain open.
