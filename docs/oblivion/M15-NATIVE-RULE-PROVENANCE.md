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

## First physical arithmetic helpers (behavioral gate still open)

Read-only static inspection of the same hashed original executable identifies
`0x547b90` (luck-adjusted skill), `0x547f00` (fatigue factor), and `0x547070`
(weapon pre-mitigation product). Retained disassembly is in
`S2/sources-01/{effective-skill-rule,fatigue-damage-rule,weapon-damage-rule}.txt`.
The native caller at `0x484f80` selects actor value 3 (Agility) for weapon type
5 (bow), otherwise value 0 (Strength); value 7 supplies Luck. It supplies
current/original weapon condition. Attribute/skill actor values are converted
to integers by the caller; the pure helper explicitly takes integers and does
not invent a rounding rule for future world adapters.

With `R` denoting a store to a 32-bit float, the reviewed arithmetic is:

- `effectiveSkill = R(clamp(skill + R(R(luck * luckMult) + luckBase), 0, 100))`.
- `fatigueFactor = R(fatigueBase - (1 - fatigueRatio) * fatigueMult)`.
- `baseTerm = R(baseDamage * weaponMult)`.
- `conditionTerm = R(conditionBase + conditionRatio * conditionMult)`.
- `skillTerm = R(skillBase + effectiveSkill * percent * skillMult)`.
- `attributeTerm = R(attributeBase + min(attribute, 100) * percent * attributeMult)`.
- `subtotal = R(conditionTerm * baseTerm * skillTerm * attributeTerm * fatigueFactor)`.
- `damage = R(subtotal * callerMultiplier)`; an explicit original argument can
  bypass the fatigue factor with one.

The percent constant at `0xa3b150` is the exact double representation of the
float `0.01f`; the skill cap at `0xa309f0` is exactly double 100. The new pure
helper uses double intermediate arithmetic and the observed float-store
boundaries. This is not a claim of bit-for-bit emulation of every x87 extended
intermediate: extreme/near-rounding gameplay probes remain an open gate.
Nonfinite/overflow inputs diagnose explicitly, as do negative physical factors
or governing attributes. Skill/Luck integers remain signed before clamping.
Condition above one and finite fatigue ratios outside 0–1 are not silently
clamped. Whether an actor may attack in that state belongs to controller policy.

| Setting | Compiled initializer | Installed winning value |
| --- | ---: | ---: |
| iActorLuckSkillBase | -20 | absent: -20 |
| fActorLuckSkillMult | 0.4 | absent: 0.4 |
| fFatigueBase | 1.25 | 1 |
| fFatigueMult | 0.5 | 0.5 |
| fDamageWeaponMult | 1 | 0.5 |
| fDamageSkillBase | 0.2 | absent: 0.2 |
| fDamageSkillMult | 1.8 | 1.5 |
| fDamageWeaponConditionBase | 0 | 0.5 |
| fDamageWeaponConditionMult | 1 | 0.5 |
| fDamageStrengthBase | 0.5 | 0.75 |
| fDamageStrengthMult | 1 | 0.5 |

Compiled facts are in the retained `native-setting-initializers.json`; winning
values come from the independent hash-bound inventory. The existing live
original probes verify skill/strength multipliers, but not this entire table
or the resulting damage. Tests use explicit installed inputs, hand-derived
products (26.71875, 53.4375, 119.53125, 1.875, 3.75), integer clamp boundaries,
repaired condition, fatigue bypass, finite-domain rejection and overflow.
One adjacent-float fatigue case deliberately tests rounding: half an ULP above
one rounds back to even one; the next ratio produces the next output float.

These helpers do not yet choose weapons, detect contacts, apply mastery,
block, armor, difficulty, or enchantments, or mutate actors. Those independent
rule families and original physical outcomes are still required for S2.

## Hand-to-hand, block fraction and player difficulty arithmetic

The same original executable's routines `0x547280` and `0x5474a0` provide the
next pure arithmetic slice (`sources-01/hand-armor-block-rules.txt`). With the
same `R`, percent constant, effective skill and fatigue factor defined above:

- Hand skill term: `R(fHandDamageSkillBase + effectiveSkill * percent *
  fHandDamageSkillMult)`; strength term similarly uses the capped Strength and
  `fHandDamageStrengthBase/Mult` settings.
- Hand interpolation factor: `min(1, R(strengthTerm * skillTerm * fatigueFactor))`.
- Hand health damage: `R(fHandHealthMin + (fHandHealthMax - fHandHealthMin) * factor)`.
- Hand fatigue damage: `R(healthDamage * fHandFatigueDamageMult +
  fHandFatigueDamageBase)`, or zero when the explicit suppression argument is
  set. The pure helper preserves this separate output. The gameplay conditions
  selecting suppression are not yet established by a behavioral probe.
- Block skill term: `R(fBlockSkillBase + effectiveSkill * percent * fBlockSkillMult)`.
- Block fraction: `min(fBlockMax, R(fatigueFactor * skillTerm * equipmentFactor))`.
  The equipment factor is one for a shield, `fBlockAmountWeaponMult` for a weapon,
  and `fBlockAmountHandToHandMult` for unarmed. Block eligibility, timing, angle,
  mastery and durability remain separate requirements.

Installed Hand Health Max is 15 (compiled 20), Hand Fatigue Damage Base is 1
(compiled 0), and Hand Fatigue Damage Mult is 0.5 (compiled 0.25). Other hand
inputs are compiled defaults: skill base 0/multiplier 1, strength base
0/multiplier 0.75, health minimum 1. Installed block settings use the compiled
values: skill base 0/multiplier 1, maximum 0.75, weapon 0.5, unarmed 0.25.
Typed setting builders apply native overrides and reject invalid domains.

`0x5e2560` supplies difficulty scaling (`sources-01/difficulty-rule-full.txt`).
For normalized slider `d` in [-1, 1] and native multiplier `m`, first compute
`scaled = R(d*m)`. The factor is `R(1+scaled)` at nonnegative difficulty and
`R(1/R(1-scaled))` at negative difficulty. For damage (not healing), multiply
when the player is the victim; otherwise divide when the player is the source.
An unknown source or neither-player case is unchanged. The original victim
branch has priority for self-inflicted damage. The helper takes an explicitly
resolved role and does not guess actor identity or alter the global slider.
Zero difficulty is unchanged. The already observed installed multiplier 5
(compiled 10) yields 6x incoming/one-sixth outgoing at maximum difficulty,
with the reverse at minimum. Identity dispatch and live damage observations
remain open; testing these scalar branches alone does not close that gate.

## Armor input rounding and total cap

The original armor caller at `0x488d3e` reads the unsigned 16-bit hundredths
field, divides by 100, then truncates to an unsigned integral armor value
before calling `0x547370`. The helper obtains effective skill, stores
`range = R(fArmorRatingMax - fArmorRatingBase)`, and computes
`scaled = R((fArmorRatingBase + effectiveSkill/100 * range) * integralBase)`.
It floors that value with a minimum of one, then multiplies by
`R(fArmorRatingConditionBase + conditionRatio * fArmorRatingConditionMult)`.
That ordering matters: 14.99 base at skill 50 becomes integral 14, floors to
9 after skill scaling, and yields 4.5 at half condition. Flooring after the
condition multiplication would produce a different answer. Zero condition
still produces zero with the native condition settings. No equipped items is
an inventory/aggregation case, not a call to this individual-item helper.

Original evidence: `sources-01/armor-rule-caller.txt`,
`hand-armor-block-rules.txt`, and `float-integer-helper.txt`. All four item
settings use absent-record compiled defaults: skill base 0.35, skill maximum
1, condition base 0 and condition multiplier 1. The separate total-rating
function around `0x60e763` caps against `fMaxArmorRating` only when positive;
zero disables that cap. Its compiled maximum is 90, installed maximum 85.
The pure cap helper preserves zero-as-disabled and validates finite,
nonnegative inputs. Mastery/effect contributions before the cap and mitigation
of an actual hit remain separate reviewed requirements.

## NPC health width: original loader overrides the editor schema

The pinned xEdit NPC DATA definition describes a 16-bit health plus two unused
bytes. That description is not the runtime rule. Independent official-content
inspection finds `TestStair` (`085e4b`), `TestArena02` (`0274cc`) and
`TestArena01` (`0274cb`) encoding **100,000** in the four-byte field. Treating
the upper bytes as padding would incorrectly reduce each to 34,464.

Original TESNPC RTTI `0xb02fb4` identifies its main vtable at `0xa53dd4`;
LoadForm slot 7 is `0x527e40`. The DATA branch calls common actor-data loading
at `0x46bda0`. That routine casts to TESHealthForm (`0xb05cf4`), reads a DWORD
at `0x46bef8` and writes all four bytes to the health component at `0x46bf04`.
The native reader therefore keeps its existing **uint32 health**. A proposed
padding change was discarded before commit, and the regression fixture uses
100,000 health. Retained investigation and the rejected patch/test are in
`S2/npc-inputs-01`; this was a corrected investigation, not a preexisting
32-bit-health defect.

## Melee attack fatigue arithmetic

Original `0x547560` computes `cost = R(base + weaponWeight * weightMult)`,
then, for a power attack, `R(cost * powerMultiplier)`. Caller `0x5e4010`
selects equipped weapon weight, uses zero for unarmed, and excludes weapon
categories 4 (staff) and 5 (bow) from this debit path. It negates the cost for
its fatigue mutation. Controller timing and bow draw fatigue remain separate
work. The pure helper takes explicit nonnegative finite weight/settings and
rejects arithmetic overflow; it does not mutate an actor or infer equipment.

| Input | Compiled | Installed and live original lookup |
| --- | ---: | ---: |
| fFatigueAttackWeaponBase | 8 | 7 |
| fFatigueAttackWeaponMult | 0.1 | 0.1 |
| fPowerAttackFatiguePenalty | 5 | 5 |

Disassembly is retained as `S2/sources-01/attack-fatigue-{rule,caller}.txt`.
`original-08/fatigue-settings.png` was opened and inspected; observations
35–37 extend the setting ledger. This run uses the synthetic reference plugin
and includes placement diagnostics, so its initialization differs from the
first prison-only lookups. No damage measurement is claimed. Independent
arithmetic expectations include weight 0/1/20/100/10000 -> cost
7/7.1/9/17/1007, the fivefold power branch, and a halfway rounding case that
requires the intermediate float store. All three new tests failed against
unimplemented stubs before the implementation.

## Mastery rank and directional power damage selection

Original `0x56a300` compares the supplied integer skill in order against
`iSkillApprenticeMin`, `iSkillJourneymanMin`, `iSkillExpertMin`, and
`iSkillMasterMin`. The compiled thresholds are 25/50/75/100, absent from the
installed overrides. Equality advances to the next rank; skills above 100 stay
Master and negative skills are Novice with those thresholds. The immutable
helper validates nonnegative, nondecreasing configured thresholds; equal
thresholds are supported and follow the original ordered comparisons.

Original `0x546ba0` selects the generic power multiplier below the applicable
unlock: Standing requires Apprentice, either side requires Journeyman,
Backward requires Expert, and Forward requires Master. Its callers at
`0x5ff44d` and `0x5ff4e1` supply the integer combat skill directly, without the
luck adjustment used by base damage. Caller policy distinguishes actual power
attacks from normal attacks; the new helper accepts only a typed power
attack direction. Invalid directions diagnose even at Novice rank. This
selection does not implement input timing, disarm, knockdown, paralysis or
other mastery consequences. Those remain open cases.

| Setting | Compiled | Installed |
| --- | ---: | ---: |
| fDamagePowerAttackBonus | 3 | 2.5 |
| fDamagePowerAttackStandBonus | 4 | 3 |
| fDamagePowerAttackSideBonus | 3 | 2.5 |
| fDamagePowerAttackBackBonus | 3 | 2.5 |
| fDamagePowerAttackForwardBonus | 3 | 2.5 |

Retained disassembly: `mastery-level-rule.txt`, `power-attack-damage-rule.txt`,
`power-attack-damage-callers.txt` under `S2/sources-01`. Tests use deliberately
different multipliers for every branch and threshold-minus/at/plus-one, so
identical installed directional values cannot conceal a wrong selection.
All four new cases failed against unimplemented stubs before the fix.

Original live read-only observations 38–46 confirm these nine inputs in
`original-11/{mastery,power,directional}-settings.png`. Each capture was opened
and inspected. The setting ledger now has 46 observations, including all 42
physical-input facts. Live lookup does not establish the mastery consequences
or the full power-attack outcome matrix.

## First original hand-to-hand health observation

[The damage probe ledger](M15-ORIGINAL-DAMAGE-PROBES.json) now retains both
original-11 attempts, including the failed power measurement. The successful
ordinary first punch used Hand to Hand 10, Strength 40, Luck 50, full fatigue
140, default difficulty zero and an unequipped/spell-free target at 500 health.
The expected range was recorded before attacking. At full contact fatigue,
`1 + 14 * (.1 * .3) = 1.42`; with all seven attack fatigue points still missing,
`1 + 14 * (.1 * .3 * .975) = 1.4095`. The original display changed
500.00 -> 498.58. Normal mouse input and inspected contact/recovery captures
support this bounded observation; precise contact fatigue was not measured.
A new regression test checks this independently observed health value and the
hand-derived depleted bound. Existing arithmetic already agreed with this
new characterization, so there was no newly failing production case.

The next held attack, after the opponent engaged, left target health unchanged.
The opponent attacked during that sequence; player contact/interruption timing
was not established. That attempt fails its recorded expected-health check
and is not discarded or relabeled as a formula pass. Authored zero CSTY attack
chance does not guarantee a passive actor once combat bonuses are considered.
Future isolated damage measurements use a fresh first strike. All full rule
matrices and native runtime combat integration remain open. The Proton wrapper
returned 143 after normal console exit; no whole-process acceptance is claimed.

## Melee damage is calculated before that attack's fatigue debit

Fresh first-power-punch case `M15-S2-ORACLE-HAND-03` in original-12 changes
500.00 -> 496.45 health at the same 10/40/50 skill/Strength/Luck inputs.
The expected bounded range was recorded before attacking. Post-attack player
fatigue reads 129.09, so a cost occurred, but the health result matches the
full-fatigue product `1.42 * 2.5 = 3.55`. This is a bounded first-hit observation,
not a complete directional/mastery or frame-timing gate.

Further independent call-path inspection resolves the debit order. Original
PlayerCharacter RTTI at `0xb080a4`, complete-object locator `0xac3d80`, main
vtable `0xa73a0c`, slot `0xeb` resolves to contact handler `0x5febf0`. The
attack update calls that handler at `0x5fcda4`, then debits attack fatigue via
`0x5e4010` at `0x5fcdb5`. The handler contains the previously reviewed hand
calculation at `0x5ff419` and power scaling at `0x5ff44d`. Thus that attack's
cost must not be subtracted before evaluating its damage. The ratio helper
`0x5f4880` supplies current fatigue / modified maximum, or one for a nonpositive
maximum. Full actor-value/max derivation remains world-adapter work.

Retained evidence: `player-attack-vtable.json`, `player-contact-handler.txt`,
`attack-contact-debit-order.txt` and `contact-fatigue-ratio.txt` under
`sources-01`. The new regression checks the independently observed 3.55 and
shows that a prematurely applied 35-point debit would yield a distinguishable
3.41875. Native controller execution of this ordering is still pending S4.
The original wide predictions are retained unchanged; they preceded this
ordering review. These observations replace the early working assumption that
an attack necessarily depleted fatigue before contact.

### Original first ordinary sword strike

`M15-S2-ORACLE-WEAPON-01` independently checks a first ordinary sword strike
in original-12 after an unchanged prison quicksave reload. Normal pickup and
inventory equip show damage 17 and condition 100; the player has Blade 10,
Strength 40, Luck 50 and fatigue 140. The authored weapon has base damage 100,
health 1000, weight zero and no enchantment. The fresh naked opponent has
health 500 and no armor/spells. Before attacking, the recorded prediction was
`100*.5*1*(.2+.1*1.5)*(.75+.4*.5) = 16.625`. One ordinary mouse attack produces
console health **483.38**, consistent with 483.375 rounded for display. The
component characterization checks this independently observed result and
rejects premature fatigue debit. Captures show equip, windup and recovery,
not an exact impact frame. Durability, block and the remaining weapon matrix
are not established by this bounded observation. See the damage probe ledger
and `S2/original-12/sword-observation.json` for hashes and frozen inputs.

### Durability amount arithmetic

Original functions `0x547240` and `0x547260` independently establish the wear
amounts. Weapon wear is `R(baseDamage * fDamageToWeaponPercentage)`, with one
float store. The contact caller at `0x5ff5b4` obtains the weapon's unsigned
16-bit TESAttackDamageForm value at offset `0x88`, calls `0x547240` at
`0x5ff5d8`, then passes the result to the actor condition mutation. This uses
base damage, not computed skill-scaled health damage or maximum condition.
Armor wear amount is `R(incomingDamage * absorbedFraction *
fDamageToArmorPercentage)`, with no intervening float store. Contact code at
`0x5ff789` supplies pre-armor health damage and the capped absorption fraction;
blocking/mastery can alter the amount later, before condition mutation.

Compiled constructors at `0x9e8c7f`/`0x9e8caf` establish defaults `.01`/`.5`.
Installed native overrides are approximately `.06`/`9`; original-12 read-only
lookups display `.06`/`9.00` (GMST probes 47–48). The settings table records
exact float values and keys. No shared TES3 setting lookup is used.

Four tests cover zero/base-damage extremes, absorption endpoints and adjacent
floats, native defaults/typed overrides, invalid domains/overflow, and a binary
rounding discriminator: `(1+2^-23)*(1-2^-24)*3` must round to `3+2^-22` rather
than a prematurely rounded 3. All four failed against zero-return stubs before
implementation (`S2/physical-rules-09/red.xml`). These helpers compute amounts;
contact eligibility, armor-piece distribution, block/mastery adjustments,
condition mutation and original runtime wear observations remain open. Retained
original evidence: `sources-01/durability-rules.txt`,
`durability-initializers.txt`, `contact-damage-continuation.txt`. The newly
captured `armor-wear-selection.txt` is follow-up evidence, not yet a completed
selection implementation.
