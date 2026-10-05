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

Bounded independent rule checks are recorded in the checkpoint entries below;
these passes do not close the complete S2 or runtime gates. The initial data
inventory's open-gate result is structural evidence, not a current summary of
later rule comparisons. Entries must state units, rounding/clamping order,
thresholds, supported version, independent expected values and exact retained
original-game probe evidence. Do not infer gameplay acceptance from record
inventories or bounded instruction execution alone.

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
  set. The pure helper preserves this separate output. Checkpoint50 establishes
  caller/getter selection from original instructions; normal-input gameplay
  remains a separate open gate.
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

### Armor mitigation, wear mastery and condition boundary

Original contact `0x5ff71e` calls the victim's armor-rating slot `0x348`;
PlayerCharacter's reviewed vtable maps it to `0x60e580` (the previously
reviewed rating aggregation/cap function). Constants at `0xa309f0` and
`0xa2fe7c` both decode to 100. Rating is limited to 100, divided by 100 and
stored as float at `0x5ff759`, then limited by `fArmorRatingMax` (storage
`0xb36ea0`, already catalogued). The caller's bypass branch substitutes zero.
Health damage is `R(damage * (1 - storedFraction))` at `0x5ff7a9`–`0x5ff7b9`.
The new helper accepts already aggregated rating and caller-resolved bypass;
it does not establish mastery/sneak bypass eligibility or block ordering.

Player condition slot `0x2c4` maps to `0x65ff10`, forwarding to `0x5f3870`
when the player cheat guard permits. The latter skips nonpositive wear, applies
armor mastery unless its bypass argument is set, then subtracts wear from
current item health with a float store. Remaining condition **below one** is
snapped to zero (`0x5f3940`–`0x5f3953`), not merely clamped at zero. A zero-wear
call leaves even fractional condition unchanged. Repaired excess is not capped.
Breakage/unequip/events and the original player cheat guard remain world policy.

For armor, `0x4b4c70` decodes BMDT heavy bit 7; heavy uses skill AV `0x12`,
light uses `0x1b`. `0x5f3870` selects Novice factor at rank zero, unity at
Apprentice, Journeyman factor at rank >= 2. Four native settings have compiled
and live values 1.5 (both Novice) and .5 (both Journeyman); no installed
overrides. Original probes 49–52 independently display those values. The
factory uses the native settings inventory and verified fallbacks. These two
mastery effects now have pure amount selection; actual equipped-item wear and
persistence still need native integration and runtime acceptance.

Five tests first failed against stubs, then passed: mitigation/cap/bypass,
one-point break boundary with adjacent floats, mastery threshold minus/at/plus
one for both armor classes, typed defaults/override, and invalid inputs. Full
results: `S2/physical-rules-10`. Disassembly: `condition-mutation.txt`,
`player-condition-mutation.txt`, `armor-weight-class.txt`, and
`contact-damage-continuation.txt` under `sources-01`.

Exploratory original inventory capture `original-12/sword-condition-after.png`
shows condition 99 after the first sword hit (before: 100). This is consistent
with 6 wear leaving 994/1000, but integer UI precision does not prove exact wear
and the wear prediction was not recorded before that hit. Its diagnostic
record explicitly leaves acceptance false; it is not a new completed gate.

### Armor wear slot selection

Original `0x5e5a00` uses a random integer modulo 100 and at most seven attempts.
Within one attempt it tries head (then hair), upper body, lower body, hands,
feet using cumulative native chance thresholds. A missing candidate falls
forward if the same draw is below the next threshold. **Missing feet do not
fall through to shield**: that path retries. Shield is the complement interval
at/above the sum of the five thresholds. Despite its name,
`iArmorDamageShieldChance` is not read anywhere in this routine. A selected
candidate ends selection immediately; cached missing lookups do not eliminate
retry draws. `0x486790` queries biped slots 0–5; the last branch queries the
process's equipped shield slot `0xf8`. Caller equipment eligibility remains
separate from this pure availability/selection helper.

The five threshold defaults and original live lookups are 10/25/15/10/10;
shield's independently looked-up setting is 30 but deliberately is not an input
to this routine. Tests exhaust all 100 draws, each missing-piece fallthrough,
head/hair preference, no backward wrapping, absent gear, zero/100 settings,
invalid draws/types/ranges, and a fixed-seed 100,000-draw distribution with
predeclared 700-count tolerance. All five tests failed against stubs before
implementation. The world must still implement up-to-seven RNG draws and
candidate ownership/lifetime; this helper represents one attempt, not a world
transaction. Evidence: `sources-01/armor-wear-selection.txt`, original-12
GMST captures 53–58, and `physical-rules-11`.

Additional actor mastery query inspection (`actor-mastery-query.txt`) shows
`0x5f23b0` obtains the skill via `0x5f1910`, then calls the reviewed threshold
rule `0x56a300`. Native actor integration must preserve that value source;
it must not substitute luck-adjusted damage skill.

### Creature natural attack arithmetic

Independent RTTI/vtable inspection identifies Creature RTTI `0xb10a1c`,
locator `0xac3864`, vtable `0xa710f4`. Natural-damage slot `0x34c` points to
`0x624f90`; armor slot `0x348` points to `0x5e0cd0`. The natural-damage method
obtains unsigned 16-bit TESAttackDamageForm damage through `0x468a10`, obtains
fatigue ratio through `0x5f4880`, calls `0x546b00`, then truncates the result via
`0x9828c0`. `0x546b00` calls the reviewed fatigue multiplier and stores the
base-damage product as float before that integer conversion. Thus the pure
contract is `truncate(R(baseDamage * fatigueMultiplier))`, with signed output
and no invented fatigue ratio clamp. The contact handler at `0x5ff3b1`–
`0x5ff3c5` converts that integer back to floating damage for subsequent policy.
It does not route this unarmed creature branch through NPC hand/skill math.

Two tests first failed against stubs, then passed. Cases include base damage
0/1/65535, fatigue zero/full/excess/negative, float-store boundaries, compiled
versus installed fatigue settings, and checked signed-integer overflow.
Creature armor's virtual method queries actor value `0x2b`; aggregation/effect
semantics, auto-calculated creature base stats and world hit acceptance remain
open. Retained sources: `actor-physical-vtables.json`,
`creature-natural-damage.txt`, `creature-damage-rule.txt`,
`creature-base-damage.txt`, `creature-armor-rating.txt` under `S2/sources-01`.

### Bow launch arithmetic

Original release code `0x5fd278` reads the player's bow timer at `+0x640` and
computes `min(1, R(timer * fArrowBowTimerMult + fArrowBowTimerBase))`; the
nonplayer branch supplies full draw. The min helper is `0x4ac760`. Timer
accumulation/release eligibility still need controller tracing and runtime
acceptance; the pure helper takes the already resolved timer value.

The projectile constructor at `0x60c940` stores launch damage at `+0x70`.
At `0x60ca45`–`0x60ca76`, it calls the reviewed item damage function separately
for bow and ammunition, adds their results with a float store, then multiplies
by draw fraction with another float store. The ammunition branch of
`0x484f80` uses Marksman, Luck, Agility, current fatigue, base ammunition damage
and condition ratio 1. Bow uses its own condition. Both item calls add the
actor's integer AttackBonus AV `0x2a` after base damage; the pure launch input
exposes this caller-resolved value without executing an effect. Original
impact reads stored damage (`0x4b9f60`) rather than rebuilding launch stats.

Speed: `full = R(ammoSpeed * fArrowSpeedMult)`, then
`R(full * (draw + (1-draw) * fArrowWeakSpeed))`. Gravity coefficient at
`0x547700` uses the existing luck-adjusted skill, then
`full = R(fArrowGravityBase - skill*fArrowGravityMult)`, followed by
`max(0, R(full*draw + (1-draw)*fArrowWeakGravity))`. This function does not read
`fArrowGravityMin`; world acceleration, trajectory integration and the other
uses of that setting remain open. These are launch arithmetic contracts, not
projectile runtime acceptance.

Seven compiled defaults are .25/.4 (timer), 1500/.01 (speed), .3/.002/1.75
(gravity). No installed overrides were found. Original-13 normal Continue
into the prison with only Oblivion.esm active produced read-only setting
observations 59–65 and exited cleanly with code 0. The final capture contains
all seven lookups and was opened. **GravityMult displays 0.00**, so that lookup
cannot verify .002 precisely; exact initializer bytes and absence of an
override provide the exact-value provenance. The setting ledger records that
precision limitation rather than treating displayed zero as the actual value.

Six new tests failed against stubs. The first implemented run exposed a test
expectation error: the first representable draw below .5 still rounds to
.975f gravity, while the second smaller draw crosses the next float midpoint.
Independent rational arithmetic from exact decoded operands established the
plateau (`projectile-rules-01/gravity-boundary-independent.json`); the test was
corrected and both failed full-suite/sanitizer attempts are retained with the
`initial-` prefix. No production formula was changed for this correction.
Final evidence is in `projectile-rules-01`; original source traces are
`arrow-launch.txt`, `bow-draw-release.txt`, `arrow-gravity-rule.txt`,
`arrow-impact-damage.txt`, and `weapon-rule-callers.txt` under `sources-01`.
The earlier candidate `ai-weapon-valuation-rule.txt` was identified as AI
weapon valuation, not bow damage, and is not used as a bow-rule oracle.

## Bow fatigue and launch sampling

The first original bow prediction failed: with Marksman 5, Agility 30,
Luck 50, initial Fatigue 140, bow damage 100 and ammunition damage 20,
assuming full fatigue at release predicted 14.85 damage. The observed first
shot reduced target health from 500 to 486.26. The draw capture visibly shows
fatigue depletion. This is a retained failed prediction, not a verified
release-time value inferred backwards from damage (`original-14`).

Original `005FACC8–005FAD16` applies a hold debit only to the player, with
process action 5 and Marksman (`AV 1C`) mastery rank zero. It multiplies
`fMarksmanFatigueBurnPerSecond` (15) by the caller's frame duration, stores
one float, negates it, and calls fatigue mutation `005E07D0`. The pure
`bowHoldFatigue` models this positive debit; the controller must supply the
actual hold action and duration. NPCs do not use this hold branch. The
mastery check uses the established thresholds (default Apprentice 25), not
`iMarksmanFatigueBurnPerSecondSkill` (20), for which only initializer and
registration references were found.

Original `005FD47C` constructs the projectile with launch damage before
`005FD4B0–005FD4D1` applies `fMarksmanFatigueBurnPerShot` (5) to a novice
archer. This latter branch does not exclude NPCs. It must not reduce the
fatigue ratio used by the already-created projectile. Weapon wear follows
at `005FD50C–005FD53C` using base bow damage and the established wear rule.
Hold/shot settings have independent compiled constructor provenance and
read-only original lookups GMST-66–68. GMST-69–70 also record regeneration
settings; regeneration arithmetic and integration remain open.

Three tests fail with unimplemented stubs, then pass: all mastery boundaries,
custom thresholds, player/NPC/hold combinations, zero and fractional frame
durations, invalid settings/types, and overflow. Full component and focused
ASan/UBSan results are in `S2/projectile-rules-02`. Source disassembly is in
`sources-01/bow-hold-fatigue.txt` and `bow-shot-fatigue.txt`. These helpers
are not yet connected to a native bow controller or actor fatigue state.

A fresh normal F9 reset and pickup/equip repeated the shot with fatigue sampled
while paused at full draw: 119.58/140. A new prediction was recorded before
release: target health 486.233989, with .05 tolerance for display precision
and the hold-to-release transition. Normal release produced health 486.26
and consumed one arrow. `M15-S2-ORACLE-BOW-02` passes only that bounded
health comparison; it does not establish exact frame timing. An additional
regression already agrees with the existing launch arithmetic and explicitly
distinguishes full-fatigue and premature per-shot-debit alternatives.
The initial failed prediction remains in the ledger alongside this repeat.

## Damage knockdown roll

Original `005475D0` takes victim Agility, Luck, fatigue ratio, and integer
contact damage. Caller `0060024F–00600282` truncates the contact's saved
pre-armor damage (`005FF5A2`), queries fatigue and integer AVs 7/3, and passes
them to this rule. Unconsciousness and mastery knockdown are separate caller
branches; this helper does not decide those policies.

With R denoting a stored float, adjusted agility is
`R(effectiveCombatSkill(agility,luck) * fatigueMultiplier)`. Numerator is
`R(damage * fKnockdownDamageMult + fKnockdownDamageBase)`; denominator is
`R(fKnockdownAgilMult * adjustedAgility + fKnockdownAgilBase)`. The ratio is
stored as a float and capped at `fKnockdownChance`. Original signed integer
RNG remainder `%100` is divided by double 100, and succeeds when **less than
or equal to** this threshold. Exactly .25 therefore accepts 26 draws; a
stored ratio just below .06 rejects draw 6. The rule does not clamp negative
ratios up to zero. A zero numerator with positive denominator accepts draw
0; original unordered 0/0 rejects all draws, positive infinity is capped,
and negative infinity rejects all draws. The native pure helper handles
these zero-denominator outcomes explicitly without propagating NaNs.

Compiled factors are agility base 0, agility multiplier 1, damage base 0,
damage multiplier **-3**, cap .25. Installed Oblivion.esm overrides the last
two to .3/.3. The negative compiled multiplier is intentional provenance,
not a transcription error to silently replace with an installed value.
All four arithmetic coefficients accept finite signed settings; maximum
chance must be finite in [0,1]. Inputs and intermediate arithmetic overflow
are checked. World reaction eligibility, RNG state, animation and physics
remain outside this helper.

Four tests fail against stubs and pass after implementation. Cases exhaust
all 100 draws for multiple agility/luck/fatigue combinations, check exact
float/inclusive boundaries, zero denominators, signed factors, typed native
defaults/overrides, malformed inputs and a fixed-seed 100,000-roll sample
with predeclared 26% +/-700 tolerance. Source traces: `knockdown-rule.txt`,
`contact-reactions.txt` and `contact-damage-continuation.txt` in
`S2/sources-01`. Test evidence: `S2/physical-rules-13`.

## Knockback and actor-value normalization

Original `00547690` divides luck-adjusted agility by the fatigue multiplier,
stores that float, then stores `damage * fKnockbackDamageMult +
fKnockbackDamageBase` and `adjustedAgility * fKnockbackAgilMult +
fKnockbackAgilBase`, and multiplies the stored factors. Caller
`005FFFAF–00600038` uses truncated saved contact damage and applies the
upper force cap. It does not add a lower zero clamp. Direction and timed
physics application follow at `0060003C–006000AF`, using `fKnockbackTime`.
The helper returns signed capped force and leaves transform/physics to the
caller. A zero fatigue multiplier would make original arithmetic nonfinite;
this native boundary diagnoses it rather than handing NaN/inf to physics.
Correct world eligibility/recovery for such a state remains an open case.

Compiled settings (also unoverridden in the audited load order) are agility
base 1, multiplier -.008, damage base 50, multiplier 10, maximum 512, time 1.
Original-15 lookups GMST-76–81 confirm the displayed values; GMST-77 displays
-.01 and cannot establish exact -.008 precision. The exact factor comes from
compiled bytes and the absent override. All six values are visible in the
inspected `knockback-settings.png` capture.

Actor mastery calls `005F1910`, which calls base-value resolver `005EAD00`,
stores a float, truncates and subtracts one for a negative fractional part:
this is floor, not nearest rounding or luck-adjusted skill. The resolver uses
the base-form AV virtual `+128`; player dynamic AVs 8–11 also include its
base adjustment path. Effects/current-value queries are separate. Native
`combatBaseValue` models the checked float-to-integer floor only; resolving
all actor/effect contributions is still an integration task.

Fatigue ratio `005F4880` stores the integer base fatigue as a float and divides
the current AV (`+288`) by it, storing the result. **Only zero** base fatigue
returns 1; signed nonzero bases still divide and current fatigue is not
clamped. `combatFatigueRatio` preserves that storage order and behavior.
Source traces: `actor-base-value-query.txt`, `actor-value-base-components.txt`,
`actor-fatigue-ratio.txt`, `knockdown-rule.txt` (includes knockback), and
`contact-reactions.txt`, all under `S2/sources-01`.

Five new tests fail against stubs, then pass, covering signed/zero/normal/
maximum force, fatigue and luck, cap boundaries, malformed inputs and singular
physics input diagnostics, typed settings, base-skill floor immediately around
all mastery thresholds (including negative values), integer overflow, fatigue
zero fallback and a base value above float's exact integer range. Evidence:
`S2/physical-rules-14`. These arithmetic tests do not establish world
knockback, controller interruption, or mastery ability acceptance.

## Mastery proc decisions

`masteryrules.*` defines pure decisions with explicit `mConsumesDraw`; the
caller must advance persistent RNG only at the corresponding original branch.
These decisions do not themselves drop equipment, apply paralysis or change
controllers. The ability matrix remains pending for those real consequences.

- Contact `006002EA–006003A4` requires Expert in the attack's base skill and
  target **base Speed > 0** before taking one `%100` draw. It takes that draw
  even for an attack direction without a mastery effect. Arrow/backward-power
  knockdown uses `draw < iPerkMarksmanKnockdownChance`. At Master,
  arrow/forward-power paralysis uses the **same draw**, with strict `<
  iPerkMarksmanParalyzeChance`, and clears the knockdown flag. The Marksman
  setting names do not restrict these settings to arrows. Existing damage
  knockdown remains unless paralysis supersedes it. Native attack group values
  17/18/19/1A hex map to forward/back/left/right power in the pinned xOBSE
  `NiNodes.h` enumeration and reviewed original dispatch.
- Side power disarm `005FC090–005FC299` excludes creature attackers, needs
  left/right power animation, a target weapon entry, and Journeyman attack
  skill. The draw is **inclusive <= iPerkAttackDisarmChance**. Quest-weapon
  and attacker-weapon-drawn guards follow the roll; they must not erase RNG
  consumption. The item quest guard is TESForm virtual +78; drawn guard is
  `005E0DA0 -> process +304` (GetWeaponOut in pinned GameProcess.h).
- Defensive disarm `005FC2B0` excludes creature defenders and requires an
  opponent weapon entry. With weapon/shield present it needs Master Block
  **and a shield**; with neither it uses Master Hand to Hand. The inclusive
  roll precedes quest-item and defender-drawn guards. Real drop implementation
  is visible after `005FC3A1` and remains a native integration requirement.
- Defensive stagger query `005F3C30–005F3CBA` uses Expert Block with shield
  when a weapon/shield is present, otherwise Expert Hand to Hand. Its roll
  is inclusive `<= iPerkBlockStaggerChance`. Thus the unarmed Expert branch
  needs an additional ability-matrix row despite being absent from the
  Hand-to-Hand SKIL rank message. Full caller/contact timing remains open.

Compiled proc settings are disarm 5, defensive disarm 5, stagger 5,
knockdown 5, paralysis 5, unarmed-block recoil 25. Installed stagger is **25**;
original-15 read-only GMST-82–87 confirm all six values. The recoil setting
is inventoried here; its complete contact eligibility is still to implement.
Seven new tests fail against stubs, then pass, including skill/equipment
permutations, exact roll/endpoints, shared-roll precedence, RNG-consumption
guards, custom thresholds, all setting mappings, invalid enums/types/ranges,
and fixed-seed distributions with predeclared bounds (`S2/mastery-rules-01`).

Paralysis source trace: `00600355–006003A2` gets a default SpellItem through
`0041B880` (global B335AC). Initializer `0041C7FC–0041C884` tries FormID
000137, absent from the independently scanned master, then creates a fallback
named DefaultMarksmanParalyzeSpell. Fallback `0041D590` constructs PARA and
calls duration setter `004133E0` with **10 seconds** (`0041D62A`). EffectItem
+0C is duration, independently documented in pinned GameForms.h and confirmed
by the setter. Content overrides and effect/controller resistance/lifecycle
must still be implemented; the pure proc's boolean is not effect acceptance.
Disassemblies and absent-record scan are retained under `S2/sources-01`.

## Blocking fatigue and blocking-item wear

Original `00547590` receives current integer Block, incoming damage, and
absorbed fraction, but its arithmetic **does not read the damage argument**.
It stores `currentBlock * fFatigueBlockSkillMult + fFatigueBlockSkillBase`,
then stores `fFatigueBlockMult * absorbedFraction + fFatigueBlockBase`, then
stores their sum. Caller `005F5B70` (actor vtable +3B4, reached at `005FF8D9`)
selects a shield first, otherwise a weapon. It uses **base** Block mastery:
Novice applies the signed fatigue debit; Apprentice skips fatigue; Journeyman
and higher also skip blocking-item wear. The wear amount is the stored product
of incoming damage and absorbed fraction and passes directly to the item's
condition mutation (with any armor mastery behavior there). Do not multiply
it by the separate generic armor-damage GMST again.

`blockContactCosts` returns these two amounts without mutating inventory or
fatigue. It separates base and current skill, preserves finite signed fatigue
settings/results, and validates damage/fraction/range/overflow. The original
Novice branch is not conditioned on an equipped blocking item; the item only
controls whether a wear mutation exists. Normal block/contact eligibility and
controller timing still belong to the integration caller.

Compiled cost factors are base 0, mult 1, skill base 5 and skill mult -.04.
Installed skill base/mult are 20/0. Original-15 GMST-90–93 confirm these values.
The same inspected capture has essential recovery settings GMST-88–89; those
are provenance for the next recovery task, not implemented recovery behavior.
Three new tests fail against stubs, then pass, checking below/at/above mastery,
current-vs-base skill, item absence, zero/one/fractional absorbed damage,
signed settings, damage independence, custom thresholds, typed overrides and
invalid inputs. Evidence: `S2/physical-rules-15`; source traces
`block-fatigue-rule.txt`, `block-fatigue-caller.txt` and contact continuation.

Additional defensive sequencing trace at `00600528–00600565`: ranged contacts
skip the stagger query; a successful stagger skips the subsequent block disarm
query. These decisions require ordered RNG advancement, not parallel rolls.
The pure proc helpers remain individually usable, but the future contact
controller must preserve this order (`defensive-proc-caller.txt`).


## Essential unconscious health reset

Original `006006B4` initializes the process recovery timer from
`fEssentialDeathTime` (compiled/installed 10 seconds). The essential branch
sets actor state 6 and resets health to base Health times
`fEssentialHealthPercentReGain` (compiled/installed float32 .3). It first
stores the integer base as float, then stores the product, then stores
`target - current` before invoking actor-value mutation. The same arithmetic
runs again at `00603EFC–00603F5B` during recovery. It can lower current health;
there is no minimum-one or maximum-base clamp in this calculation.

`essentialRecoveryHealth` implements those stored arithmetic steps and
returns target and adjustment. It validates finite current health,
nonnegative base health/settings and arithmetic overflow; a health fraction
above one remains valid. Controller state, effects, mutation caps, elapsed
time and saves are deliberately outside this pure helper. Read-only original
GMST-88–89 confirm settings; they do not prove knockout gameplay.

Recovery update `00603E97–00603EF7` requires actor state 6 and process knocked
state 1 or 3, invokes the process timer update/getter, and recovers at timer
less than or equal to zero. Constant A2FAA8 independently decodes to float32
zero (`essential-timer-comparison.json`). Names and transition semantics of
the process states remain a runtime investigation; they are not guessed into
a new public state enum. Source traces: `essential-recovery.txt` and
`essential-recovery-update.txt` under `S2/sources-01`.

Two tests fail against the zero-return stub, then cover independent arithmetic
values, signed current health, zero/base/over-base fractions, adjacent float
boundaries, integer-to-float rounding, typed defaults/overrides, invalid types
and overflow (`S2/physical-rules-16`). These tests establish arithmetic only.


## Armor coverage, Master rating, and worn weight

Original `0060E580` accumulates worn armor rating with duplicate base-form
suppression across equipment slots, stores the item total, then checks base
Light Armor mastery (`005F23B0` calls base-value query, then rank lookup).
At Master, `0060E727–0060E74E` requires heavy coverage zero and light coverage
at least `iPerkLightArmorMasterMinSum` (5). It multiplies item total by
`fPerkLightArmorMasterRatingMult` (1.5), stores, adds Defense actor value 2B
through `005E0CD0`, stores, then applies the previously documented maximum.
`masteryArmorRating` provides this final nonnegative-rating arithmetic; the
world still owns per-item aggregation, deduplication and cache invalidation.

Coverage is **not a piece count**. `005E5C80–005E5EAF` checks head, hair,
upper body, lower body, hands, feet and active shield. Matching armor weight
class contributes the corresponding `iArmorDamage*Chance` value: head/hair
both use Helm, then Cuirass, Greaves, Gauntlets, Boots and Shield. Unlike wear
selection, this query really reads ShieldChance. It clamps the sum to 0–100.
Our nonnegative validated settings use a wide sum to avoid signed overflow.
It does not deduplicate occupied slots in this coverage query. Thus default
single-slot light coverage can exceed minimum 5; inventing a five-piece
requirement would change behavior. `004B4C70` reads armor's heavy flag.

`00487FFA–00488099` applies equipped armor's weight reduction to one worn
instance before adding remaining stack members at ordinary weight. Base
Heavy Armor Expert uses `fPerkHeavyArmorExpertSpeedMult` (.5), Master uses
`fPerkHeavyArmorMasterSpeedMult` (0); base Light Armor Expert and Master use
`fPerkLightArmorExpertSpeedMult` (0). Despite the GMST names, these call sites
multiply weight. The equipped-weight path at `00488342–00488390` agrees.
`wornArmorWeight` computes the one-instance value, not stack totals or movement.
Original-15 GMST-94–98 were captured, opened and inspected and match all five
compiled settings. Files `armor-rating-aggregate.txt`, `armor-equipped-sum.txt`,
`actor-mastery-query.txt`, `armor-weight-mastery.txt`,
`armor-equipped-weight-mastery.txt` and `armor-additional-rating.txt` retain
source traces. Four new tests fail against stubs, then cover all 128 coverage
subsets, threshold/permutation/rank cases, stored arithmetic, overrides and
malformed values (`S2/physical-rules-17`). Real armor mastery execution and
restart remain pending in the ability matrix.


## Sneak attack contact policy

Original `005FF1FC–005FF2C9` excludes creature attackers and requires
`005E0550` sneaking movement flag 0400 without swimming flag 0800. It queries
the victim process for awareness of this attacker. Positive detection denies
the bonus. If the attacker is in the victim's combat-controller target list,
awareness must also be at most `iAICombatMinDetection` (-50, inclusive).
`00613670` traverses the controller's target list at +40; pinned GameProcess.h
independently identifies that member. HighProcess RTTI resolves vtable A71814,
slot +1C8 to `006285D0`: lookup detection data via +3B0, return signed +0C
level, or INT_MAX if absent. The pinned DetectionList::Data definition agrees.
Missing awareness must therefore deny, not become undetected by default.

`005477F0` chooses by base Sneak rank and weapon type. Unarmed (-1), one-handed
Blade (0), and one-handed Blunt (2) use melee multipliers. Bow (5) uses Marksman
multipliers. Two-handed weapons and staffs return 1. Compiled melee ranks are
4/6/6/6/6, Marksman 6/8/8/8/8; installed Marksman overrides are **2/3/3/3/3**.
Original-15 GMST-99–109 captures were opened/inspected and verify ten
multipliers plus the signed combat threshold. The actor's own modified Sneak
value does not replace base mastery rank.

At Master and **multiplier > 1**, `005FF2B6–005FF2C4` sets a bypass flag.
`005FF712–005FF71C` makes absorbed armor zero, and `005FF7D9–005FF7F1`
suppresses the victim's block query for this same flag. A two-handed attack
or a custom multiplier at/below one gets neither bypass. `sneakAttack`
returns multiplier and both bypass decisions; it does not emit fake damage
or detector results. Four tests fail against stubs, then cover all ranks,
weapon types, awareness/posture/target combinations, exact -50/0 and float-one
boundaries, every setting mapping and malformed input (`S2/stealth-rules-01`).
Block bypass was additionally traced while implementing and shares the same
proven predicate; it is separately asserted in the final tests.

**Open integration prerequisite:** the existing M14 `calculateDetection`
returns a normalized [0,100] score and probabilistic detected flag, which
cannot directly supply the signed original awareness required here. This must
be corrected within the existing detector with native source/oracle evidence,
regression tests and persistence migration; do not subtract an invented offset
or create a second detector to make these pure tests look integrated.
Sources: `sneak-attack-multipliers.txt`, `sneak-attack-caller.txt`,
`sneak-posture-query.txt`, `combat-target-list-query.txt`,
`sneak-victim-detection-query.txt` and contact continuation under sources-01.


## Signed native detection arithmetic and Sneak noise mastery

Original `005463F0–005465FB`, called at `005F68DB`, computes a signed integer
awareness score. Let R denote a float32 storage boundary:

- Maximum = exterior ? R(maxDistance * exteriorMultiplier) : maxDistance.
  Distance beyond maximum returns zero unless the target is attacking.
  Distance factor = R((maximum - distance) / maximum); attacking targets can
  therefore have a negative factor beyond range.
- Boots = moving ? R(bootWeight * bootMultiplier + bootBase) : 0.
  Sound = max(0, R((runningMultiplier * boots + targetCombatBonus) * distance
  factor * soundLOSFactor * soundMultiplier)), with running/combat factors
  enabled only for their corresponding target states. SoundLOSFactor is 1
  with LOS and the setting without LOS; absence of LOS is not a hearing gate.
- Light = max(0, R((targetLight + lightOffset) * (LOS ? distanceFactor : 0)
  * (100 - observerBlindness) / 100 * ((100 - targetChameleon) / 100)
  * lightMultiplier)). The input is integer native light amount, not RGB.
- Skill = R((min(observerAdjustedSneak,100) * distanceFactor
  - (targetSneaking ? min(targetAdjustedSneak,100) : 0)) * skillMultiplier).
  Caller adjusts both current skills by luck before truncating them to integer.
- Underwater **observer** sets sound to zero and stores light times the swim
  multiplier. Sleeping **observer** replaces light with sleepBonus. It does
  not simply add that bonus to ordinary light.
- Total = R(base + sound + light + (targetAttacking ? attackBonus : 0) + skill).
  Truncate toward zero, except strictly positive totals below one return 1.
  There is no [0,100] normalization or random roll in this helper.

Original caller `005F6695–005F6734` gets target boot weight, then zeroes it
while sneaking at Journeyman base Sneak. At Expert it also clears moving and
running inputs. Boot base noise remains at Journeyman; multiplying all footwear
noise by zero would be wrong. Sneaking query excludes swimming. Invisibility
or Chameleon >=100 is separately handled by the caller (score -100), as are
LOS, light sampling/night-eye, action noise, cached awareness and detection
state transitions. These do not become implemented merely from this scalar
subroutine. Source files `detection-rule.txt`, `detection-rule-caller.txt` and
`float-to-int-runtime.txt` retain the reviewed instructions.

`nativeDetectionAwareness` and `sneakDetectionNoise` are scalar subroutines in
the **existing** detection component, with typed settings from winning GMSTs.
The legacy normalized public adapter remains active until its caller, state
history and save schema are migrated together; the new helpers are not a
second actor-pair detector or gameplay acceptance. Current runtime-state
validation also enforces [0,100] and requires deliberate version migration.

Original-15 GMST-110–124 captures were opened and inspected: 15 native factors,
including installed differences from compiled defaults. A supplementary local
Unicorn oracle executes the actual original `005463F0` bytes and original
integer-conversion helper, with PE constants and audited installed settings.
It is independent of production arithmetic and generates expected test values,
but is **not original-game runtime acceptance**. Twenty named cases agree under
both x87 control words 027F and 037F; direct captures remain the live-setting
oracle. Emulator scripts, version, hashes and results are under
`S2/oracle-emulator`; no proprietary bytes are tracked. Four new tests fail
against stubs before implementation; existing four M14 characterization tests
remain intact. New cases cover signed results, light/sound separation, posture,
mastery boundaries, distance edges, integer conversion and invalid inputs.

The supplementary comparison also passes **1,024** cases: all 512 combinations
of nine boolean inputs and 512 seeded numeric vectors (seed 5051870), with
zero C++/original-instruction differences and zero differences between the
two original x87 precision controls. `detection-comparison.json` and the full
input/expected corpus are retained; no mismatches or retries were discarded.


## Pickpocket chance, amount and check boundaries

Original `00546660–005466F1` stores each of target skill * multiplier + base,
actor skill * multiplier + base, and amount * multiplier + base as float32.
It sums those three stored terms and truncates to integer **without a final
float32 sum store**, then compares the integer (converted to float) against
minimum and maximum. Fractional limits truncate when returned. Compiled inputs
are actor 0/1, target 0/-1, amount 0/-3, bounds 5/75; installed inputs are
actor **40/.6**, target **0/-.6**, amount **0/-.1**, bounds **5/85**.
Original-15 GMST-125–132 inspected captures confirm all eight settings.

The transfer caller `0059AF49–0059AFDC` obtains integer item value through
`004842E0 -> 00470520` (TESValueForm, or MagicItem value fallback), multiplies
by selected quantity B13E94 as integer, then stores float. It does not use
item weight in this amount. `pickpocketAmount` checks negative/unsupported
value and integer multiplication overflow before converting. Both actors'
current Sneak and Luck integer values feed `00547B90`, and adjusted results
truncate before the chance helper. Existing `effectiveCombatSkill` supplies
that luck arithmetic; raw base mastery skill is not the chance input.

`0059B007` succeeds on **draw < chance** for transfers. The untouched-menu
exit check at `00598465–00598490` supplies amount zero and succeeds on
**draw <= chance**. Both draw modulo 100. The session flag B13E90 starts at
one (`0059A23B`) and a pickpocket transfer path clears it (`0059A519`), gating
that exit check; do not always roll again on closing after a transfer. These
helpers only evaluate the arithmetic/comparison. Native session lifecycle,
prohibited items, reverse transfer, victim eligibility, actual inventory and
crime dispatch remain mandatory world/UI work, not accepted by a boolean.

Four new tests fail against stubs, then pass; include all 100 draws at chance
0/1/5/40/75/85/99/100, distinct exit/transfer boundaries, value/count safety,
every typed setting binding, fractional clamps and adjacent float truncation
cases. Fixed seed 5051852 has predeclared 100,000-sample bounds of 85,000 +/-600
transfer and 86,000 +/-600 exit successes at chance 85. Twelve independent
original-instruction examples include equal skill 50 yielding **39**, and
100/100 with amount 10 yielding **38** because stored .6 products do not cancel
as an ideal decimal formula would. Supplementary direct-instruction comparison
passes **1,024** cases (400 threshold combinations +624 numeric vectors,
seed 5051852), with zero C++ mismatch or x87 precision-control difference.
Emulation is not live original-game pickpocket acceptance. Sources under
`S2/sources-01/pickpocket-*`; arithmetic evidence `S2/pickpocket-rules-01` and
`S2/oracle-emulator/pickpocket-*`. Runtime permitted-item and complete session
semantics remain open.


## Crime base fines, faction scaling and sentence arithmetic

Original `00606140` maps native crime kinds 0–5 to theft, pickpocket, trespass,
assault, murder and horse theft. Theft takes item value (`00470520`) or the
incident's supplied fallback, substitutes **1 only when value is zero**, then
stores value * `fCrimeGoldSteal`. Other kinds convert their integer GMST to
float. This is a per-incident base amount, not final legality or bounty. It
preserves fractions: value-zero/one theft gives .5 with installed settings;
there is no whole-gold rounding here. Jail break is separate (`004B9199`),
converting its integer fine to float for the bounty mutation.

Compiled/installed fines are assault 40, murder 1000, theft multiplier .5,
pickpocket 25, trespass 5. Horse theft is compiled 25 / installed **250**;
jail break compiled 100 / installed **50**. Original-15 GMST-133–142 captures
are inspected. The last group also confirms prison divisor 100, responsibility
multiplier installed 2 (compiled 1.7) and attack-minimum bounty installed 500
(compiled 1000); the latter two are provenance, not completed witness/guard
policies.

`005234A0` starts a faction multiplier at **1** and takes the **largest** value
from faction memberships (`0051F0A0` returns TESFaction +38, independently
identified as crimeGoldMultiplier in pinned GameForms.h). `0060F49B–0060F4C6`
stores base fine times that result before mutation. Direct emulation of the
actual list walk independently confirms six cases including sub-one/zero
factors leaving 1 and [2,3] choosing 3. An initial comparison interpretation
was corrected against that independent oracle before implementing the helper.
Faction membership/record resolution and which reporting branch applies the
factor remain caller responsibilities; the helper does not invent per-hold
bounty or a faction payment discount.

Sentence entry `00670239–00670265` divides float bounty by integer
`iCrimeDaysInPrisonMod`, truncates to integer, and `0067029C–006702AA`
raises it to at least one. On service `00670719–0067073F` schedules **days*24
hours** and adds days to the jail statistic before `006707E8–006707F5` caps
skill-change attempts at **10**. That cap must not cap elapsed days. Our helper
checks the original int32 hours range instead of allowing integer overflow.
Prison destinations, admission, time mutation, skill selection, confiscation,
release and escape remain native lifecycle work.

Four tests cover offense defaults/overrides, zero/fractional theft, highest
faction factor with adjacent float-one boundaries, sentence boundaries,
long sentences with ten skill attempts, all typed settings and invalid/overflow
inputs. Three behavior tests fail against stubs (the separate implemented
setting-binding test already passes), then all pass. Ten independent
original-instruction fine examples and six faction-list examples are retained
under `S2/oracle-emulator`; they supplement live settings, not gameplay.
Sources: `crime-fine-rule.txt`, `crime-fine-caller.txt`,
`faction-crime-multiplier.txt`, `jailbreak-fine.txt`, `jail-sentence-rule.txt`,
`jail-sentence-continuation.txt` and `jail-serve-rule.txt` under sources-01.

Open jail-selection finding: `0067080A–00670847` initially draws modulo 21,
then while the candidate is below 12 adds further draws modulo 10. Its reachable
actor-value candidates are 12–20, despite later Security/Sneak (30/31) increment
branches. Do not replace this with uniform selection of 21 skills or claim the
unreachable branches occur naturally. Required original gameplay observations
and the narrow skill mutation/state/RNG implementation remain open.

## TES4 faction data

`loadfact` now exposes DATA's one-byte Hidden/Evil/Special Combat bits,
CNAM's optional finite nonnegative float, and ordered XNAM pairs of resolved
faction FormKey plus signed int32 reaction. Unknown bits, wrong lengths and
duplicate singleton fields diagnose; rank strings/indices and unknown fields
remain in the lossless logical payload. Only TES4 0.8/1.0 layouts are typed;
later games retain the existing raw fallback. Deleted records need not carry
DATA. Winning stores and relationship resolution use the actual master order,
including overrides and empty tombstones.

The pinned xEdit FACT definition and xOBSE TESFaction declaration agree on
these fields and flags. The installed census and independent `esmtool` typed
read agree on all **495 winning factions** across eleven hashed official
plugins, including signed relationship lists and **129 absent CNAM fields**
(`S2/faction-data-01/installed-comparison.json`). The parser retains absence.
For runtime resolution, the original TESFaction constructor's `fld1` at
`0051F876`, vtable `00A53524`, and store to `+38` at `0051F894` establish the
missing CNAM multiplier **1.0**. RTTI descriptor `00B05374` independently
identifies that vtable as TESFaction. CNAM loading at `0051F99C–0051F9AD`
writes the same `+38`; getter `0051F0A0` reads it. Original executable hash
remains the locked 1.2.0416 hash. Trace `S2/sources-01/faction-native-record.txt`.
DATA is structurally required by the reviewed format; the constructor default
is not permission to silently accept malformed nondeleted records.

Three parser tests fail against the stub, then pass, exercising compressed
records, lossless unknown/rank payloads, signed reactions, all invalid flag
values, malformed lengths, nonfinite/negative multipliers, duplicates,
truncation and version rejection. Full component/sanitizer and store results
are recorded with the chunk report. Typed data availability does not establish
crime reporting, faction combat legality or gameplay acceptance.

## Jail skill selection and penalty arithmetic

`advanceJailSkillSelection` consumes exactly one supplied original-domain
15-bit draw. A null candidate starts with draw modulo 21. While the candidate
is below 12, each subsequent draw adds its remainder modulo 10; zero leaves
it pending. Values 12–20 complete selection. The pure step rejects already
completed candidates and out-of-domain draws instead of consuming extra RNG.
The controller must persist both pending selection and the authoritative RNG;
that integration remains open. There is no production retry cap, uniform
21-skill replacement, or synthesized Security/Sneak increase.

Original `0047DF80` delegates to `009859DD`: the latter updates a 32-bit state
by `state * 0x343FD + 0x269EC3`, shifts by 16 and masks with `0x7FFF`.
This establishes the draw domain, not a second simulation RNG. Nine retained
instruction-oracle cases execute `00670808–00670849` with supplied draws,
confirming results and the exact consumed prefix (`jail-selection-table.json`).
An independent exact absorbing-chain calculation includes modulo residue bias
and zero redraws. Under independent uniform 15-bit draws its expected counts
per 100,000 for actor values 12..20 are **14903, 14177, 13370, 12473, 11477,
10370, 9141, 7775, 6316**. The fixed-seed test (`0x4D15A1`, predeclared
600-count tolerance per bin) checks that distribution; it does not assert that
the original global LCG stream consists of independent samples.

`jailSkillBaseAfterPenalty` covers the reachable decrement branch only.
`00670860–0067087A` queries the modified/current skill, truncates to int32 and
skips mutation if the integer is at most one. Otherwise `006708AE–006708CD`
gets the base skill, subtracts one and invokes its setter. Getter `005F1910`
floors the base value from `005EAD00`; skills come from the base form, without
the special health/magicka/fatigue/encumbrance treatment. TESNPC vtable
`00A53DD4` (RTTI `00B02FB4`) slot `+134` is `00523310`; its skill branch stores
the low byte at `00523338`, then marks change mask `0x200`. Thus the minimum
check is **not a clamp on the base**: base one/current two becomes zero, and
base zero/current two stores 255. Finite/int32-domain input validation avoids
undefined host conversions. Notification, derived-stat recalculation and
persistent base-state changes remain controller work.

Nine supplementary instruction-oracle penalty cases execute the actual
current-value check/conversion and TESNPC byte setter, with synthetic actor
queries and identity/change-notification adapters. The first harness attempt
had the wrong incoming register and failed with an unmapped read; its log is
retained. Corrected EAX initialization produces the independently expected
threshold and byte-wrap outcomes. This is not buffed-actor gameplay acceptance.
Artifacts: `S2/oracle-emulator/jail-*`, source traces `random-integer-rule.txt`,
`jail-base-value-getter.txt`, `npc-base-value-setter.txt` and
`npc-skill-setter-vtables.json`. Raw executable data stays outside Git.

## Responsibility and disposition alarm check

`responsibilityAllowsAlarm` is the narrow `00546700` predicate:
`double(responsibility) * fCrimeAlarmRespMult > disposition`. Equality is
false; there is **no RNG call and no float store** before comparison. The
compiled multiplier is 1.7f; installed is 2.0 (already captured in Original-15
GMST-141). The typed factory validates finite, nonnegative factors; integer
inputs remain signed and unclamped. Other caller eligibility can still prevent
an alarm. This helper alone never creates a witness, report or bounty.

The first argument is now identified rather than guessed. Crime caller
`0060FF15–0060FF2A` queries the candidate witness's actor virtual `+224` toward
the offender, then supplies that result and the witness to `00605E20`.
`00605E20` queries integer Responsibility (actor value 36) on its actor argument
and invokes the predicate. Actor vtable `00A73A0C` slot `+224` is `005EA800`,
the disposition query also identified by pinned xOBSE's Actor virtual slot 89.
The AI detection path `0064127E–006412B4` similarly stores this disposition
query, subsequently supplied to the same predicate at `006417FC`. Actor
identity selection in that larger AI path is not new witness policy here.

Fifteen supplied-input original-instruction oracle cases execute `00546700`
under the original 53-bit control setting. They independently confirm strict
thresholds, neighboring float multipliers, signed inputs and the important
compiled-default case: responsibility 100 against disposition 170 is **true**
for 1.7f's actual stored binary value. Prematurely storing the product as float
would incorrectly make it false. No stochastic test is invented for this
nonrandom check. Source traces and call-site list are retained under
`S2/sources-01/crime-*`, with `actor-disposition-rule.txt`; supplementary
oracle artifacts are `S2/oracle-emulator/alarm-responsibility-*`.

## Native melee reach

Original `00547540` multiplies weapon reach by `fCombatDistance` and stores
float32. NPC natural reach (`005E40A0`) supplies `fHandReachMult` to that
helper. The contact caller `005FEF87–005FF017` chooses the equipped weapon's
reach (`TESObjectWEAP+98`) or actor virtual +26C, stores the base reach, then
multiplies by virtual +EC (`GetScale`) and stores float32 again. NPC `00611D00`
includes the sex-specific race height in that scale; the helper accepts the
resolved actor scale, not just the placed-reference scale. Thus scale
must follow base rounding. Character/Player +26C resolves to `005E40A0`;
Creature resolves to `00625220` (local `sources-01/reach-vtables.json`).

Creature natural reach is the unsigned RNAM byte (`TESCreature+10A`), already
a distance. Type 5 (Giant, independently identified by the pinned xOBSE enum)
multiplies it by `fCombatGiantCreatureReachMult` before the base float store.
Other types preserve RNAM unchanged. A missing creature base uses the NPC
natural-reach helper; resolving such a base remains the caller's responsibility.
No weapon/creature distance is silently substituted for invalid native data.

Compiled settings are distance 128, hand multiplier .5, giant multiplier 2.
Installed overrides are .6 and 2.2 respectively; original read-only GMST
probes 143–145 confirm the displayed values. Fourteen actual-instruction
cases in `S2/oracle-emulator/reach-table.json` confirm independent expectations
for weapon/unarmed/creature reach, adjacent floats, zero and scale. The first
emulator harness incorrectly popped ST0 before entering the caller and returned
zeros; its source/log are retained separately, and it supplies no acceptance
values. This is arithmetic evidence, not contact geometry or animation timing
acceptance. `fObjectHitWeaponReach` belongs to a separate object-hit path and
is deliberately not substituted for actor melee reach.

## Original jail course repeat

Original-16 case 02 was declared before a fresh normal F9 load. Confirmed
normal assault contact changed guard health 127→125.71 and bounty 0→40.
Normal arrest choice and jail bed activation produced release, bounty 0,
Hand to Hand penalty message and base value 9 (pristine value 10). The paused
clock observations are day 1/hour 6.6691 and day 2/hour 6.6693: **24.0002 hours**,
within the predeclared ±.01-hour UI-transition budget. Case 01's stricter
±.0002-hour timing assertion still fails at 24.0003 and is not relabeled.
All observations, failed activation attempts and input logs remain under
`S2/original-16`; screenshots were inspected. Lost X input focus explains the
last ignored activation inputs; normal Space worked after window focus was
restored. No inventory-count/confiscation acceptance is claimed. Normal `qqq`
was requested, the game process exited and pristine quicksave hash is unchanged,
but the wrapper returned 143, retained as a failed whole-process exit gate.
These reference results do not close native OpenMW jail gameplay acceptance.

## Contact-facing cone arithmetic

The block branch at `005FF83E` calls `006131D0` with the victim and physical
source reference. That function reads their positions and obtains horizontal
bearing from `00683CB0`, then reads victim Z rotation through virtual +1E0.
`combatHitCone` accepts these already computed float angles. Its reviewed
arithmetic is `R(abs(R(facing − bearing)) × 57.2957763671875)` degrees. The
conversion constant is the original double at `00A30DC8`, not recomputed
180/pi. If degrees >180, replace it with `abs(R(degrees − 360))` exactly once.
Return true only when `fCombatHitConeAngle > degrees`; equality fails. Preserve
out-of-normal-range results rather than adding an invented modulo operation.
Nonfinite inputs and float overflow diagnose explicitly.

The compiled cone setting is 20 degrees (`00B36F28`, initializer `009E92AF`);
installed master override `0287A3` is 35 degrees. Original-17 live read-only
probe GMST-146 confirms 35.00 after normal Continue into the pristine prison. Fourteen supplied-
angle original-instruction cases independently establish strict boundaries,
adjacent floats, the wrap and conversion rounding (`S2/oracle-emulator/hit-cone*`).
This is a geometric predicate, not a chance-based block or timing window.
The preceding victim query `005E5670` requires native process action 6;
controller/animation timing and the complete posture/eligibility chain remain
integration work. Block data and facing arithmetic alone do not prove a normal
block works. Bearing construction and overlap/vertical contact remain caller
geometry work; no duplicate detection service is introduced.

## Ownership claim predicate

Original reference helper `004DE770` first resolves owner (`004DB6B0`), required
rank (`004DB830`) and permission global (`004DB7D0`), then compares the actor's
base identity. `hasOwnershipClaim` covers its supplied-input predicate for a
valid resolved actor base. A matching actor owner succeeds. A different actor
owner requires a present, nonzero global; negative and positive subnormal values
are nonzero, not coerced through integer conversion. Unowned property returns
false from the claim helper; this does **not** mean taking unowned property is
criminal. Nonfinite globals diagnose invalid state in the native adapter.

Faction ownership always requires actor rank >= required rank. With the native
caller's faction-ownership mode enabled, the global is ignored. With that mode
disabled, a nonzero global is additionally required. Signed ranks are preserved:
a required rank -2 is satisfied by absent-membership rank -1. The original rank
resolver defaults missing reference/linked-reference/cell rank to zero, but
resolving that hierarchy belongs to the world adapter. It must not replace
explicit negative ranks with zero.

Eighteen independent executions of the original helper with only identity,
record-access and rank-query boundary stubs confirm these branches
(`S2/oracle-emulator/ownership-claim*`). Reference/teleport/cell inheritance,
player extra faction ranks, evil-owner exemptions, public-cell access, trespass
and witness reporting are **not** implemented by this claim predicate. The
outer `004DEBF0` off-limits query applies evil-owner and object/door checks
separately. This helper neither changes inventory ownership nor creates a new
inventory or crime authority.

## Resolved combat-style audit projection

The semantic audit now applies the reviewed DefaultCombatStyle getter mapping
to typed winning GMSTs and the checked-in initializer catalog. It resolves all
historical authored tails and documented zero sentinels without mutating the
raw decoded records. Advanced CSAD applies only with the Advanced flag;
missing required CSAD, incomplete defaults, bad types, ambiguous setting names,
nonfinite values and invalid domains fail explicitly. The catalog hash is
recorded alongside this projection; it is not a gameplay acceptance flag.

`S2/audit-12` resolves **130 policies** (native default plus 129 authored styles)
and **3,636 actor-to-policy links**. The 2,218 actors with no ZNAM are now
explained as native default consumers; the reviewed `default_actors` count
still counts them and does not change to zero. An independently compiled
runner calls the actual C++ setting builder, binary CSTD/CSAD decoders and
policy resolvers. All **7,540 fields** agree with the Python grouped-layout
projection (`S2/policy-audit-01/comparison.json`), using source payloads whose
per-style hashes match the fresh audit. This is data/rule agreement, not an
executed opponent policy or magic effect. Equipment, spell, faction and actor
flags remain separately preserved inputs; selected campaign behavior still
requires its complete policy explanation and gameplay gates.

## Sustained incapacitation requirement

Original high-process update `006545E0`, reached through process virtual
`+2F4` at actor update `00603E2B`, tests current fatigue at `00654642–00654656`.
Helper `005E0A60` reads modified actor value `0A` (Fatigue). For finite inputs,
the entry predicate is **fatigue < 0**, or nonzero paralysis, or actor state 6
(essential unconscious). Exactly zero, including negative zero, does not
require a new fatigue knockout. Recovery branch `00654886–006548B4` requires
fatigue >= 0, no paralysis, and actor state other than 6 before looking up
and starting a recovery animation. These are entry/recovery eligibility checks,
not proof that the physics body has settled or recovery animation has finished.

The paralysis virtual `+1A0` is `005E17E0`: integer actor value `30` is tested
against zero. An imported header's `HasFatigue` name for that vtable slot is
misleading; the implementation and actor-value table identify Paralysis.
`requiresIncapacitation` takes the resolved predicate, not an active-effect
container. Its essential input means already unconscious, not the essential
base flag. Random impact knockdown is a separate cause and is not erased by a
false result here. Nonfinite fatigue is rejected at the native rule boundary.

Original entry and recovery instruction slices independently pass **96 cases**:
negative/positive extremes, ±1, the floats adjacent to zero, both signed zeros,
paralysis values -1/0/1, essential state 6, and nonessential state 5 controls.
Only actor-value reads are boundary stubs; original fatigue/paralysis/state
queries and branch instructions execute unchanged. Evidence:
`S2/oracle-emulator/incapacitation.py`, `incapacitation-table.json`,
`incapacitation.log`; source traces `current-fatigue-query.txt`,
`has-fatigue-query.txt`, `process-knocked-update.txt` and
`fatigue-state-update.txt` under `S2/sources-01`.

Two new component tests fail against an always-false stub, then exercise the
reviewed branch, independent causes and nonfinite rejection. Native animation,
ragdoll, mounted interruption and live recovery acceptance remain controller
work; this helper does not mutate actor state.

## Block contact eligibility and zero-absorption reactions

Original contact branch `005FF7C9–005FF7F5` suppresses active blocking for
paralysis or the previously resolved Master sneak bypass. Otherwise
`005E5670` requires process action **6**. `005FF836–005FF848` then requires
the reviewed strict facing cone. The actor controller supplies active posture;
this rule does not invent a timed-parry window or permit blocking from a
merely held input while another action is active.

Process equipment queries `+F8` and `+EC` distinguish shield, weapon and
unarmed. At `005FF87E–005FF892`, an unarmed defender facing an attack with a
weapon record (`stack+2C`) or projectile reference (`EBP`) gets **zero absorbed
fraction**, while the block-reaction branch continues. Otherwise the existing
`005474A0` block-fraction helper evaluates skill/luck/fatigue/equipment. Thus
an active unarmed block against a natural/unarmed attack can absorb damage,
but the same posture against weapons/arrows must not gain that fraction.
A zero fraction is not permission to skip all subsequent native reactions.

`blockContactDisposition` returns None, ReactionOnly or Absorb, preserving this
distinction without mutating damage or condition. The existing `blockFraction`
remains the arithmetic rule for Absorb. The caller must use zero for
ReactionOnly and still apply the appropriate reaction logic. Shield/weapon
selection comes from the equipped instance, not the nominal inventory contents.

Evidence: `S2/sources-01/block-contact-eligibility.txt`,
`block-contact-inputs.txt`, `contact-argument-setup.txt`, `block-state-query.txt`
and the previously reviewed sneak-bypass caller. **160 original instruction
cases** independently verify active posture/paralysis/bypass and the zero
absorption branch with both attack source kinds and their combinations
(`S2/oracle-emulator/block-contact.py`, `block-contact-table.json`,
`block-contact.log`). Only actor-value and process-action reads are boundary
stubs. Facing-cone arithmetic has its separately reviewed original cases.
Two new component tests first fail against the None stub, then cover all gate
failures, all equipment classes, weapon/projectile combinations and invalid
equipment. World posture, equipped-instance selection, reaction ordering and
normal-input blocking remain native runtime acceptance work.

## Independent ownership-field inheritance

Original owner query `004DB6B0` first reads reference ExtraOwnership. Actors
(virtual `+190` true) return that owner directly, including null. Other
references with no owner try the teleport destination reference's *direct*
ExtraOwnership (`ExtraTeleport` type `32`, destination getter `0042B410`).
There is no recursive traversal of the destination's owner hierarchy.
If still null, furniture type `20`, door `18` and activator `12` stop;
other reference types try the **current** cell owner at `004CA970`.
The destination cell's access restrictions are evaluated separately by the
door off-limits path; they are not an owner fallback here.

Required rank `004DB830` and permission global `004DB7D0` each independently
try reference, teleport destination reference, then current cell, including
for actor/furniture/door/activator references. Rank **-1** means inherit even
when explicitly authored; all other signed int32 values are retained. An
unresolved final rank becomes 0. Cell rank `004CA990` applies the same -1 to 0
conversion. Global inheritance selects the first nonnull global *identity*;
a resolved global with value zero does not cause fallback to another global.

`resolveOwnership` consumes three already resolved layers and the reference
kind, and returns one read-only owner/rank/global projection. Winning record,
valid FormKey/type and teleport reference resolution remain caller duties.
Null/missing layers and explicit null owner/global fields permit fallback;
this rule does not create another inventory/ownership authority. Invalid
reference-kind enums diagnose. Ownership permission and crime legality remain
separate from these field queries.

Evidence: `S2/sources-01/owner-inheritance-rule.txt`,
`ownership-rank-global.txt`, `cell-ownership-rules.txt`; **35 independent
original instruction cases** in `S2/oracle-emulator/ownership-inheritance*`.
Each case executes original owner, rank and global helpers. Only extra-data
reads and actor/base-kind virtual boundary queries are stubbed; inheritance,
teleport dereference, cell queries and signed sentinel branches run unchanged.
Three component tests first fail against the empty-result stub, then cover
field precedence, type exceptions, empty inputs and signed rank extremes.
World inheritance, door/public access and normal crime actions are still
integration gates.

## Native cell trespass classification

Actor virtual `+354` (`0060E320`) queries its current cell through
`004CABC0`. That cell helper first exempts NPCs whose class has the guard
flag (`005E6C60` -> `0051BEF0`, class flags bit 1). No owner, cell Public
(`20`) or HandChanged (`40`), a nonnull permission-global **record**, or a
non-NPC actor also returns false. The global's numeric value is never read
in this helper. This differs from `hasOwnershipClaim`, and callers must not
collapse the two policies.

For the remaining NPCs, an NPC-owned cell is trespassing unless the owner's
base matches the actor's base. A faction-owned cell is trespassing when the
actor's signed faction rank is **less than** its required rank; raw cell rank
-1 becomes 0. No interior-bit, evil-owner, global-value, witness, alarm or bounty
check occurs in this helper. Its caller may apply additional contextual rules;
a true result alone must not create a reported offense.

`cellTreatsActorAsTrespasser` implements this classification over immutable
resolved inputs. Guard classification is the class flag, not aggression,
responsibility or a guessed faction name. Invalid owner kinds and inconsistent
identity/guard inputs diagnose before exemptions. This does not change native
world state or enable trespass gameplay by itself.

Independent original-helper execution passes **272 cases**, including all 256
cell-flag bytes, each exemption and signed rank boundaries. Record/identity/
class/rank reads are boundary stubs; original classification, mask, sentinel
and comparison instructions execute unchanged. Evidence:
`S2/oracle-emulator/cell-trespass.py`, `cell-trespass-table.json`,
`cell-trespass.log`, and `S2/sources-01/cell-access-rule.txt`.
Three new component tests fail against the false stub, then exercise the
reviewed policy. Trespass warning/report timing, witnesses, door activation and
native save/restart remain separate gates.

## Player trespass door-exit exemption

Within native door permission helper `004B72C0`, the player branch
`004B73F9–004B743C` queries current-cell trespass (actor virtual `+354`).
When true, it requires both teleport data and lock data. Lock level **exactly
100** rejects this exemption; there is no >=100 comparison. Otherwise it
permits a missing destination cell, an exterior destination, or an interior
with the Public bit (`20`). Original flag helpers are `004C97F0` (Interior)
and `004C9830` (Public). HandChanged (`40`) alone does not satisfy this
particular destination test, unlike cell trespass classification.

`playerHasTrespassExitExemption` implements only this player branch. A false
result is not a final denial: ownership, guard and follower permissions have
other branches. A true result neither unlocks nor activates a door nor commits
a crime. A missing destination cell represents the native query returning
null; failed stable-reference resolution must still diagnose at the world
boundary rather than masquerading as that input.

Two tests fail against the false stub, then cover each required input, exact
lock-level boundaries and destination masks. **266 independent original
instruction cases** cover all destination flag bytes, levels 0/1/99/100/101/255,
missing data and current trespass. Only the current-cell trespass query is a
boundary stub; the native exit branch and cell flag helpers execute unchanged.
Evidence: `S2/oracle-emulator/door-exit.py`, `door-exit-table.json`,
`door-exit.log`, `S2/sources-01/door-access-exemption.txt` and
`door-access-exemption-continuation.txt`. Normal door activation and arrest/
trespass event ordering remain runtime acceptance work.

## Player reference off-limits query

Original `004DEBF0` first exempts an evil resolved owner: faction flag `02`,
or the corresponding evil-faction query for an NPC owner. This is the
reference's **owner**, not automatically the target NPC's own faction.

For doors, an owned/unclaimed door first consults native door permission.
If permitted it is not off limits; otherwise a locked door is off limits.
An unlocked door in that branch is off limits when its destination has a
non-evil owner, is interior and lacks both Public and HandChanged. A claim
on that destination does not override this branch's unclaimed door owner.
When the door is unowned or claimed, the same restricted-destination test
additionally requires **no claim on the destination cell**. Door and cell
claims are distinct queries; the latter uses `004CAAC0`, which compares NPC
base identity or faction rank without the reference permission global.

For other targets, sneaking at a living NPC is off limits before checking a
reference claim. Otherwise an owned/unclaimed non-actor object or horse is off
limits, but other actors are not classified as theft targets by that branch.
Native horse query `004D74D0` checks creature type 4. Dead NPC sneak activation
is therefore not the living-NPC pickpocket branch. This query is not the full
legality/witness/report path and does not itself create bounty.

`playerReferenceIsOffLimits` preserves these branches over resolved access
inputs. Native door permission remains an explicit input, including the
reviewed trespass exit exemption and separate ownership/guard/follower rights.
It is not replaced with a guessed key or lock test. Invalid kinds and claims
without an owner diagnose. No inventory, crime ledger or UI state is mutated.

Five tests fail against the false stub, then cover object/actor/horse,
living/dead sneak, owner exemptions, independent door/cell claims, lock and
permission controls, destination flags and invalid inputs. **291 independent
original-helper cases** include all 256 destination flag bytes and both
faction/NPC evil-owner branches. Record, claim, door permission and actor-state
queries are boundary stubs; original off-limits decisions, base type checks,
cell flag helpers and horse query execute unchanged. Evidence:
`S2/oracle-emulator/reference-access.py`, `reference-access-table.json`,
`reference-access.log`, `S2/sources-01/ownership-player-query.txt` and
`ownership-player-continuation.txt`. World resolution, interaction and crime
reporting remain native runtime gates.

## Pickpocket item eligibility

Normal live-NPC pickpocket inventory filtering uses `004854F0` from
`005993DB`. ARMO/CLOT biped non-playable flag `40` hides the entry. For the
victim's inventory, the first extra-data instance's ExtraBoundArmor marker
(`0041DF50`) and either worn marker (`00484E80(0)`) hide it. A quest-item flag
alone does not hide a victim's item. Player inventory visibility and corpse/
container transfers have different paths and must not reuse this predicate
without their context.

Placing first rejects quest items (`0059A549`), then the player's drawn
weapon when the selected entry has a worn marker and its base matches the
process's equipped weapon (`0059A589–0059A5F9`). Common transfer handling
rejects the first extra instance's bound marker (`0059A601`). The live-NPC
reverse-pickpocket branch rejects strictly positive base weight
(`0059AC42`); both signed zeros pass. Negative/nonfinite weights are outside
our supported data domain and diagnose, rather than exploiting the original
raw floating comparison. Placing skips the taking success roll.

`pickpocketItemDecision` represents these item restrictions over resolved
inputs. It does not transfer inventory or consume RNG. The drawn-weapon input
means the complete conjunction above, not merely that the player has drawn
some weapon. Native transfer click handling clears the untouched-menu flag
`B13E90` at `0059A519`, **before** item retrieval or rejection; session handling
must preserve that ordering. The M13 term "unbound" for script binding does
not represent ExtraBoundArmor and must not be substituted for it.

Three component tests fail against an always-allowed stub, then cover each
restriction, taking/placing asymmetry, signed zero, positive subnormal and
invalid inputs. **73 independent original instruction cases** exercise
visibility, quest, bound, weight and drawn-weapon guards. Record virtual reads
are boundary stubs; biped flags, extra-data queries, worn-list traversal and
policy branches execute unchanged. Evidence:
`S2/oracle-emulator/pickpocket-items.py`, `pickpocket-items-table.json`,
`pickpocket-items.log`; `S2/sources-01/pickpocket-transfer-full.txt`,
`pickpocket-transfer-prologue.txt`, `container-list-update.txt`,
`inventory-display-filter.txt`, `inventory-equipped-filter.txt` and
`item-biped-query.txt`. Session persistence and ordinary UI transfers remain
runtime gates.

## Pickpocket check scheduling and knocked targets

`planPickpocketCheck` applies inside a live-NPC pickpocket session, after item
eligibility for transfers. Taking always schedules the transfer roll;
placing schedules none (`0059AF2B–0059AF49`). Untouched exit schedules the
zero-amount exit roll only while the target is not knocked
(`005983A0–005983CB`). A transfer click has already cleared the untouched flag
before item eligibility, including rejected clicks (`0059A50E–0059A520`).

Taking against a knocked target still consumes its roll. The failed-roll path
checks the target's knocked predicate at `0059B072`; a true result skips the
caught branch and proceeds with transfer. It does **not** turn the failed roll
into a success for the preceding successful-pickpocket statistics. The helper
therefore returns roll type and failure detectability separately. Target
virtual `+19C` is `005E04F0`, which reads process virtual `+2E4` and tests
nonzero; absent process returns false. It is not a base essential flag,
paralysis check or unconditional success probability.

Two component tests first fail against an empty plan, then cover all operation/
untouched/knocked combinations, invalid operations, and the previously verified
strict taking versus inclusive exit boundary. **20 original instruction cases**
independently verify exit/transfer scheduling, failure detection suppression and
transfer-click flag clearing. The real actor knocked query executes, with only
its process value as a boundary stub. Evidence:
`S2/oracle-emulator/pickpocket-session.py`, `pickpocket-session-table.json`,
`pickpocket-session.log`, and `S2/sources-01/pickpocket-knocked-query.txt`
alongside the preceding transfer/menu/session traces. Runtime session ownership,
UI closure, skill events, inventory transfer and crime dispatch remain pending.

## Actor faction crime predicates

Native actor-base query `00467560` returns evil only for a nonempty faction
list whose every populated entry has FACT flag `02`. One non-evil faction
clears the result. Native `004675A0` returns special-combat status when any
entry has FACT flag `04`. Neither reads membership rank or relationship
modifiers; Hidden and unrelated flag bits do not affect either predicate.

`actorFactionCrimePolicy` implements these distinct reductions over resolved
faction flags. Missing/deleted faction identities must diagnose during world
resolution, not be silently removed before this input is built. Reference
owner exemptions use the owner's policy, not the target's guessed alignment.
The assault/murder paths separately query special-combat status on both actors
(`00610A1C`, `00610F9C`); their exemption does not require a shared faction.
Complete incident legality and reporting remain separate work.

Two component tests first fail against an empty policy, then cover mixed and
ordered factions, all flag bytes and composed owner-access consequences.
**1,284 independent original cases** execute both whole native helper bodies
without call stubs: empty lists, every single flag byte, all pairs of the eight
native flag combinations, and each with ranks -128/-1/0/127. Evidence:
`S2/oracle-emulator/faction-policy.py`, `faction-policy-table.json`,
`faction-policy.log`, `S2/sources-01/crime-faction-flag-queries.txt` and
`crime-actor-offenses.txt`. This establishes the predicates, not live assault,
Arena legality or crime reporting acceptance.

## NPC attack-crime alarm entry

`00610930` (assault) and `00610EB0` (murder) are entered on the NPC victim,
with the offender argument. Do not use the external header's misleading
OnAlarmAttack argument description. Native incident constructor `006070B0`
stores the victim in `+08` and offender in `+0C`; assault increments the player
Assaults statistic (`+6D4`) only when the incident's `+0C` is the player.
Higher-level hit/self-defense selection occurs before these methods.

`attackCrimeAlarmEligible` preserves their common and differing entry gates:

- The victim must have a playable race, or be a class-flag guard.
- A player offender with positive jail days is exempt when native player
  combat/pursuer query `006605A0(0)` is false. Days are `Player+608`, assigned
  by jail sentencing and cleared on release; `0065DA50` tests strictly >0.
  The query is supplied after native list pruning, not guessed from bounty.
- Assault rejects a **trespassing victim** (`006109D8`); murder rejects a
  **trespassing offender** (`00610F59`). Preserve this actor distinction.
- The offender must be an NPC with a playable race, and must not be a guard.
- If both actors have special-combat status, no incident is created by these
  paths. They need not share a faction.
- A non-player offender with current integer Sneak **exactly 100** and native
  sneaking posture is exempt. 99 and 101 are not 100. The player is excluded
  from this exemption. Posture query `005E0550` requires movement bit `400`
  and absence of swimming bit `800`.

The race query `005E32F0` resolves NPC base `+E8` and reads race `+70` bit 0;
it is not an actor crime-enable flag. This helper is scoped to these NPC-victim
alarm entries, not creature harm, full legal combat classification, witness
eligibility or bounty commitment. Invalid offense kinds diagnose before any
exemption. Runtime actor capture must resolve the explicit inputs correctly.

Three component tests first fail against a false stub, then cover victim/
offender asymmetries, race/guard/special-combat combinations, jail and exact
skill boundaries, player exclusions and invalid offenses. **8,752 independent
original instruction cases** cover every combination of twelve boolean inputs
for both paths plus signed integer boundaries. Record/race/guard/faction/AV/
pursuer queries are boundary stubs; original entry branches, jail query and
sneak-posture query execute unchanged. The first harness attempt omitted the
offender pointer in EAX before `00610995` and faulted; that setup failure is
retained in `attack-alarm-harness-failed.log`, not counted as evidence.

Evidence: `S2/oracle-emulator/attack-alarm.py`, `attack-alarm-table.json`,
`attack-alarm.log`, and `S2/sources-01/crime-actor-offenses.txt`,
`crime-incident-constructor.txt`, `crime-player-context-a.txt`,
`crime-player-context-b.txt`, `crime-sneaking-query.txt`,
`crime-actor-base-query.txt` (the race query), and existing jail-sentence traces.
Native attack/death callbacks, witnesses, reporting, Arena and jail gameplay
remain integration and runtime acceptance gates.

## Trespass warning decisions and timer

Native package installation `00641E09` sets warning threshold 1, or 0 when
cell helper `004CA690` reads the record-header Off Limits flag `00020000`.
This is not the CELL DATA Public/HandChanged mask. Trespass-package constructor
`0067D3A0` stores that threshold at `+50`, initializes warning count `+40` and
timer `+3C` to zero, and sets incident id `+4C` to -1. Warning/count callbacks
are separate from the update decision: `0062A2BB` adds one frame duration to
the timer and increments count through `0067D330`; another dialogue path at
`0062FFF6` adds zero to the count and adds a small timer increment. Do not
invent warning completion events from an elapsed-time estimate.

For an existing package with a resolved actor target, `0064D1D1` first leaves
when the actor no longer trespasses. Threshold 0 escalates immediately.
Otherwise, count **greater than 1** and a nonpositive timer escalates. At a
nonpositive timer with remaining warnings, it requests the warning procedure.
A positive timer advances by `R(timer + R(2 * frameDuration))`. It resets to
zero only when the stored result is **strictly greater than**
`fAITrespassWarningTimer`; equality keeps waiting. The update remains in its
wait/continuation branch even when it resets the timer: warning/escalation
happens on the following update, not in that same tick.

The executable initializes this GMST to 10 (`009E7C0F`, storage `00B36B30`);
installed Oblivion.esm `005682` overrides it to 30. The typed settings adapter
preserves both. No live-setting read is claimed for this new row. Frame duration
is the native `B33E9C` input; this alone does not establish wall-clock delay or
scheduler cadence. `advanceTrespassWarning` implements the decision and timer,
not package admission, dialogue playback, count callbacks, guard reporting or
non-guard combat dispatch. Supported persistent inputs are nonnegative finite
timers/durations/counts; arithmetic overflow diagnoses before storing invalid
state. Leaving or immediate escalation does not perform timer arithmetic.

Three policy tests first fail against the waiting stub. A fourth settings test
covers the default, winning override, zero and malformed values. **1,920 original
instruction cases** cover leave/off-limits/count/timer branches, signed zero,
subnormal timers, limits 0/10/30 and adjacent floats. Only target trespass is a
boundary stub; original branches and float instructions execute unchanged.
The first harness reused translated stub code incorrectly; its failure is
retained. The corrected harness then disproved an initial >= expiry expectation
at timer 10/limit 10; implementation/tests now require strict >, and that failed
expectation is also retained rather than hidden.

Evidence: `S2/oracle-emulator/trespass-warning.py`,
`trespass-warning-table.json`, `trespass-warning.log`,
`trespass-warning-harness-failed.log`, `trespass-warning-boundary-failed.log`;
`S2/sources-01/trespass-warning-update.txt`, `trespass-warning-prologue.txt`,
`trespass-warning-count-policy.txt`, `trespass-package-install.txt`,
`trespass-package-constructor.txt`, its continuation, `trespass-warning-count.txt`
and `trespass-warning-speech.txt`. Normal entry/warning/leave/escalation,
real speech timing and save/restart remain runtime gates.

## Crime witness candidate enumeration

`0067A290` enumerates current actor candidates for an incident. A present
offender with Disabled (`800`) or Deleted (`20`) reference flags, or native
life state 1/2, prevents this enumeration. This offender check calls
`005E33B0(1)`, which excludes states 1/2 but not essential-unconscious state 6.

Each candidate must be an actor, differ from the offender, lack paralysis and
Disabled, and have a signed detection score **strictly above zero** for the
offender. Candidate life state 3 is excluded explicitly; `005E33B0(0)` excludes
1/2/6. These are actor `+B0` states, not process knocked values. Candidate
Deleted is not tested in this loop; the world collection must supply current
references. Do not add guessed NPC-only, guard-only or responsibility filters
at this stage. Willingness and report delivery occur elsewhere. Paralysis uses
the previously reviewed `005E17E0` AV-30 predicate, not the external header's
misleading HasFatigue label.

`crimeWitnessCandidate` preserves the offender/candidate distinction over
resolved inputs. Native state numbers remain explicit at this pure boundary;
the future persistent lifecycle must validate its own state domain. Missing
content/identity resolution cannot be disguised as an absent offender.
Existing detection remains authoritative; a normalized awareness threshold or
a new crime-specific detector would change this rule.

Three tests fail against the false stub, then cover every pair of states 0..6,
identity/paralysis controls, signed detection extremes, and asymmetric flag
masks. **19,600 original instruction cases** execute the native offender and
candidate filter branches, the real actor state/dead/paralysis queries, with
only actor identity, paralysis value and existing detection as boundary stubs.
Evidence: `S2/oracle-emulator/witness-candidate.py`,
`witness-candidate-table.json`, `witness-candidate.log`,
`S2/sources-01/crime-witness-selection.txt` and `crime-dead-query.txt`.
The original list subsequently sorts by distance to the **player** using
`00673B70`, independently of the offender identity; sorting and runtime event
order are not implemented by this predicate. Witness hearing, reporting,
incident idempotence and bounty remain integration gates.

## Alarm recipient spatial reach

Native `0067A420`, called at `0062F90F`, collects alarm recipients through
process-level actor lists. After its actor/package filters, the spatial branch
at `0067A593` uses a direct distance check if the recipient has the same nonnull
cell as the offender, or their worldspace queries match and either has a
nonnull exterior cell. Two null cells alone do not establish the direct path.
`004D6670` queries cell worldspace through `004C9CF0`; interiors return null.

Direct-space recipients qualify when the stored float distance to the offender
is **<= the integer** `iCrimeAlarmRecDistance`. If outside, this branch ends;
it does not retry via doors. Other spaces can qualify via the caller's supplied
nearby teleport doors: the destination cell must match the recipient cell
(including the native null comparison), or its cell must be null and its
worldspace query match the recipient's. The recipient-to-destination-door
stored distance uses the same inclusive radius. A different nonnull destination
cell does not qualify merely because its worldspace matches.

`crimeAlarmReachesLocation` implements this spatial policy over stable resolved
locations and the caller's actual door destinations. It must not receive all
doors in the world as a shortcut. Missing stable resolution is an error, not a
null-cell exception. The function validates finite nonnegative distances and
interior/cell presence, including supplied unused door inputs. It preserves
integer radius precision by comparing as double, matching x87 integer loading;
float 2147483648 is outside integer limit 2147483647.

The executable default is 10000 (`009E772A`, storage `00B36A50`); installed
Oblivion.esm `02BE94` sets 4000. The typed adapter and rule-input table record
both; no live-setting probe is claimed. This is recipient reach, not the
positive-detection witness filter, line-of-sight simulation, willingness or
bounty commitment. Other recipient exclusions and dialogue/AI dispatch remain
caller responsibilities.

Three spatial tests first fail against a false stub; a fourth tests typed
settings. Cases cover distinct interiors, shared/foreign exteriors, null
locations, door fallback, no retry after direct-distance rejection, inclusive
thresholds, integer precision and malformed inputs. **1,728 original instruction
cases** independently execute region selection, door-destination matching and
both radius branches. Cell flags are read by the native helper; destination
cell/worldspace queries are boundary stubs. Evidence:
`S2/oracle-emulator/alarm-reach.py`, `alarm-reach-table.json`, `alarm-reach.log`,
`S2/sources-01/crime-witness-selection.txt` (`0067A420` onward),
`crime-alarm-recipient-continuation.txt`, `crime-alarm-recipient-caller.txt`,
`crime-reference-worldspace.txt` and `crime-cell-worldspace.txt`.
Ordinary alarm propagation through actual load doors and save/restart remain
runtime acceptance gates.

## Fight score for AI and alarm response

Original `00546190` accepts target disposition, friend disposition, current
integer Aggression, distance, friend-enable, an unused argument, responsibility-
gate enable and current integer Responsibility. Aggression <= 0 returns zero.
Each disposition/aggression/distance term is stored as float after multiplying
by its GMST and adding its base. The distance term is **min(0, term)**: positive
near-distance terms are discarded, negative distance penalties remain.

The friend term applies only when enabled, target disposition < friend
disposition, and either the responsibility gate is disabled or the unrounded
Responsibility × `fCrimeAlarmRespMult` is **strictly less than** friend
disposition. This is different from the reporting-willingness predicate.
Equality excludes the term. Its float-stored value is friend disposition ×
`fFightFriendDispMult` + `fFightFriendDispBase`. The original adds aggression,
disposition, distance and friend terms on x87 without a final float store,
truncates to integer, then caps at 100. Negative scores are preserved.

`FightScoreInput`, `FightScoreSettings` and `fightScore` implement this pure
rule. The typed settings adapter preserves compiled defaults and the installed
Aggression base (-55 instead of -80), friend base (-25 instead of -50) and
responsibility multiplier (2 instead of 1.7f). Eight additional setting rows
record native initializer provenance; no live GMST probe is claimed. Finite
settings, nonnegative finite distance, float-term and integer-conversion
bounds are checked. Actor resolution, disposition calculation, incident
eligibility, AI target choice and combat dispatch remain caller responsibilities.

Four policy tests fail against the zero stub before implementation; one
additional test covers typed settings. Original instruction execution exposed
two incorrect preliminary expectations: the distance clamp direction and the
responsibility comparison direction. Both failures are retained in
`fight-score-boundary-failed.log` and `fight-score-responsibility-failed.log`.
The corrected oracle runs the complete original helper and its integer
conversion without function stubs, with both x87 precision controls 0x27f and
0x37f. Evidence: `S2/sources-01/actor-aggression-rule.txt` (only `00546190` through
`00546253`), `S2/oracle-emulator/fight-score.py`, `fight-score-table.json`,
`fight-score.log`, `fight-score-driver.cpp`, `fight-score-cpp-comparison.json`.
The corpus includes 5,400 compiled/installed boundary combinations, 1,024
fixed-seed generated inputs/settings, and two explicit float-store boundaries.
C++ results are compared directly with the original-instruction expected
values. This evidence does not establish normal-input AI or alarm acceptance.

## Alarm recipient delivery gate

After collection/spatial filtering, `0062F970` skips non-actors, actors whose
current process package is type 0x0f, sleeping actors (native sit/sleep state
**9**, not entering/leaving sleep), and actors for which `IsInCombat(true)` is
true. Guards also skip an incident whose byte +2c suppresses further guard
response. Remaining guards respond only when the **emitting actor's** base is
not evil. Non-guards instead require a strictly positive native fight score;
they do not use either guard-only exemption.

`crimeAlarmRecipientResponds` expresses that gate over resolved inputs. It does
not dispatch packages or report bounty. The emitter is the first parameter of
`0062F810`, passed from `0060F76F`; the offender and incident are the next two
parameters. The friend disposition used in the non-guard score comes from the
incident's actor victim (+8), while the target disposition/distance come from
the offender (+0c). The call enables the friend term, disables responsibility
gating, and supplies Responsibility 100. The emitter must not be silently
substituted with the victim when checking evil factions.

`005E6BA0` checks process +8 package byte +20 against 0x0f. `005E0F30` calls
process virtual +36c, which resolves to `0064B090` on HighProcess and reads the
sit/sleep byte +11d. The explicit comparison is 9. Older local evidence filenames
`crime-recipient-flee-query.txt` and `crime-recipient-creature-query.txt` are
misleading preliminary labels; neither query tests those properties.

Two tests first fail against the false stub, then cover all byte sleep states,
guard/non-guard combinations, signed score extremes and exclusions. **2,816
original instruction cases** execute the delivery branches and real package/
sleep queries; class guard, evil-base, combat, disposition/AV/distance and
resolved fight score are boundary stubs. Evidence:
`S2/oracle-emulator/alarm-response.py`, `alarm-response-table.json`,
`alarm-response.log`; `S2/sources-01/crime-alarm-recipient-delivery.txt`,
`crime-alarm-recipient-caller.txt`, `crime-alarm-emitter-dispatch.txt`,
`crime-recipient-sleep-state.txt` and the two query files above. The original
branches after `0062FA4B` choose process delivery versus immediate reporting;
those effects and their idempotence remain integration work.

## Report eligibility and bounty-driven infamy

The shared report gate in four `0060F250` alarm-handler branches requires a
present incident, NPC offender, an unset incident reported byte +11, and either
reporter Responsibility >= 100 or a guard class. `005E32D0` tests offender base
record type 0x23; it is **not a player-only test**. `0051BEF0` tests class flag
bit 1. `crimeReportEligible` implements only these common conditions, after the
handler's admission/context gates. Reporting uses the reporter's faction fine
multiplier, calls the offender's bounty mutation, then marks +11. Neither the
witness willingness comparison nor the alarm fight score replaces this gate.
The eventual service must commit bounty and the reported state atomically.

The normal player bounty mutation `0060FC20` adds float bounty via ExtraCrimeGold
and calls `00660710` only when the **increment is strictly greater than 1**.
That call truncates the increment to integer and adds it to the existing signed
integer accumulator. It stores that accumulator to float before comparing with
`fInfamyBountyMod`. At or above the threshold it increments infamy **once**, then
truncates float-stored-accumulator minus threshold back to integer. It does not
loop, preserve a fractional increment, or process the accumulator for an
increment of 1 or less. A 1500-gold increment at threshold 500 awards one point
and leaves 1000 accumulated; a later 2-gold increment awards another and leaves
502. Bounty reductions do not reverse this accumulation through this path.

`advanceCrimeInfamy` returns an immutable next state for that normal player
path. Nonfinite inputs, negative counters, nonpositive thresholds and signed
counter/conversion overflow diagnose explicitly. Compiled threshold 2000
(`009E779F`, storage `00B36A68`) is overridden by Oblivion.esm `06C64B` to 500.
Alternate player bounty routing, largest-bounty statistics, other infamy
sources and the authoritative world mutation are separate integration work.

Four policy tests fail against stubs before implementation; a fifth covers
settings. **480 original infamy instruction cases** run in both x87 precision
modes using the actual conversion helper, including rounding near one, the
threshold and integer-to-float precision boundaries. **384 original report
gate cases** cover all four handler branches, real NPC-type and guard-class
flag checks, and boundary stubs for resolved base/class identity and the
Responsibility value. One harness failure omitted the null-incident exit stop;
it is retained and corrected without changing expected policy.
Evidence: `S2/oracle-emulator/report-infamy.py`, `report-infamy-table.json`,
`report-infamy.log`, `report-infamy-harness-failed.log`,
`S2/sources-01/crime-alarm-handler-reporting.txt`, `crime-fine-caller.txt`,
`crime-bounty-update.txt`, `crime-infamy-accumulator.txt`,
`crime-offender-npc-query.txt` and `crime-class-guard-query.txt`.
These are rule checks, not committed/restarted world-crime acceptance.

## Bounty storage, query rounding and player realm routing

`0060FBC0` reads normal ExtraCrimeGold through `0041FC90`, except when the actor
is the player and player byte +116 is set: then it uses player float +700.
The command handler `0050EA30` writes +116 from its boolean argument and prints
that the player is/is not in the SE world. This establishes explicit Shivering
Isles routing, not a Gray Cowl identity or an inferred cell-name jurisdiction.
Both paths expose a stored value strictly between 0 and 1 as **1**, without
changing its underlying fraction. Zero, values >= 1, and negative alternate
values retain their value.

`0060FC20` adds an increment directly to alternate storage and returns; this
path neither clamps negative values nor updates normal player statistics.
Otherwise `004269E0` adds to ExtraCrimeGold, stores as float, and removes the
extra record if the result is <= 0. The absent normal record reads as zero.
Normal player statistics are updated only for increments strictly >1;
non-player actors always use normal storage and do not update those player
statistics. The separate `advanceCrimeInfamy` rule covers infamy; largest bounty
and mutation notifications remain integration responsibilities.

`CrimeBountyState`, `queryCrimeBounty` and `modifyCrimeBounty` preserve the two
storage paths, fractions, float rounding and statistics-dispatch condition.
They return an immutable result. Both input buckets must be finite, normal
storage must be nonnegative, and finite arithmetic overflow diagnoses before
returning a change. The native negative alternate value is explicitly retained;
a future persistent validator must not erase it under a generic bounty clamp.
The realm flag is supplied from resolved player state, not guessed geography.

Three tests fail against stubs, then cover realm/actor routing, adjacent one
boundaries, subnormal fractions, negative reductions, stored-float precision,
statistics dispatch and invalid inputs. **512 original mutation cases**, each
with before/after original bounty queries, pass in both x87 precision modes.
The actual extra-data update arithmetic, getter, realm branch and infamy helper
execute. Extra lookup/removal and change notification are boundary stubs;
allocation for an absent extra is represented by an existing zero-valued node.
Evidence: `S2/oracle-emulator/bounty-storage.py`, `bounty-storage-table.json`,
`bounty-storage.log`; `S2/sources-01/crime-bounty-query.txt`,
`crime-bounty-update.txt`, `crime-bounty-extra-update.txt`,
`crime-bounty-extra-query.txt`, `crime-player-seworld-setter.txt` and
`player-bounty-mode-references.txt`. Runtime storage, script flag/query adapters,
identity-specific exceptions and fresh-process persistence remain open.

### Arrow age expiry and fade

Original update `0060C170` stores `R(age + duration)` at arrow +68, then
changes state +60 to 3 only when that stored age is **strictly greater** than
`fArrowAgeMax` (`00B37048`, compiled 90 seconds, constructor `009E995F`, no
installed override in audit-12). Equality survives; sub-float-step increments
can leave age unchanged. This limit is independent of the AI shooting-distance
setting `fArrowMaxDistance`; it is not a 2000-unit flight-distance cutoff.

The state-3 branch `0060C52C` advances opacity +64 on the same tick using
`R(opacity - duration / 3.0)`, with double constant 3 at `00A30E48`. At zero
or below, it stores zero and requests reference deletion. Thus the whole tick
that crosses the age limit contributes to fading, not just its over-limit
remainder. Already-fading arrows continue even below the age limit. Opacity
alone does not enter the fade state. This pure rule takes a live non-deleted
arrow; collision, reference-budget selection, attachment transforms, actual
deletion and exclusion of deleted arrows from future updates belong to the
projectile authority.

`ArrowLifetimeState` and `advanceArrowLifetime` return an immutable age/fade/
removal change. Supported simulation durations and ages must be finite and
nonnegative; opacity must be finite in [0,1]; invalid settings and age overflow
diagnose before returning a change. The original negative-duration update
branch skips its lifetime work; negative engine simulation steps are outside
this API's supported domain.

Three policy tests fail against the stub before implementation. **2,016 original
instruction cases** pass in both x87 precision modes, covering all four native
states, zero/adjacent/exact age thresholds, opacity boundaries and fade completion.
All cases also match a separately compiled C++ driver exactly.
The full original update entry executes with null scene/process query and
deletion-notification boundary stubs; collision and rendering are excluded.
Evidence: `S2/oracle-emulator/arrow-lifetime.py`, `arrow-lifetime-table.json`,
`arrow-lifetime.log`; `S2/sources-01/arrow-update.txt` and
`arrow-update-tail.txt`. Runtime ticking, visual fading/removal, normal-input
flight/impact and save/restart acceptance remain open.

### Final arrow inventory-recovery roll

After actor-impact eligibility and attachment handling, original `0060B08D`
checks arrow enchantment +7C. A non-null enchantment skips the random draw and
inventory insertion. This is the arrow enchantment; bow enchantment +80 is
separate. An unenchanted arrow always consumes a draw, even at 0% or 100%.
`0047DF80(0)` supplies the nonnegative random sample, which is reduced modulo
100 and compared with **strict less-than** against `iArrowInventoryChance`
(`00B370C8`, compiled 50, constructor `009E9C47`, no installed override in
audit-12). On success the original invokes the impacted actor's inventory
insertion with the arrow base, null extra data and count 1, then sets arrow
byte +95 to 1. Failed rolls do neither.

`arrowInventoryRecovery` represents that final roll, including whether a draw
is consumed. Its caller must first establish impact eligibility, and must not
consume a draw for enchanted ammo. Probability outside [0,100] and draws outside
[0,99] diagnose, consistently with the existing supported mastery percentages.
This does not establish the earlier creature +104 eligibility flag's meaning,
world-surface recovery, collision/bounce choice, actual inventory insertion,
spent-projectile deduplication or save/load behavior.

Three new policy/distribution tests fail against the stub. **1,200 original
instruction cases** cover every draw at chances 0/1/49/50/99/100 with and without
arrow enchantment. A separately predeclared **100,000-sample** run using seed
`0x4d15a3`, native 15-bit samples and the original modulo/branch code recovers
**49,870** arrows, within the declared 49,400–50,600 interval. Boundary stubs
supply RNG/base queries and observe inventory insertion; the latter verifies
base/count/extra arguments and the real insertion marker. Evidence:
`S2/oracle-emulator/arrow-recovery.py`, `arrow-recovery-table.json`,
`arrow-recovery-summary.json`, `arrow-recovery.log`, and
`S2/sources-01/arrow-recovery.txt`. This is independent branch evidence, not
normal-input projectile acceptance.

### Arrow reference-count cleanup selection

Original creation tail `0060CD25` increments live arrow count `00B3B7D0` and
calls cleanup only when the resulting count is strictly greater than
`iArrowMaxRefCount` (`00B370D0`, compiled 15, constructor `009E9C67`, no installed
override in audit-12). Cleanup `00608120` traverses process-list query 1 first
(manager +0), then query 0 (manager +68) only if the first traversal found no
candidate. The actual getter `00673A50` and list-head helper `007616D0` establish
these pools; cached external structure labels are not used to rename them.

After the reference-eligibility virtual query and arrow RTTI cast, only native
state **2** qualifies. The chosen age must be strictly greater than the current
best age, initialized to zero. Thus zero-age arrows do not qualify, equal-age
ties preserve traversal order, and an older fallback-pool arrow does not replace
a qualifying preferred-pool arrow. No eligible settled arrow means no selection:
this is not a hard cap that deletes flying arrows. The creation call requests
fading rather than immediate deletion; cleanup writes selected state **3**.
It does not decrement the live global count. Its returned count-minus-one is
unused by this caller; removal/destruction still owns actual count changes.

`selectArrowForCleanup` returns the selected input index without mutating it.
The caller supplies resolved eligible arrows, preserves each pool's traversal
order, maps the index back to stable identity and starts fading once. Finite
nonnegative ages/counts and a nonnegative signed-int setting are required;
malformed input diagnoses even when the count is below the threshold.

Three tests fail against the stub. **6,586 original instruction cases** pass
in both x87 precision modes and match a separately compiled C++ driver exactly,
executing the original count gate, both native
list queries/traversals, age/state selection and fade write. The reference
eligibility virtual and RTTI cast are boundary stubs. Cases cover empty eligible
sets, all combinations of flying/settled/fading states and zero/equal/older ages,
priority/fallback behavior, exact/above count thresholds and int32 maximum.
Evidence: `S2/oracle-emulator/arrow-cleanup.py`, `arrow-cleanup-table.json`,
`arrow-cleanup.log`; `S2/sources-01/arrow-cleanup.txt`,
`arrow-count-increment.txt`, `mobile-process-list-query.txt` and
`process-list-head.txt`. Live collection, fade/deletion bookkeeping and
save/restart resource bounds remain projectile-service acceptance requirements.

### Fatigue regeneration request

Original actor update `005F2720` reads the maximum fatigue modifier from player
modifier category 0 through `0065D270`, or the actor process virtual +468.
An actor without that process contributes zero. The original player reader
accesses the category-0 array at +204; the process override `00658870` reads
the maximum-modifier collection at +94. Script/damage modifiers are not added
to this maximum term.

The updater floors current fatigue, obtains floored base fatigue through
`005F1910`, and requests regeneration only when `floor(current) < floor(base)
+ maximumModifier`. The sum is compared on x87 without a final float store.
This is not a direct comparison of current and base floats: for base 10.5,
modifier 0 and current 10, there is no restoration; with modifier .5, the
restoration path runs even at current 10.5. The separate AV mutation authority
owns Damage-channel mutation; the combined wrapper verification below corrects
the earlier assumption of a final clamp to the displayed maximum.

Rate helper `00547F20` computes `R(fFatigueReturnBase + currentIntegerEndurance
* fFatigueReturnMult)`. The caller then stores `R(rate * duration)` and invokes
fatigue AV 10's restore operation (virtual +2A4, final argument 0) only for a
strictly positive amount. Compiled and installed settings are 10 and 0; their
existing GMST-69/70 probes and input-table rows already record provenance.
There is no luck adjustment or TES3 encumbrance factor in this rule. Signed
Endurance/settings retain their arithmetic; a nonpositive result dispatches
no restore.

`fatigueRegeneration` returns that positive requested delta or zero, without
mutating actor values or pre-clamping to a remaining deficit. It validates
finite inputs/settings, nonnegative elapsed time, floor-to-int32 bounds and
float overflow. For an otherwise valid already-full actor, the unused rate
arithmetic is not evaluated. Mutation, processing eligibility and gameplay
recovery from negative fatigue remain integration responsibilities.

Three policy tests first fail against the stub. The independent original
updater, base/current flooring, actual rate helper and positive-delta dispatch
pass **32,404 cases in both x87 modes**, all matching a separately compiled C++
driver exactly, including subnormal thresholds, the
unrounded maximum sum and separate rate/duration rounding. Actor AV reads,
NPC maximum-modifier access and mutation notifications are boundary stubs;
the player maximum-modifier reader executes original instructions. Evidence:
`S2/oracle-emulator/fatigue-regeneration.py`, `fatigue-regeneration-table.json`,
`fatigue-regeneration-02.log`; `S2/sources-01/fatigue-regeneration.txt`,
`actor-base-av.txt`, `player-av-modifier.txt`, `process-av-modifier.txt` and
`fatigue-return-base-547f2c.txt`. The initial 32,400-case run is retained in
`fatigue-regeneration.log`.

### Actor base level and player-level offsets

Original `004677F0` reads ACBS level/offset as a 16-bit word. Without flag 0x80,
it returns that word directly: minimum, maximum and the minimum-one fallback
are not applied. With flag 0x80, it adds the resolved player's base-record level
in **16-bit arithmetic**; a missing player base leaves the offset unchanged.
It then interprets the result as signed for bounds comparisons.

The minimum check runs first. A nonzero minimum greater than the result is
returned immediately. Otherwise a nonzero maximum less than the result is
returned immediately. Only after those checks does a remaining value below 1
become 1. Zero means no bound. Inverted bounds are not sorted: minimum 20,
maximum 10 returns 20 for candidate 10, but 10 for candidate 20. Bounds are
unsigned words for comparison; their returned low word is interpreted as signed
by the NPC stat caller. `005222D0` explicitly sign-extends this return and
subtracts one before computing growth. That caller's full auto-calculation
remains a separate rule task.

`resolveActorLevel` preserves this raw ACBS lookup, using explicit unsigned word
addition and bit-casting to avoid C++ signed overflow or implementation-defined
narrowing. It intentionally does not repair raw fixed levels or bad bounds;
the future actor-construction adapter must diagnose unsupported effective
configurations rather than silently manufacture level 1. This pure helper does
not mutate shared base records or implement level-change propagation.

Three new tests fail against the stub. **3,528 original cases**, all matching a
separately compiled C++ driver exactly, execute the
complete level helper, with only the player's base-record lookup stubbed.
They include missing player base, fixed/scaled modes, signed-word wrapping,
zero/inverted/wide bounds and exact thresholds. Evidence:
`S2/oracle-emulator/actor-level.py`, `actor-level-table.json`, `actor-level.log`,
`S2/sources-01/actor-base-level.txt` and `npc-calc-stats.txt`. Live NPC/creature
initialization, auto-calculated stats and save authority remain open.

### NPC auto-calculated dynamic base values

Original image SHA-256 remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
NPC calculation `005222D0` subtracts one from its resolved signed level before
calling health helper `00547F80`. Let `g = level - 1`. Attribute health is
`trunc((Strength + Endurance) * fNPCAttributeHealthMult)`. Per-level health is
`iNPCBasePerLevelHealthMult`, plus one for favored Endurance and plus one for
Combat specialization, minus one for Magic, unchanged for Stealth. Add
`perLevel * g` using integer arithmetic. If `g < iLowLevelNPCMaxLevel`, multiply
by `R(base + R(R(g / maximum) * (1 - base)))`, where `base` is
`fLowLevelNPCBaseHealthMult`; otherwise multiply by one. Truncate the result;
there is no final float store before integer conversion. `R` denotes a float32
store. A zero low-level maximum disables scaling for supported positive levels.

NPC magicka helper `005482B0` returns
`trunc(Intelligence * fNPCBaseMagickaMult + Intelligence)`, with no intermediate
float store. Fatigue helper `005479D0` adds Strength, Endurance, Agility and
Willpower. NPC setters store health as a dword and magicka/fatigue as words.
This is base auto-calculation, before race spells, abilities and runtime AV
modifiers; it is not the player health formula or a level-up health increment.

Compiled GMST constructors: attribute multiplier `.5` (`009ED98F`, storage
`00B37BE8`), per-level multiplier `4` (`009ED967`, `00B37BE0`), low-level maximum
`3` (`009ED947`, `00B37BD8`), low-level base `.25` (`009ED91F`, `00B37BD0`),
NPC magicka multiplier `.2` (`009EBE2F`, `00B37718`). Installed Oblivion.esm
changes maximum to `4` (`0C7912`), low-level base to `.4` (`0C7913`), magicka
multiplier to `1.5` (`09E642`); attribute/per-level overrides retain `.5`/`4`.
The typed builder uses compiled fallbacks and validates winning override types.

The supported pure API requires level >= 1, specialization 0..2, nonnegative
finite attribute/magicka multipliers, per-level multiplier >= 1, nonnegative
low-level maximum and low-level factor in [0,1]. It rejects integer arithmetic
and magicka-storage overflow instead of reproducing malformed wraparound.
These are explicit supported-domain restrictions, not claims of native clamps.

**6,300 original complete health/magicka helper cases**, in both x87 modes,
match a separately compiled production C++ driver exactly. No helper calls are
stubbed; the original integer conversion executes. Evidence under
`S2/oracle-emulator/npc-dynamic-stats*` and
`S2/sources-01/{npc-health-formula,npc-health-caller-52267c,npc-magicka-fatigue-formulas,actor-base-fatigue}.txt`.
The first component attempt retains a mistyped maximum-level test expectation;
the oracle confirms 196851 and the corrected run passes. Actor construction,
attribute/skill auto-calculation and runtime persistence remain open.

### NPC attributes and skills

Original `005222D0–00522706` computes NPC attributes and skills from resolved
race/sex attributes, class favored attributes/major skills/specialization and
winning SKIL governing attributes/specializations. Its player-specific branch
is outside this API. NPC Personality (native AV 6) retains the authored byte;
other attributes start from the appropriate race sex. The first favored
attribute gets `fAttributeClassPrimaryBonus`, otherwise the second gets
`fAttributeClassSecondaryBonus`. Both compile to 5 (`009E3C2F`/`009E3C5F`,
storage `00B362E4`/`00B362EC`); a repeated favored attribute gets only the first.
For each of the 21 skills in native AV order, if its governing attribute matches,
add `level - 1` for a class major or `(level - 1) * double(.2f)` for a minor,
storing float32 after each addition. Cap above 100 and round to nearest-even
with `FISTP`, then store the resulting byte. Personality bypasses that cap.

Skills start at `R(gained + 25)` for majors, otherwise
`R(gained * double(.1f) + 5)`. Matching specialization applies
`R(R(skill + 5) + gained * .5)`. Each matching one of seven ordered racial
pairs adds its **signed** byte bonus with a float store; duplicate matches add
repeatedly. Then apply the upper 100 cap, nearest-even conversion and byte
storage. Negative results wrap in the byte; there is no lower clamp. The pure
API preserves this and unused skill sentinel -1, diagnoses invalid native AVs,
levels/specializations and nonfinite/overflowing arithmetic. It does not depend
on the host rounding-mode setting for nearest-even conversion.

**384 original instruction cases** cover both x87 precision modes and both
race sex branches across levels 1..32767, varied class/skill definitions,
fractional favored bonuses and duplicate signed race bonuses. Every attribute,
skill, health, magicka and fatigue output matches a separately compiled C++
driver exactly. Original loops, class/race readers, skill-store lookup, AV
mapping, byte/dynamic setters and dynamic formulas all execute. Only change
notification and its RTTI boundary are stubbed; execution stops before class
service assignment at `00522706`. This is not runtime actor construction.
Evidence: `S2/oracle-emulator/npc-auto-stats*`, `S2/sources-01/npc-calc-stats.txt`.
Initial harness attempts are retained: the skill lookup and AV mapper accept
byte indices, so provisional full-dword stubs were incorrect. The successful
harness executes those original helpers instead.

The runtime input adapter still needs typed winning SKIL definitions and seven
ordered signed RACE bonus pairs; the existing unsigned map loses duplicates.

### Native skill definitions and ordered racial bonuses

The installed eleven-plugin raw inventory contains 21 SKIL records and 15 RACE
records (`S2/sources-01/skill-race-inventory.{py,json,log}`, with source hashes).
SKIL INDX is a native skill AV (12..32), independent of its FormId; DATA has
exactly 20 bytes: the same AV, governing attribute, specialization and two use
floats. The independently reviewed runtime layout places those five fields at
`+2C..+3C`, consistent with the original NPC auto-calculation's reads at `+30`
and `+34`. The typed reader checks lengths, required/duplicate fields, AV
agreement, enum domains and finite use values while retaining descriptions,
mastery text and other raw subrecords. Progression semantics remain M17-owned.
A complete winning-skill resolver rejects missing, deleted and duplicate-AV
records rather than substituting TES3 definitions.

TES4 RACE DATA has seven signed skill/bonus byte pairs, two padding bytes,
four height/weight floats and race flags, totaling 36 bytes. The old parser
mistook padding for an eighth pair. The native-only path now keeps the seven
ordered signed pairs and padding separately, preserving duplicates and negative
bonuses for NPC calculation. The existing map remains for legacy consumers.
Later-game layouts use their existing paths. All fifteen installed records
have zero padding and valid unique skill IDs; synthetic tests exercise nonzero
padding, negative/duplicate entries, truncation, compression and version split.

SKIL is registered in the native record catalog, store tuple, explicit dynamic
store instantiation, loader and esmtool; later-game dispatch stays raw. Real
ESMStore binary-load tests cover differing master order, overrides, stable
identities and empty deletion. The independent Python audit now validates SKIL
fields, duplicate AVs and inventory completeness, with a reviewed count of 21.
`S2/audit-13` has zero data failures and a passing content count lock; gameplay
and rule gates remain open. `S2/native-stat-record-dumps-01` parses all eleven
plugins successfully and compares all 21 typed skill definitions against raw
payload decoding (core integer fields exact; printed use floats within declared
relative 5e-6 / absolute 1e-7 text precision tolerance).

Component/sanitizer/Python checks pass in `S2/npc-stat-records-01`. Its engine
attempt exposes a missing store-header include; attempt `-02` exposes the
required explicit `TypedDynamicStore<Skill>` instantiation at link time.
Both failures are retained. Corrected `-03` builds openmw/openmw-tests/esmtool
and passes all engine tests, including the actual-store loader fixture.

### Native creature base-stat scaling

Original executable helpers `51CC00/51CB80/51CB00` calculate combat, magic and
stealth skills; `51C980` calculates damage, `51CA10` health, and `51CAA0/51CAD0`
magicka/fatigue. Only PCLevelOffset creatures scale. Each uses resolved level
floored at one (not levels gained). Skills/damage add level times their GMST,
truncate without an intermediate float store, then store a byte/word. Dynamic
base values multiply their authored word by level and wrap to a word. Attributes
stay authored. Full base-AV dispatch `51D540` maps the three groups to AV12..18,
19..25 and 26..32. There is no skill cap of 100 in these getters.

The independently hashed executable corpus has **2,880 cases**, both x87 modes,
fixed/scaled flags, signed levels, fractional/negative multipliers and byte/word
wrap boundaries. All match compiled production C++ exactly. An additional
**420 original dispatch cases** verify group mapping. Only the player-level
query is stubbed; the original getters, level logic and integer conversion run.
Evidence is in `S2/oracle-emulator/creature-base-stats-*` and
`creature-skill-dispatch-*`; disassembly/GMST initialization provenance is under
`S2/sources-01/creature-*`. Defaults are combat/magic/stealth 2 and damage 1,
with no installed overrides. The input manifest now includes these and the
verified NPC auto-calculation settings. Nonfinite inputs and signed-conversion
overflow are diagnosed, not assigned invented native results.

`S2/creature-base-stats-01` passes full components and ASan/UBSan ESM4 suites.
These are construction rules, not evidence of live combat integration.

### Creature runtime actor-value aliases differ from base-form getters

RTTI-verified Creature vtable `A710F4` slots A1..AC select wrappers
`6253C0..6257E0`. Both current-value getters and all ten setter/modifier
wrappers map skills 12..18 **and Marksman (28)** to Combat AV12, skills 19..25
to Magic AV19, and the remaining 26..32 to Stealth AV26. Other AVs pass through.
This additional runtime layer was missed by the earlier base-form-only
`51D540` dispatch corpus. Base-record Marksman remains in the stealth group;
the live creature class now reads Combat for Marksman.

`S2/oracle-emulator/creature-actor-av-dispatch.py` executes **1,776 original
forwarding cases** (12 wrappers, AV0..72 plus UINT_MAX, both x87 modes). It
stops explicitly at the common parent method, verifying forwarded AV, actor
pointer and every remaining argument. It does not claim to test the parent's
mutation arithmetic. Hash-bound RTTI/function mapping and traces are under
`S2/sources-01/actor-value-mutation-*`. That mapping also identifies `A73A0C`
as **PlayerCharacter**, not generic Actor; Character is `A6FC9C`.

The live creature regression now checks distinct base/live Marksman values
and all 21 skill reads. All 560 engine tests pass with rebuilt binaries in
`S2/creature-actor-av-01`. Earlier construction and NPC escort results remain
valid within their recorded scopes; they did not verify this runtime alias.

### Scalar actor-value modifier addition

`0065BC70` adds two floats, explicitly stores the sum as float, and—when its
third argument is zero—replaces strictly positive results with positive zero.
Negative and signed-zero results survive. Player modifier dispatch `65D310`
uses this helper for maximum, script and damage arrays; caller policy chooses
the clamp. `addActorValueModifier` implements this narrow arithmetic, with
finite-input and finite-result diagnostics. It is not a complete modifier
mutation or permission to combine the three stored categories.

**1,156 complete original helper cases** match production C++ bit-for-bit:
17 by 17 signed, fractional, adjacent float, subnormal and large exact values,
both clamp modes and both x87 modes. No original helper is stubbed; a final
fixture instruction only stores ST0 for comparison. Evidence:
`S2/oracle-emulator/actor-modifier-add*`, source
`S2/sources-01/actor-value-modifier-add.txt`. Full components and ASan/UBSan
ESM4 suites pass in `S2/actor-modifier-add-01`.

NPC process mutation requires separate verification: `65CA60` uses a sparse
map and its absent-entry branch is not identical to this scalar helper.
Player/NPC composition order, caps, side effects and persistence remain open.

### Sparse NPC modifier presence semantics

`0065CA60` looks up a modifier entry before arithmetic. An absent entry with
nonzero delta is created directly, even when the clamp argument is false.
An existing entry adds/stores the float sum, optionally caps positive results
at zero, and removes zero-valued entries. Therefore absent and stored zero
are observably different inputs. `addSparseActorValueModifier` represents this
with an optional float and validates finite arithmetic. A persistence adapter
must retain absence; replacing every missing NPC modifier with dense zero
would change later restoration behavior.

**2,312 original cases** match production result bits and presence exactly,
including removal followed by a new addition. The original complete function
executes with only lookup/allocation/insertion/removal boundaries stubbed.
Both x87 modes, both clamp modes, absent/present entries, rounding boundaries
and subnormals are covered. Evidence: `S2/oracle-emulator/actor-sparse-modifier-*`
and `S2/sources-01/actor-value-map-add.txt`. Full components and ASan/UBSan ESM4
suites pass (`S2/actor-sparse-modifier-01`). Actual controller mutation remains
open; this is storage arithmetic, not a restoration or combat acceptance case.

### Native scalar state and composition order

`ActorValueState` retains base plus distinct optional maximum, script and
damage modifiers. Immutable category updates select player dense arithmetic
or NPC sparse arithmetic; the caller still owns delta eligibility, base-field
conversion, derived values and side effects. Invalid enums, nonfinite fields
and arithmetic overflow diagnose before producing a new state.

Original PlayerCharacter float getter `65E110` composes
`R(base + maximum + script + damage)`, with only the final float store.
LowProcess getter `6433E0` composes `R(base + script + damage)`. MiddleLow
`6587E0` adds maximum after that stored result: `R(low + maximum)`. MiddleHigh
shares this path; HighProcess `6289F0` delegates ordinary AVs to it. These
getters do not clamp negative current values to zero. With base 1, maximum -1,
script 2^-24 and damage 0, player output is 2^-24, active NPC output 0, and
low-process NPC output 1. Low-process reads do not erase retained modifiers.

**6,200 original instruction cases** match production composition bit-for-bit
across player/low/middle/high paths, both x87 modes and AV0/8/10/12/28. Only
base-value and sparse-map lookups are stubbed; original composition, calls and
stores execute. The corpus includes cancellation boundaries and deterministic
mixed values. Evidence: `S2/oracle-emulator/actor-value-composition*`; RTTI-bound
process mappings/traces: `S2/sources-01/process-av-*`.

This core intentionally does not model outer magicka/encumbrance special
queries, null-process base-only fallback, integer actor-value getters or
life/death transitions. Those remain engine-adapter work. Full components and
ASan/UBSan ESM4 suites pass (`S2/actor-value-state-01`).

### Player dynamic base contributions and native Magicka scale

Common base getter `005EAD00` applies `005E2210` only to the player for
AV8–11. It adds a derived contribution to the base-form integer value, then
multiplies, storing once at the end. The contribution itself is stored as float:

- Health: `R(trunc(currentIntegerEndurance * fPCBaseHealthMult))`.
  Helper `00548020` ignores its Strength argument.
- Magicka: `R(trunc(currentIntegerIntelligence * fPCBaseMagickaMult
  + currentIntegerIntelligence))`, helper `005482B0`.
- Fatigue: `R(wrapSigned32(Strength + Willpower + Agility + Endurance))`,
  using current integer AV queries and original integer additions (`005479D0`).
- Base Encumbrance: `max(0, R(R(currentIntegerStrength)
  * fActorStrengthEncumbranceMult))`, helper `00547ED0`. This base query
  supplies capacity; current Encumbrance uses inventory, a separate path.

Magicka alone uses `scale = R(currentAV40 / 10)`, replacing stored zero with
one. The divisor is **10**, the verified double at `00A3F3E8`; an earlier
uncommitted `/100` inference failed **544/2,368** original cases. The final
base result is `R((integerFormValue + storedContribution) * scale)` without an
extra float store before multiplication. NPC outer getter `005F1A60` references
the same divisor; it scales its already composed process value separately.

`calculatePlayerDynamicBaseValue` and `actorMagickaScale` implement these
read-only arithmetic boundaries, with finite/overflow diagnostics. They do not
resolve actors/effects, modify base records, infer bar maxima or change saves.
`PlayerDynamicBaseInput` takes current integer attributes, not mastery/base
attributes, and retains the separate form contribution for later authority.

**2,536 original instruction cases** match optimized production C++ bits
exactly with both x87 modes, signed/zero inputs, custom multipliers, float-store
boundaries and 32-bit fatigue wrapping. Only current AV and base-form AV lookups
are stubbed; original getter, dispatch, helpers and `009828C0` conversion run.
Evidence: `S2/oracle-emulator/player-dynamic-base*`, including the retained
incorrect-divisor comparison; `S2/sources-01/player-base-av-adjustments.txt`,
`player-health-adjustment.txt`, `player-magicka-adjustment.txt` and
`actor-capacity-adjustment.txt`. Full **1,872 component** and **350 ASan/UBSan
ESM4 tests** pass (`S2/player-dynamic-base-01`).

Compiled defaults are health 2, Magicka .5 and encumbrance 5. Winning installed
records supply Magicka **1** (`Oblivion.esm:09E62F`) and encumbrance **5**
(`Oblivion.esm:010554`); there is no installed health override in audit-13.
These inputs are recorded in `M15-PHYSICAL-RULE-INPUTS.json`; the .5 compiled
default must not replace the installed Magicka multiplier. Live actor authority
and modifier-driven derived updates remain open.

### Integer actor-value composition is a separate query

Player current integer getter `0065E030` obtains the resolved integer base via
`005F1910`, then truncates `base + maximum + script + damage` without a float
store before conversion. The base query itself floors its base float, as
documented earlier; current-value composition does **not** floor the final sum.
LowProcess integer getter `00643340` truncates `base + script + damage`.
MiddleLow/MiddleHigh `00658790` then truncate `lowInteger + maximum`; HighProcess
`00628940` delegates ordinary AVs to this path. This differs from casting the
float current query: base 100, maximum .5 and script -.5 yield player integer
100, active NPC integer 99, but active NPC float 100.

`composeIntegerActorValue` takes an explicitly resolved integer base and the
three retained modifier categories. Invalid enums, nonfinite modifiers and
integer overflow diagnose. It does not resolve base forms, aliases, missing
processes, inventory Encumbrance or the outer NPC Magicka scale.

**8,280 original instruction cases** match compiled production integers exactly
across player/low/middle/high, AV0/8/10/12/28, both x87 modes, signed fractional
modifiers, integer endpoints and values above float's exact integer range.
Only resolved integer-base, valid player-base presence and sparse-map lookups
are stubbed; original composition and `009828C0` conversion execute. Evidence:
`S2/oracle-emulator/actor-integer-composition*`, hash-bound
`S2/sources-01/process-integer-av-vtables.json` and corresponding traces.
All **1,875 component** and **353 ASan/UBSan ESM4 tests** pass
(`S2/actor-integer-composition-01`). Actor bridge integration remains open.

### Dynamic maximum and nonplayer outer Magicka queries

Original executable SHA-256 remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
The health restoration path `005F2530` constructs its unclamped maximum as
`R(integerBase + eligibleMaximumModifier)`. The base integer is supplied by
`005F1910`; Player maximum lookup is `0065D270(category=0, AV=8)`, while a
nonplayer process uses virtual `0x468`. LowProcess implementation `0060D0B0`
returns zero; MiddleLow/MiddleHigh/High `00658870` reads their maximum map.
No-process also contributes zero. The prepared dynamic view uses this integer
base boundary, not the raw float base or current minus damage.

**2,352 original executions** captured maximum bits from the complete health
restoration routine across player/active/low/no-process and both x87 modes.
Only base integer, modifier lookup, current float and the final damage-modifier
callback are stubbed; arguments/return stack are checked. The actual Low getter
executes. Production `dynamicActorValueMaximum` matches every captured maximum.
This does not claim complete restoration/death/event behavior from the helper.

Nonplayer outer getters `005F1A60` (float) and `005F1970` (integer) query current
float AV40, store `R(AV40 / 10)`, substitute one for stored zero, and scale their
respective process result. Float returns `R(processFloat * scale)`. Integer
returns `trunc(processInteger * scale)` without converting that integer to float
first. **992 original getter paths** match `scaleNpcMagicka` and
`scaleNpcIntegerMagicka`, including signed/fractional input, zero/underflow
fallback and integers beyond exact float representation. The original base
accessor, outer arithmetic and integer conversion execute; only current AV40
and process AV9 lookups are stubbed.

Ignored evidence: `S2/oracle-emulator/dynamic-maximum-health-restore*`,
`npc-magicka-outer*`, `actor-projection-rules-driver*` and
`actor-projection-rules-comparison.json`; hash-bound process lookup metadata is
`S2/sources-01/process-maximum-modifier-vtables.json`. The comparison records
production source/driver hashes. An initial emulator stop-address/cache setup
failure is documented separately; no production arithmetic was changed for it.
All **1,881 component /359 ASan+UBSan ESM4 tests** pass
(`S3/npc-value-authority-01`).

The High-process AV48 float getter `006289F0` returns a cached integer query,
not ordinary float composition; Encumbrance AV11 also has inventory/cache
behavior. The new NPC scalar service explicitly rejects these two queries
until their additional state is integrated. They are not silently approximated.

### Actor-value command dispatch and ForceAV delta

For original executable SHA-256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`,
the script command table at `00B0C8C0` identifies SetAV handler `005016D0`,
ModAV `005017E0`, and ForceAV `00501890`. Set forwards integer arguments to
actor virtual `290`; Mod forwards integer arguments to `2A0` in script mode
or `2A8` in console mode (`00B361AC`). Force queries current float through
virtual `288`, subtracts that value from the exact int32 request, stores the
delta once as float, and forwards it to `29C` (script) or `2A4` (console).
The mutation slots correspond to the previously identified Script/Damage
wrappers; callback source is null for these commands. In particular, casting
request 16,777,217 to float before subtracting current 16,777,216 would lose the
original delta of 1. The same issue occurs at signed int32 extremes.

`S2/oracle-emulator/actor-value-script-commands.py` executes **10,230** original
handler paths, with both x87 modes, script/console modes, six AV IDs, integer
boundaries and seeded fractional current values, plus failed extraction and
non-actor returns. Argument extraction supplies already typed integers;
dynamic_cast, actor getter and actor mutation are explicit stub boundaries.
This does not establish textual decimal coercion, base setter fields, god-mode
eligibility, death or callbacks. All **3,408 Force delta cases** match optimized
`forceActorValueDelta` C++ bits (`force-actor-value-comparison.json`).

The result is a modifier delta, not a promised final current AV: the ordinary
NPC Magicka outer scale runs again after applying the delta to Script storage.
Actual class/service tests retain this behavior (68.625 current, request100,
delta31.375 produces115.6875 with scale1.5), and player health force changes
current without replacing maximum. Automatic script routing remains open.

### Fatigue regeneration through native mutation wrappers

The same hash-identified original executable now executes updater `005F2720`
through Player DamageFloat `0065E530` and category-2 update `0065D310`, or
common actor DamageFloat `005E2BE0` and process virtual +288. Low dispatches
`006434F0`, MiddleLow/MiddleHigh `006588A0`, and High `00628CA0`. All NPC fatigue
paths reach sparse modifier add `0065CA60` with allow-positive false. Player
fatigue uses fixed field +44C and scalar add `0065BC70` with that flag false.
High's additional cache invalidation concerns Encumbrance/Paralysis, not fatigue.

These wrappers do not clamp current fatigue to its displayed maximum. They
clamp an existing Damage modifier's positive sum to zero; an absent NPC entry
instead inserts a nonzero positive delta directly. Thus base 40, Script -1,
absent Damage and restoration 12 produce NPC current 51, but player current 39.
With an existing zero NPC Damage entry, that first restoration removes the
entry and leaves current 39. Sparse presence must survive both mutation and
save/reload. Low-process regeneration excludes the Maximum modifier; Player
includes it independently of the service's process classification.

`S2/oracle-emulator/fatigue-authority.py` and `fatigue-authority-table.json`
retain **2,700 combined original-instruction cases**, including both x87
precision modes, five owner/process paths, fractional bases and thresholds,
negative fatigue, absent/zero/negative Damage entries, and zero/short/long
updates. Original updater, rate helper, wrapper dispatch, channel arithmetic
and branch decisions execute. AV current/base/Endurance reads, NPC maximum
lookup, sparse collection allocation/lookup/insert/remove, and notifications
are explicit boundary stubs. Notification counts are checked, but actual UI,
event delivery and waking from negative fatigue are not established here.

### Running and jumping fatigue expenditure

The hash-identified original run block `005FABB7–005FACC8` requires process
movement flags 0x200 and at least one direction bit 0x0F. It reads current float
Strength and current integer Encumbrance. Capacity helper `00547ED0` stores
`R(Strength * fActorStrengthEncumbranceMult)` and clamps negative results to
zero. Weight is loaded as an exact int32, divided by that capacity, then stored
as float; the ratio is not clamped to one. Helper `00547F40` stores
`R(fFatigueRunBase + ratio * fFatigueRunMult)`. The caller stores the product
with elapsed time, then multiplies/stores the Athletics mastery multiplier.
Base Athletics comes through `005F1910`; tier resolver `0056A300` uses the four
native skill thresholds. The five fatigue multipliers default to 1/.75/.5/.25/0.

The accepted jump block `00672AEF–00672B8F` gets the same integer weight and
float-Strength capacity (through `005E0D20`), stores their ratio, and invokes
`00547F60` for `R(fFatigueJumpBase + ratio * fFatigueJumpMult)`. Base Acrobatics
mastery through `005F23B0`/`005F1910`/`0056A300` selects the Expert multiplier
`fPerkJumpFatigueExpertMult` for tiers Expert and Master. That multiplier defaults
to .5. The compiled jump defaults are 4 and 4; installed Oblivion.esm overrides
06EE20/06EE21 are 30 and 0. Run defaults and installed 0274D9/0274DA are 8 and 0.
Ten setting rows append to the physical input table, bringing it to 180 rows;
capacity and mastery thresholds were already recorded. No new live console
setting probes are claimed for these rows.

Positive costs are negated into expenditure wrapper `005E07D0`. It rejects
nonnegative requests, a false actor virtual +278 result, and current fatigue
at/below zero. It limits a negative request to minus current fatigue before
calling DamageFloat +2A4. This differs from direct fatigue damage, which may
cause negative fatigue. The predicate alone passes 396 original-instruction
cases in both x87 modes, including adjacent-to-zero values.

Zero capacity is a legitimate native arithmetic branch. Zero weight produces
NaN ratio and no positive-cost dispatch. Positive weight produces +Inf ratio;
a zero rate multiplier produces NaN/no dispatch, whereas positive infinite cost
is limited to current fatigue. An infinite run subtotal times Master multiplier
zero also produces NaN/no dispatch. The C++ rule reproduces these transient
results explicitly and returns only a finite nonnegative debit. Nonfinite
external inputs/settings, negative inventory weight and negative elapsed time
are rejected; finite signed setting overrides preserve native arithmetic.

`S2/oracle-emulator/movement-fatigue{,-installed}.py` executes the actual run/
jump arithmetic, capacity/rate helpers, mastery resolver and expenditure wrapper
for 38,880 cases, all bit-matching a separately compiled C++ driver in
`movement-fatigue-comparison-02.json`. A separate two-case int32-weight boundary
executes both x87 modes with weight 16777217 and capacity 16777218, yielding the
float immediately below one. Time/input acceptance, actor AV reads, jump state,
expenditure eligibility and final AV mutation are boundary fixtures/stubs;
these are not normal-input gameplay or animation tests. Initial movement unit
preflight retained one incorrect halfway-rounding expectation; the corrected
non-halfway discriminator agrees with original execution.

The enclosing jump method is `00671620`, found in PlayerCharacter vtable
`00A73A0C` at +228; it accesses the player-specific +7FC/+800 fields. The only
jump-rate helper/GMST consumer is its accepted-jump branch. The live bridge
therefore applies the jump debit to the player, while native nonplayer jumps
bypass the shared TES3 fatigue writer. The running block remains in the common
actor update and precedes regeneration at `005FAD25`.

Expenditure predicate +278 resolves to constant-true `00977C50` for the reviewed
PlayerCharacter/Character vtables. Creature vtable `00A710F4` selects `006250F0`,
which reads byte +104; setter +274 selects `006250E0`. Creature constructor
`00625100` sets that byte to zero. Four constructor initial-byte cases, eight
setter/getter cases and two player/NPC getter cases execute unchanged original
instructions in `S2/oracle-emulator/fatigue-eligibility.py` and `fatigue-eligibility-table.json` (the base
Actor constructor is stubbed). The live bridge uses the native default false
for creatures; full mutable eligibility-flag lifecycle/migration remains open.

Signed-zero follow-up falsified the initial `max(0, capacity)` model: native
`00547ED0` retains -0 because its comparison clamps only values strictly below
zero. Positive weight / -0 yields -Inf; positive jump multipliers then dispatch
no cost, while a negative multiplier can yield a positive capped cost. The
retained failing oracle log is `S3/movement-fatigue-negative-zero-01.log`.
The C++ capacity/zero-division handling now preserves the sign. An additional
23,328 original negative-zero cases join the preceding 38,880 cases, giving
**62,208 bit-exact C++ comparisons** in `movement-fatigue-comparison-03.json`.
Four additional original cases cover a negative capacity setting and both
signed-zero Strength values. Component tests cover both signs and multipliers;
no nonfinite value is published to actor authority.

### Health restoration and Magicka regeneration requests

The same hash-identified 1.2.0416 executable provides Health helper `005F2530`
and Magicka helper `005F25F0`, with rate helper `00548D60`. `R` below means
an explicit float store. Health resolves the eligible Maximum modifier and
integer base, stores `maximum = R(integerBase + modifier)`, compares the
current float, and requests `R(maximum - current)` only when positive.
Its duration argument is unused. It queries current twice on the positive
path; the immutable helper assumes no intervening actor mutation. This is a
request to the Damage channel, not an unconditional assignment to maximum.

Magicka stores the same maximum, floors the stored current float and stores
that integer as float before comparing. It requests only while
`maximum > R(floor(current))`. Positive current integer Stunted Magicka (AV57)
suppresses the rate; zero and negative values do not. Rate is
`R((fMagickaReturnBase + integerWillpower * fMagickaReturnMult) * (maximum / 100))`,
then request is `R(rate * duration)`, dispatched only when positive. There is
no deficit cap. Do not round the Willpower term before multiplication by
maximum. The helper's boolean argument controls its early nonnull
MagicCaster +30 gate. Player vtable `00A739BC` dispatches that getter to
`0066B130` (actor +1E8); Creature vtable `00A710A4` uses `005E5400`, which
queries process +2A8 or returns null without a process. The normal actor
update's inline path `005FAD2A..005FAE3F` performs the same calculation and
always checks this gate. Full spell-pointer lifecycle integration remains open.

Compiled GMST defaults and installed winning records agree: base **.75**
(`Oblivion.esm:037B21`) and Willpower multiplier **.02f** (`037B22`).
Their original storage/initializers are `00B37F30`/`009EEB3F` and
`00B37F38`/`009EEB6F`. These two input-manifest additions are initializer/record
audits, not new live console probes.

`S2/oracle-emulator/restoration.py` executes **2,016 Health /34,992 Magicka**
cases across both x87 control modes. AV reads, caster queries and final mutation
are boundary stubs; the Player maximum-modifier getter, flooring, arithmetic,
branches and positive-dispatch path execute original instructions.
`restoration-compare.py`/`restoration-comparison.json` show **37,008 exact float
bit matches** with the production helpers. Duration zero/one/hour, negative
current/maxima, fractional maxima, equality, actor/no-process paths, integer
Willpower, signed Stunted Magicka and both active-item gates are represented.
Health also covers integer base 16,777,217 without premature float rounding.
The focused C++ tests additionally reject nonfinite/unsupported inputs and
arithmetic overflow; these rejection rules protect state rather than claim
original undefined-overflow behavior.

The hourly player dispatcher `0065F770`, slice `0065F7C3..0065F88D`, first
checks byte `00B14E4C`. With it enabled, effect advancement precedes Health,
Magicka (check gate true), then Fatigue, each with **3,600 seconds**. The
sleep/wait flag does not change these calls. With it disabled, all these
calls are skipped. `rest-hour-dispatch.py` executes **32 original slice cases**,
checking order, arguments and balanced stack with effect/restoration boundaries
stubbed. Inspected jail caller `00670700` sets this byte false before its
hour loop; the hourly completion resets it true. This does not yet verify a
complete jail/effect lifecycle or authorize sharing ordinary restoration with
jail time.

`S2/oracle-emulator/resource-authority.py` additionally executes **3,600**
Health/Magicka helper-to-storage cases through original Player `0065E530` /
`0065D310` scalar modification and nonplayer `005E2BE0` process wrappers
(Low/MiddleLow/MiddleHigh/High) with `0065CA60` sparse modification. Actor reads,
caster state, container helpers and notifications are boundary stubs. The
matrix checks signed/fractional Maximum and Script, absent/zero/negative Damage,
zero/fractional/one/hour duration and both x87 modes. Positive restoration
clamps an existing Damage entry at zero, removes a zero nonplayer sparse entry,
and can create a positive entry when the nonplayer Damage entry was absent.
Player Damage uses fixed scalar clamping. This preserves a surprising repeated
restoration behavior: with Script -1, a first NPC Health restore can remove its
negative Damage, and a second can insert +1 and reach maximum. The authority
reload test explicitly covers that behavior instead of flattening channels.
This sparse-container observation applies to Health and other ordinary sparse
AVs, **not Magicka/Fatigue**. The later complete-container audit in the milestone
report ("permanent nonplayer Magicka/Fatigue modifier slots", commit
`7bafa684c0`) supersedes the helper harness's stubbed container behavior for
AV9/10: constructor `65BE10`, lookup `65C010` and zero-removal `65C9B0` retain
permanent zero slots. `actor-dedicated-modifier-add.py` and its C++ comparison
verify2,312 original cases without game-function stubs. Positive Damage cannot
offset a Script penalty in those slots, even after repeated regeneration.
The first harness attempt failed while extracting a reusable Python prefix;
`S3/authority-draft/resource-authority-oracle-01.log` is retained, followed by
the successful `resource-authority-oracle-02.log`.

### SetAV base storage widths, aliases and shared ownership

Original Player SetInt `0065D1E0` resolves its base through `005E02E0`, calls
base virtual +134, then issues notifications. Common actor SetInt `005E2360`
resolves its base through +170/+190, delegates through process +274 when a
process exists, and otherwise calls the base directly. Low/Middle processes
use `00643480`; High uses `00628A80` and then the same base call. These are
shared base writes, not per-reference Script or Damage modifier replacement.
Creature wrapper `00625460` aliases runtime skills first, including Marksman
28 -> Combat12. Base vtables are TESNPC `00A53DD4` (+134 -> `00523310`) and
TESCreature `00A5324C` (+134 -> `0051D590`), both delegating non-skill fields to
`00519F50`.

The base-record write has these exact storage rules:

- AV0..7 attributes, AV12..32 skills, and AV33..36 AI data keep the low byte,
  without clamping. Creature skills target group12/19/26 after runtime aliases.
- Health AV8 keeps the signed int32 request, including values not exactly
  representable as float (`00519C50`).
- Magicka9/Fatigue10 keep the low unsigned 16 bits (`00467290`/`004672B0`).
- AV11 and AV37..39 perform no base write. Do not interpret that as a command
  with no notifications or process side effects.
- Extra AV40..71 store `R(int32Request)` through sparse setter `0065CB00`.
  Zero removes an existing sparse entry or leaves an absent one absent.

`prepareActorBaseValueSet` returns this typed base change (or no base write).
It retains integer versus stored-float identity and leaves shared ownership,
process caches, notifications and publication to the authority integration.
High-process setters independently store Encumbrance11 as float at +294 and
Paralysis48 as int32 at +298 before forwarding the base call. Therefore the
existing generic scalar state is insufficient to activate those cached queries,
and a rounded per-reference base float cannot be the sole owner of a new
signed-int32 Health base override. Both remain explicit S3 integration work.

`S2/oracle-emulator/base-value-set.py` executes **7,488** direct original
NPC/Creature base-setter paths over all 72 AVs, signed int32 extremes and byte/
word/float boundaries, sparse absence/presence and both x87 modes.
`actor-base-value-set.py` then executes **26,208** complete runtime-to-base
paths: Player, NPC and Creature, with Low/High/no-process nonplayer dispatch.
Original field writes, skill aliases, sparse setter and High cache writes run;
base resolution/dynamic cast, sparse container helpers and notifications are
boundary stubs. `base-setter-compare.py`/`base-setter-comparison.json` compare
all 26,208 typed C++ results exactly (integer values or float bits plus AV key).
The first direct harness incorrectly expected AV11 to reach generic extra
storage; its retained failure led to decoding the jump table and the no-write
cases above. Logs: `S3/authority-draft/base-value-set-oracle-01.log`, corrected
`base-value-set-oracle-02.log` and `actor-base-value-set-oracle-01.log`.

### Script GetAV/GetBaseAV enabled-state dispatch

Against the hash-identified 1.2.0416 executable above, GetActorValue handler
`00501670` calls `004F6060`; GetBaseActorValue `00501A00` calls `004F45D0`.
GetAV tests **reference flags +8 bit 0x800 (Disabled)**: enabled actors use
current-value virtual +288; disabled actors resolve the base through
`005E02E0(false)` and call form virtual +12C. The latter returns an integer
promoted to x87 without a float store for NPC Health. The command writes a
**double**, preserving disabled raw Health 16,777,217. GetBaseAV always uses
`005F1910`, including its float store and floor-to-integer path. Disabled
Creature Marksman consequently reads form Stealth rather than runtime Combat.
Player disabled resources read raw form values, without derived contributions.

The first draft incorrectly called bit 0x800 a death flag. Before committing or
running the engine scenarios, this was corrected by independently executing
GetDisabled `004F60E0` over eight flag combinations; its direct test is 0x800.
GetDead `00502870` is a separate virtual query. GetDisabled additionally calls
an enable-parent predicate, stubbed false in that test; GetAV itself only tests
the direct bit. The runtime adapter reads resident/saved/authored enabled state,
not a death marker, and does not load cells to answer a query.

`S2/oracle-emulator/actor-value-script-queries.py` executes **96** enabled/
disabled, base/current cases with six signed/boundary Health inputs, two modifier
patterns and both x87 modes. Original form/current/base arithmetic and result
stores execute; actor identity, base lookup and sparse lookups are boundary
stubs. `script-disabled-flag.py` independently checks the eight flag cases.
The original 72-name table at `00B0A1A8` supplies **438** exact/lower/upper-case
and rejection comparisons against the C++ name resolver. Logs and retained
initial draft are in `S3/authority-draft/native-query-*`; the corrected oracle
log is `native-query-oracle-02.log`. These establish query dispatch only;
process-cache completeness, command writes and normal-input activation remain
separate gates.

### ModAV/ForceAV wrapper eligibility, conversion and callback boundary

Original Player ScriptInt/Float `0065E300`/`0065E3C0` and DamageInt/Float
`0065E490`/`0065E530` check god-mode byte `00B3BB06` before mutation: negative
inputs to AV8..10 return without writes or callbacks. Common actor wrappers
`005E28F0`/`005E29F0` and `005E2AB0`/`005E2BE0` instead gate negative Fatigue10
through virtual +278. Player ignores that nonplayer fatigue predicate. Creature
runtime aliases occur before the common wrapper, as previously audited.

Mod's int32 argument is stored as float and converted back through the original
rounding helper before storage; Force supplies its already stored float delta.
For typed int32 input, both CPU conversion paths agree. Notably INT_MAX rounds
to 2^31, converts to INT_MIN, and stores -2^31. Eligibility ran on the positive
original argument, so god mode does not suppress that write. A resulting negative
Health delta invokes virtual +3B8 **after** storage. It is a health-reaction
callback, not by itself a death verdict. Script and console choose Script and
Damage channels respectively; SetAV remains a separate ungated base write.

`S2/oracle-emulator/command-modifier-wrappers.py` executes **6,272** Player/common
actor integer/float wrapper cases over script/console channels, god-mode and
fatigue eligibility, seven AV IDs, seven signed/int32 precision boundaries,
both x87 modes and both `00BAABE0` CPU paths. Original eligibility, conversion,
branching and callback dispatch execute. Scalar/sparse/process writes are
captured at their boundaries; notification and health callbacks are stubs.
All 6,272 accepted/suppressed results, exact float bits, channels and Health
callback indicators match optimized C++ `prepareActorValueModifierCommand`
(`command-modifier-comparison.json`). Force-wrapper comparison uses request0
and negated float input as current; the independent 3,408 handler comparisons
above separately cover exact request-minus-current computation. This does not
resolve arbitrary out-of-range float conversion or implement callback effects.

### Negative-Health callback and script GetDead gates

Player vtable `00A73A0C` slot +3B8 points to `0065D6F0`; Character `00A6FC9C`
and Creature `00A710F4` use `006034B0`. The common callback first executes
state query `005E33B0(false)`, which excludes states 1, 2 and 6. Otherwise it
queries current Health and invokes transition `006005F0` only when Health is
**below 1** (constant `00A2F948` is float 1). Player adds a second state query
after the common callback. The script GetDead helper `004F4890`, reached by
`00502870`, instead calls `005E33B0(true)`: states 1/2 return true, essential
state6 returns false. This is independent of the Disabled flag used by GetAV.

`S2/oracle-emulator/health-reaction-gate.py` executes **1,024** original callback
paths across Player/common actor dispatch, states0..7, signed/zero/fractional/
adjacent-one Health values, both x87 modes, null/non-null attribution and two
negative deltas. Original state queries and gating execute; current Health is
a boundary getter and the death/essential transition is captured, not executed.
An additional **16** original GetDead helper paths execute the state query with
only actor-type identity stubbed. Logs: `S3/authority-draft/health-reaction-gate-02.log`;
corpora `health-reaction-gate-table.json` and `script-dead-query-table.json`.
The fork's persisted Alive/Dead/EssentialUnconscious enum describes logical
phases; it does not claim to reproduce all original animation-state numbers.

### Native breath timer and drowning arithmetic

Pinned EXE SHA-256:
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Complete helpers `00548960` and `00548980` use FILD int32 Endurance/base Health,
respectively. Breath multiplies by live `00B37580`, adds `00B37578`, then stores
float. Damage rate multiplies by `00B37588` and stores float; caller `00604657`
through `0060467B` multiplies the rate by frame duration with another float store.
The underlying base query is `005F1910`/`005EAD00`, not current Health or
base-plus-Maximum. The null-source sink `005E58F0` does not apply difficulty
and only forwards positive damage to the float Damage-channel Health writer.

Compiled settings are `fActorSwimBreathBase=10`, `fActorSwimBreathMult=.5`,
`fActorSwimBreathDamage=.2`; winning installed Oblivion.esm overrides Base=4,
Mult=.3, with no Damage override in the full GMST audit. Typed native settings
must resolve the winning values; shared TES3 suffocation damage is unrelated.

`S2/oracle-emulator/drowning-rules.py` executes **11,280** complete helper paths,
including signed integer extremes and both x87 control words. `drowning-timer.py`
executes **1,620** arithmetic/branch paths from `00604619` to `00604644` or
`006046D9`: nonzero integer WaterBreathing adds frame duration, zero subtracts;
the stored result strictly below zero enters drowning. Otherwise the maximum
clamp follows the negative test. The drowning branch later sets the timer to0.
`drowning-frame.py` executes **280** actual caller/helper paths with synthetic
base getter and captured damage sink. C++ comparisons match all **13,180**
rows exactly, with positive-only damage and post-drowning timer-zero transforms
explicitly distinguished from helper output. A separate original -50 Endurance
case returns `-11.000000953674316`, explaining the corrected initial test literal.
Base Health13, duration.3, multiplier.2 produces `.7800000905990601`; combining
rate/frame multiplication without its intermediate store produces a different bit.

These probes establish arithmetic and timer branches. They do not execute the
full water/controller predicates, AI resurfacing, audio, native process save
storage, or ordinary gameplay. Their synthetic boundary declarations, complete
results and comparison binaries remain in the ignored oracle directory.

### Native creature capability and breath eligibility predicates

For the same pinned executable, complete519C70/519CC0/519CE0 queries read Creature
base flags at+28: swim is(Swim0x10|Biped0x01), walk is(Walk0x40|Biped0x01),
fly is0x20. `creature-capabilities.py` executes768 queries across256 patterns.
The actual Creature-class regression checks pure aquatic/flying/land consumers
as well as direct capabilities. Native NPC/Player classes remain bipedal.

`drowning-eligibility.py` executes4,096 original paths604599..6045DF/6045F9,
including real5EA680/5E1E90, base getters and god-mode getter65D820; no function
stubs. Synthetic actor/base/process/global data supplies flags, identity,
movement bits and the deep-water boolean. Pure aquatic creatures consume breath
out of deep water; other actors consume it only in deep water with process
Swimming0x800. Player god mode selects the reset branch. The water-height probe,
physical scheduling, AI and damage sink are outside this eligibility probe.

HighProcess constructor62908F..62909B and reset632F0E..632F1A store constant
00A417B4(float20) at process+238; setter629830/getter629840 own that timer.
Actor-height helper5E0660 subtracts model minZ from maxZ, multiplies actor scale
and storesfloat. Water predicate5E06C0 stores(height*ratio) asfloat, then adds
positionZ into a double temporary before comparing with water height. Actor
update ratios are .01,.7,.875; the latter feeds breath eligibility. Shared TES3
submerged thresholds and its final float position store are not equivalent.

### Native water-plane comparison

For the same pinned 1.2.0416 executable, `005E06C0` stores actor height times
probe ratio to float, adds positionZ without a final float store, and compares
water height strictly greater than that sum. Original callers use .01/.7/.875;
.875 supplies the breath predicate. `actorWaterProbe` retains that ordering.
At position16777216, height2, ratio.875 and water16777218 the result is true;
rounding the final position to float would incorrectly return false.

`S2/oracle-emulator/drowning-water.py` executes **864** complete predicate paths
in both x87 modes, with explicit return-value stubs for height `005E0660` and
cell water `004CACE0`. `compare-water-result.json` compares all864 C++ results
exactly, including the missing-cell guard in the adapter. A retained failed
comparison swapped the table's height/position columns; correcting only that
harness mapping produces the match. Geometry, cell-water lookup and controller
scheduling remain outside this instruction probe. The World adapter maps
height to twice the physics rendering half-extent, uses native .875 arithmetic,
and takes swimming state from the same predicate as the shared character
controller. The deep flooded-room course exercises frame/authority wiring;
normal surface crossing and varied model/controller geometry remain separate
campaign acceptance cases.

### Separate actor-manager restoration dispatch during wait

A direct-call scan followed by aligned disassembly found the same original
Health/Magicka/Fatigue helpers in `00677EC0`'s actor-manager loop. The bounded
slice `00678006..0067804E` skips a null selected actor; otherwise it calls the
real Player predicate `0065D550` (signed remaining-hours at+590 >0), then requests
Health, Magicka with active-item gate true, and Fatigue with **2.0 seconds**
from `00A379B4`. This differs from Player's explicit3,600-second hourly dispatcher.
`S3/authority-draft/rest-npc-dispatch.py` executes20 original cases over null/non-null
actor, remaining-hours -1/0/1/2/24 and both x87 modes, asserting ordered calls,
arguments and balanced stack. Restoration bodies are boundary stubs. Prior
actor eligibility/effect updates and other process tiers are not established by
this slice; it must not be promoted into a universal NPC hourly-rest formula.

An initial vtable inspection used MagicCaster address points for actor virtual
slots; that was an inspection error, not a game discrepancy. The verified
actor tables are Creature00A710F4, Character00A6FC9C and Player00A73A0C, whereas
00A710A4/00A739BC are their MagicCaster tables. Actual actor+1C0 resolves to
00605770; +368 resolves to005FAAE0. The earlier active-item getter findings
remain valid for the MagicCaster subobjects. Original executable SHA is the
same pinned `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.

### Combat query dispatch and original rest menu

For the pinned executable SHA
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`,
command-table entry00B0F5E8 identifies IsInCombat opcode1121, command00505FC0
and condition004F8F30. Both use the same condition helper. It checks actor
virtual+190 then calls+334 with true. Creature/Character actor tables dispatch
+334 to005E6110; Player dispatches to006FE080 (false), but the condition helper
then explicitly replaces the player's result with006605A0(false). Do not infer
player script behavior from its virtual alone.

005E6110 requires a process and process+8 package type0C/0D. Its true argument
adds an actor+380/process+36C gate. The independent ignored
`S3/authority-draft/combat-query-dispatch.py` executes8,192 virtual combinations,
224 command/condition combinations, two null-subject and four Player cases.
All pass. Actor+380 and process+36C returns are boundary inputs; the player's
006605A0 list result is supplied0/1. Actual dispatch, package checks, actor
predicate, Player virtual, output double storage and balanced return stacks run.
Console diagnostics are disabled. This establishes dispatch, not full package /
pursuit/list-maintenance equivalence to the new membership service.

Bounded inspection of006605A0 shows it prunes a Player+5AC list, checks opponent
IsInCombat(false), and has a separate005E6CD0/distance branch (constant1500).
Those paths still need full oracle/runtime coverage before closing native
hostility and pursuit gates. Tables/disassembly remain ignored under
`S3/authority-draft/combat-query-*`; no proprietary bytes are committed.

Original menu audit: `Oblivion - Misc.bsa` SHA
`a84290011a4ed5a1ca8d8cff3822eda67bbacd47c750744b7d786cc0a5527ca5`;
`menus/sleep_wait_menu.xml` SHA
`563583496adc3ca916b1f3dd03481a2e311b417518f752de9ae98d7230348b4b`.
Its two action buttons are sleep/wait and cancel. The only mode switch is
sleeping versus waiting; there is no Until Healed action. Prompt/hour text
comes from menus/strings.xml traits _restquestion/_waitquestion/_hour/_s.
This is structural original-asset evidence, not acceptance of this fork's
currently incomplete rest dialog. Extracted licensed XML stays ignored.

### Expanded rest dispatcher eligibility and simulation clock

`S3/authority-draft/rest-high-eligibility.py` executes5,120 original instruction
cases from `00677F30` through `0067804E`, including early exits at678331/678364.
Actor flags bits21/5/11, process absence and nonzero process tier reject the
special restoration path. With a high-process actor and remaining Player hours
positive, H/M/F each receive2 seconds, even when predicate5F1330 selected the
optimized branch that bypasses the normal actor +1C0 update. Both x87 precisions,
null/present processes, all four tiers, actor/nonactor predicates, both optimizer
branches and auxiliary virtual outcomes are covered; stack balance asserted.
List membership, process-tier getter, distance, actor/special predicates,
auxiliary/normal updates and resource callees are explicit boundary stubs.
This expands eligibility evidence but does not emulate the optimizer itself.

`rest-clock-dispatch.py` executes70 cases over original65F78B..65F7C3 and the
actor elapsed prefix5FAAE0..5FAB4A. The Player hourly caller first advances the
actor-manager clock by float-stored `3600 / TimeScale`. Setter673B10 stores the
new float clock, resets values strictly above100000 to zero, and sanitizes NaN/
nonfinite inputs. Probe inputs are finite: only the TimeScale getter and CRT
NaN/finite predicates are stubbed. At TimeScale30, ordinary elapsed actor time
is120 seconds (at tested clocks0/.1/.3/1/1000), and wraps produce0 at clocks99900/
100000. Actor elapsed outputs are recorded from original instructions; no C++
implementation comparison or low-process gameplay acceptance is claimed.
The normal actor update at605B58..605B81 calls +368 with that manager clock;
therefore the special2-second call cannot represent total NPC rest regeneration.

`S3/authority-draft/rest-clock-special.py` adds54 original-instruction cases:
clock0/1000/99900, TimeScale -3600/-30/-1/0/30/infinities/NaN/minimum positive
float, both x87 precisions. Getter and CRT finite/NaN predicates are explicit
stubs; original hourly caller/setter and actor elapsed prefix execute. Stack
balance and nonnegative elapsed are asserted. Negative, zero and nonfinite or
overflowing increments yield zero actor elapsed for these nonnegative prior
clocks. This is arithmetic dispatch evidence, not actor-update/gameplay coverage.
The hourly adapter handles zero/negative/division overflow; persistent manager
clock wrap and each actor's previous-update cadence remain unimplemented.

### Actor manager clock and common actor elapsed time

Original 1.2.0416 image SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`:
manager getter `00673B00`, setter `00673B10–00673B67`, float storage `00B3BCF0`.
The setter retains finite values <=100000, including negative values and signed
zero. Strictly larger values and nonfinite values become +0. It does not take a
remainder. `manager-clock-setter.py` executes the whole setter and its actual
CRT checks at `00984012`/`00983FFD`, with no boundary stubs:12 bit-pattern inputs,
FPCW027F and037F,24 results.

Player `00A73A0C`, Character `00A6FC9C` and Creature `00A710F4` vtable slot+368
all resolve to `005FAAE0`. Its clock prefix through `005FAB4A` subtracts the
previous actor+BC time and stores elapsed as float. If previous>current or
previous<0, elapsed becomes zero except for current strictly in (0, binary32
0.3), where elapsed becomes current. At ordinary function exit `005FAE7F` the
current argument is stored to actor+BC. The prefix oracle stops before virtual
dispatch; it does not emulate full effects, resource changes or eligibility.
`actor-clock-delta.py` executes81 combinations including adjacent floats around
0.3, reset/negative time, signed zeros and100000, under both x87 controls:
162 results, no instruction replacements. Both controls agree.

The standalone C++ `actor-clock-compare` checks all186 expected bit patterns
against `components/esm4/actorclock.cpp`; all pass. Local scripts, result tables,
bounded disassembly and comparison report are retained under
`build/oblivion-compat/m15/S2/oracle-emulator/`. The comparison consumes recorded
original-instruction outputs, not outputs generated by the C++ rule. Nonfinite
per-actor timestamps are rejected as unsupported state; manager setter behavior
for nonfinite input is modeled explicitly. These scalar rules do not by
themselves establish scheduler cadence, manager advancement or persistence.

### Manager advancement callers and ordinary hourly-rest eligibility

Same hash-pinned original image. Frame arithmetic slice `00678534–0067855B`
stores frame duration as binary32, adds it to the manager clock with a float
store, then executes the actual setter. Hourly slice `0065F78B–0065F7C3`
executes actual TimeScale getter `004029D0`, stores `3600 / TimeScale` as float,
adds it to the manager time with another float store, and executes the setter
and its CRT checks. `manager-clock-advance.py` records120 results: clocks
0/-1/.125/99999/100000, six frame durations and six scales including negative,
zero and division overflow, with FPCW027F/037F. No game instructions are stubbed;
these are arithmetic caller slices, not a complete scheduler. All120 results
match the standalone C++ comparison bit-for-bit. Tables, comparator, bounded
reviews and `manager-clock-advance-comparison.json` remain in ignored evidence.
Actor initialization stores `00A30634` (binary32 BF800000, -1) to actor+BC at
`005E16CE` and `005E197A`; this supports the missing-timestamp initialization.

The hourly branch at `0065F7C3` tests `00B14E4C`, the ordinary-rest suppression
flag described above. It is separate from Player+6E5 (character creation).
Character creation selects direct Player effect dispatch at `0065F83C`; it
still reaches explicit Player Health/Magicka/Fatigue calls at `0065F84C`.
Outside character creation, the NPC manager dispatch is conditional on byte
`00B3BD98`. Native toggle code `00501296` flips that byte and formats the
original string at `00A4B978`, "All AI Processing is  %s". Turning AI off skips
NPC hourly dispatch but still reaches explicit Player restoration. Player's
common +368 update at `0065F8D0` follows the explicit calls even when the
ordinary-rest flag suppressed them. These branches supersede the earlier
adapter's incorrect use of character creation to suppress Player restoration.
The fork's ordinary hourly adapter rejects jail until its dedicated transition
is implemented. Full native scheduling/effects/character-creation gameplay
eligibility remains a separate gate.

### Maximum float wrappers and LowProcess write suppression

Pinned executable SHA256 remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Player Maximum-float virtual +294 is `65E260`; common Character is `5E2800`,
and Creature `625570` aliases skills before that common wrapper. Unlike the
Player Script/Damage wrappers, this Maximum wrapper has no god-mode debit gate.
Common negative Fatigue still checks +278. Negative Health dispatches +3B8 after
the storage boundary, even with no process. The callback itself remains a
separate policy/lifecycle boundary.

Process Maximum setter +278 resolves to `890B90` on Low: its complete body is
RET12. MiddleLow/MiddleHigh use `658850`, which dispatches the sparse Maximum
map. High `628C00` delegates to that setter and also invalidates Encumbrance/
Paralysis caches. Therefore a LowProcess Maximum request must not change retained
storage that could become visible after promotion to an active process tier.

`S2/oracle-emulator/maximum-float-wrappers.py` executes2304 wrapper cases;
`maximum-float-process-wrappers.py` expands this to5760 cases across Player,
Character and Creature, absent/Low/middle/High processes, god mode, Fatigue
eligibility, signed/fractional/zero inputs and both x87 control words. The
executable hash is checked before mapping. Actual Low/middle/High dispatch,
wrapper branches and callback selection execute. Scalar/sparse storage,
actor-base lookup, notifications and Health reaction effects are explicit
boundary fixtures. These are branch/dispatch probes, not gameplay acceptance
or a5760-row production comparison. Tables and logs remain ignored under
S2/oracle-emulator and S3/authority-draft; independently read targets are recorded
in `process-maximum-write-vtables.json`.

The service now suppresses Maximum writes for nonplayer Low-process actors.
Updated actual-Ptr tests reproduce the former latent write, retain existing
Maximum values across ignored set/clear requests and verify activation later
exposes only those retained values. Creature resource/attribute probes include
a finite delta that would overflow active arithmetic, plus invalid-input
rejection and unchanged lifecycle authority. Lua resource Maximum adapters and
their Health reaction/god-mode policy remain open.

### Float base setter conversion before native field widths

For the pinned executable above, Player actor setter `65D220`, common setter
`5E2430` and Creature alias wrapper `6254B0` dispatch the shared form float
setter `51E7B0` (+130). That setter calls actual `9828C0`, then the native form
integer setter (+134). Fractional input therefore truncates before byte/uint16/
int32/extra-value storage; the form float path does not preserve fractional
extra values. Process float setter +270 is `6434A0` on Low/middle and `628AC0`
on High. High preserves raw float Encumbrance in its process cache, but floors
Paralysis for its integer cache separately from the form's truncation.

`S2/oracle-emulator/actor-base-float-value-set.py` records72576 original cases:
Player, NPC/Creature absent/Low/High processes; all72 AVs; signed fractional,
zero, width and binary32 precision boundaries; present/absent extra entries;
both x87 words and both BAABE0 conversion branches. The domain is finite
binary32 inputs whose truncated integer fits int32. Arbitrary outside-domain
CPU-dependent conversion remains open. Original wrappers, conversion, form
setters, aliases and High cache writes execute. Dynamic/base lookup, change
flags and sparse allocation/container operations are boundary fixtures.

A standalone C++ driver composes existing `actorBaseValueInteger` and
`prepareActorBaseValueSet`; all72576 canonical field IDs, storage types and
value bits match (`base-float-setter-comparison.json`, table SHA256
`e71efb5a0a5693b3f42d5feb88a2e3c1e368cb0fed311ec68d74b438a32d017f`).
This comparison covers base fields, not High cache effects or Lua/world adapters.
The first probe failed before execution because its bit encoder was undefined.
The second incorrectly expected truncation for High's Paralysis cache;
instructions and the corrected probe establish floor. Both failed scripts/logs
are retained. The final metadata-complete run is
`S3/authority-draft/base-float-value-set-oracle-04.log`. The first standalone
link omitted unrelated modifier dependencies; section garbage collection in
`base-float-driver-build-02.log` links only the actual field helpers used.

### Complete finite float base conversion in both CPU modes

A further independent probe extends the preceding bounded conversion domain.
`base-float-conversion-domain.py` executes the complete shared form float setter
`51E7B0` and converter `9828C0`, capturing only the downstream integer-setter
argument. All7088 cases cover1772 finite binary32 inputs across both BAABE0 CPU
branches and both x87 words: signed subnormals/zero, adjacent powers through
2^127, int32/int64 thresholds, maximum finite magnitudes and seeded samples.
SSE yields INT_MIN outside int32. Non-SSE truncates through FISTP int64 and
returns the low32 bits; its int64 overflow sentinel consequently returns0.
Original code executes without a conversion stub. Table SHA256 is
`760f74dbf800627b396a236d7bd9cf933afd48186c188f6dd54e20f108a097c7`.

Production `convertActorBaseFloat` now models both finite domains with an
explicit `ActorValueConversionMode` argument. `prepareActorBaseValueFloatSet`
then applies existing native field widths and aliases. Both reject nonfinite
inputs or invalid mode. No implicit host-dependent/default CPU mode is selected;
world/Lua integration must choose its policy explicitly. Existing strict base
integer queries retain their previous supported-domain contract.
`base-float-mode-comparison.json` matches all7088 conversion bits and all72576
recorded typed base fields against the new helpers. The full component tests
also exercise overflow sentinels, sign/fractional/adjacent boundaries, wrapping,
creature aliases and malformed inputs. This closes the finite scalar conversion
rule, not base-write adapters or gameplay acceptance.

### Native float conversion mode selection

`S2/oracle-emulator/base-conversion-mode-selection.py` executes initializer
`99CB37` and capability check `99CAD7` in the same hash-pinned original image.
All20 cases pass across SSE/SSE2 flags, initial EFLAGS ID states and declared
OS support. The original stores SSE mode in BAABE0 only when CPUID reports
SSE2 and the OS probe succeeds. CPUID outputs are supplied at the original
instructions; OS probe `99CA87` is a boolean boundary stub, so this does not
observe an actual Windows/Wine process mode. The new Lua/world attribute/skill
base adapter explicitly selects this modern SSE reference policy rather than
using an implicit C++ overflow cast. Actual world publication/restart acceptance
remains open.
### Fresh nonplayer construction inputs and signed form Health

The pinned original executable remains SHA-256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
`S3/authority-draft/initial-native-values-03` executes828 cases across x87
control words027F/037F and both BAABE0 modes. Actual ActorValues constructor
65BE10 and lookup65C010 prove fresh permanent AV9/10 zero nodes and absence
for other indices. NPC form getter5232D0, common519D90, sparse65CB80 and
conversion9828C0 prove zero untouched form AV37..71 and truncation of96
controlled stored extra values per mode pair. Original getter paths map
asymmetric AI object bytes6C..6F to AV33..36. Only allocation is hooked,
supplying fresh poisoned memory. The form container is initialized directly;
full actor/form construction, racial abilities and active effects are excluded.
The first probe incorrectly reused additive modifier writes as assignments;
its failing log is retained before fresh-container correction.

`initial-health-query-05` records3120 original signed Health/current/base
query cases:13 int32 boundary inputs, four process/no-process paths, five
modifier patterns, both x87 words and CPU modes, three queries. Actor base
identity/type and sparse modifier returns are boundary fixtures. Actual NPC
form field/accessors, current float/integer and base integer wrappers, process
dispatch and conversions execute. Current form composition must retain the
signed integer before Script/Damage and the first float store. Base-integer
5F1910 instead stores the form float first: INT_MAX becomes2^31 and the base
query returns INT_MIN in both CPU modes. This is not the float setter's
CPU-dependent overflow behavior. A no-process current-float query at INT_MAX
also returns float(INT_MIN); no-process is not represented by the current
Low/active authority pair and is excluded from the production comparison.

Optimized production actor-value helpers match780 float-current,768 supported
integer-current and1040 base-integer outputs exactly. Twelve out-of-int32
integer-current cases are recorded but excluded from the strict supported
composition domain. Table SHA-256:
`0006b1d9741071bacd12f37b0da48805fd8ca4cc12a65ee16e9a46e7202a05de`.
The comparison records driver/source hashes and excludes no-process current
explicitly. Failed mixed-stop emulator setup and incorrect CPU/no-process
expectations remain in attempts01..04; the first standalone comparison link
also remains retained before unused-section removal.

Runtime-state v17 preserves optional signed nonplayer form Health separately
from its rounded float view. Legacy absence is preserved; it is not replaced
using today's winning records. Native current float/integer and disabled form
queries use that exact input, and typed shared-base writes refresh it when
present. Base queries/shared maxima retain the separately proven float-store
boundary. The fresh value resolver uses previously audited winning-record
manual/auto/scaled stats and the verified initial slot/AI rules. Full actor
activation and world restoration remain later integration work.

Production C++ serialization/canonical JSON and Python decoding/re-encoding
match56 exact v16/v17 payloads with signed extrema, both process tiers and
modifier presence under `raw-health-cross-codec-01`. This is codec evidence,
not engine fresh-process gameplay acceptance. Regular and sanitizer component
runs pass1948 cases; corrected engine runs pass644 cases, with191 Python
cases also passing. The milestone report identifies fingerprints, fixture
failures, sanitizer scope and the current Git/runtime sandbox limitations.

### Authored death inventory and common DATA Health decoder

The independent `S3/authority-draft/starts-dead-record-audit-01` checks the
installed eleven official plugin hashes against the S0 inputs and audits
winning NPC/creature base header bit19 (0x80000), using pinned xEdit TES4
record definitions at commit9fb016884bec138ea6c7b872cec831537d464c3e.
It finds158 flagged bases (109NPC,49creature), including six with positive
raw authored Health. This is a content-input inventory, not native initial
life/resource/event behavior. Reference bit9 is not this flag. Report SHA256:
`aa34a191a0bda0fd379a0968a91972569da30f998efa570ae619255c59c6c1cd`.

`S3/authority-draft/npc-health-record-load-01` independently executes original
common DATA decoder0046BDA0 for32 cases: eight four-byte Health patterns,
zero/21-byte prefix offsets, and x87 control words027F/037F. Original RTTI
name at00B05CF4 identifies TESHealthForm;0046BEF5..0046BF04 reads/stores a
full dword at the component+4, preserving signed/high-word patterns. The
reader and RTTI identities are fixtures; memcpy/security checks are stubs;
original stack allocation and decoder branches execute. This confirms the
common decoder width, not full NPC loading or fresh-life initialization.
Pinned EXE SHA256 remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`;
report SHA256 is
`9ec3770d176b8b59ba74cc0fe9098fc3a7819cf424a25b59ba2d9b89433494c8`.

### AI current queries and the guarded integer projection

`S3/authority-draft/ai-current-queries-01` executes1008 pinned original queries:
four asymmetric AI bytes (AV33 Aggression,34 Confidence,35 Energy,
36 Responsibility), Low/Middle/High processes, both x87 control words and
BAABE0 modes, seven signed/fractional modifier patterns, and current float,
current integer and base integer wrappers. Actor base identity/type and sparse
lookups are fixtures; original NPC/common form byte reads, process dispatch,
composition and conversion execute. Every result matches the independent
expectation. The table SHA256 is
`51a9f46179034ed275a0312d06deb99358bee2ebea24519a08c0eec31dbc2590`.
A hash-checked existing optimized component driver matches336 cases of each
query (1008 total). A newly compiled optimized Stat<int> adapter matches all336
integer cases for capped/uncapped/unchanged-preview reads and672 write-guard
checks. The first standalone link failure omitted actorstats.cpp; its log is
retained. No game AI decisions or normal-input acceptance are claimed.

`S3/authority-draft/initial-life-health-predicate-01` executes224 cases of
004D7DD0: actor-kind gate, Player reference id7/other, seven raw Health dword
patterns, base header bit19 clear/set, both x87 words and both boolean arguments.
Actual true-kind virtual977C50, base getter4D9B40, component getter and unsigned
int32-to-x87 conversion execute; false-kind dispatch and Health RTTI identity
are fixtures. The predicate returns true only for a supported nonplayer id
with raw unsigned Health zero. Header bit19 and the argument do not affect it.
Caller004DFA50..004DFA73 can then set state2, but full factory/load dispatch and
autocalculation order remain unproved; this is not a new fresh-life default.
Report SHA256:
`e9da38b7619722df49e928b93f195dd69e7e7117875fbb8167744af67dbbeaa5`.
Both probes use the pinned original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.

### Recoverable ability writer selection for the Player

For original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`,
RTTI navigation identifies the candidate ValueModifierEffect vtable at `A76D34`.
Navigation alone is not semantic proof. The reviewed mutation dispatcher
`6A84D0` executes the actual Player predicate `5E04E0`, tests spell type4 and
recoverable-effect flag2, then invokes Player virtual +2AC for that combination.
Other actors or other spell types with that flag use +294. The Player vtable
`A73A0C` resolves +2AC to `65D1A0`, which invokes base-form float ModAV (+138),
then notifies/updates the Player. Native NPC/creature form +138 resolves to
`51E810`: query the base float (+12C), add the float delta with a float store,
and dispatch the float setter (+130). Thus permanent Player ability admission
must not be implemented as an ordinary Maximum-modifier write.

`S3/authority-draft/recoverable-effect-writer-dispatch-01` independently executes
864 original dispatch cases: absent/Player/NPC target, spell types0/2/4, eight
actor values, six signed/fractional/zero/width-boundary deltas and both x87 words.
Writer selection and actual Player identity execute. Actor writer callbacks are
boundary fixtures; no base field mutation, magnitude construction, resistance,
effect admission/removal, events or gameplay execute. Report SHA256:
`0154cd23d1f5099420685406f9147b6cc3c3057c851595cda2b946e8cf106ff9`.
The wrapper/getter/add/store/setter chain above is static reviewed evidence,
not a claim that all its conversions have been compared in this new probe.

The independent original-master ability-record audit is
`S3/authority-draft/player-ability-record-audit-01`, report SHA256
`e2070f4dc5c20d09255083c377b6e155ae5da20cb408d1db1109ce14542dad51`.
The companion effect-definition byte audit finds145 MGEF records,14464-byte
DATA fields and one36-byte field (DARK). All have nonzero form IDs. It decodes
only offsets0/8/16; interpreting their flags/AV choices still requires reviewed
loader/getter paths. Its report SHA256 is
`6dee6cb9ef05abbb7d53d57e575a94f238f93da41af8ada99317ddc13d2c2e11`.
These standalone-master inventories do not establish winning overrides, full
active-effect semantics, abilities during normal startup or M15 acceptance.

`value-modifier-effect-construction-01` executes2880 original `6A82F0`
constructors, including common `68D7A0` and actual magnitude/duration getters.
The MagicItem spell-type virtual returns4 at a boundary. The constructor chooses
setting data when flag0x01000000 is set, otherwise the EFIT actor value; flag0x100
sets magnitude1, and flag0x80 sets duration0. Otherwise raw quantity bits undergo
signed-int32-to-float conversion. Poison guards, bit boundaries and both x87
words are covered. No effects apply. Report SHA256:
`6142cd9df5b5f2bd52caa278826b51609b140d9b13dd7488fb6890993539315a`.

The default-effect initializer `417420` produces161 argument sets, independently
captured at the allocation/registration boundary `417220` without allocating
effects. All145 original-master MGEF codes appear in this compiled inventory.
Arguments carry code, flags, data, school, base cost, resistance and counters;
ready-setting construction is not inferred from capture alone. Report
`compiled-effect-default-arguments-01` SHA256:
`74e29aba76f4901ada4cdd2e2957eea6c2e44b23f71be53eb1400df16088aae7`.

Crucially, original MGEF loading is not a direct replacement of all compiled
flags with authored DATA. `41617B..416229` resets/reads the64-byte block (including
short36-byte input), merges editable bits using the original flag tables,
retains static ActorValue data when the previous flag0x01000000 was set, and
clears bit0x00200000. Editable mask is0x0FE03C00. The native flag tables and merge
execute in768 cases against initial/file flag patterns, both lengths, three
poison patterns and both x87 words. Only file read `450C20` is a boundary;
memset/reset and complete flag/data reconciliation execute. Report
`effect-definition-load-merge-01` SHA256:
`0409cfa325ff5adf8b5f0f40b9ff8f7d28d54307f43729c46055707b0ded5dbb`.
Setting constructors, EDID selection, remapping and effect execution remain
outside that probe. A future typed MGEF parser must distinguish authored fields
from these ready flags/data before Player ability admission.


### Player character-generation base calculation and permanent float writer

Pinned executable SHA256 remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Original form calculation `005222D0` identifies the Player by full form ID7.
The Player calculates Personality from race attributes; NPC Personality keeps
its authored byte. The temporary class comparison uses winning
`iClassCharactergenClass` (compiled143590, storage`00B37D00`). A match skips
favored-attribute/level-growth and major/specialization bonuses, while ordered
signed racial skill bonuses still run. Other classes retain the original
first-favored-only choice, skill membership, float-store boundaries, upper100
cap, nearest-even conversion and wrapping byte storage. This character-base
calculation does not establish progression, ability application or selection
transaction ordering.

`authority-draft/player-character-base-calculation-01` retains a missing-import
setup failure. Corrected02 executes256 cases;03 adds eight curated duplicate,
negative and rounding cases, totaling264 across both sexes, both x87 words,
levels1/2/5/51 and both class branches. Native race/class/SKIL readers, loops,
byte setters and dynamic calculations execute. The argument suppresses resource
writes; execution stops before class-service assignment at`00522706`.
Notification/RTTI are declared boundaries. Actual production C++ compares all
264 Player and384 previously captured NPC attribute/skill cases in
`native-player-character-base-comparison-01`.

Original Player float base writer`0065D1A0` dispatches form virtual+138 to
`0051E810`. The form float getter`0051E790` leaves an exact int32 in x87; the
float delta is added before a binary32 store, followed by float setter
`0051E7B0` and original CPU conversion`009828C0`. Converting the initial int32
to float prematurely changes boundaries such as16777217+.5.
`authority-draft/player-base-float-modifier-01` executes4800 cases across twelve
AVs, integer precision/signed boundaries, fractional deltas, both x87 words and
both CPU branches. Base resolution, integer getter/setter and the two
notifications are boundary stubs. Width/sparse storage and effect clamp,
application/removal are outside that probe. The production comparison uses
Health dword output to observe all conversion bits and matches all4800 cases;
separate component cases cover native byte/word/extra storage, ignored AVs and
Creature aliases. Evidence is under`build/oblivion-compat/m15/S3/`.


### Compiled passive factory, native clamps and removal compensation

The compiled racial/birthsign VMOD subset is versioned as editable facts in
`M15-PASSIVE-ABILITY-DEFAULTS.json`, pinned to the original executable and
initializer-argument report. It contains14 codes, flags/data and resistance
inputs; production exposes only flags/data at this point. Other codes remain
unadmitted by that subset rather than inheriting a guessed generic policy.

Original custom-factory registration is`0068DA10`;23 identified initializer
routines pass code/factory arguments. Their original calls execute in
`authority-draft/effect-factory-registration-arguments-01`, with registry mutation
as a boundary. None of the14 selected codes is among those23 custom keys.
`passive-value-modifier-factory-02` executes the full factory`0068EA50`, native
VMOD constructor/common constructor and original quantity getters in336 cases
across14 compiled definitions, success/allocation-failure, magnitudes0/50/UINT_MAX,
item AV0/9 and both x87 words. Successful objects have VMOD vtable`00A76D34`;
failed allocations return null and leave the guarded object unchanged. Registry
lookup uses captured compiled keys at a boundary; allocation and MagicItem type4
getter are boundaries. DLL/indirect registration and actual gameplay admission
are outside this probe. Attempt01 is retained;02 also explicitly checks every
draft input against the captured original initializer facts.

Original clamp`006A8220` queries current only for AV0–32 except Fatigue10,
code other thanABHE, and delta<=0. It stores current+delta as float and, when
negative, returns delta minus that stored sum with a float store. Zero magnitude
can therefore produce a positive correction for a negative current attribute;
Fatigue, extra AVs, absorb Health and positive deltas bypass the query.
`value-modifier-clamp-01` executes15552 cases: all72 AVs, FOAT/ABHE, deltas
-50/-.5/-0/+0/.5/50, nine signed/fractional/zero/subnormal current values and
both x87 words. The VMOD AV getter executes; actor resolution/current are
boundaries. Exact float bits and query presence match the independently declared
expectations. Production rejects invalid AV/nonfinite required inputs and
nonfinite intermediate arithmetic; those are explicit supported-domain checks.

Original full apply`006A86F0` and remove`006A88D0` run in1440 cases using14
compiled definitions and FOAT Strength/Endurance, magnitudes0/25/50, apply and
remove current-5/0/10/100, and both x87 words. Constructor, detrimental sign,
clamp, Player identity, recoverable writer selection and UI exclusion execute.
Actor current, target resolution and writer storage/notifications are boundaries;
post-Endurance Health is10 and the actor terminal-state predicate is false. Apply stores the
clamped signed magnitude. On positive stored magnitude, removal issues an
initial Damage write of R(clamp(-magnitude)+magnitude), **including zero writes**,
then always dispatches the base inverse-magnitude. It does not substitute the
clamped inverse for the base subtraction. Nonpositive magnitude omits initial
Damage. Health-specific compensation and terminal-state Health cleanup are not
proved by these cases. Attempt02 records/checks exact bits including signed zero;
attempt01 retains the earlier numeric comparison.

`native-passive-effect-primitives-comparison-01` compiles actual production C++
and matches all14 compiled inputs,15552 clamp/query cases and1440 magnitude/
initial-removal-compensation/base-inverse cases. These pure preparations do not
apply fields, manage ability ownership, resist effects or prove runtime/restart
acceptance. Evidence is under`build/oblivion-compat/m15/S3/`.

Original probe report SHA256 values:

- `effect-factory-registration-arguments-01`: `bc2447bcb1d54e9dfa434af31b01ce679d82c2420c1b0566c75488826bfcc9ba`.
- `passive-value-modifier-factory-02`: `9be106ce4994c2a4e289a087146649660c614121e640a4abd8fef7453f7c0e71`.
- `passive-value-modifier-apply-remove-02`: `a244aa7feffc6822706c662f341caaeae07adf19aa436c19911947c9ea98ec67`.
- `value-modifier-clamp-01`: `906af975e1b09cf297266ddc18c7ade02814ef7d5c2da7ca41473ddd7120b47c`.

### Passive grant identity and resistance admission

Two additional isolated original-instruction probes pin the same 1.2.0416
executable SHA `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Neither imports production code or establishes live gameplay acceptance.

`S3/authority-draft/passive-grant-dispatch-01` executes240 cases through
Actor5E0990, Player6646D0 and the complete6A1DF0 active-item list scan.
Actual actor base/type virtuals execute. Base spell-list insertion, change
marking, MagicItem type, target list getter, the two caster virtuals and Player
notification are boundaries. Across six spell types, added/not-added base-list
results, five active-list fixtures, two owners and both x87 words, Ability4 and
Disease1 dispatch when no matching active MagicItem exists. A matching item
suppresses dispatch even when effect flags are set. A spell already in the base
list can still dispatch missing active effects and report success. This proves
the separate ownership layers, not caster execution/application or persistence.
Probe SHA `d0fbb9db44f525cfbafc24117bc936c16258e1327c3ce87e78af328d396e4ec3`;
report SHA `4770cad864510c41ca6b5b2324bc3d5155008bdfd124dc38d00e38a202c17245`.

`S3/authority-draft/passive-resistance-admission-01` executes108 cases through
6A27F0..6A293D with a non-SEFF definition. It crosses nine MagicItem types,
Player/nonplayer target identity, three ranges and both x87 words. Parent/type
getters, record marking and a supplied resistance factor are boundaries.
Ability4 and Disease1 bypass the resistance virtual and store factor1;
Player self spells of types0/2/3 also take their explicit bypass. Other cases
query the supplied resistance and store.25. The probe stops before god-mode,
replacement, active-effect insertion/application and save handling. It does
not prove general spell resistance or the subsequent admission path.
Probe SHA `8714a0b0d1a09ed9264d69585e03e664f87be201f5ede14292e5b88c746153e6`;
report SHA `bc7c8b157b55554d0902030a49a11757cf3fa2af9899b9dbe9640e548f9d45af`.

### Passive Endurance application and terminal-state removal cleanup

The original executable identity remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
`passive-endurance-health-reaction-07` executes560 original constructor, clamp,
application and actual caster-parent resolution cases. Current Endurance
includes negative/zero values, magnitude includes signed inputs, and Health
crosses negative, zero, adjacent floats around1 and positive values, with
nullable caster and both x87 words. After the recoverable base write, the
original calls the direct terminal/essential entry6005F0 only when the stored
clamped magnitude is strictly negative and current Health is <=1. Actual
self-caster resolution supplies the actor as killer; null caster supplies null.
Current/base storage, target resolution and terminal entry are declared
boundaries. This probe does not execute physical death or a gameplay course.
Attempt02 falsified the positive-magnitude assumption;04 falsified a strict
Health<1 assumption. Attempts01/03 retain harness failures. Successful05–07
expand the domain without overwriting those failures.

`passive-removal-life-cleanup-01` executes1024 full original6A88D0 cases,
including actual5E33B0(false) virtual dispatch across eight native life states,
four stored magnitudes, four current Endurance values, four integer base
Health values and both x87 words. Following the base inverse, states1/2/6
with positive integer base Health issue DamageHealth=-R(baseHealth). The
predicate is dead/essential-unconscious state, not the essential form flag.
The earlier primitive-probe prose incorrectly named this an essential
predicate; that label is corrected above. Alive Endurance removal has no
direct terminal callback on this path. Getters, storage writers and target
resolution are boundaries; Health AV8-specific removal is outside the admitted
runtime subset. Neither probe proves frame lifecycle, god-mode integration
or live gameplay acceptance.

Probe/report SHA256 pairs:

- `passive-endurance-health-reaction-07`: probe `c00bdea006652f38612c964906e88bb93ab5accd6a1fb98e5c46b9a6dbaa3323`; report `c1184455b3f091feccedad9a54bd585d688af85da24cc8d73208785bc61776ce`.
- `passive-removal-life-cleanup-01`: probe `68f7d951a980728397aff4584ca2e9f6415e81f527218544c89dccb5310bd3be`; report `07b2e598b5e05d64b44edfaf70d18dbac33cdf8d07641ad36ea7189a5541b4ad`.

### Original passive removal-mark traversal

`passive-removal-traversal-02` executes4608 original6A1F70 and68EA10 cases through
four-node linked lists, all24 permutations, disabled masks, two spell
identities, nullable/two caster filters and both x87 words. With no EffectItem
filter, it marks matching undisabled effects in forward linked-list order.
Original68EA10 sets the disabled flag before the immediate zero-duration
update. The target list getter and68E670 update are boundaries; this does not
prove insertion sorting, update/application storage or physical removal.
Attempt01 retains a harness failure caused by placing the list head at the
emulated stack address;02 uses separate guarded regions. The executable
identity remains the pinned1.2.0416 SHA above. Probe SHA `d7e71a1cf9b74a3f33cb35d5e4bb772ccab6d9e4063cc61f75ca649840111ed7`;
report SHA `4a9ececa447b798d9b78648876d96f5d94c4a53458c0db9285bbbcb6ebe3a2e0`. No live gameplay acceptance is claimed.

### Actual master passive constructor inputs and production admission

`passive-master-constructor-inputs-01` executes66 cases through original
DATA merge41617B..416229 and complete VMOD/common constructor6A82F0, using
the independently audited33 EFITs in23 master racial/birthsign Ability4 spells,
with both x87 words. Compiled definition flags/data come from the independently
captured initializer arguments. The source master SHA is
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`;
the original executable SHA remains pinned above. The file read supplies the
actual audited flags/data and payload length; unrelated payload fields are
zeroed and are not read by this quantity path. MagicItem type4 is a declared
boundary. Probe SHA `4e448c71bdbae3c7626b1ddf0d999628214e6e7ac7c69b6cff76aaf3fab8e405`;
report SHA `b85e4e665dda71a690dc043e1d333fe2d2829282e0a3a5d50d69663ccbda9e3d`.

`native-passive-winning-records-02` compiles an actual production-store adapter
driver against the configured normal engine libraries, reads the complete
master and collects all racial/birthsign/Player NPC sources. All23 admitted
abilities,33 effects and14 codes match the original observations: stable
selected identity, EFIT index/order, prepared flags, actor value and magnitude/
duration float bits. Its verification records executable/library-linked
driver identity, input-log hash, master identity and the tested source
fingerprint. Attempt01 retains a driver compile failure from missing complete
record includes. These are record preparation/admission checks, not proof of
source-grant ordering, resistance/application lifecycle or live gameplay.

### Winning selected Player declaration graph

`native-player-spell-sources-master-01` reads the complete original master
through production ESMStore and resolves all210 race/birthsign selections:
15 RACE records crossed with13 BSGN records plus no sign. Independently parsed
master record bytes supply Player form7's two SPLOs, all RACE/BSGN SPLO lists
and SPEL SPIT types. Exact selected declarations, stable identities, source
de-duplication and Ability4 selection agree. The driver admits33 effect inputs
across the union of23 passive abilities, already checked against66 original
constructor cases above.

Master SHA `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`;
linked driver SHA `9095f6aace4955e0d26f6030f78ca3ac6002858756bcb2c93c8e56579b266e7e`;
input log SHA `27b38a7785e6aad9abd8c03e9d1fa77f8cb67778a43eba14bc715c692b380c78`.
This verifies the source collector's stated declaration contract; it does not
assert original source-grant or active-list order, execute ordinary spells or
powers, or establish normal-input gameplay/restart acceptance.

### Original passive-list comparison operands and ASCII case folding

`S3/authority-draft/passive-list-comparison-04` executes full original6A25E0,
the412F20 school getter,9836C9 multibyte comparison wrapper and its982525 ASCII
case-fold loop, plus9811E2 stack-cookie verification. All10976 cases agree
across seven school values, independent ignore-duration/ignore-magnitude flags,
signed integer magnitudes, integer durations, mixed-case/different/truncated
ASCII names and both x87 precision words. The original image hash is
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`. Probe SHA256:
`12d0dfe080fc02080cc46aa898968bcd5643519759114d3cb688dd57ac671467`; report SHA256:
`917e31974c818d11d6b1c3217245b29231afbcfec10d872a9a00102478d505d5`.

The raw format is `%c%.30s%.1f%.1f`. Actual argument placement establishes
**magnitude before duration**, correcting the earlier unexecuted inference.
The comparison is **case insensitive for ASCII**, correcting the presumed
strcmp label. School values0..5 map to C/E/A/D/F/B, other values to Z. Each
ignored quantity uses the actual1000.f literal. School's script-effect pointer
is EffectItem+18; otherwise its setting+64 field is read. This does not change
the earlier original forward removal-traversal result.

Effect-name retrieval, free, explicit C-locale construction and sprintf are
boundaries. The formatting boundary admits exact integer quantities and ASCII
names only, so decimal rounding, non-ASCII locale/encoding and original name
allocation are not established. Attempts01..03 faulted in uninitialized CRT
thread/locale retrieval; attempt04 uses a declared C-locale fixture and executes
the real ASCII comparison rather than replacing it with a host comparison.
Sorted insertion, source grant order, full caster behavior and gameplay
acceptance still require their own evidence.

### Original sorted insertion and actual CRT formatting

`S3/authority-draft/passive-list-insertion-01`: Full416650 sorted insertion,446CB0 head insertion and6A25E0 comparator execute. All24 input permutations in five synthetic profiles and both x87 modes pass960 insertions. Equal keys insert before; comparison is against the then-current stored quantities, so applying a sign change after insertion can leave a list that differs from sorting its final keys. Allocations, C-locale construction, integer formatting and effect names are declared boundaries; quantity changes are an explicit application boundary. Source/caster grant order and gameplay are not established.
Original executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Probe SHA256 `b719790ac7bb132928b0830e8a2b300c935b2f9de76b641c3a5e614343d4eeae`;
report SHA256 `a14099acbf43514cd0884ecce8dcf19735f2a8abafc03c15b10e96e6718a6147`.

`S3/authority-draft/passive-native-format-04`: Actual982837 initializes the original CRT floating-conversion function table before full98208B sprintf executes, including original floating conversion. The thread getter supplies a fixture with the original image C-locale globals; pointer decoding returns the original unencoded image code pointer. No numeric/string formatting routine is replaced. Nineteen observations include signed zero, quarter/half-decimal boundaries, binary32 tenths,2^24,2^32,1e16/1e20 and signed maximum finite binary32. Native0.25 formats0.3; negative0.25 formats-0.3. Maximum float formats340282346638528860000000000000000000000.0, so exact fixed decimal output from a host formatter is not interchangeable. Broad generated comparison, non-ASCII locale behavior and gameplay still require their own evidence.
Original executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Probe SHA256 `d607b2d746a883a2fa11a1749148076cfca1baae4b7b9484e519fcea7eef88cc`;
report SHA256 `3c0722358ccb5b6f74285d1adebab0297067cf0a6ae229a876c70c0c9a316f82`.

Formatter attempts01/02 lacked initialized OS pointer decoding and then the CRT floating-function table, respectively. They are harness failures, not observed game-rule differences. Attempt03 executes the actual floating initializer and twelve formatting observations pass; attempt04 extends that successful path to nineteen.

### Temporary chargen class: fork content identity (checkpoint45)

The previously audited native iClassCharactergenClass value143590 (0x230e6)
and original favored-attribute/major/specialization suppression remain unchanged.
The fork additionally loads builtin.omwscripts before Oblivion.esm. Consequently
native runtime plugin ordinal0 must resolve to fork content slot1, preserving
both local ID and complete loaded-file identity. ESMStore records TES4 content
indices in native file order and resolves the integer GMST through that table;
this is not a disk record's master-relative FormID transformation. Fixtures
cover a second native file and same-local-ID distinct classes. Independent
installed-master driver outputs across all420 choices are equal with and without
the script prefix; actual rendered fresh/load observations now agree at
Health80/Magicka80/Fatigue140. Evidence is under
S3/native-class-content-index-* and S3/native-actor-activation-offscreen-03.
No additional original-executable formula claim is inferred from this adapter fix.

### Native melee acquisition selection (checkpoint46 investigation)

Full original6156C0 acquisition executes432 observations in both x87 precision
words. A present selected combat target is tested exclusively: facing and
inclusive reach gate it, with no fallback when it fails. Without a selected
target, eligible resident nondead candidates inside inclusive reach are ranked
by smallest supplied facing angle; equal angles replace the previous result.
This corrects a potential nearest-distance/TES3 target-list substitution.
The same6131D0 facing predicate audited above is called in attack acquisition
as well as blocking. Distance, facing/bearing geometry, combat-target getter,
actor flags/residency, manager lists/RTTI and final Player check are supplied
boundaries. Geometry, LOS, physical contact and normal gameplay remain unproved.
Original executable SHA256:
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Evidence: S4/melee-acquisition-instructions-01. Selection probe SHA256
`f82e0f93c92304a126ec29de8ac828faba145d4dab646072b5c77d475a1ce37a`; report SHA256
`2b37cc6b6d54b7e52537715682f66f62ddda7e0c4c04927e7f686baaf825ab48`. Bounded original geometry instructions
are retained for the next contact implementation; no additional geometry rule
is inferred solely from their labels.

### Shipped first-person melee text keys (checkpoint47 investigation)

Read-only extraction and typed NIF decoding of five stock first-person KFs are
retained under S4/native-melee-kf-inspection-01. Oblivion - Meshes.bsa SHA256:
`d05bb62f933856105536beb06c26bcdf5fa37f685152a39ece587e7f26b4e99b`.
handtohandattackleft/right both contain Hit at approximately0.2 seconds and
end at approximately0.666667; handtohandattackpower contains Hit at0.433333337
and end at0.900000036. Equip contains Attach, Enum: Equip and a0.2-second end;
handtohandblockidle spans0..1.29999995. Exact binary32 times, source file bytes,
configured driver commands and textkeys.tsv are retained in ignored evidence.
These native groups/keys differ from the shared TES3 windup-section contract;
the existing M11 sequence loader lowercases/trims bare text keys and adds
group-prefixed start/stop keys. Bare Hit becomes hit; it is not prefixed.
The shared controller currently skips that bare native key. Decoding key declarations does not establish dispatch, collision,
fatigue/damage timing or visual/audio combat acceptance.

### Native adjusted melee contact distance (checkpoint48)

Full original612F50 executes6,912 observations in both x87 precision words,
including original404C90 vector norm and9828C0 non-SSE truncation. A FLT_MAX
reference-distance sentinel returns unchanged. For two actors, both flying
or a selected target with absolute height difference **at least** the slope
GMST permits horizontal distance when vertical bounds overlap inclusively.
Otherwise the initial reference distance survives. Each bounds maximumY is
multiplied by its resolved scale; the double products are summed and truncated
once before subtraction. Negative contact distances are valid.
The production helper accepts resolved bounds/positions/reference distance,
rejects malformed finite-domain inputs and explicitly rejects radius sums
outside the nonnegative int32 domain. It does not guess upstream bounds
scaling, same-space validity, LOS, physics or target selection.

The hermetic432-row C++ corpus contains literal binary32 expectations from
original instruction execution. Additional cases cover actor gates, sentinel,
height equality, negative distance and malformed/overflow input. Oracle04
adds all four actor-flag pairs and sentinel/non-sentinel cases. Getter outputs,
flight flags, raw reference distance and slope GMST lookup are declared stubs.
Evidence: S4/melee-distance-oracle-03/04. Original executable SHA256:
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Oracle04 probe SHA256 `2e7adc9a3ebf80581a87d668b1d600aee7f3762f5486263742244986a300ab5c`;
report SHA256 `ef693ebed6d94a22605f701412b732a3e85184ef8e9f7c647eaf59494b556e8d`.
Attempt01 failed on a harness return: rewriting one float-return stub reused
Unicorn's previously translated ret8 for a ret0 call. Attempt02 separates those
addresses and then falsifies the preliminary strict slope comparison at48.
Attempt03 corrects that equality and passes864 observations;04 passes6,912.
These retained failures do not establish physical gameplay acceptance.

### Script combat dispatch (checkpoint49)

The pinned executable's command registry identifies StartCombat opcode1016,
handler514660, and StopCombat opcode1017, handler501D30. StartCombat takes one
actor argument; StopCombat has none. Full StopCombat executes16 null/cast/query/
Player profiles. It calls +334(true), then +340(false) only if the predicate
is true. Actual Player +334 is6FE080 and returns false; its command remains
a no-op even when the special IsInCombat Player-list query reports combat.
RTTI, NPC/Creature combat predicate and stop application are supplied boundaries.

Full StartCombat executes256 profiles under a nonspecial actor state with no
current controller. Parsed nonnull alive source/target both require process
pointers. Eligible paths call process +228 with the verified ten argument
slots; the supplied alarm-policy predicate additionally selects +22C.
Parsing/RTTI, dead predicates, special/alarm flags, combat controller/query and
start/alarm application are stubs. This does not establish underlying package
installation, symmetric membership, crime, Player initiation or gameplay.
The fork's current script adapter publishes membership only; those other
command consequences remain required open integration work.
Evidence: S3/script-combat-command-investigation-01. Original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
stop-probe.py SHA256 `22c2ffd3282790894260bcba9ce57687fc0ce8d88a10baafd87fbc49c21660e7`.
stop-report.json SHA256 `baa34915977fa83300e6418ade3c2a277d04c0a30ca4f41652b9cee019d5ed76`.
start-probe.py SHA256 `42e06d748a32554a8a62aad4bfac6227d654ef27dddf1eaaaf9a1b146acc4bfd`.
start-report.json SHA256 `e577ef99576bd3dc564b3e6019e5832e129ff945fafd4fe08885138fcc1aeb31`.

### Hand-to-hand contact caller and victim knocked state (checkpoint50)

The original caller5FF3CA..5FF41E supplies the suppression byte from the
victim's virtual +19C, common5E04F0. That getter returns true for any nonzero
native process +2E4 result. The actual High/MiddleHigh getter64B080 sign-extends
process+11C; actual Low/MiddleLow getters return zero. No process returns false.
This is not the essential-recovery gate limited to states1/3, and it is not a
TES3 animation-state enum. Health damage is unchanged by this flag.

S4/hand-contact-caller-oracle-01 executes12,800 exact observations across all
256 signed byte values, all four actual process getter bodies, no process,
five attacker Fatigue quantities and both x87 words. The caller, common victim
predicate, real process virtual bodies, full5F4880 fatigue ratio, full547280 and
luck/fatigue arithmetic helpers execute. Attacker base/current AV reads and
installed physical/hand GMSTs are supplied. The caller's original stack and
output pointers are verified; no hook chooses the suppression argument.
Contact eligibility, attack-cost timing, mutation, reactions and normal-input
combat are outside this probe.

HandToHandContactInput accepts current/base attacker Fatigue and the resolved
victim getter result; handToHandContactDamage derives the ratio and suppression
before using the already audited arithmetic. A hermetic test carries five
literal original Health/Fatigue expectations and exercises no-process/zero/all
256 byte results, zero base and invalid inputs. It introduces no native
knocked-state storage or inferred controller state; the runtime controller
still needs to supply its actual native process state.
Original executable SHA256:
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
probe.py SHA256 `ff8ac91a7e48daf1f9c5832281141420cf9b4cc6b901e8a6a33199626bcec91e`.
report.json SHA256 `4e1fa799cb7e459d42aeca41e5ad7df5e51a8f1a033ca27d32d093054cbf35b2`.

### Ordinary attack contact ordering and expanded stock KF inventory (checkpoint51 investigation)

S4/normal-attack-contact-order-oracle-02 executes eight original ordinary
AttackLeft/Right subtype4 dispatcher profiles across Player/non-Player,
combat/noncombat and both x87 words. The contact virtual invocation precedes
the attack Fatigue debit. Full5E4010/547560 computes the unarmed basic cost7
from the supplied winning attack GMSTs; the debit writer observes-7.
Process/magic-item/engagement predicates, contact application and debit mutation
are supplied boundaries. This proves invocation order, not complete contact
arithmetic, input timing, physical mutation or normal gameplay. Attempt01 used
incorrect GMST addresses and is retained as a failed harness setup.
Original executable SHA256:
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
probe.py SHA256 `9b544e6e7248ac80a99e4f54ed5dd315228f2275164cc66a0165448d04b5026c`;
report.json SHA256 `38854212037a797c5b3710d6a18e4eedec1273356bd9c1cc20e16cda2418c1fd`.

The original animation metadata table is atB102E0, with36-byte entries.
AttackLeft/Right are type3/subtype4; AttackPower and four directional power
groups are type3/subtype5; BlockAttack is type3/subtype6. The preliminary
B102E4 table read was misaligned and is retained separately. This metadata
inspection does not by itself establish power-attack gameplay semantics.
Evidence: S4/attack-event-order-investigation-02/group-table.json, SHA256
`9e991c0cbf307906b5f8eea345ce7c5b4576588f600cdd1a2a1c85522a1161c7`.

S4/native-melee-group-keys-02 extracts and decodes53 attack KFs in each of the
stock _1stperson and _male directories. All58 hand-to-hand/one-hand/two-hand
melee groups across these views contain exactly one bare Hit key. Some timings
differ between views; controller contact must follow actual animation events
instead of a shared fixed timer. Other decoded attack assets include bows and
block attacks; they are not counted as these58 melee groups. Archive list paths
use backslashes; the initial slash-only selection failed its nonempty assertion
in attempt01, which is retained. No production NIF key rewriting was made.
Archive SHA256:
`d05bb62f933856105536beb06c26bcdf5fa37f685152a39ece587e7f26b4e99b`.
assets.json SHA256 `d7c8563a0164e6f25e693eb8cb0d90381c0a480696334cafba26f274094022b8`;
commands.json SHA256 `689a380655f298f495dac9fc3e73e4e95371b1d87df978100aaea4ce1c2425e6`;
melee-contact-keys.json SHA256 `29335217c9343273ad965141ba1fb4ac0c2f370d1ae1c189193279b979f580b8`.
The reused typed NIF inspection driver is the checkpoint47 asset decoder;
this read-only inventory is separate from the checkpoint51 tested source and
from required input/controller/runtime acceptance.

### Native acquisition selection, swimming correction and slope default (checkpoint52)

The new component selection rule matches216 literal outcomes from the existing
full6156C0 original probe (432 observations including both x87 words).
The selected branch reads facing/distance exclusively and does not inspect
candidate death/residency flags; a failed selected target never enumerates a
fallback. The enumerated branch skips dead/nonresident actors and picks the
smallest facing angle, replacing equal angles in manager order. Inclusive
reach admits equality; negative adjusted hull distance is valid. Runtime contact
application has separate lifecycle eligibility. These observations still supply
geometry/manager membership and do not establish LOS or physical gameplay.

The earlier adjusted-distance provenance called helper5E0530 a flight predicate.
That label was incorrect. Native IsSwimming opcode10B9 registers execute5053B0
and condition4F56D0; the actual condition calls5E0530, which tests bit0x800 from
process virtual+2C0. Actual High getter6285A0 reads process+1FC. Low, MiddleLow
and MiddleHigh all use60CF50 and return zero. Flag0x2000 does not enable this
predicate. The contact input is renamed mSwimming; numeric distance outcomes
are unchanged, but callers now supply swimming rather than flight/swim unions.
S4/melee-swimming-state-investigation-02 executes90 observations through the
full registered condition, common predicate and real process getter bodies.
Only the actor-type virtual predicate is supplied. Attempt01 incorrectly
expected MiddleHigh to read flags; that retained failed expectation is corrected
by actual virtual dispatch in02. Water/process flag updates and live swimming
contact remain outside this probe.
Original executable SHA256:
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
probe.py SHA256 `3abdc5253bd25586c11c1cdaf0dc714f1cedc8e6cd87dfc256c8a4b1d4e3d2b9`;
registry.json SHA256 `b1647ae7ee465d887103522f8f486291ca880bc9380f02c2e7032e7eb08a1ce2`;
report.json SHA256 `4d48260849355efa4c968c7741720fd1f57cd7550093d9c5d42e350984317ae4`.

Independent raw-master traversal in S4/melee-slope-winning-investigation-01
finds no fAICombatSlopeDifference override. Original initializer prefix
9EA850..9EA864 supplies that exact name, float48 and storageB37330 to41BAE0.
The settings resolver therefore uses verified compiled48 when absent, accepts
case-insensitive typed winning overrides and rejects ambiguity/bad values.
The prefix probe does not execute registration, override loading or gameplay.
Its initial system-Python invocation failed because Unicorn is only installed
in the established oracle venv; that failure is retained separately.
Master SHA256:
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`.
master-settings.json SHA256 `91f2469f9c8b4ca1b8ce430d09f4847e66765890e70667df1dc5d001b4e5a829`;
initializer-probe.py SHA256 `b5ed1d93d86842627e7858939b5b275812b99bec3815f2e29de077c0e330fcd1`;
initializer-prefix.json SHA256 `bf824e642de7066d4eb81e8ded287a9a1b8f48b2d427a606c9a8370c8c393ff0`.

The engine acquisition query adapts actual collision-body bounds, native height
scale and active-actor swimming, then applies native distance/cone/selection and
actual same-space/LOS checks. The Bullet adapter test proves its as-built
collision representation, not equivalence with original Havok process bounds.
Original resident ordering, water/process state updates, upstream bounds/scale
resolution and LOS remain independent runtime/original acceptance work. A
missing physical context is not turned into a fabricated contact or miss.
Controller/animation dispatch, damage, reactions and restart remain open.

## Native controller fatigue prerequisite (checkpoint53)

CreatureStats::isFatigueKnockedOut and the CharacterController recoil caller now
preserve the independently established fatigue<0 entry rule for native
projections, without importing TES3's additional base==0 condition. This is
only the fatigue cause. Essential unconsciousness and random knockdown remain
separate; native paralysis and physical animation/recovery mapping are open.
The new projection regression is included in both712-case engine runs recorded
in the milestone report. A retained failing reproduction records the exact old
controller expression and source hash; it is not a rendered controller run.

## Original ordinary/hold and queued attack input (checkpoint53)

Pinned original1.2.0416 executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
S4/native-attack-input-oracle-02 executes65EB57 to65EC36/65EF38, including full
403520/403490/4032D0 keyboard queries,51AC80/51ACC0 animation-group predicates,
5E0550/5E0530 and High-process6285A0 movement getter. All18,432 observations pass
in both x87 modes. Keyboard query1 is a new press, query0 is currently held.
A press outside an attack group selects20/21 immediately and resets held time.
Held time has a binary32 store after frame addition. The65EE10–65EE25 comparison
requires held time strictly greater than delay, not equal. Power/group/state
selection is distinct from applying a multiplier to an already started strike.
Original queued state writes1/2 and held-time resets execute unchanged.

The ordinary/power/block metadata subtypes4/5/6 are read from original B102E0,
not supplied. Process+304=true,+138/+13C=false, actor+25C=false, animation getters,
side selector, Acrobatics base-rank and airborne predicate are supplied. The
sequence+48 offset and animation-data+94 sum is supplied; real clock advancement
and interpretation of those fields are not proved by this instruction harness.
Input-oracle01 remains a passing diagnostic with less precise field labels;
02 records offset and raw animation-data94 separately.

S4/native-attack-queue-oracle-01 executes the preceding65E9EC branch in768 cases,
with original subtype predicates and supplied stage/animation queries. Queued
state1/2 can dispatch after button release; a power group blocks resumption,
and an ordinary/block group resumes only when the supplied stage equals2.
This probes the branch, not the NiController event that produces stage2.
Later65EF21 clears the queue after the animation-start call; full native start,
sneak/directional conversion, actual contact, save continuation and gameplay
remain outside these probes and still require implementation/acceptance.

native-attack-input-oracle-02/probe.py SHA256 `d0a0d1c4eeca289398aa09aaf574241e24e94c3cf30f906ee7d8f35006bcb52d`.

native-attack-input-oracle-02/report.json SHA256 `81173c83815cb13684a7c9b48aef821c5f771d0ea60a485ec32941902eb212c2`.

native-attack-queue-oracle-01/probe.py SHA256 `740320ace388a488a4f658cb48c8b96973dd5f03c7dee8777e8d13068020de62`.

native-attack-queue-oracle-01/report.json SHA256 `a92597018ccf01c90be4799d250aa6a1f10fc9fb6f50b3a47c78c98946aeaeee`.

## Persistent melee intent (checkpoint54)

Schema21 stores the adapter's logical strike kind, exact animation selection,
playback progress and owned ID separately from native input queue/held state.
These are explicit engine save semantics, not asserted original save layout.
The original press/hold/queue traces in checkpoint53 motivate distinct strike
intents and queues that survive release; they do not prove controller clock
mapping. A consumed contact retains follow-through while the ledger excludes
its ID, preventing replay after restore. The native physical-contact service
uses this operation; generic cancellation clears the strike instead.
Tests pass at the source fingerprint and counts in the milestone report.
Stock idle schema20→21→21 migration preserves nine admitted actors and has
no melee state. Native animation start/advancement, exact gear instance,
Hit-event physics acquisition and caller eligibility remain unimplemented.

## Native ordinary stage/clock and key ingestion (checkpoint55)

Pinned original executable SHA256 remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
S4/native-attack-stage-oracle-03 executes192 ordinary phase-prefix and18 clock
observations in both x87 modes. 4770D4–47710E reads supplied TESAnimGroup frame
times through original51AE20/984012 and increments animation-data+54 by at most
one phase per call, only when the binary32 sequence-offset(+48)+clock(+94) sum
is strictly greater than the next key time. Equality does not advance. Original
476F93–476FA6 adds supplied frame delta to clock+94 with a float store. 470750
returns data+54 for argument3; it is not NiControllerSequence easing state.
The ordinary subtype4 slot names are Start, Hit, a:, End; power subtype5 uses
Start, Hit, End and block-attack subtype6 Start, Attack, End. Ordinary stage2
therefore follows a:, not Hit or End. The full actor/sequence clock setup,
caller order, phase catch-up across large frame steps and gameplay are outside
these bounded probes. Stage01 failed an equality hypothesis; stage02 passed a
synthetic table with slot2 misleadingly labelled End. Both remain; use03's
corrected native key-name/slot audit, not02 as evidence for End semantics.

S4/native-attack-key-ingestion-oracle-01 executes14 observations through
51B87C–51B95A/51B9B1, including original9864D9 case-insensitive prefix matching
and the actual subtype key-name table. Supplied stock key times are stored in
preallocated frame-array slots: Start0, Hit1, a:R/a:L/A:r2 and End3; mismatched
Hit at slot2 writes nothing. Both x87 modes pass. This checks native key
matching/storage, not KF allocation/reader, side-suffix interpretation, animation
start, advancing actor clock or a real contact. Stock third-person HtHLeft's
a:R is0.433333397 and End0.666666985; first-person HtHRight's a:L is0.43333292
and End0.666666508. Native production timing/variant/FPS fidelity remains open.

stage03 probe SHA256 `0e9851ff7afabb8ba0a27ec8a42d85070483d29a070b812731f7e792da05efc3`;
report SHA256 `f15343742dc77caa32abfab7a76a93c3496a66543f843349c98499961bd2ff57`.
Key-ingestion probe SHA256 `fc00955443ae032399a995c19354a2e2a8617bb68d18918907e099c03ba03ff3`;
report SHA256 `14c1fbff67c653515273ac87a74d940b7e42142a0a505ac04792bf2755cbac87`.
Original instructions, extracted content and full reports stay in ignored paths.

The checkpoint55 renderer adapter persists exact selected group/speed/time;
its queue-window comparison currently uses scoped a: time. That does not yet
reproduce original one-phase-per-call catch-up or prove identical ordering at
all FPS. Normal keyboard input and active-strike fresh-process continuation now
pass in first/third person at the milestone's reported binary/fingerprint.
Those are playback/ownership observations with unchanged actor pools; native
bare Hit still cannot dispatch damage. No contact, block, mitigation, condition,
reaction or audio gameplay acceptance is claimed.

## Native melee input settings and contact phase (checkpoint56)

Pinned original executable SHA256 remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
S4/native-attack-start-gate-oracle-01 executes156 observations through original
5F48D0–5F48F8 and56A300 mastery in both x87 modes, with supplied Acrobatics
getter,5EC180 airborne predicate and threshold storage. Only Novice rejects
airborne start in this prefix; the separately probed held-power path rejects
both Novice and Apprentice. Process/drawn/block/start-clock/contact eligibility
and real gameplay are outside this prefix.

S4/native-attack-delay-oracle-01 executes24576 observations through original
65EB57 input branch with native B36B48 delays[-0.1,0,0.3,0.5], preserving signed
finite thresholds. Supplied animation queries/rank/airborne boundaries remain;
this does not prove the full original input/controller integration.

S4/native-contact-phase-oracle-01 composes the original4770D4–47710E phase
prefix with5FCC36 contact gating and5FCDDE ordinary contact/fatigue dispatch,
including full5E4010 fatigue calculation. All64 observations pass in both
x87 modes: phase must equal1 for ordinary contact; exact Hit equality does not
advance phase0. Native contact precedes fatigue debit. Composition order,
actor/process context and contact/debit writers are supplied. Earlier process
eligibility and later state3 anti-replay writer are outside this bounded probe;
repeated phase1 invocation is not a full anti-replay test. The runtime adapter
needs original phase/clock/caller ordering before damage wiring.

Read-only caller inspection finds contact before animation update at60193E/
60195C and66CB49/66CB7D; exact actor identity/full eligibility remain unproved.
The65589F/6558EF calls occupy distinct conditional branches and cannot establish
reversed Player order. Original5EC180 consults animation groups40/41/42 or
controller state2 via65A2C0/88D370; controller type/state meaning is unproved,
and must not be labelled AI procedure or replaced by ground contact alone.
An unavailable Capstone attempt is retained; objdump inspection succeeded.
No disassembly-only observation is claimed as executed gameplay evidence.

native-attack-start-gate-oracle-01:
probe.py SHA256 `c018db2c18ef77362a95d19e8305e410fbd9951e732260304582f91745eeedfd`.
report.json SHA256 `f290d61229467a45c066f61d6819cd571c93757810f8d224fc626035b8dd35e8`.

native-attack-delay-oracle-01:
probe.py SHA256 `72f271c6db29a46d90b43852123bf84fd604598fba7b72ddd4401fa1f3296c8b`.
report.json SHA256 `27dd82ea1b6cfd2c535db0e9625bf59317dc97bdf2a07e1701abb87f7f6becd7`.

native-contact-phase-oracle-01:
probe.py SHA256 `b2e1d07924e32dbcdd92250c7cd70c0b7a0e5881cc558825de1c279d6c83190b`.
report.json SHA256 `1e636c37c53bc9a76d408a0560f5bc84cb6b1cca80e15e8a9c1192ac6aeef68a`.

## Native airborne predicate and ordinary phase arithmetic (checkpoint57)

S4/native-airborne-predicate-oracle-01 executes full original5EC180 in1024
cases, including actual4706E0 slot0 sequence retrieval,51AC70 low-byte group
retrieval,65A2C0 controller resolution and88D370 context-state retrieval. Actor
animation and process controller virtuals supply pointers; temporary smart-pointer
ownership is null, and body lifetimes/state production remain outside the probe.
The predicate is true for an active slot0 sequence in groups40/41/42, otherwise
for a resolved controller whose context at+1E0 contains state2 at+0C. Missing
animation, sequence, process and controller paths and groups39/43, states1/3/4/5
are covered. Cached xOBSE headers corroborate bhkCharacterController context
layout and hkCharacterState InAir2; this is not proof of an exact Bullet state
adapter. Prior speculation about an AI procedure is rejected.

The new production ordinary phase and clock rules compare exactly with the
checkpoint55 independently executed192 phase/18 clock observations, including
both x87 modes. Phase uses an explicit binary32 offset+clock store before the
strict next-key comparison and advances once per update. Clock uses a float
store after addition. Supported typed inputs reject malformed keys/phases and
arithmetic overflow; this does not claim original malformed-content behavior.
Power/block subtype phase tables, speed/anchor offset correction, native caller
ordering, saved phase and contact dispatch are still outside this implementation.

native-airborne-predicate-oracle-01:
probe.py SHA256 `9f3929bc2db8cfb5bb667403fb5ed413be440b4a02f4d8f6efef724cd65ab95f`.
report.json SHA256 `3acf9c63a7201147d33be0eb09ce8ca6b908609f7db7ee9fb7e6c2b8f0d9750b`.

native-melee-phase-comparison-01:
original-observations.tsv SHA256 `e55724f138605a5aa050795e47fcbbfd7728229201cd4708ac6e47d20b130fbc`.
verification.json SHA256 `5ba951c4e909bf97bf8679d9abfa9010f4f87099a623e360dd259e5cc8711548`.

## Ordinary-contact process replay guard (checkpoint58)

S4/native-contact-replay-oracle-04 executes16 cases/48 repeated dispatches
through original5FCBB5 process-state/sequence gating and native jump table,
ordinary phase/subtype gating, full5E4010 fatigue cost, full5E5640 attacking
predicate and full5EFFD0 state writer, stopping at5FD7DE. Both x87 modes pass.
Supplied process getter/sequence virtuals and setter store the native state3;
contact/debit writers are supplied. With process state2 and ordinary phase1,
trace is contact, fatigue debit, state3. Two further calls at the same phase
reject contact. Initial state3 or other supplied phases produce no contact.
The native dispatch table routes process state2 to5FCC05 and state3 to5FD7DE;
5E5640 tests inclusive states2..5 before the post-contact transition.

Full function entry/body acquisition, callback-induced state changes, animation
update scheduling, blocking/magic/other process states and real gameplay are
outside this bounded probe. It establishes a process-state replay guard, not a
full original contact transaction or the production controller's contact timing.
Attempts01/02 failed harness generation (truncated loader string/missing newline);
03 omitted live ECX=actor at mid-function entry and faulted. Corrected04 passes;
failed sources and explicit failure records remain in ignored evidence paths.

Stock key inventory includes Sound: WPNBlockShieldHeavy/WPNBowDraw/bowShoot and
multiline Enum: Left plus Sound: WPNHitHand. Native SOUN editor-ID routing is now
wired in the CharacterController, with typed winner and actual handler-boundary
regressions. No original sound dispatch execution or audible playback is inferred
from those headless checks. Full sound selection/creature inheritance and actual
captured audio remain acceptance work.

native-contact-replay-oracle-04/probe.py SHA256 `34bbe047518de502468a7e14a07290e44fd15d55e9aa2cc3b00c19ad6b2a7826`.

native-contact-replay-oracle-04/report.json SHA256 `d1d11aad950b94fc4d902fa55851c936335bae4d6b2d42f7a89799f52335aa8a`.

## Synthetic native animation sound routing capture (checkpoint59)

Final S4/native-animation-sound-wave-02 executes configured normal keyboard
input against unchanged checkpoint58 C++ at binary SHA256
`fb04056153fc7de335de0df31258a2a77c6d5a2484faad9e73656de9008e575a`.
Generated TES4 SOUN localID00000800 names M15FixtureTone and a separately authored
1kHz,0.25-second PCM resource. The synthetic shared NetImmerse4 first-person
ordinary group emits Sound: at0.15. Its missing-editor-ID counterpart changes
only that keyframe, retaining identical plugin/tone hashes. No original game
sound or proprietary asset is copied into versioned fixture source.

Both OpenAL Wave File Writer courses exit normally; predeclared stereo PCM16,
48kHz,0.1-second windows require positive RMS>=.01 and1kHz fraction>=.8, negative
tone RMS<=.001. Channel-independent analysis and negative controls pass; numeric
metrics are in the milestone and final stereo-02 reports. Native actors/resources
and pristine saves are semantically reinspected, configured Use/actual playback
and distinct process epochs verified. Exploratory wave-01 declared thresholds
after launch and is explicitly excluded from predeclared acceptance evidence.

This demonstrates native SOUN lookup/typed FormId dispatch/resource decoding
and real mixing for a synthetic animation event. It is not stock attack/creature
sound selection, original-game acoustic fidelity, saved audio continuation,
contact/damage or full S4 acceptance. Both final images are directly reviewed;
post-save poses do not identify the saved phase. Audio-tool review was attempted
but the tool cannot supply audio input here; no perceptual listening result is
claimed. The captured review clip remains ignored for later playback.

wave-02/audio-expectations.json SHA256 `4d106e1dcc9b2b890e7e431e63fb6995afc55c85f6864aa35cb5a7fc0c2d21d2`.

wave-02/positive/run/rendered-audio.wav SHA256 `ef5e4abfefb1613a926c6fe7defafb7ef3354de180e75f0258a33194ccb31ad5`.

wave-02/negative/run/rendered-audio.wav SHA256 `9cd8f5af5b48a4a0802098cfb5721d2075e843a473565d69d20de6d55390f55b`.

wave-02/verification-stereo-02.json SHA256 `25e6b8a731549d6e695a2a807f1468f74be01d3baa65589de9deababd7449160`.

## Native airborne start production bridge (checkpoint60)

The ordinary/power start adapter now applies the independently verified Acrobatics
mastery gate before interrupting a strike or allocating an action. Winning typed
input settings resolve at most once per controller update without a cross-frame
cache. Held-power eligibility uses the same airborne classification. The immutable
classifier retains original animation priority: native JumpStart/JumpLoop/JumpLand
(groups40/41/42) or character-context state2 mean airborne, with missing pointers
handled explicitly. The controller bridges active shared jump playback and common
live swimming/flying/ground/in-air states; exact original Havok transitions,
climbing/noclip/jump windup and complete native start-process eligibility remain
unproved, not claimed as production fidelity.

S4/native-airborne-predicate-comparison-01 passes1024 comparisons against the
independent checkpoint57 original-instruction observations. Oracle report SHA256
`3acf9c63a7201147d33be0eb09ce8ca6b908609f7db7ee9fb7e6c2b8f0d9750b`;
numeric corpus SHA256
`517471ab7cb04285f37e5b912c386a8a4dee141f792a9e13168216676e2b4fb1`.
S4/native-start-airborne-main-01 passes all1998 component/719 engine cases;
sanitized-01 passes473 ESM4/719 engine cases under ASan/UBSan with leak checks
disabled. Both exact inventories have zero failed/skipped cases and unchanged
tested fingerprint
`5b16231f334c937c1f962557ea09524d322e3b62bd8f8b0fb976ce82ed4ca3e9`.
Python is unchanged from checkpoint59's222 passing cases.

Actual S4/native-start-airborne-offscreen-01 normal configured keyboard grounded
first/restart courses pass. ID1 saves at animation time.4000000059604645,
resumes in a distinct fresh process without another selection/ID, and completes
with nextID2/pendingempty. All nine actor values/life/breath/bases/death/combat
membership and public80/80/140 pools stay unchanged. Both captures were directly
reviewed: textured prison and full bars, first subsequent fist playback and second
idle. Capture pose does not prove the earlier saved phase.

Predeclared S4/native-start-airborne-rejection-02 uses normal E jump then U Use
at actual pristine Player base Acrobatics5. It rejects the start, performs no
melee selection, keeps nextID1/pendingempty/noowners and no active strikes.
Saved z7.311005592346191 exceeds pristine z-109.68915557861328. All nine actors'
values are unchanged except Player Fatigue Damage from0 to-24.0000057220459;
that difference is permitted explicitly for jumping. Life/breath/bases/death/
combat membership stay unchanged. The directly inspected capture shows an
elevated prison viewpoint/readable HUD/reduced Fatigue, not independent proof of
eligibility. Rejection-01's relative launcher-base replacement failure occurred
before game launch and is retained;02 corrects only that launcher path.
Runtime executable SHA256
`b88b0cce02c61d88c8864b1d2236280f05040b8cef9e126c85643fb1eef75a6f`.

These checks cover actual below-mastery airborne rejection with a grounded
positive/save continuation control, not every physics mode or full native
start/attack/block/contact/damage acceptance. S2/S3 remain in progress;
S4–S14 remain open. Previous checkpoint59 commit
`3bcaac7686280ed738f2292ad1f1f8f0bcd79b94`; verified bundle37 SHA256
`7b5879455bfe9e3404537cccb68366b322eaa55f343aac5ecca3945b6d3ff5d6`.

## Durable ordinary melee phase authority and clock evidence (checkpoint61)

Runtime schema22 adds a validated byte ordinary_phase (Start0/Contact1/Queue2/End3)
to an active strike after its schema21 fields. C++ binary/canonical JSON and
Python readers/writers agree. Historical schema21 wire bytes stay unchanged;
missing historical phase deliberately defaults to Start without inferring it
from animation time or changing consumed contact/action ownership. Current
missing/malformed phase fields, invalid bytes, truncation and lossy downgrade
of an advanced phase fail. Python old-save promotion retains unrelated records.
Power key layouts are not interpreted as ordinary phases by the service.

The real native combat service now exposes a validated owned-strike ordinary
phase update using checkpoint57's independently verified immutable rule. Equality
at Hit does not advance; crossing multiple keys advances only one phase per
call. Wrong actor/ID and power kind do not update; malformed clocks/keys fail
before publication. Phase survives binary restore with its action still pending;
advancement alone never consumes contact or applies damage. Tests also retain
committed follow-through and reject lossy old-version capture atomically.
This service method is not called by the renderer/controller yet: phase0 in the
runtime course is explicitly unadvanced, not a claim of native phase timing.

S4/native-melee-phase-state-main-03 passes all1999 component/719 engine/223 Python
cases. Sanitized-02 passes474 ESM4/719 engine cases under ASan/UBSan (leak checking
disabled). Exact C++ inventories have no failures/skips. Tested fingerprint:
`9cd32fdaa1d784aee4daddc08912ee757dbc71fb88f120aa962e5355d56a004c`.
Main-01 retains two test-only compiler errors (vector erasure by ID and incorrect
JSON method name). Main-02 and sanitized-01 retain an incorrect test assertion
comparing canonical sorted action IDs with an unsorted fixture; corrected to the
explicit sorted expectation. No failure is replaced or hidden. Existing aggregate
initializer and shared animation-queue compiler warnings remain visible; this is
not the full compiler gate.

Actual S4/native-melee-phase-state-offscreen-01 starts from pristine schema21,
saves schema22 ordinary ID1/time.4000000059604645/phase0, and resumes in a distinct
fresh process before completing with nextID2/pendingempty/no new selection.
Both normal configured keyboard courses exit normally, preserve all nine native
actors/AVs/life/breath/bases/death/combat membership and public80/80/140 pools.
Both captures were directly inspected: textured prison/full bars, first later
fist playback and second idle. They do not prove the earlier saved strike phase.
Runtime executable SHA256
`205c2d7639e510c8668425e688bc3498aea0c8186c14d9524189349dfa8d6548`.
Phase/clock production, contact/damage/block/FPS and full S4 remain open.

Independent additional clock research is retained in
S4/native-sequence-clock-inspection-01 and native-sequence-clock-prefix-oracle-01.
RTTI identifies NiControllerManager vtableA79804/+54 target6C4200, which dispatches
sequence6CA950. Its exact update prefix6CA950→6CA99F (inactive exit6CAC36), with
no calls/stubs, passes1344 observations across states0–6, signed/large clocks,
sentinel/custom offsets, initial/existing ease start, two ease durations and
both x87 modes. For active sequences a sentinel offset becomes negative current
clock; a sentinel ease-start becomes current clock and adds it to ease-end with
float store. Supplied sequence/state/clock are boundaries; activation, blending,
scaling/controller body and actor caller ordering are outside this prefix.
No new production clock initialization rule is claimed from read-only inspection.
The first constants read omitted the emulator module search path and failed
before image loading; corrected read records that failure. RTTI slot inventories
stop at the first non-code value and do not label adjacent strings as methods.

S2/S3 remain in progress; S4–S14 remain open. Previous checkpoint60 commit
`2c4eba7b59ac974ad2e2504b671b7769e13cc16a` and verified bundle38 SHA256
`5c1ad7ec0629d7fdf0462d71b97c8f35c9c19296feaa4751c2fdd4cfac492c17`.

native-sequence-clock-prefix-oracle-01/report.json SHA256 `ff3cd8c4b47f831ae1800fa9d19774948b0d55bb8a8b5ab1774392283b4e818d`.

native-sequence-clock-inspection-01/controller-vtables.json SHA256 `f9e012eafb1ce268fc48d6d745d778b358f72f91f52bdcfe4665f5268c2d347e`.

## Native sequence offset initialization and speed correction (checkpoint62)

Immutable rules now initialize a resolved new sequence offset from the negative
native animation clock (including signed zero) and correct an eligible melee
sequence's offset for playback speed. Correction preserves the native separately
stored offset-minus-begin term, corrected term and final anchor addition. It does
not round speed-times-duration separately, optimize away duration0/speed1 anchor
round trips, or use the renderer's accumulated duration-times-speed clock.
Finite signed clock/offset/begin inputs, positive speed and nonnegative duration
are validated; nonfinite/overflow inputs fail explicitly. Caller eligibility,
actual clock production and metadata resolution are not supplied by these rules.

S4/native-melee-speed-offset-oracle-01 executes the exact original477086→4770B1
arithmetic with no calls/stubs for4096 observations in both x87 modes, varying
signed offsets, source anchors, speeds and zero/144/60/30 FPS/other durations.
Together with checkpoint61's6CA950 prefix, clock-offset-comparison-01 matches
4384 original observations bit for bit:288 eligible sentinel initialization
observations and4096 speed corrections. Original report SHA256 values:
`ff3cd8c4b47f831ae1800fa9d19774948b0d55bb8a8b5ab1774392283b4e818d`,
`5fe888534a73e6782bf7989ef794e19dffdb6418b120ff4da562c22a2f9898e8`.
Reviewed numeric corpus SHA256
`60cb22eb9fae7da146bb2dc802f2d49df4f90d745231a7130c5a3ae863c3bd35`.

A literal regression fixes exact output bits: offset-1/speed.1/duration.2 becomes
BF970A3D; speed.7 becomes BF87AE15. With begin1000/offset.2/speed1/duration0, the
native stores produce.20001220703125, so zero duration is not an identity for
all source anchors. Initialization flips signed zero. Invalid factors/nonfinite
inputs and overflow fail. S4/native-melee-clock-offset-main-01 passes all2000
component cases; sanitized-01 passes475 ESM4 cases under ASan/UBSan (no leak
checks). No failed/skipped cases, unchanged tested source fingerprint
`44e1cbaa7fabdfe11a9b30f61bb406f5714f0578ac4f748a9cb6d4bb5c6258ea`.
Engine and Python source are unchanged from checkpoint61's719 normal/instrumented
engine and223 Python checks; this checkpoint does not claim a fresh engine build
or a runtime course for helpers that are not yet wired.

Inspection also identifies a required input boundary: the shared KF loader
normalizes controller-sequence key times into renderer timelines, and
KeyframeHolder does not retain original sequence begin/frequency/raw key times.
Actual native phase integration must preserve and resolve those inputs from the
winning playing source before using these rules. It must not infer them from
normalized renderer time. Contact/damage/block, original clock lifecycle,
freeze/eligibility and full S4 remain open. S2/S3 remain in progress and S4–S14
remain open. Previous checkpoint61 commit
`e26cee7a9c5b96a6fabea82cc4d490a110e14e9a`; verified bundle39 SHA256
`b669b6e9106f629bfd0a14a7b9f053d58e3987addd3777541f750e7633164189`.

## Original controller-sequence metadata boundary (checkpoint63)

The KF loader now preserves each native NiControllerSequence's original begin,
stop, frequency and authored text-key times/text/order before renderer timeline
normalization. Shared KeyframeHolder copies preserve this metadata independently.
Animation resolves metadata from the actually playing source, otherwise the
latest source supporting the requested group, including lazy sources and aliases.
A winning source without native metadata does not borrow older coordinates;
duplicate native sequences for that group return no metadata. Alias copies rename
the group without mutating cached source metadata or raw key text. TES3 sequence
stream helpers continue to have no native controller-sequence metadata.

The loader regression preserves nonzero source anchors, case, surrounding spaces,
CRLF, out-of-order authored keys and copy isolation. The engine regression writes
an editable synthetic Gamebryo20.0.0.5 KF binary and uses the real NIF parser,
resource manager and animation source selection: original Hit10.25 remains10.25
in metadata while the existing renderer projects it to.25. A playing old source
remains authoritative after replacement; disabling it selects the replacement's
begin20/frequency.75/raw spaced Hit. Actual generated legacy and duplicate-native
binaries exercise the missing/ambiguous negative controls. Lazy lookup and alias
isolation are also checked.

S4/native-animation-metadata-main-02 passes all2001 component and720 engine
cases. Sanitized-02 passes476 ESM4 and720 engine cases under ASan/UBSan with
leak detection disabled. Inventories exactly match executed XML; no failures or
skips. Both runs preserve tested source fingerprint `32dd3a2c0964c9af149fff206c0df00df2ae9a5f07931b22ce4bd10d66f0e577`. First runs
main-01/sanitized-01 also passed before the negative controls were added. Python
source is unchanged from checkpoint61's223 checks. This checkpoint changes no
save schema, damage authority or gameplay phase advancement. Original metadata
is now available for the next integration, but native clock lifecycle, raw-key
interpretation, contact dispatch, damage/block/condition/reactions and full S4
remain open. No normal-input runtime acceptance is claimed for this boundary.
S2/S3 remain in progress and S4–S14 remain open.

Previous checkpoint62 commit `b62c6fe024786744110f3da4ab786a3baaaef0d5`;
verified bundle40 SHA256
`615a62b2ddfa307ca2218f101dc03f43d71a73548d8ad41add51c7d49a005210`.

## Native ordinary preparation and cancellation ordering (checkpoint64)

Native ordinary windup now answers the production attack-preparation query from
its authoritative Start phase. The Lua stance handler previously saw the shared
AttackEnd renderer state and rejected a normal weapon-hide request as TES3
recovery. Ordinary Contact/Queue/End do not become preparation states; powers
continue through their existing path until their native phase mapping is added.
The controller now finishes the native strike and clears its rendered identity,
weapon group and upper-body state before disabling animation and emitting its
Lua end event. Cancellation is logged separately from natural playback completion.

Editable keyboard manifests draw a weapon, start an ordinary strike, release
Use, request weapon hiding, require interruption of action1, save, and restart
in a separate process without restoring or selecting another strike. The fixed
5 FPS offscreen course uses acknowledged held input rather than an ephemeral
press. S4/native-melee-unequip-cancellation-offscreen-03 preserves all nine native
actors' values/life/breath/base/death/combat fields, clears all strikes and input,
and leaves ledger next2 with no pending action or owner in both saves. Source
schema21 migrates to22; binary SHA256
`bf33f31c82dda0230740ef7035f75bc7be72c47acf51e31656d951d4d260a129`. Input copies/source hashes match;
separate process epochs and public80/80/140/alive queries are verified. The actual
captures are reviewed independently of saved timing; they do not prove callback
execution or an earlier animation phase.

Offscreen-01 and-02 remain failed evidence: neither observed the required
interruption. The first short press was initially suspected of missing the
frame; the held-key repeat exposed the shared preparation-query gate, which was
then fixed. Passing broad checks of the earlier source did not close this runtime
gate. Main-03 passes720 engine and223 Python cases; sanitized-03 passes720 engine
cases under ASan/UBSan with leaks disabled, exact inventories and no skipped or
failed cases. Tested source fingerprint
`d5b4b7e07dd86ae38d5f8a38ba6b744513f01a66dec90fa61a52a96db65c67dd`. Component source is unchanged
from checkpoint63's2001 normal/476 TES4 sanitizer cases. Main/sanitized01 and02
passed before the query fix and are retained as earlier-source evidence only.

Native phase advancement itself remains unwired, so Start here is the service's
current unadvanced phase. This proves ordinary preparation-query integration and
weapon-hide cancellation, not timing/contact/damage, power interruption, all
cancellation types, first/third-person/device/FPS campaigns or full S4. S2/S3
remain in progress and S4–S14 remain open. Previous checkpoint63 commit
`05cf9e0cd070cfc848f4e46180faed4bf1bf6b36`; verified bundle41 SHA256
`5ceb9d7df5d2cb9410a4a37de7e69101897c272e48adee3381392f98789546be`.

## Ordered native ordinary-key ingestion and admission (checkpoint65)

The pure raw-key reader follows the original authored counter: Start, Hit, a:,
End must be consumed in that order. Case-insensitive prefix matches retain suffix
semantics, whitespace is not trimmed, and the original LF/CRLF and NUL behavior
is preserved. Missing slots start at zero; a matched key advances the counter
even when its time is <=-1 and is not stored. Supported raw times are finite;
nonfinite inputs explicitly reject rather than claiming original equivalence.
The winning native controller metadata now gates new NPC/player ordinary L/R
strike admission before action allocation/playback. Missing, ambiguous, malformed
or unsupported phase keys reject. Powers and native creature groups remain on
their existing paths. This does not validate restored strike metadata, advance
phases, dispatch contact or prove all queued replacement/cancellation ordering.

Independent pinned original executable a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6
ran1728 full raw-key ingestion loops (S4/native-attack-key-full-loop-oracle-04)
and the production helper matches every stored float bit and authored counter
(S4/native-ordinary-key-comparison-01). The original loop runs51B688–51BA50,
using preallocated group20/count4 storage and only a diagnostic logger stub;
KF parsing/allocation, terminal group validation and actual actor playback are
outside this probe. Report SHA256
9626c0d85c902bf2d3c2873459fdff9553038bbd4ec5561d740132271394f119;
comparison corpus SHA256
34c07fdf704734b955eadf822cd3ea34137c29af1e3e96ce953ee68b33aa27d8.
Earlier loops01/02 retain content-lookup faults rather than hiding them with a
Sound stub. A separate20-case allocation probe proves zero initialization;
480 activation cases characterize empty-controlled native sequence activation,
including positive-ease state2, for future clock wiring, not production timing.

The editable synthetic audio writer now generates genuine Gamebryo20.0.0.5
NiControllerSequence metadata, plus an explicit missing-Hit fixture option.
This independent TES4 writer is not TES4 support in the imported TES3 MCP.
Stock ordinary admission/save/restart passes with action1, unadvanced Start
phase and saved time0.4000000059604645; restart completes without a new ID.
Missing-Hit normal keyboard admission rejects before allocation, leaves next1
with no pending action/owner/strike, and fresh restart does not reselect/restore.
Both courses preserve all nine native actors' value/life/breath/base/death/combat
fields and public80/80/140/alive values. Schemas21 migrate to22. Reviewed captures
show textured geometry and readable full HUD bars; post-save imagery does not
establish the earlier saved phase. Evidence: S4/native-ordinary-key-admission-
offscreen-01 and S4/native-ordinary-key-rejection-offscreen-01.

S4/native-animation-sound-wave-03 repeats real OpenAL stereo PCM mixing with
complete native phase keys. Positive RMS0.20566245777217138 and tone energy
fraction0.9999999940007696 pass declared thresholds; missing-editor-ID negative
tone RMS6.674746935512792e-7 passes its <=0.001 limit. Only the fixture Sound
text-key editor ID differs. Separate fresh-process epochs, input copies,
fixture hashes, native saved state and playback logs verify. Both images were
directly reviewed. Audio input is unsupported in this session; numeric PCM
verification passes without perceptual listening acceptance. This is synthetic
sound routing, not stock sound or contact/damage acceptance.

Main checks pass2004 component,720 engine and224 Python cases; ASan/UBSan checks
pass479 TES4 component and720 engine cases with leaks disabled. Exact inventories
and XML agree with no failures/skips or source drift. Tested source fingerprint
`3b6dfa4a1daefd496bbe3687ee94da191786e2088e155ca88a35a9c5dd5e16fd`; runtime binary SHA256
85db68e3b5d15078177c624c03804486da579ddf2358472a8f6b69856d1706c3.
Known older compiler warnings remain part of the open final warning gate.
S2/S3 remain in progress; S4–S14 remain open. Native phase clock/speed correction
and contact dispatch remain unwired. Previous checkpoint64 commit
6975e66b4f5c9c24b2dacda1708c4f6ced29bd23; verified bundle42 SHA256
6f526cf922446f38c509192aa91dbe79014fde992ee1b0ff1341991bed80f728.

## Native unsynchronized clamp sequence timing (checkpoint66)

An immutable sequence timing rule now models the native uninitialized offset,
ease-start and previous-input states explicitly, initializes them from the
caller-supplied global animation clock, and preserves the native state2→state1
inclusive ease threshold. A transitioning update does not enter state1's separate
previous-output assignment until the following update. Weighted time keeps its
unclamped history; the displayed result clamps to the authored begin/end bounds.
Frequency multiplication and delta/history stores follow original6CA950/6C5FC0.
The supported scope is unsynchronized clamp-cycle timing, with finite values and
positive frequency. Activation, synchronization/other cycles, transforms, native
actor state persistence, caller phase order and contact dispatch are not supplied
by this rule and are still open in production.

S4/native-sequence-lifecycle-oracle-02 runs1920 original activation courses and
28800 full sequence updates, executing actual6C9BA0,6C6A50,73A5E0,6CA950,6C5FC0
and6C6DC0 with empty controlled blocks and preallocated manager capacity. No
called-function stub is used. Sequence+54 remains the native sentinel and there
is no synchronization source; actor caller and nonempty transforms remain outside
the probe. Pinned executable SHA256
a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6.
The production rule matches all six float fields and state at every update
(S4/native-sequence-timing-comparison-01), including signed zero, clock origins
0/1000/1000000, zero/tiny/nonzero easing, nonunit frequency, negative/nonzero
begin anchors and retained weighted history beyond end. Original report SHA256
7bae34518d951974213e50fac89f8ee77341cb71f59ce82bf5c36d7fad046615;
independent corpus SHA256
b9d9ba1ca097bf429caf5e219082e614f016f9efd027bb41411a83a52c7fc931.
The earlier144-course/2160-update original run remains separate evidence.

S4/native-ordinary-frame-prefix-oracle-01 independently verifies two original
constructor-prefix clock initializations to positive zero and2304 frame cases.
Actual clock addition, native group decoding, freeze/sequence-state eligibility,
speed offset correction and ordinary phase branches execute without function
stubs. A hook selects slot3 after clock addition; other slots/full caller/contact
ordering/NiManager update are explicitly outside this frame prefix. This informs
future controller integration and does not establish gameplay acceptance.

The new tests first fail compilation on the missing baseline API
(S4/native-sequence-timing-baseline-01). Main-01 then retained one unit fixture
failure: literal1000.2f is not the clock accumulated from1000 with two float
frame additions. The corrected test uses the independently observed accumulated
clock1000.199951171875 and does not alter the already bit-matching rule.
Main-02 passes2006 component tests and481 TES4 ASan/UBSan cases, with exact
inventories/XML, no skips/failures/source drift; leaks disabled. Tested fingerprint
`ff76b838f0478d416073aa1d358472502fc94c6888601585f1a0fa0a9f12a072`. Runtime source is unchanged
from checkpoint65's720 engine/224 Python checks and four normal-input courses;
this helper is not yet called by the controller. S2/S3 remain in progress,
S4–S14 open. Next: persist the native global clock and sequence state, then wire
actual ordinary frame phases/contact in the verified order. Previous checkpoint65
commit7becfd8327962f51cfe076ecf6db27b419e7eb7e; verified bundle43 SHA256
62de8416be85f6be2012d10c70b7baf8a7b94952397db1ebe357948a8390ab61.

## Durable native animation clock and sequence timing (checkpoint67)

Runtime schema23 stores each native actor's float animation clock independently
of melee input/strike state, plus nullable per-strike sequence timing. The latter
keeps easing, optional offset/ease-start/previous-input initialization, ease end,
weighted history and displayed time. Clock ownership requires the existing native
actor/life authority, allowing Alive, Dead and EssentialUnconscious actors; clocks
survive strike cancellation/incapacitation and clear with the service. A timing
record requires its actor clock and canonical all-present/all-absent initialization.
Finite values, nonnegative clocks, identities, duplicates, bounds and flags are
validated before restoring or publishing state. Binary and canonical JSON and
the Python codec/migration change together. Legacy schemas retain null timing and
empty clocks; old-version downgrade rejects owned timing before capture mutation.

The combat service exposes validated clock reads/frame additions and owned-strike
timing publication. Actual service tests cover exact float additions, wrong actor/
action rejection, invalid duration/timing without mutation, save/restore, clock
survival through essential incapacitation and failed restore/capture atomicity.
These APIs are not yet called by Character; they do not advance ordinary phases,
dispatch contact, freeze a live sequence or synchronize rendered transforms.

A separate production C++→JSON/binary→Python exact-byte check revealed a genuine
signed-zero bug despite broad checks passing: integer-looking -0 lost its sign
through JSON parsing. Cross-codec-01 retains10 failed JSON cases out of16 (all16
binary round trips passed). New timing/clock JSON emits -0.0 for signed zeros;
cross-codec-02 passes all16 binary and JSON cases, with source and driver hashes.
Its driver SHA256
fb23b26bb0d2f587778eb12072b96491926f7812248ae0f345c34208c6e03d64.
Component tests exercise explicit signed-zero JSON and binary state, partial/
nonfinite/dangling timing, duplicate/overflow/noncanonical clock identities,
malformed boolean flags and bounded truncations. Python exact-wire and malformed
state cases independently cover the same new fields and old-schema promotion.

Baseline-01 preserves missing-state API build failure and two preliminary Python
fixture failures (obsolete current-version expectation; a terminal actor left in
an engagement). Main/sanitized-01 preserve a test API-name compile error. Checks02
passed2008 component/483 TES4 sanitizer/720 engine/226 Python before the cross-codec
bug was found and are prior-source evidence only. Corrected checks03 pass2008
component,720 engine,226 Python and ASan/UBSan483 TES4 component/720 engine cases
with exact inventory/XML, no skips/failures/source drift; leaks disabled. Tested
fingerprint `6e99b2cf8aafb0ae4b981e3bc73e5fe2532716197250c6a6a8707ea2238f3600`. Older missing-owner
initializer warnings remain within the open final warning gate.

S3/native-animation-timing-state-offscreen-01 uses normal configured keyboard
input, saves action1/ordinary Start/time0.4000000059604645, then resumes and finishes
it in a distinct process without another ID. Pristine schema21→23 migration
preserves all nine native actor value/life/breath/base/death/combat fields and
public80/80/140/alive queries. The new clock collection remains empty and timing
null, accurately reflecting unwired APIs; saved phases remain unadvanced. Both
captures were directly reviewed: textured prison and readable full HUD bars, a
later hand/forearm pose then idle, not saved-phase/damage evidence. Input copies,
source hash, process epochs and runtime binary SHA256
ec222722b37cca96070abc05704bfc7036e534e982b7621a5051b7efc1d3a9f1 verify.
The first verifier omitted sequence_timing=None in its complete expected strike
dictionary; verify-01.py/failure note remain and corrected verification includes
the predeclared nullable-field expectation. This is migration/continuation, not
nonnull clock/timing gameplay acceptance or full S4. S2/S3 remain in progress;
S4–S14 remain open. Next: wire ordinary frame timing and native contact dispatch.
Previous checkpoint66 commit d654f6211f803942731cbc859a783a69db621f0d;
verified bundle44 SHA256
74f898c117c678e89a978b9543adbe43172e63e523a215636e694e0879d27455.

## Exact renderer frame time and sequence timeline metadata (checkpoint68)

Native sequence time must reach the shared renderer without subtracting and
readding float remainders at intervening text keys. Animation now accepts one
absolute, monotonic renderer-track time for the next runAnimation call. The
supplied float remains intact across text-key traversal, takes precedence over
shared duration/speed multiplication for that group only, and is consumed once.
The existing callback and root-motion traversal remains in place. Absent groups
return false; nonfinite times, rewinds, times beyond stop and looping groups
reject before publication. The retained final pose emits no duplicate keys.
Default groups retain duration/speed progression. Scripted-only suppression
leaves an unprocessed group's pending time intact until it can be updated.

Controller-sequence metadata now exposes the original serialized cycle type and
actual normalized renderer timeline start/stop, alongside existing raw times,
frequency and authored keys. The loader records the actual multiplexed timeline;
alias/resource copies retain these values. This is needed to map native output
coordinates onto the renderer track and to reject unsupported cycle types.
Parser/component tests preserve a nondefault cycle and native coordinates;
actual parser/winning/playing/alias/lazy renderer tests cover timeline metadata.
The real renderer test crosses a small Hit timestamp where remainder/readdition
would change the result by one float bit; exact equality, one-call behavior,
invalid-state atomicity, repeated timestamps, once-only callbacks, final-pose
retention and looping rejection pass. There is no Character caller yet, no
nonnull native timing runtime course and no gameplay contact claim.

Normal checks pass2008 component and720 engine tests; ASan/UBSan checks pass483
TES4 component and720 engine tests, with exact inventory/XML, no skips, failures
or source drift; leaks disabled. Tested source fingerprint
`18b15ef19e909c64b81cd887207a343eb4359e02871d5d929a17b6db744bb1c3`; normal openmw SHA256
`9ae6d74e6b896825f35ce91b2f98408ce0528f7ff2733faaf9d0a5955734425b`. A GCC maybe-uninitialized warning remains in the preexisting
unpersistAnimationState/std::min path; its log is retained and the final warning
gate remains open. Python codecs are unchanged by this chunk, so their prior226
passing cases are prior-source evidence, not a new Python run.

Pinned-original caller audit S4/native-contact-caller-identity-inspection-03
resolves Player/Character/Creature virtual+304 to601790, with contact60193E before
animation60195C. The separate Player routine containing66CB49 starts66C6F0;
all three inspected direct callers load globalB333C4 as this. Its contact also
precedes its two animation calls. This is a bounded static/vtable audit, not
execution of full actor updates or proof of menu/first-person semantics. It
provides no reversed Player frame-order exception.

Original authored-blend probes identify byte storage and last matching Blend:
key wins. S4/native-blend-key-oracle-01 executes32 full raw loops with actual
integer parsing, including signed/modulo byte examples and CRLF duplicates;
only the existing diagnostic logger is supplied. Expanded oracle02 retains a
native fault for a leading bare CR without LF; no additional native function
stub was introduced. The malformed case remains outside its completed scope.
S4/native-blend-duration-prefix-oracle-01 executes3072 cases with no called
function stubs, full new-byte range, six prior-byte/null courses and both x87
words. Original474852→47488D uses max(prior,new)/30, or compiled default
0.10000000149011612 when zero. The divisor read at A3AA50 is30, correcting a
preliminary inspection assumption of100. These probes stop before special
context/override/global-speed/manager branches and do not implement or prove
live blend acceptance. Full normal-input contact, timing, interruptions and
S4–S14 remain open. Next: native ordinary timing/controller/contact integration.
Previous checkpoint67 commit21d7c7c5c1b515fab79f78eaa579f8e223a205b2;
verified bundle45 SHA256
828e6eb2fe2ad5a258a52635b53dcbcc21369e9b44ef30bed61d5b72f1c06c80.

## Ordinary frame authority and authored blend rules (checkpoint69)

The immutable ordinary frame transition composes actor-clock addition, active
sequence speed correction, strict single phase advancement and the subsequent
unsynchronized clamp sequence-manager time conversion. Easing and uninitialized
sequences skip active phase/speed correction; the manager initializes timing and
finishes easing inclusively, making the phase path eligible on the following
frame. Original mode5 clock freeze preserves clock/phase and an initialized offset while the manager
still updates its stored history. This does not model slot-only freezes3/6.
Finite/bounded inputs, positive speed/frequency, complete optional initialization
and ordered nonnegative four-key metadata validate before return. The result
contains clock, phase and the full timing history without mutating its input.

The combat service now advances an existing ordinary owned strike through this
transition. Stale/foreign IDs, powers and absent timing return false; timing
requires the actor clock. It prepares and validates the candidate before writing
clock/phase/timing with nonthrowing scalar publication. No ID is allocated or
consumed and no contact/resource/render callback is issued. Actual service tests
verify uninitialized rejection, activation/ease/phase order, failed-frame exact
save-byte atomicity, foreign/stale rejection, saved-history restart equivalence,
frozen progression and clock survival after strike completion. This method is
not yet called by Character, which still uses shared playback and unadvanced
ordinary phases; this is not normal-input timing/contact acceptance.

The raw Blend: parser preserves authored order, ASCII case-insensitive prefix
matching without space trim, NUL boundary and original CR/LF traversal. Original
atoi-compatible whitespace/sign/digit-prefix parsing stores the low uint8 byte;
last matching key wins, including zero replacing a previous nonzero value.
Supported integers are bounded to int32; overflow and a malformed leading bare
CR without LF reject explicitly instead of claiming original undefined behavior.
The separate duration rule selects max(prior,new)/30, using caller-supplied
validated default duration when zero. Special UI/root/forced-zero/global-speed
branches remain caller-owned and are not implemented by this duration rule.

S4/native-blend-key-oracle-03 executes1124 original full raw loops including
signed/full-byte-range/whitespace/last-key/NUL/CRLF cases, actual original decimal
conversion, and only the existing diagnostic logger boundary. Oracle02's native
bare-CR fault remains, with trace/registers and completed observations. Original
activation-prefix duration oracle01 supplies3072 cases with both x87 modes, no
called-function stubs. Native-blend-comparison-01 compares all4196 parser-byte/
duration-float cases exactly; corpus SHA256
83b2da352579a10948e5ef523741730b29f294fd3513811d1c49bbc724d66d4c.

S4/native-ordinary-sequence-frame-oracle-02 executes648 independent original
courses/9720 updates: actual6C9BA0 activation, ordinary476F95 prefix, then full
6CA950 manager update including actual time conversion and empty-controller
application. Original clamp/null-sync groups/managers are preallocated; slot3 is
supplied at476FA6. No called-function stubs. Other slots/full actor contact caller,
frame-time producer, visual transforms and early mode5 freeze are outside this
composed probe. Oracle01 retains a harness-only syntax failure caused by splitting
at rows=[] inside a loader string; corrected02 retains the complete loader prefix.
Native-ordinary-sequence-frame-comparison-01 matches every clock/phase/easing and
six timing float fields bit-for-bit for all9720 updates, including origin1e6,
nonzero begin, speed/frequency variants and zero durations. Independent report
SHA256 a85b0b14481564a5da27e35fe99cdac8757dff3e22d4fa339c8274526bc5f539;
comparison corpus SHA256
a6c2b03921313b8c78f85e5cd87625995b0f01a4e26b86738d5d129802322e1e.
No production-generated expectations or full gameplay claim.

Normal checks pass2010 component and721 engine tests; ASan/UBSan checks pass485
TES4 component and721 engine tests, exact inventories/XML, no skips/failures or
source drift, leaks disabled. Tested fingerprint
`5b1198a55187f6abc20e42a5338acac043850ab6e858e41d8968c52b6c974f8f`; normal openmw SHA256
`28b9ff1817e9037ed618f694dcbf2764f86967d0c715959ff2d1b7b816a8198b`. Existing RuntimeInventoryItem owner initializer and GCC
unpersistAnimationState warnings remain in retained logs; final warning gate
open. Schema23/Python codecs are unchanged, so prior226 Python cases remain
prior-source evidence. S2/S3 in progress; full S4–S14 open. Next: connect this
frame authority to Character/rendering and native contact dispatch, then complete
normal-input/restart/interruption courses.
Previous checkpoint68 commit548e6982922cf15b44f9b12b342ea75bfb234b94;
verified bundle46 SHA256
816f5222258c8c2595773c804c98a26420181f0616a623f7c82323867d802f27.

## Native ordinary playback in Character (checkpoint70)

Character now initializes durable sequence timing before ordinary playback and
advances the owned native actor clock, ordinary phase and full timing history
before renderer callbacks. The renderer receives the exact mapped native frame
output; intermediate text-key positions do not replace saved authority. The
phase opens queuing and ends the ordinary action independently of shared elapsed
playback. Idle clocks advance too; cancellation clears owned state before the
new timing path's renderer callbacks. Rejected replacements preserve the prior
strike. Powers and creatures still use legacy playback.

Admission validates complete authored ordinary keys, finite positive frequency,
ordered raw/timeline bounds and clamp cycle metadata. Blend duration uses raw
Blend: keys from the actual active renderer RightArm group and the incoming
sequence. This is not proof of original slot3 identity, special UI/root/forced
zero/global-speed semantics, hot-load source replacement or slot freeze3/6.
The read-only active-group query follows renderer mask/priority/tie selection
and performs no lazy loading. Legacy nullable timing migrates from the stored
pose without replaying keys; this is an engineered migration, not original
save/load parity. Native output that would rewind the current forward-only
renderer cancels explicitly; the full large-clock/rounding domain remains open.

Normal and ASan/UBSan engine checks pass721 cases each with exact inventory/XML,
no skips/failures/source drift; leak checking disabled. Tested fingerprint
`70288200585fe92dfc0d308cdc598f47a4b1c0fb7b81f2539042bf30d5651417`; openmw SHA256
`7db40c659483b6e9bfd723731737d94b52cfc841177a01f835956f8d06e8fb8c`.
Unchanged component/Python suites retain checkpoint69/67 evidence, not new runs.

S4/native-character-sequence-timing-offscreen-01 uses normal keyboard input and
stock first-person ordinary playback: saves action1 in Start with initialized
sequence timing and all nine actor clocks, then a distinct process resumes it
through Contact/Queue/End without a new ID. Native actor resource/life/base/
breath/death/engagement fields remain unchanged. Public pools80/80/140 and death0.
Original-code replay native-character-timing-runtime-oracle-01 matches the first
save's timing/clock and final clocks bit-for-bit in both x87 modes, using observed
origin/dt and independently audited installed keys. This replay supplies stop as
End; compared early outputs are below stop. It does not prove actual KF stop,
original normal-input/save-load or full actor-caller parity. Report SHA256
9a5cfe1f0765ff552668e165ef772401f59406cc413f5f9efbfdd1b39013e7ee.
The first process selects a power after the snapshot while input remains held;
that later action is outside the ordinary saved continuation. Both captures
were directly reviewed: textured prison/readable HUD; first later forearm pose,
second idle. Configured frame cap60; no measured render-FPS claim.

Native-character-legacy-timing-offscreen-01 restores schema23 action1 with null
timing/empty clocks, migrates and completes through phases1/2/3 without another
ID, rejection or resource/lifecycle change. Its idle prison/HUD capture was
reviewed directly. Native-ordinary-key-rejection-offscreen-02 reruns the genuine
synthetic Gamebryo20 missing-Hit negative control: normal input allocates no ID,
plays no strike; fresh-process continuation preserves next1/empty ledger and
all nine native actor field sets. Both idle prison/HUD captures reviewed.
Audio was muted in these courses; prior audio evidence is prior-source evidence.

Native-blend-context-prefix-oracle-01 additionally executes576 original prefix
cases including real UI singleton methods, supplied root identity/forced-zero
flag/global speed and both x87 modes, without function stubs. It bounds the
native context branches but does not establish engine UI/root field identity or
whole activation. Do not label Player+5D8 a first-person root from that probe.

Native contact/damage dispatch is still absent from Character. S2/S3 remain in
progress and S4–S14 remain open, including full input variants, mastery,
condition, reactions, opponent policy and all official-content campaigns.
Previous checkpoint69 commit db5d94273fa6ca531aea09d7cf9bbbfa808db819;
verified bundle47 SHA256
3c1acb5b5542d1f080bf04a00b88b953bb7908ea3e51b0d0f52966b786554ca8.

### Checkpoint71 — fractional native inventory condition

Projected TES4 inventory health now has an authoritative optional float, separate
from legacy integer/light charge, enchantment charge and TES3 remainder. NCHL
persists its exact float bits in shared reference saves. Integer display derives
ceil with saturation; stack eligibility compares exact health. Actor hydration,
player/native actor capture and native add-item preserve fractional health.
Runtime schema24 stores float32 condition; the double envelope preserves legacy
int32 schema4–23 values exactly. Downgrade rejects fractional, oversized and
negative-zero values; current conversion deliberately rounds legacy INT_MAX to
native float. Invalid nonfinite/negative health fails before inventory mutation.

Normal checks pass2031 components,723 engine and227 Python cases; ASan/UBSan
passes487 TES4 components and723 engine cases, plus19 explicit NCHL save-format
parser cases. Inventories/XML match without skips or source drift; leaks disabled.
Tested fingerprint3f49f7636ed2bc72166d1c389a8093ba8668e73f983daa8b64c610746f525ff3.
Current openmw SHA2562bf6efb218748890085c5db48992ecdc8461965f34bfd162fe8a95cf87fb5a1d.
S3/native-inventory-condition-cross-codec-02 verifies30 C++ binary/JSON to Python
byte-exact cases including signed zero, subnormal, near-full, overrepair and old
large integers. Copy/storage tests use protected stack insertion and public
stack predicates; they do not prove public GUI inventory transfers.

S4/native-inventory-condition-state-offscreen-04 completes saved schema23 ordinary
action1 through phases1/2/3, saving schema24 without an active action, then runs a
fresh idle process. Both scenarios and reviewed prison/HUD captures succeed.
The strict full-reference inventory verifier FAILS: first load adds78 references
(19 nonempty inventories), second adds146 (17 nonempty); none of the existing
first-save inventories changes or disappears. This retained failure is not a
full restart acceptance. No fractional wear, public transfer, contact, audio or
measured FPS acceptance is claimed. Contact/wear/breakage and S4–S14 remain open.

Retain earlier compile/fixture/runtime failures. Public headless inventory add
hit an unavailable WindowManager; the repaired fixture narrows its coverage.
GDB ptrace was unavailable and the attempted ASan preload targeted a linker
script, so neither diagnostic supplies sanitizer coverage. Interrupted builds
left two zero-byte generated objects; deleting only those objects and rebuilding
produced the passing fresh sanitized-07 run. Warning cleanliness remains open.
Previous checkpoint70 commit099fe16fe2e5bec394349d3160d277800db4778a;
verified bundle48 SHA2560dcd4bcfa1778ed09a50659d8f145f959ae421449cd127591f1a5e1fe3331073.

### Checkpoint72 — ordinary physical miss dispatch

Character reads the prior ordinary Contact phase before advancing the native
animation frame. The public service gate requires an owned pending action,
matching ordinary strike, uncommitted contact and Alive actor. The controller
resolves actual equipped native weapon identity, winning weapon weight/reach
settings and current race-height scale. The physical adapter distinguishes
unavailable context from an acquired victim and a real miss. Only a real miss
commits native attack fatigue through the existing atomic resource/ledger
transaction. Powers, creatures and acquired-victim policy remain open; this
chunk does not claim successful-hit damage/mitigation, wear or reactions.

The new gate assertion fails in retained native-ordinary-contact-gate-baseline-01
with the unimplemented false-return baseline. Final normal and ASan/UBSan engine
checks each pass724 cases, exact inventory/XML without skips or source drift;
leaks disabled. Fingerprint9c905bd968c236142f02fe91e2eeeeaf7da343f0f95212bf38cb068980434ff1.
Engine tests cover both ordinary variants, stale/wrong owners, prior Start/
Contact/Queue, power rejection, consumed contact, no duplicate fatigue, continued
follow-through and unavailable physics without spending or consuming an action.
Unchanged component/Python suites retain checkpoint71 evidence, not new runs.

S4/native-ordinary-miss-contact-offscreen-01 runs the actual engine with ordinary
action1 resumed from initialized windup: exactly one miss commit debits140 to133
before Queue; public GetAV later reports137 during native regeneration. The
final save restores140 and keeps next2/empty ledger/no strike. A distinct process
loads that save without contact, selection or restore replay. Both scenarios,
script compilation/diagnostics and directly reviewed idle prison/HUD captures
pass. Current executable SHA25677e49596329149c477d6e2f10fe90e63b4ddde7ec4f201224a174fb43001c8a0.
The retained first verifier copied an obsolete no-contact Fatigue140 assertion;
verify-02 follows the predeclared debit/regeneration range133–140 rather than
requiring observed137. Audio muted; no measured FPS or whole S4 acceptance.

Correction to checkpoint71's first-load addition counts: actual condition
runtime course04 adds1141 references,82 with nonempty inventories, rather than
78/19 from an earlier course. Its second adds146/17 as already reported. The
strict complete-map assertion remains failed. Subsequent diagnostic checks
confirm every existing inventory and all eight native field sets unchanged.
The new miss course also adds1141/82 then146/17; existing inventories remain
unchanged. Newly captured references are reported, not treated as complete-map
restart equality. The final idle AV equality alone does not prove fatigue
spending; the transaction/gate tests and earlier real GetAV/debit establish it.

S2/S3 remain in progress and S4–S14 remain open. Next: acquired-victim native
mitigation and reaction authority, remaining power/creature timing variants,
block intent and equipment condition consequences. Previous checkpoint71
commitb4ecf10ee5b40bcc1623491efdd7e21215d403e0; verified bundle49 SHA256
d2b2ba2d138bd24dcdfce1f375d8bc7f7a434d68dafb33f28c414d322838e813.

### Checkpoint73 — native live armor query

Native NPC and Player class armor queries now read actual native equipped armor,
condition and actor values, bypassing projected TES3 item ratings/Lua combat
formulas. Items are visited in native slot order with base-form deduplication
(the process shield slot is separate), light/heavy totals stored separately,
then native coverage/base-Light mastery, signed Defense AV43 and the winning
upper-only cap. Zero item maximum health gives condition ratio0. Fractional
condition and overrepair remain live inputs. Float skill/luck composition is
truncated once at armor's original caller boundary; per-modifier integer
queries and base/mastery flooring do not replace that read. Current supported
conversion domain is finite int32; native CPU overflow handling remains open.
Creature's distinct virtual query returns Defense directly, without NPC items,
mastery or total cap. Unknown projected armor, overlapping native slots and
unrepresentable projected armor-ring selection diagnose explicitly. Full native
equipment adoption/public transfer and all armor-slot projection domains remain
open; this read-only query introduces no second mutable inventory or cache.

The no-armor native Defense query fails against the retained baseline. Final
native-armor-query-main-04 and native-armor-query-sanitized-03 each pass727 engine
cases with exact inventories/XML, no skips/source drift; leaks disabled.
Fingerprint716c9c8f5a4c739ebab19dfc3cffd1c410756b3244775c60de807b139e1880ae.
Tests cover NPC signed Defense, near-full exact health, combined-slot armor,
float skill composition, Light Master/heavy denial, broken armor, winning cap
and disabled cap, Player versus Creature behavior and foreign/empty rejection.
The equipment fixture stages structural contents below GUI and pointer
registration callbacks, so it is not gameplay equip/wear acceptance.
Main-01 retains a fixture signature compile failure; main-03/sanitized-02
retain Creature fixture admission failure from missing RNAM reach. Corrected
fixture supplies reach64. Earlier main-02/sanitized-01 pass the narrower tests.
Unchanged component/Python coverage retains checkpoint71 evidence.

Original native-shield-query-inspection-01 executes100 getter/list/wear-bit cases
without executed function stubs: High-process64B2D0 arg1 calls484E80 and41DF10;
actually worn extra-data bits determine the shield entry, not weapon draw state.
Cached native-armor-cached-aggregate-oracle-01 executes2592 cases in both x87
modes through60E580's cache branch, Master multiplication, full5E0CD0 dispatch,
signed Defense addition and upper-only cap. Rank/coverage/Defense reads are
supplied boundaries; zero-cache inventory is explicitly empty. This is not
full item accumulation or actor/equipment adoption parity. Corpus SHA256
9e421f82baf0682fec6b5868a98cf70a50c56389b4911314293cfa20ad4c9860.

S4/native-armor-query-ui-offscreen-02 passes real inventory UI/save/restart.
Legitimate Console SetAV stages Defense12 once; normal Tab opens the actual
Player class query, visibly Armor12; Escape closes GUI and F5 saves. A distinct
process repeats Armor12 and saves without SetAV or any action/contact replay.
Only staged Player AV43/native base entry changes; all other native resource/
life fields, player inventory and every existing reference inventory remain
unchanged. New captured references322/28 nonempty then473/27 are reported;
whole-map equality is not claimed. Binary SHA256
7d7d75e7a949e7fefe5867b1a5cab55a617af09eb7dc2b2dd6737623a5f5e491.
All four captures directly reviewed: Armor12/readable pools and closed textured
prison HUD. Inventory icons have magenta backgrounds and preview is incomplete;
visual parity remains open. Audio muted, no measured FPS claim. This is a
staged stat/UI query diagnostic, not worn armor, mastery, wear or hit acceptance.
Course01 retains the failed save after Tab navigated focus instead of closing
GUI; source KeyboardNavigation/ActionManager establishes Escape's actual exit.

S2/S3 remain in progress; acquired-victim damage, mitigation ordering, block,
knocked/reaction authority, powers/creatures and S4–S14 remain open. Previous
checkpoint72 commit5955b999ecfcdf619fd6353322b61e08d5d1ebe2; bundle50 SHA256
021d3408af2d6643f6eb361e84bb5a8025ea39484aefe8264f650acd94bab0b4.

### Checkpoint74 — signed armor mitigation and nonpositive condition wear

The native armor query introduced in checkpoint73 can return negative Defense.
Physical mitigation now accepts finite signed ratings and applies the original
upper-only fraction cap. Negative fractions amplify incoming damage and can
produce negative wear; condition mutation accepts finite signed wear and
preserves current condition exactly for nonpositive wear rather than repairing
it or snapping an existing sub-one condition to zero. Negative damage/current
condition/settings and nonfinite inputs remain rejected in their own domains.

S2/signed-armor-baseline-01 retains the failing newly introduced regression.
S2/signed-armor-main-01 passes all 2,032 normal component tests and 488 ESM4
ASan/UBSan tests, with exact inventory/XML agreement, zero skips and no source
drift. Sanitizer leak detection is disabled. Tested source fingerprint:
cb63b6035a9d69161e6490bc62cef0cd5f807e29a419639a7f1fb96e4a41763b.
Existing missing-initializer/dangling-else warnings remain recorded. Unchanged
Python/engine results are retained from their previous checkpoints; no new
engine or normal-input hit/wear acceptance is claimed for this pure change.

S4/native-signed-armor-oracle-02 independently executes the pinned original
5FF712..5FF7BD mitigation/bypass/cap continuation and full 547260 wear helper,
plus the 5F3870 nonpositive condition prefix: 1,306 cases pass in both x87
precision modes. Virtual armor-query values and signed contact sentinel are
supplied boundaries; sentinel negative/zero/positive branches are all checked.
Corpus SHA256: 2eb9a7067285ecf6a6497c502ce43e1b923dc38a8a980827508cc6ed502b9c69.
Literal outcomes include damage100/rating-8 -> fraction-.08, damage108 and
wear-72 with wear multiplier9; condition100 remains100. Signed-zero results
are bit checked in the oracle. The loader's unrelated hooks are not invoked.
Oracle01's 442-case narrower successful course is retained alongside two
failed harness setups: a positive signed sentinel skipped wear, and the first
condition setup also used a null entry. Corrected entry/sentinel inputs precede
successful execution; these failures did not cause a production relaxation.
Positive condition/inventory writing, wear selection and the full hit caller
remain separate gates.

S2/S3 remain in progress and S4–S14 remain open. Next: acquired-victim native
damage, difficulty/profile policy, block intent, reaction authority and actual
equipment wear consequences. Previous checkpoint73 commit:
0ceea06fdb8071ed00be42c806536c7534c292b6. Verified bundle51 SHA256:
39bbfea2af57c2855fce26b98ff61930ea2d3248371951e956c7453ccd022592.

### Checkpoint75 — versioned native process knocked byte

Runtime schema25 adds an optional signed int8 process knocked byte to native
actor values, with identical C++/Python presence and value encoding. Known zero
is distinct from unknown. Legacy schemas1–24 do not invent a lost process byte;
24 binary decode/reencode remains exact, and promotion to25 retains unknown.
Known values require25 and an Active process; lossy downgrade, invalid presence
markers, Python bool/float/string/out-of-range inputs and known Low-process
bytes are rejected. JSON uses signed integer output rather than character text.
The new field is an immutable snapshot member at this checkpoint: live service
construction, process changes, reactions and contact queries are the next work,
so no new gameplay knocked-state authority or hit acceptance is claimed yet.

S2/native-knocked-baseline-01 retains a failed new assertion showing the declared
byte was lost by the unimplemented codec. S2/native-knocked-codec-main-01 passes
all 2,034 normal component tests, 490 ESM4 ASan/UBSan cases and 228 Python tests,
with matching inventories, zero skips and no source drift. Leak checks disabled;
existing initializer/dangling-else warnings remain recorded. Tested fingerprint:
5c6edea41fcd077f20cafac5aad441d59fc62b669ded031615982864b98ed758.
S3/native-knocked-cross-codec-01 passes 516 C++ binary/JSON -> Python reencode
byte-equality cases: both actor owners, all256 signed bytes and unknown in25,
plus legacy24 unknown. Probe binary SHA256:
f19c60d080bc356498ae41e69091e2a625b98510075c46b01c1af55ce84e2071.
Codec tests do not establish process adoption or normal-input runtime acceptance.

S4/native-knocked-state-oracle-02 passes 1,281 independent original-instruction
cases using actual process vtables High A71814, MiddleHigh A72684, MiddleLow
A72D64 and Low A720A4 with synthetic actor/process objects. High/MiddleHigh
+2E8 dispatch to629440 (low-byte setter), +2E4 to64B080 (signed getter);
Low/MiddleLow dispatch to68F970 (no-op setter),6F7070 (zero getter).
Common actor5E04F0 tests any nonzero result. Every raw byte is checked for each
process tier, plus256 constructor-store cases at64B52D with EBX zero as set by
64B42F and a null-process false query. No hooked game functions execute.
Corpus SHA256: 72183631c01f7a1a8749af2f75f5e91eb71c9f341b73b9a55fa79b8d5544a8fb.
Oracle01's narrower513-case course is retained. The constructor slice does not
prove full allocation/base construction, process replacement/save, or meanings
of individual nonzero states; no TES3 animation enum is substituted.

S2/S3 remain in progress; S4–S14 remain open. Next: wire the native process byte
into live construction/query/reset/restore before unarmed fatigue contact, then
acquired-victim damage and gear consequences. Previous checkpoint74 commit:
b8c99947820194adf70969da81f73bfeea9cb755. Verified bundle52 SHA256:
f1d88b09c68566ad8b66a1f3b6477d10dfebffb6ebc27be5f3f7c799b579d7be.

### Checkpoint76 — live native process knocked-state authority

Fresh native NPC/Creature/Player Active construction now initializes the signed
process byte to zero. The combat service owns raw-byte queries and transitions:
Low returns zero and ignores writes; Active returns the stored signed byte;
missing authority and unknown legacy Active state are diagnosed. The world
adapter validates the actual Player/NPC/Creature binding before querying.
No shared animation/recoil flag is imported. Low-to-Active construction supplies
zero; same-process activation preserves known or unknown restored state. Full
resurrection reset clears nonplayer state with its Low process and recreates
Player Active state at zero. Capture rejects known bytes below schema25.
Raw-byte writes preserve AV channels and lifecycle rather than inventing the
meanings of nonzero values. Actual knockdown/essential-entry/recovery reaction
transitions and renderer/controller responses remain separate work.

S3/native-knocked-live-baseline-01 retains the expected failures against fresh
construction and first signed transition. Main01 retains two failed old whole-
snapshot assertions at Player full reset and restored Low-to-Active promotion.
Both expected snapshots now include the independently established constructor
zero; their AV/lifecycle/death-history assertions remain. Player reset is also
staged with raw-128 to verify clearing an actual known nonzero byte.
S3/native-knocked-live-main-02 and native-knocked-live-sanitized-01 each pass all
730 engine cases, with exact inventories/XML, zero skips and no source drift.
ASan/UBSan engine leak detection is disabled. Tested source fingerprint:
a0919f55f23a097ac1f6f9ba8da1c0b69b0a28b4d973b26f9434bb723727e882.
Tests cover every signed NPC byte, actual Player/Creature aliases, Low no-op
writes, typed binary/content restore, unknown legacy state without inference,
activation/reset, missing actors, foreign profiles and lossless captured state.
Unchanged component/Python/original-process results remain checkpoint75 evidence.
Existing compiler warnings are retained; no fresh compiler/editor/full campaign
or performance acceptance is inferred from these incremental checks.

S4/native-knocked-save-ui-offscreen-01 passes a staged persistence diagnostic.
An editable isolated schema25 fixture supplies Player raw-128 and retains null
for all other legacy bytes. Legitimate Console SetAV stages Defense12, native
inventory UI reads Armor12, Escape closes it and F5 saves. A distinct process
loads that save, repeats read-only queries/UI and saves without another SetAV
or combat action. Both saves preserve raw-128 and every other unknown byte;
only the declared Defense/base-extra writes change. All other resource/life/
death/engagement fields, Player inventory and every existing reference inventory
remain unchanged. New captured references322/28 nonempty then473/27 are reported;
whole-map equality is not claimed. Idle action namespace stays next2 with no
pending owners/strikes, and no contact/selection/restore replay is logged.
Executable SHA256: 4babe7beb0e3347b0fd608a6d902c20f64d4243a01c6e89222804212b36babd7.
Distinct epochs: PID13/start13884707 then PID12/start13896397, boot
 a80f9d5d-d691-4275-a06f-c8a6dcb4a3f8. All four captures directly reviewed:
readable Armor12/pools and textured closed prison HUD; magenta icon backgrounds
and incomplete body preview remain open. Audio muted; no measured FPS claim.
The first preparation's nonexistent validate_state call failed before any engine
launch; the retained explanation records correction to encode_payload/write_save.
The staged byte is not a normal-input knockdown and does not establish its
animation, reaction, fatigue-contact suppression or recovery gameplay acceptance.

S2/S3 remain in progress and S4–S14 remain open. Next: native acquired-victim
contact damage and mitigation ordering, then block/gear/reaction consequences
and remaining power/creature animation paths. Previous checkpoint75 commit:
23f2065c1bb6bfc841f048f07bc38635f5aa395d. Verified bundle53 SHA256:
25c4923effae2ebfeef4a9dd231608428c132b27dd06a6336672e6b0d1931539.

### Checkpoint77 — native post-mitigation contact fatigue

Added the explicit native contact Fatigue rescale: store the post-armor/block
Health divided by pre-armor Health ratio as float, then store incoming Fatigue
times that ratio. Difficulty follows this step and changes Health only. Zero
incoming Fatigue skips division and preserves signed zero. Invalid inputs and
nonfinite/singular intermediates are diagnosed; native singular sink behavior
and its eventual caller policy remain open. This helper is not yet wired to
acquired-victim contact or claimed as a completed gameplay gate.

S4/native-contact-fatigue-oracle-01 executes the hash-identified original
5FFEB5 branch and 6000CC ratio/product stores, with explicit no-damage exits,
without invoked game-function stubs: 81 unique inputs, 162 cases across x87
precision controls027F/037F, all pass. Corpus SHA256:
52edf523ee15568ae87f7896cdcf57c474411a434f791d1f4e9633a207f13020.
The checked-in immutable expectations come from those original instructions.
Fatigue100 times Health1/3 yields float bits42055556 because the ratio is
stored before multiplication. Tests also cover full mitigation, zero Fatigue,
signed zero, malformed input and overflow. S2/contact-fatigue-baseline-01 retains
expected failures against the stub. S2/contact-fatigue-main-01 passes all2036
component cases and492 ESM4 ASan/UBSan cases, exact inventories, no skips,
failures or source drift; sanitizer leak detection remains disabled. Tested
fingerprint44fc3b9b9d82f65f9e2eb76b6c1554bb75ba6465eb9b16174301096af015703a.
Unchanged engine/Python evidence remains in checkpoints75/76.

S2/S3 remain in progress and S4–S14 remain open. Next: acquired-victim damage
integration, native mitigation/block/gear/reaction policy and the remaining
stages. Previous checkpoint76 commit cdb1715f9704081625b7174adeafbdbcb29ad154;
verified bundle54 SHA256 d5c99e9ede095068386e8d0fd4d825578fc10c3792e2df6b2ae8df5f63d3b197.

### Checkpoint78 — live native unarmed damage inputs

The world adapter now resolves actual Player/NPC integer HandToHand/Luck/
Strength getters, current Fatigue, the separate floored base Fatigue query,
and the actual Player/NPC/Creature victim process knocked byte. It reads
winning native hand/physical GMSTs and computes pre-mitigation damage through
the independently verified original caller rule. Creature natural attacks
are a separate path. Missing/foreign actors and unknown legacy Active victim
state are diagnosed without writes; Low victim processes query zero. This
is a read-only contact prerequisite, not acquired-victim damage publication.

Three engine cases cover1024 NPC input/process combinations and256 Player-to-
Creature combinations using recorded original checkpoint50 expected values;
Player self-query, Low/unknown victims, actual aliases and wrong attacker/profile
are also covered. An NPC fractional-modifier discriminator reads float skill11
but native integer skill10, proving the separate getter boundary. Exact binary
snapshots preserve all registered resource/life/action state during queries;
Player/Creature snapshots preserve their values. Winning setting changes are
read immediately. The malformed-type test replaces the same GMST FormId and
checks the exact type diagnostic, avoiding duplicate-name rejection as a false
positive.

S3/native-hand-contact-main-01 retains two fixture failures: the attribute/skill
request API rejected staged Fatigue, and a serialized state omitted the second
actor reference. Fixtures now use the native Fatigue writer and complete
reference inventories; no production validation was relaxed. Main02 and
sanitized01 passed before the malformed-type assertion was strengthened.
Final main03 and sanitized02 each pass all733 engine cases, exact inventories,
zero failures/skips and no source drift. ASan/UBSan leak detection is disabled.
Tested fingerprintad6439c9db7bf8d7a5ac00d33e61e58ae1debce5fa99fbd9de5384f57c37519c.
Unchanged components/Python evidence is retained from77/75 respectively.
No new normal-input hit, block, wear, reaction or campaign gate is claimed.

S2/S3 remain in progress; S4–S14 remain open. Next: finish the native contact
mitigation/sink pipeline, difficulty binding, block intent, gear/reactions and
acquired-victim runtime publication. Previous checkpoint77 commit
bd44e2742dfd202b601bb5628922d955f6088b35; verified bundle55 SHA256
60d7954ba9f433f04985e0f3b186259a39a0005710f12f6843b0ccb2205751d2.

### Checkpoint79 — native contact resource writer selection

Added physicalContactDamage: rescale incoming Fatigue using post-armor/block
Health divided by pre-armor Health, then apply difficulty to Health only and
select positive resource writer amounts. Zero original and remaining Health
with nonzero Fatigue follows native NaN suppression and produces no Fatigue
writer, without putting NaN in saved AVs. Positive remaining Health with zero
original produces native infinite Fatigue; that nonfinite writer domain is
still diagnosed, as are overflowing finite inputs. The raw ratio-store helper
retains its stricter intermediate contract. Signed-zero amounts select no
writers. Actual engine contact publication, gear/block/reactions remain open.

S4/native-contact-sink-oracle-01 executes the full original common sink5E58F0
and difficulty5E2560:600 cases/300 unique inputs in both x87 modes, covering
Player source, victim, self, neither and null source across difficulty and
zero/positive/NaN/infinite Fatigue. Fatigue is never difficulty-scaled. Corpus
SHA256e8928f0511aa3c65986b2cd500c04a359411abc1e908ef73d62c5f0f74808a92.
Score-context getters return null, AV writers record arguments, and the dead
getter returns false; opponent maintenance and actual lifecycle writes are
not established. S4/native-contact-fatigue-oracle-02 extends the original ratio
probe to166 cases/83 inputs including zero denominator, corpus SHA256
12a0ae8646ab70466aa69ed4e68f2c3d63bcf9c560f9ecdad448dfae45a3a296.
No game-function stubs are invoked in the ratio probe.

S4/native-contact-pipeline-oracle-01 carries exact original ratio-store bits
into the full sink/difficulty path:250 cases/125 inputs, both x87 modes, corpus
SHA256fcd35fa6ad485dfdba106a6a2678f7d5a1a1e3c68034f683d4fc420c04b50696.
It uses the same explicitly supplied score/writer/dead boundaries as the sink
probe. The checked-in125-row immutable expectations record actual resource
writer arguments; all25 infinite-writer rows explicitly expect the finite
runtime diagnostic, rather than claiming parity over unsupported storage.
Additional tests cover malformed values, zero-writer validation, signed zero,
invalid roles/sliders/settings and overflow. S2/contact-pipeline-main-01 passes
all2038 component cases and494 ESM4 ASan/UBSan cases, exact inventories,
no skips/failures/source drift; leak detection disabled. Tested fingerprint
b5f4e70aa6511e82c01082eb0cdc9919f3271c0bf6624f5fa727977ab649c8c8.
Unchanged engine/Python checks remain78/75 evidence; no new runtime gate.

S2/S3 remain in progress; S4–S14 remain open. Next: native held-block control
and process authority, difficulty binding and acquired-victim damage with
condition/reaction consequences. Previous checkpoint78 commit
6ea4635e484bbc41787ebaed3df939ed3bc13211; verified bundle56 SHA256
1f40b902a62191332258670fbeba3789d6c482790ca8c0e17835d69acdfe6782.

### Checkpoint80 — versioned native process action code

Schema26 adds optional signed int16 process_action to C++/Python native actor
values. It follows the schema25 knocked presence/value in the wire and precedes
Player form values. Known codes require Active process and version26; older
schemas retain absent/unknown action without inventing posture. Known zero and
minus one are distinct from unknown. Lossy downgrade, malformed presence,
wrong JSON type/domain and Low-process known codes are rejected. Native
blocking predicate compares the supplied process action exactly to6. Held
input alone is not posture. Live constructor/setter/controller/renderer action
integration and held-block controls remain separate work.

S4/native-process-action-oracle-01 executes65,577 original cases: all65,536
signed High action words through actual raw setter628220 (+2D8), signed getter
628200 (+2D0) and common block predicate5E5670; eight memory profiles each for
MiddleHigh/MiddleLow/Low actual vtable getters6F7030 returning-1; one null-
process predicate; sixteen constructor prefixes628F12..628FB9. High action
storage is process+1F4, action data+1F8, and constructor initializes-1/data0.
No game-function stubs are invoked. The full higher-level selector63C730,
action-data meaning, held-input scheduling and renderer transitions are not
executed or claimed. Corpus SHA256:
74f3a35463cb4d2410ba35a6dbc1728e4bef89e60052b9458c58fad0763e269a.
The fork's Active process is its admitted High-process adapter; other original
process tiers are not silently equated with High blocking posture.

Both codecs test every65,536 signed action word. C++ also checks canonical JSON,
legacy25 byte round-trip, unknown promotion, corrupted presence, downgrade
and process validation. Python additionally verifies exact little-endian wire
insertion for each word and rejects bool/float/string/out-of-range metadata.
The pure block predicate covers all words and absent process. S2/native-action-
codec-main-01 passes2041 component cases,497 ESM4 ASan/UBSan cases and229 Python
cases, exact C++ inventories, zero skips/failures and no source drift; sanitizer
leak detection remains disabled. Tested fingerprint:
237c4bf35faf88158900999c78a39b9ec4cd1efa99e9865033a739b8c91631e7.
S3/native-action-cross-codec-01 passes40 C++ binary/JSON-to-Python reencoding
cases: signed boundaries/unknown with both owners in26 and legacy25 unknown.
Probe binary SHA2560d5bd54503b38c6d59631fd1e8c920daa4797a4597f74f7ef9f0482a80ef1ec5.
No new gameplay/persistence UI course or stage gate is inferred from codec
coverage; unchanged engine evidence remains78.

S2/S3 remain in progress; S4–S14 remain open. Next: live action construction/
query/reset/restore and native held-block control/posture scheduling, followed
by actual contact damage, condition/reactions and all later acceptance gates.
Previous checkpoint79 commit7fd7ce33cc75cd452861ebd3a772b42a97c831dd;
verified bundle57 SHA2565ce06ecf7873fee920dd4e914b4d1bdf1a6faba7b14ba4a68b589604c321286c.

### Checkpoint81 — live native process action authority

Fresh native Active Player/NPC/Creature construction initializes the signed
process action to-1. Combat-service queries return known Active action codes,
return-1 for Low, and diagnose missing/unknown legacy Active authority. Low
raw-code writes do nothing. World adapters validate the actual Player/NPC/
Creature binding before querying; native block posture compares exactly6.
Raw-code writes do not mutate AV/life/knocked fields, select animation, or
pretend to execute the original higher-level selector/action-data lifecycle.
Low-to-Active construction initializes-1, same-process activation preserves
known or unknown state. Full reset clears nonplayer action with Low process,
and recreates Player Active action-1. Capture refuses known action below26.

Three new engine cases cover all65,536 Active NPC action codes with actual
world queries/exact-six posture and every other actor-value field unchanged;
all65,536 Low setter no-ops; typed binary/content restore; unknown legacy25
without inference; activation/reset/missing/foreign actors; and actual Player/
Creature aliases at signed boundaries. Existing Player reset and restored Low
NPC expected snapshots now include the independently verified constructor-1;
all resource/life/death-history assertions remain. Legacy24 knocked fixtures
explicitly remove the new action field as well, preserving their intended old
schema rather than relaxing the downgrade validator.

S4/native-process-action-oracle-02 extends checkpoint80 with actual lower-tier
raw setters60CF60 (ret8) leaving action/data unchanged in all24 lower profiles.
All65,577 cases pass; the expected query/constructor corpus remains identical,
SHA25674f3a35463cb4d2410ba35a6dbc1728e4bef89e60052b9458c58fad0763e269a.
No game-function stubs are invoked. Full higher-level action selection, action-
data meaning, held-input scheduling, renderer and gameplay remain outside this
probe. S3/native-action-live-main-01 and native-action-live-sanitized-01 each
pass all736 engine cases, exact inventories, zero failures/skips/source drift.
ASan/UBSan leak detection is disabled. Tested fingerprint:
17b42aa5f5acf811c3f78810d36c0d93e46797604ed49f34bd7392c5e4132a03.
Unchanged component/Python/cross-codec evidence remains80. No fresh runtime
block/animation or campaign acceptance is claimed.

S2/S3 remain in progress; S4–S14 remain open. Next: held-block input and native
posture/animation scheduling, including release/menu/cancellation/save callback
ordering, then acquired-victim damage with difficulty/condition/reactions and
all later gates. Existing ordinary strike begin/end also need to publish the
appropriate native process action; raw action getters alone do not establish
that dispatcher. Previous checkpoint80 commitd3ac10ac4649c04caea739156b37d5b66221beec;
verified bundle58 SHA256b499415579dcfd485f6a940de0239d8ac032585c54333910d2f496820d9d8f28.

## Ordinary WEAP entry and separate AttackBonus query

Full original484F80 takes the actor and a float attack multiplier, not an actor
and a boolean. Its weapon branch calls float AV virtual+288 for Luck7,
Strength0 (Agility3 for bow), and type-selected skill. Actual4BB060 maps types
0/1→Blade14,2/3/4→Blunt16,5→Marksman28. Each float is stored and9828C0
converts it before full547070. Current item condition is read as double and
divided by the unsigned original maximum before its float ratio store.
485125 then queries integer virtual+284 for AttackBonus42 and adds that signed
integer to the stored product. A zero supplied multiplier leaves only that
bonus; it is not an ordinary damage outcome.

S4/native-weapon-entry-oracle-02 runs the full entry/product with ordinary1 in
both x87 modes,1152 observations/576 profiles and two16.625 original sword
controls. Boundary readers supply current float AVs, base Fatigue, damage and
condition; skill selection/conversion/ratio/product/bonus addition execute in
the pinned original executable. Corpus SHA256
9d26ff1d49f3756c612dd5f6548f47c15b8ca43027845da9b1d4ac773bbdf51f.
Oracle-01 supplies multiplier0 and is retained solely as a corrected-context
control; the first C++ corpus comparison fails, exposing that setup mistake.

The new actual World query supports only ordinary melee WEAP types0..3,
Player/NPC equipped-instance bindings and finite original maximum>0. NPC
float and integer modifier paths remain distinct, including signed bonuses.
It validates authority before inventory caching and reads native condition
and winning definitions instead of the TES3 projection. It does not dispatch
contact, mutate condition, resolve immunity/block/reactions or execute magic.
The repeated-query/state tests and sanitizer checks are engine integration
evidence; actual normal-input sword/contact/wear campaigns remain open.

## Condition wear reads before float publication

Original5F3870 skips nonpositive wear before reading condition. Its weapon branch
calls full484850 with argument0, subtracts the float wear in x87, stores the result
once at5F393C, then replaces a stored result below1 with0. Full484850's absent
condition path FILDs the maximum and corrects unsigned values with2^32 before
return; present condition retains its loaded float. Therefore an unsigned maximum
16777217 with wear1 publishes16777216, not16777215 from an early float conversion.
Nonpositive wear does not turn an absent full-condition instance into an explicit
float condition.

`S4/native-condition-write-oracle-03` executes these paths in the pinned original
exe under both precision words:160 cases/80 profiles and two nonpositive controls.
Corpus SHA256809a388c6e180c97281873d933520bb28475a3b0b463985c0d4d1e59ca2e817c.
RTTI/maximum/extra-data and publication boundaries are supplied. Actual armor
mastery, equipment break/drop/unequip/audio and runtime acceptance are outside
this probe.41E6F0 is RET0; the four pending arguments are consumed by488830 RET16.
The first two probes supplied wrong cleanup and are explicitly invalidated;
their zero corpora must not be reused as expected values. A corrected red test
fails12 comparisons on the premature Float reader before the new helper fixes it.


## Ordinary weapon contact runtime controls

`S4/native-weapon-runtime-oracle-01` uses installed WEAP090615 rusty shortsword
(native damage5/max56/weight8) and original484F80/547240/5F3870/484850 at ordinary
multiplier1. Condition55.875, Blade10/Luck50/Strength40/current and base Fatigue140
produce Health bits1062506496, wear bits1050253721 and condition bits1113476301
in both x87 modes. Entry AV/base/damage/condition and publication boundaries
are supplied as explicitly documented in checkpoints91/93; collision, armor,
block/reactions and attack debit are not emulated here. Normal and sanitizer
engine courses then independently verify exact Health and condition saves once
and no fresh-process replay with actual collision/LOS and normal attack input.

`S4/native-weapon-difficulty-oracle-01` executes full5E58F0/5E2560 with Health16.625,
Fatigue0, normalized difficulty1 and winning multiplier5: Player-source Health
bits1076974933 (2.7708332538604736), Player-victim1120370688 (99.75), neither
1099235328 (16.625), both precision modes/six observations. Difficulty factor
is1+scaled=6; the first engine expectations incorrectly used5 and are retained
as failed tests. Source snapshot/ENAM baseline failures and the corrected red
resolver assertion remain separately identified in the milestone report.
Physical weapon contact now publishes prepared condition and damage atomically;
unsupported positive armor selection/block/break/stealth/resistance/ENAM and
later gates stay open. Post-hit HUD captures do not prove contact-frame held
weapon rendering or sound.

### Native initialized random stream (checkpoint95)

Pinned executable SHA-256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Original47DF80 nonzero argument invokes9859D0 and clears initialization byte
B069C3; initialized zero-argument path tails9859DD.9859D0 stores seed at
TLS+14;9859DD uses unsigned32 multiply/add214013/2531011, stores next state,
then logical-shifts16 and masks32767.98C0F5 TLS accessor is the sole supplied
boundary in `S4/native-combat-random-oracle-01`; caller removes seed argument
(cdecl). All paths return to the sentinel at expected ESP.256 profiles under
027F/037F plus100k consecutive original draws; frozen corpus
`532bb579f9297bd63d80214903c5d8242647b7f38db64d9cb1ab16332b278fa0`.
Armor caller5E5A00 divides by100 with IDIV and uses remainder. This confirms
native sequence/modulo bias; own combat stream publication/save ownership,
automatic wall-clock initialization and global world call interleaving are
separate claims. No armor mutation, contact, reaction or gameplay gate closed.

### Complete armor-selection loop (checkpoint96)

Pinned original executable SHA-256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
5E5A00 initializes attempt0, checks >=7 at5E5A40, obtains47DF80(0) draw,
uses IDIV100 remainder, falls through missing head/hair/upper/lower/hands
within ordered cumulative thresholds; missing feet retries without choosing
shield at that threshold. Each no-selection increments and retries; first
selection exits. Shader/rendering is unrelated. Original47DF80/9859DD
executes on every draw, with only98C0F5 TLS accessor supplied.41E6F0 RET0
and486790 RET8 provide fixture inventory/equipment, process+F8 RET4 supplies
shield entry,401F00 allocator/41E860 count/484420 RET8 constructor/446CB0
RET4 attachment supply successful temporary shield construction. FS exception
chain storage is mapped; final EIP/ESP verified. Full original loop tested
for128 availability masks, four chance tuples and four seeds, bothCW:2048
profiles/4096 executions. Corpus
`26b61cc8ef610f72aa359c86741442682c1c6dcd8ab095f2ffafc7c7675a11fc`
at `S4/native-armor-selection-oracle-01`. Slot, final RNG state and actual draw
count become frozen C++ expectations. Wear application, mastery, broken-item
unequip, live callbacks/save scheduling and gameplay acceptance are separate.

### Dedicated combat stream ownership (checkpoint97)

The plan permits a dedicated reproducible stream. The combat service uses the
original initialized CRT sequence established in checkpoint95, independently
of AI scheduling, with declared seed1 for new/legacy state and full uint32
including0 in schema27. Owned contact preparation validates its full1–32 draw
transition and publishes it atomically with wear/resources/lifecycle/action
consumption. This is an OpenMW ownership/save contract, not a claim that the
original game's shared global world random-call interleaving is replicated.
The normal input/restart seed0 course makes no combat draw (armor0) and shows
independent AI progression, while unit integration covers actual draw publication
and duplicate/restart rejection. Native armor selection/draw policy and other
reaction/crime random consumers remain separate integration work.

### Armor wear admission and float publication (checkpoint98)

Pinned original executable SHA-256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
5F3870 checks incoming wear>0 before ARMO type20/mastery handling.4B4C70
reads armor+6A heavy bit;5F23B0 queries original floored base skill via
5F1910/5EAD00, then56A300 gates25/50/75/100. Novice uses B374F0(light)/
B374F8(heavy), Journeyman+ B37508(light)/B37500(heavy), Apprentice1.
5F3927 stores scaled wear as float, then484850 double current reader precedes
subtraction/final float store and below-one snap. Mastery bypass skips scaling.
Zero multiplier does not undo incoming-wear admission: the condition writer
still runs. `S4/native-armor-condition-oracle-01` records896 profiles/bothCW,
corpus `6e8dae964d09de8d31c1578f10fad472ae1b3ce8024c98b71bdaa0e0a1ef3d42`;
readers/settings/reference/writer/refresh/nonshield boundaries declared in
its report. Original complete body helper returns broken flag; live caller
unequip/drop/render/audio remains unexecuted.

An additional read-only boundary audit for upcoming integration,
`S4/native-armor-slot-lookup-oracle-01`, executes full486790,4691B0 and468FF0
for3072 integer-path cases: six body slots, every6-bit mask, ARMO/CLOT,
worn/unworn and armor-only flag.486921 checks flag1 then requires base type20,
so matching worn clothing returns absent. Full successful temporary allocation
and original copy code execute; supplied worn predicates/allocator/reference
attachment recorder/count getter are declared. Single-count entry; stack split
and conflict precedence unverified. Corpus
`8892d97c057bcabebed1c1ca6c68218010deea1fe95a2433c0a38990b778d87d`.
This audit defines the availability boundary for later live selection; it is
not a claim that condition publication is already integrated there.

### Equipped armor entry rounds before aggregation (checkpoint100)

The existing547370 formula returns fractional armor after condition scaling.
Full caller488CB0 additionally converts that float with9828C0, compares the
fraction with Double.5, and adds one at/above half before publishing an
integer-valued float. Every equipped entry is rounded separately before
60E580 accumulation, mastery and signed Defense. Applying rounding only to
the total or preserving the raw formula fraction changes live mitigation.
`nativeEquippedArmorRating` models this wrapper; `armorRating` remains the
independently verified formula. Unsupported conversion overflow remains an
explicit rejection, not a guessed original int32 overflow policy.

`S4/native-armor-entry-oracle-01` executes14 cases (7 profiles × both x87
words): official Iron Cuirass parameters1000 hundredths/max300, Heavy skill5,
Luck50 and conditions0/50/100/150/250.125/300/350. Results0/1/1/2/3/3/4.
Corpus SHA-25677e281752ff2ff33e2d68c0b2d737002a1bf4cb516944356f91e73425e9058d0.
`-02` adds the existing Light1499/max100 fixtures, half/fractional/full
condition and skill50/56.5/100: 12 executions,6 profiles. Half condition
produces5, just-below-full produces9. Corpus SHA-256
0e82e0d6ca2ab413183b538d0978794a42726e55d3a8e97c408040253006eb86.
Full488CB0,4B4C80,484850,547370/547B90 and9828C0 execute. Supplied boundaries
are native max-health reader, actor float getter and ExtraHealth float reader;
all original conversions and final rounding execute. Actor Luck is50. Both
control words agree and EIP/ESP returns are checked. No whole hit/equipment
flow is implied by these instruction paths.

`S4/native-armor-contact-runtime-oracle-01` concatenates original mitigation,
wear, selection and Heavy base mastery publication using independently
established RustyShortSword incoming.830322265625 and full488CB0 rating3.
Current Iron Cuirass250.125/max300 yields Health bits1062088581, incoming
armor wear bits1046843719 and remaining armor bits1132055018. Seeds0/1/15A4
use2/3/1 draws respectively, agreeing under both x87 words. Corpus SHA-256
6246a3ca1c7590a7bca5f5e0155f725f6fa7d0226ec0540c10fbb061c4c4f8b8.
The actual normal and sanitizer contacts/restarts are recorded separately in
M15-COMBAT-STEALTH-CRIME.md; these expectations were frozen before launch.

### Player god-mode armor guard occurs after selection (checkpoint101)

Full65FF10 reads cheat byteB3BB06 and returns false without entering5F3870
when enabled. It does not read condition or snap below-one gear in that branch.
`S4/native-player-armor-guard-oracle-01` covers32 executions, god/wear0-or1,
Light/Heavy skill5-or50 and both x87 words. Corpus SHA-256
ab318d82a4b2488ae29f556f5a8451b91f0e15588650ceb42d8cfe52364c9a4f.
Boundaries are the same native max/float/base/setting/bookkeeping/publication
readers as checkpoint98; forwarding executes full original body/mastery paths.

`native-player-armor-caller-oracle-02` executes original5FFBF9..5FFC6D,
full5E5A00/47DF80/9859DD,65FF10 and admitted5F3870 paths:16 cases under both
control words. Positive absorbed armor fraction and positive incoming wear
select three draws fromseed1, next415139642, even when Player god mode suppresses
the condition call. Nonpositive fraction or incoming wear makes no draws.
Body Upper only, inventory/TLS readers and the condition boundaries are supplied;
pre-entry non-CREA victim is declared. This does not execute the resource sink,
armor formula, later skill-use/reactions or the full hit function.
Corpus SHA-256e107d2e7539ae326da3b6ea6a28de790d37c3d596ea3a28421c1a50db12a9fe1.

Retained `-caller-oracle-01` is invalid with zero completed corpus: stack+38
was mislabelled victim Fatigue and a wrong stop sent execution into unconfigured
continuation. Original5FF759 establishes that slot as absorbed armor fraction;
zero skips selection. Corrected02 uses explicit actual branch stops and checks
EIP/ESP. No production arithmetic or result corpus was tuned to this failure.


Checkpoint104 independently freezes unchanged Dreth (Blade AV14=15, STR40,
Luck50, full Fatigue155) Rust max56: original weapon query Health bits1065431859,
weapon wear1050253721 and condition1113509069 in both x87 modes. Player full
488CB0 Iron1000 hundredths/max300/current250.125 with HeavyArmor15 yields entry4.
Original concatenated mitigation/selection/full Player guard/mastery wear yields
Health1064833122, armor condition1132041335; seeds0/1/5540 consume2/3/1 draws to
505908858/415139642/1188163031. Evidence S4 native-npc-weapon-runtime-oracle-02,
native-player-armor-entry-oracle-03 and native-npc-player-armor-contact-oracle-02
records supplied boundaries and original SHA. Actual normal NPC course02
rejected the frozen Health/armor condition values while matching source wear
and RNG. Direct original488DCE..488DDB tracing exposes the previous half-up
wrapper error (threshold atA2FC68 is double0; positive fractional entries
round upward). This discrepancy remains open pending expanded independent
original cases, production correction and fresh actual contact/restart runs.


Checkpoint105 resolves104's actual NPC contact discrepancy without changing
frozen expectations. Full original488CB0 entry rounds positive fractional
547370 results upward (488DCA negative fractional difference, A2FC68 original
double0, 488DDB add1); checkpoint100's half-up description is superseded.
Expanded oracle04:3072 executions,1536 heavy/light profiles,both x87 words,
768 distinct numerical rows frozen in armorratingentry_expected.inc;
corpus903d14ea32b7b14b32b27c0876875315c6ec7ecd88567550a18ed9b1ec222b60.
Declared native initializers/winning settings and getter/condition boundaries
are recorded. Normal and instrumented real NPC contact/save/fresh continuation
both preserve exact frozen Player Health1064833122, Rust1113509069,
Iron1132041335, seed1->415139642/3 draws. Additional normal seed0 and5540
courses confirm2/1 draws and matching Health/conditions. Exact inventories,
actor authority channels and no replay are independently checked with eight
negative verifier controls. No general AI, in-action restart, held mesh,
reaction/audio or campaign acceptance is inferred from these courses.

Checkpoint107 adds carried-right NPC presentation using the same live equipped
item as contact, without changing a native numerical rule. Winning Rust WEAP
090615 identifies `Weapons\\Iron\\ShortSword.NIF`; the renderer resolves the
record-relative path once and attaches to native `Weapon`. Normal and
instrumented first-contact captures were directly inspected; frozen105 damage/
wear/selection results remain exact. The normal full course passes independent
save/restart checks. Instrumented continuation numerical checks pass with eight
negative controls, but both initial loaded-scene captures fail unchanged image
criteria; these are retained as failed visual courses. Neither native draw-state
restoration, complete grip/strike/reaction frames nor audio is accepted here.
The authored restraint fixture and staged equipment/geometry remain declared
setup, not independent proof of native AI probabilities or normal equipment use.

Checkpoint108 introduces profile-owned logical draw-state persistence, without
adding an original-mechanics numerical claim. Schema29 nullable enums and native
non-Player reference/AV/life ownership have exact independent wire, corruption,
round-trip and schema1-28 migration tests. Absence is not a derived combat pose.
World capture, native class hydration, store-kind validation and actual rendered
restoration remain subsequent acceptance work; the codec alone does not resolve
the107 draw-state or initial-scene failures.

Checkpoint109 integrates the profile-owned schema29 draw contract with actual
native NPC/CREA world capture, class/base preflight and lazy/resident view
restoration. No new original-executable numerical rule is introduced. Three
integration cases preserve resource/action/life/RNG authority through typed
record restoration. Normal and instrumented ordinary NPC contact/F5/distinct
fresh-load courses both restore the carried Rust model before input and retain
all frozen105 results; nine verifier rejection controls include missing draw.
Direct captures show the actual prison/NPC/held mesh in both fresh processes,
under unchanged image criteria. Prior107 visual failures remain historical;
general screenshot-pipeline causality, full grip/reaction/audio and wider stage
acceptance are not inferred. Staged geometry/equipment and authored deterministic
combat style remain declared setup; ordinary script equipment publication is
still a separate unfinished adapter.


Checkpoint110 publishes ordinary native script equipment to physical NPC/CREA
instances, and cancels owned weapon/block input before observers. It introduces
no new original numerical rule. Five world cases and complete normal/instrumented
773-test engine inventories pass, with242 Python tests. Final equipment,
ring-stack and windup-cancellation runtime courses pass in both lanes:16 distinct
processes,24 directly inspected required scenes and52 rejected semantic controls.
Gear/quantity/condition/ownership and all unrelated native authority remain exact
except declared script requests and legacy normalization. Reentrant observer
equip requests terminate and retain their later result; the prior loop fails its
bounded regression before correction.

The original plain silver ring03801f has winning BMDT bit64, not192. One ordinary
ring equip from quantity2 is accepted; synthetic dual-slot tests do not prove
vanilla two-ring choice. The initial windup fixture's GetAnimAction was a foreign
command absent from the pinned TES4 table and remains a rejected fixture. A
corrected authored tick delay requires actual owned action2/id1 before interruption;
it injects no contact or outcome. SCTX fork compilation is separate from original
compiled-bytecode acceptance. Fixture style/script overrides and staged gear/
geometry are declared inputs.

The first final instrumented equipment attempt's initial HUD-only capture fails
unchanged entropy0.03; numeric-only comparison does not accept that course. The
fresh accepted instrumented retry retains an extra startup sample before input,
excludes it from scene acceptance, and requires all original captures at0.03.
Its first sample remains HUD-only while the next required capture shows the
scene before any equipment command. No general screenshot cause/pipeline fix is
claimed. Prior compilation, headless-unstack, dependency, unsupported-command,
reentry and visual failures are retained. Full grip, ring appearance, reaction/
audio, complete live inventory scripts/hydration and wider M15 gates remain open.

Checkpoint110 generated fixture hashes (editable manifests/scripts are committed,
generated plugins remain ignored): M15ScriptEquipmentOpponent.esp
25f5aa60b34914063487679eb3db74875fa9053f44fad1d00efbaaa667d82b9b;
M15RingStackOpponent.esp
04aaa13412747ec9b2c227e74ffa391b563ebddd0a577a4daa245b888b9b91cb;
M15WeaponChangeOpponent.esp
b4f47b07e52de1cf289bfedb1c99b8754ad293b32404af8c4bd664902e57ebd4.
The winning record audit pins original Oblivion.esm SHA256
a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70.


Checkpoint111 independently executes original GetItemCount4F48F0/count4869C0/
base-list lookup469CA0 in1290 cases, both x87 words. Corpus SHA256
0150356c3579076f3b8c795cc652b5608652df6acb15e6d59ac2e9504a6d1894;
original executable a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6.
Eligibility, base-container resolution and initialized container-change lookup
are declared boundary returns. Original base magnitude plus signed delta,
present zero/zero entry yielding1, first matching entry, int32 wrap/magnitude
and unavailable item/actor/container exits execute. This is representation/
evaluator evidence, not full original command argument extraction, live item
mutation or arbitrary overflow-domain acceptance. Eight frozen return cases
compare the live physical query with original results, including aggregate
INT_MIN and wrap, without native authority birth. NPC and CREA queries now
ignore stale runtime counts when live physical views exist; Player/unavailable
fallback counting and full hydration remain separate adapters.

The actual ordinary query course uncovered a distinct save-capture lifetime bug:
a FormId pointer into getOwner's temporary RefId outlived that temporary. It
lost owner025200 at F5 despite the correct count3. World Player/NPC capture now
keeps the owner value alive; matching native AI door/base lookup temporaries
are also retained. No new original AI/numerical rule is inferred. The new real
World ownership regression fails the old Player path before correction, then
preserves both Player/NPC owners through full serialization. Four new cases,
full777-test normal/instrumented engine inventories and242 Python tests pass.

Corrected normal/instrumented ordinary query/save/distinct fresh-load courses
preserve exact item metadata, draw, resources/life/action/RNG and short local3,
with no query replay. Both reuse identical frozen stages/plugins from the failed
owner-losing setup; sixteen semantic corruption controls include owner loss.
Six required captures were directly inspected at unchanged entropy0.03; extra
startup samples are excluded. Editable fixture script SHA256
2ffb32f2237b450175f427867951e3dcfefaf0e7b10ba45f4b4b406959f821d3;
generated plugin a4f49c7f57a0ec77ebb7b3ce032451d5996b2897b90244de6a76a8ddddcaaa22.
The declared extra physical stack/style/script binding is setup, not injected
query output. Native master fields are preserved by the hash-pinned builder;
SCTX fork compilation does not prove original-game bytecode acceptance.
Rejected runtime ownership, red count/owner, trace-macro compilation, synthetic
Player race validation and probe-loader syntax attempts remain retained. Wider
M15 inventory mutation/cache/hydration and stage gates remain open.

### Checkpoint 112 — native RemoveItem count clamp

Original1.2.0416 SHA256a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6
identifies RemoveItem entryB0D590/opcode1052/execute513810. Original
513911..51394E and51399E execute with original9828C0 non-SSE conversion,
both x87 control words, and only GetItemCount's double return supplied.
`S4/native-removeitem-count-oracle-02` passes112 requested/available pairs,
including signed extremes, absent quantity, count caps and aggregate INT_MIN;
corpus58a76c4e505c5a3b643af0e87a263de392a78a81fad71ae21ff332dbb262b273.
Clamp to min(requested, available), then mutate only for a positive count.
Zero/negative requests and available INT_MIN bypass mutation. The earlier
96-case report and initial exit-stop harness fault remain retained.

Live Native NPC/CREA removal now uses physical inventory and captures its
actual metadata after observers. The frozen prior executable's ordinary
RemoveItem999/F5 retained count3 and is independently rejected in
`S4/native-actor-removeitem-baseline-red-03`; final normal/SAN remove-all
courses pass four epochs/six directly viewed captures/16 controls under
`S4/native-actor-removeitem-final-acceptance-01`. Final780-case engine suites
share fingerprintc5b6222ed61b43a647eb2457a6f3c40549ac96c6fdfb28ce895917fe1ef7b28b;
242 unchanged Python checks are reused from the earlier112 fingerprint.
The expanded C++ INT_MIN test was red before the guard (headless GUI-null
path after an incorrect mutation, exit139), then green. Native instance
selection/partial-removal order, full original command resolution/mutation/
callbacks, active-windup removal, AddItem, Player/unavailable normalization,
and the wider M15 gates remain open. See the milestone checkpoint112 for
all identities, failures and boundary limits.


### Checkpoint 113 — AddItem wrapper and fresh temporary entries

Pinned original1.2.0416 executablea8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6:
AddItem entryB0C910/opcode1002/execute507320. The executed command gate
5073FC..507435 skips null item/count0, forwards signed nonzero counts raw;
24 cases in`S4/native-additem-command-gate-oracle-01`, corpus
617bb8e3bdaca53d84bdf9cdc953f041eac80fa3757649dd9146adefe1ddff25.
Actual constructor469690/Weapon insertion469D10/empty-list insertion446CB0
execute14 cases in`native-additem-temporary-oracle-01`, corpus
300e885085cbec23858ecca824c893284efeb5c60beb15010e81890673ae74a8.
Fresh signed entries retain raw counts; helper zero normalization is excluded
by the wrapper. Declared insertion-recording, allocation, RTTI, name and logging
boundaries are detailed in reports. Full actor apply/base-delta/ownership and
callbacks are not established. Signed native mutation remains open.

Positive live actor addition and nonpositive wrapper no-ops pass784-test normal
and instrumented engine inventories plus242 Python tests at fingerprint
3cfe5c63f849fa3faed5203725a12970c7b0b148b0d6b76b5ab133f448ae9c63.
The old ordinary AddItem2/F5 course saves zero physical items and is rejected.
Corrected normal/SAN add-two/F5/distinct-load/F5 courses preserve exact two-item
metadata and native authority with no command replay:four epochs/six directly
viewed captures/sixteen controls, consolidated under
`S4/native-actor-additem-final-acceptance-01`. Physical int32 capacity and
negative-count rejection are explicit unsupported-domain guards, not claimed
original parity. See checkpoint113 in the milestone for all identities,
failed attempts and remaining stage gates.


### Checkpoint 114 — Player physical-stack query arithmetic

No new original rule is inferred. The1290-case original GetItemCount corpus
from111 supplies frozen signed wrap/magnitude rows for actual Player World and
script queries. The previous shared signed count accumulator is red for MAX+3
and MAX+MAX; the common Player/NPC/CREA unsigned reduction is green without
item metadata mutation or AV/life authority birth. Full normal/SAN785-test
engine inventories and242 Python tests pass at fingerprint
ce977f34a045944b7df7c526e12ae725199fe66b3e3fd06aa5cc2772ee085f90.

Ordinary NPC activation of Player.GetItemCount on verified native EDID
IronCuirass01c6d1 passes normal/instrumented save/quit/distinct-load/resave,
exact local1 and all inventory/native authority:four epochs, six directly
reviewed captures, sixteen controls, consolidated in
`S4/native-player-item-count-final-acceptance-01`. The initial wrong editor ID,
compiler setup failures and pre-correction red test remain retained. Original
actor/base/change resolution boundary limits, signed mutation/creation,
unavailable counting and the full M15 gates remain open. Checkpoint114 in the
milestone records identities and the full acceptance scope.


### Checkpoint 115 — original inventory base/change and AddItem delta rules

Pinned originala8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6:
actual AddItem change core48FB31..48FB5A plus raw base-list lookup469CA0
execute2916 signed base/delta/request/presence/x87 cases without game-function
stubs. `S4/native-additem-change-delta-oracle-01` corpus
1baeb8645a6f284742517c9473353a93cad8b97477b17555db3f9b060caef1f1.
Existing negative delta and nonpositive raw original base select replacement;
otherwise int32 wrapped addition. Fixture entry/base-list/register/stack inputs
are explicit; creation/cleanup/instances/ownership/callback/physical semantics
are outside the slice. The command's zero gate remains a distinct rule.

Shared native magnitude/base-change-query/existing-add-delta C++ rules match
1280 numerical GetItemCount rows and all2916 new delta rows in normal and
ASan/UBSan comparison binaries:4196 exact comparisons per binary and three
corrupt-evidence rejection controls. Ten original GetItemCount eligibility/
null/resolution cases are outside the numerical API and are excluded. Forty-eight
compact frozen rows form three new component tests. Full normal/instrumented
2058-component/785-engine inventories pass at fingerprint
53e7adf25bff70f89dc32a0a7c25e44e54af8060d9870bc5e7824f4df3795726;
242 unchanged Python results from114 are explicitly reused.

The live Player/NPC/CREA physical query uses the shared magnitude rule; repeated
ordinary Player query/save/quit/distinct-load/resave passes four epochs/six
directly reviewed required captures/sixteen controls. Consolidated evidence:
`S4/native-inventory-count-rules-final-acceptance-01`. Signed delta persistence
and full physical negative AddItem remain open, rather than being inferred
from query absolute values. See checkpoint115 for identities and all limits;
M15 and its outstanding stage gates remain in progress.

### Checkpoint116 — saved native inventory class integration

World prepares detached saved NPC/Creature inventory before lazy class cache
publication. Empty saved state suppresses base stock, invalid base or late
invalid item fails before publication, and subsequent reads do not replay
saved state. This is persistence integration, not a new original-game numeric
rule or schema. Versions1–3 retain existing equipment migration; v4+ respects
saved slots. Seven actual-class/T4ST cases pass, including no AV/life birth.

All792 engine tests pass normally and under ASan/UBSan at fingerprint
`9a64989798e2dddc25153154d06deebe0751c5baf100dd244abf7e1a2586aa99`.
Unchanged component2058-per-lane and original4196-row production comparisons
from115, plus242 Python checks from114, are explicitly reused, not rerun at
this fingerprint. Four ordinary query/save/fresh-load process epochs, six
directly reviewed captures and sixteen semantic corruption controls pass in
`S4/native-lazy-inventory-final-acceptance-01`. These runtime courses prove
full-load regression behavior, not isolated cell eviction/re-entry. Original
expected corpora and query fixture/save bytes remain unchanged. No stage
closure or full negative AddItem/physical availability parity is claimed.

### Checkpoint117 — Player physical removal/capture adapter

Player removal reuses the native bounded snapshot transaction and canonical
Player cancellation key, then captures actual post-observer metadata. Full
world Player capture uses that same adapter and existing hotkey join. No new
original rule or save schema: independent RemoveItem gate112 and original
GetItemCount magnitudes remain the numerical sources, not proof of physical
instance/callback ordering. Negative Player additions now share the explicit
unsupported guard; complete signed-delta support is still open.

Four new actual-World/Host cases, three genuine baseline failures, six focused
passes, all796 normal/sanitized engine and242 fresh Python tests; fingerprint
`da427be3e06ad6837a8fd943f9e9c477c52a7ff0a4840585cf960e3f68677b11`.
Four ordinary activation/removal/query/save/fresh-load epochs, six directly
reviewed captures and sixteen corruption controls pass in
`S4/native-player-removeitem-final-acceptance-01`. Exactly one actually equipped
Player IronCuirass disappears, requested999 clamps to1, saved query/local0 and
all other metadata/native authority persist without replay. Own editable
fixture changes only NPC SCRI/ZNAM and own bindings; no inventory outcomes
injected. Partial order/windup/callback and wider stage gates remain open.

### Checkpoint118 — original stock swish size selection

Pinned1.2.0416 original6AF9DC..6AFAC0 and real403C00 setting getter execute
without game-function stubs for1936 cases in both x87 words. Nullable winning
weapon and raw weight+7C/speed+94 are fixtures; process/RTTI/instance lookup,
spatial/cap checks, events, sound lookup/allocation/playback remain outside.
Native Audio INI B162CC/D4/DC/E4/EC defaults are weights8/25, speeds float1.1/
float.95 and useSpeed=true, not GMST values. Equality at either threshold
selects WPNSwishLarge; null weapon selects Hand and reads no selected settings.
An initial independent speed-equality prediction of Small failed before
production existed; audit02 retains it. Correct audit03 corpus
`5bf5ebec321adebf3635998ebd7c13618567777d044c64a5bb18ba89035e172e`.
Four hash-recorded stock first/third-person onehand/hand-to-hand KFs have no
sound keys; initial common-basename extraction was diagnostic only.

Actual production nativeMeleeSwishSound matches all1936 symbolic outputs in
normal and ASan/UBSan comparisons; three corruption controls pass. Two new
component cases fail against a Hand-only stub, then pass. All2060 component
tests pass in each unfiltered normal/sanitized suite at fingerprint
`11f4d69f9c65374ee7188100ff118a52412e5a1e82397cb53c89e211df722cbe`.
Evidence:`S4/native-stock-melee-sound-audit-03`,
`native-melee-swish-rule-comparison-01`,
`native-melee-swish-rule-final-acceptance-01`. No runtime stock sound, INI
import, audible original-game, reaction or M15 stage closure is inferred.


### Checkpoint119 — stock miss playback integration and actual PCM

The ordinary Player/NPC controller's once-per-contact commit boundary now
emits nativeMeleeSwishSound only for committed misses through M10 winning SOUN
lookup/playback, using original Audio defaults. No new arithmetic rule or
schema. Original1181936-row selection evidence remains the numerical source;
stock hand SOUN088834 and three independently hash-identified original WAVs
are the waveform source. Sounds BSA hash
`f11e92315666b7e6ee5f6936fa68300fb5234215c59d5e3836375f262fba05ec`.

Actual old117 engine baseline commits one ordinary miss, fatigue140 ->133,
with no swish request/match and a genuine failing one-wave requirement.
Normal/sanitized production recordings each contain exactly one stock asset01
match, correlation0.99988 against preregistered threshold0.75. Both fresh-load
recordings contain none. Four process epochs/eight directly viewed scene-HUD
captures/sixteen state corruption controls/three PCM controls pass; native
resources and all inventory metadata persist, no attack/sound replay. No visible
hands/grip/reaction acceptance is inferred from these captures.

All796 normal/sanitized engine tests pass at production fingerprint
`16192acb88a3e02616bd89f69c130794f3d934c42e8ca554622621ed2d89c3bc`.
After equivalent editable replay JSON publication, direct schema validation
and242 fresh Python tests pass at
`a0806e017cd512d29768b84994f5f5e25300389c56aea6c59fb4dc5774976e69`;
C++ unchanged. Existing1182060-component suites and1936-row comparisons are
explicitly reused. Evidence:`S4/native-missed-swish-final-acceptance-01`;
stock assets/PCM stay ignored. Remaining weapon/view/control/audio/reaction,
INI import, spatial/cap, original live and full M15 gates remain open.


### Checkpoint120 — first-person anchor and authored native idle poses

Hash-pinned original executable Camera01 literal A6D448 and four native scene
node lookups establish the name; stock first-person skeleton node has local
translation approximately(0,-3.7408032,118), hash
`dafcd911371f5622f8119c8503e1316b1262893415d0ac9fa8cb96d30f85903e`.
Native first-person camera now prefers it, preserving existing incomplete-node
fallback. Full original camera/aiming behavior remains outside this audit.

Native stationary biped drawn idles route to authored handtohandidle/onehandidle/
twohandidle, reevaluate group on draw/family changes and repeat full cycle0
bounds without TES3 randomized weapon-suffix idle behavior. Original first-person
handtohandidle hash
`2e8b3adb8a1cd5ecc7e831fa1b8542b5fd48e69190f780f845786ff63f799d98`
decodes intrinsic Idle,66 tracks,cycle0,freq1,0..float2.1999998092651367,
start/end keys. A raw strings display misleadingly joined Idle with the next
count byte0x42; independent sized decode is retained. No new numeric combat
rule, saved authority or original live gameplay claim.

Actual retained119 normal/SAN predecessor captures lack ready hands. New120
normal/SAN ordinary input/save/fresh-load courses show both Player fists and
wrist irons, bind66 idle tracks with no skips, keep once-only stock swish/no
reload sound, and preserve native resources/inventory metadata. Four epochs,
eight directly viewed captures and sixteen corruption controls pass.
All796 engine tests per lane and242 Python tests pass at
`07c470c9836ed47da0695de432e84cab8332e0770e85897ca95ae0b72cd1418a`.
Existing118 component2060/selector1936 and119 PCM controls are explicitly reused.
Evidence:`S4/native-firstperson-melee-idle-final-acceptance-01`.
Onehand/twohand actual coverage, aiming/view/control/reaction/mastery/audio
policy/original live and full M15 acceptance remain open; no stage closure.


### Checkpoint121 — original stock shortsword and separate scabbard geometry

Hash-pinned original master WEAP090615/WeapIronShortswordRusty DATA decodes
one-hand type0, speed float1.2000000476837158, reach float.800000011920929,
weight8, health56 and damage5. Winning fCombatHitConeAngle is35; winning
fFatigueAttackWeaponBase7 and Mult float.10000000149011612 supply the already
reviewed normal-attack rule. The original speed selector6AF9DC..6AFAC0 and real
403C00 settings getter select Small for this exact weapon input under both
x87 precision words, with no game-function stubs. This additional two-case
execution does not rerun or replace checkpoint118's1936-row comparison.
SOUN0872C2/WPNSwishSmall resolves fx\wpn\swish\small\; the three original
mono16/44100 waves are hash-identified and compared to actual new stereo engine
PCM, with pre-existing correlation threshold. Original hardware audio and
arbitrary user Audio.INI policy remain separate open gates.

Original ShortSword.NIF SHA
`100d7d346159372906c85a605cde022198b88a8c6ba5a30e64c7e655c657519d`
contains independently located length-prefixed ShortSword, Scb:0 and
ShortSword:0 names. Original executable SHA
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`
uses literal Scb atA3CE0C with operands47928B/4807A2; retained bounded
instructions show native node lookup/removal references. Those references do
not independently prove the complete caller draw-state or original live pose.
The actual observed sheath failure and loader optimizer regression justify the
fork's separately preserved, hidden drawn sheath. Original onehandidle inline
sequence metadata decodes66 controlled blocks, cycle0, frequency1, start0 and
stop3. The original assets/audits remain ignored in
`S4/native-stock-small-swish-assets-01`; editable fixture/course sources are
versioned. Original-game SCDA acceptance remains open: this fixture uses the
fork's SCTX compiler and ordinary fork activation. Final four-epoch/eight-
capture/22-state-control acceptance is bounded to Player ready/reload visibility
and one missed Small swish, not full combat or stage completion.


### Checkpoint122 — original human one-hand idle targets

Original third-person onehandidle.kf has73 inline33-byte controlled blocks,
cycle0,start0,stop3. Its node-offset field is13 bytes into each controlled
block; the independently bounded string palette is1107 bytes at2922. The
73 decoded names have64 length-prefixed matches in the original human
skeleton. The nine absent names are Bip01 TailRoot and Bip01 Tail01–Tail08.
The actual native NPC renderer reports64 bound/9 skipped, consistent with
that stock human inventory. This does not establish beast skeleton support
or original-game pose/motion parity. Audit identities and bounded fork grip/
fresh-reload acceptance are recorded in checkpoint122's milestone entry and
`S4/native-npc-shortsword-final-acceptance-01/verification.json`. No new
physical formula, state authority or schema is introduced.


### Checkpoint123 — original ordered NPC impact sound layers

Pinned original1.2.0416 executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`
executes6AFB48..6AFD28 for800 combinations of signed weapon/material classes
and selector booleans. NPC fixtures use the actual Character vtableA6FC9C;
position getter5F10E0 executes its absent-process fallback, base getter4D9B40
executes actor+1C, and5E3270 compares the base form kind to Creature24. Only
SOUN lookup447490 is supplied: it records requested editor IDs and returns an
absent record, leaving allocation/playback outside this original probe. All
stack returns and the final stop are asserted. The dispatcher prefix locals
are initialized explicitly. No whole original hit, original audio playback or
Creature source custom-sound branch is accepted by this bounded experiment.

Armor class>=0 chooses Light at0 or Heavy otherwise. Only absent armor permits
shield class0/1. The shield branch sets its local material flag even if lookup
fails. False argument8 enables NPC PHYDamageFlesh and its flesh variant flag.
Argument9 chooses the enchanted blade/blunt variant independently of that
body flag. The original jump table6AFFD4 maps0/1 blade,2/3 blunt,4 Hand,5 Arrow;
all other signed values use Hand. Up to three requests occur in this order.
Caller5FFB89 supplies shield class;5FFCE5 supplies armor class;5FFD61 supplies
neither, with argument8 false. Original4B4C70 returns armor byte+6A bit7, the
heavy flag. Argument9's full caller semantics are not inferred from its branch
name; the runtime slice accepts only unenchanted ordinary contacts already
admitted by the damage adapter. Creature target Bone/Fur and unarmed Creature
source custom family9 remain outside the NPC palette.

Winning installed SOUNs are025418/PHYDamageFlesh(folder
fx\phy\combat\damage\flesh\, SNDX3c16000001000000e5010000) and
025413/WPNHitHand(folderfx\wpn\weaponhit\hand\,
SNDX3c12010001000000ed020000). Master identity remains
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`;
original Sounds BSA identity
`f11e92315666b7e6ee5f6936fa68300fb5234215c59d5e3836375f262fba05ec`.
Five Flesh and four Hand clips have individually retained SHA256 hashes and
mono16/44100 formats. Corpus SHA256
`c499ac640abc5a3002e7c15149ea7aaa02749caa7a13515eb286b06b0fe9e5fe`.
Original audits/comparisons stay ignored in
`S4/native-npc-hit-sound-oracle-01` and `native-npc-hit-sound-assets-01`.
Normal/SAN actual once-only contact/PCM/fresh-reload acceptance is documented
in checkpoint123's milestone entry; it does not close the full reaction/audio
or original hardware gameplay gates.


### Checkpoint124 — original direction, timed force state and continuation

The same pinned executable SHA256a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6
executes547690/5F4880 and bounded5FFFAF..6000B4 contact argument composition,
actual position getters/4121A0 subtraction/43F350 normalization/cap/duration.
Actor integer/base AV getters and Havok availability/application are declared
boundaries of the argument probe. Actual fixture inputs are Agility40,Luck50,
base Fatigue155,current153.2899932861328,raw damage1.42 (truncated1),delta
[0,100,1.3706893920898438]. Signed force40.693504333496094 yields world vector
[0,40.6896858215332,0.557729184627533]. This is independently observed, not copied
from the fork's resulting motion. The earlier Agility53 hypothesis is retained
but does not supply actual-source acceptance.

Full original8907A0 multiplies world components by doubleA39088's
0.1428767293691635, stores float, then multiplies by stored float reciprocal time.
Original SSE squared magnitude uses individual float products, then(X²+Y²)+Z²
stores. Branch890854 rejects equality and weaker magnitude. Acceleration lives
at+2F0; remaining time at+300. Full890970 adds stored(acceleration*remaining)
to base velocity+2E0 only with remaining>0 and flags+1F4&1800 clear. Suppression
preserves force/time; remaining<=0 clears acceleration. Timer896F9B..896FDB
subtracts elapsed and clears/clamps when result<=0; an initially nonpositive
remaining time leaves acceleration unchanged in that timer slice. Full setter/
velocity run native CRT cookie verification with no supplied game functions;
timer is a bounded instruction slice. Complete Havok update ordering and flag
interpretation remain separate.

43F350's binary32 epsilon9.999999974752427e-7 is inclusive: equal/below collapse,
next representable above normalizes. Its caller still multiplies the cleared
components by the signed force, so negative force on zero/tiny direction gives
three negative zeros. Both x87 words27F/37F agree in the retained probes. Normal
and sanitizer comparisons of504 actual instruction cases cover direction,
replacement/equality, velocity/flags, elapsed/expiry, signed-zero/epsilon and the
real contact context. Final corpus SHA256
`d65306441ce1a3369621eb178df12f21c6e80077d3bbd4f46463ef8c6d2d8b14`;
comparator SHA2562ead77bace46b6716f6b4c22e2752b8b2269414b9fad0fb194dfbdee97ef80f8.
Original corpora and all failed attempts stay ignored in S4/native-timed-knockback*.

This establishes original arithmetic/stores and bounded fork integration. The
contact prefix5FFF52 still contains a separately unresolved caller flag, and
Speed AV4 must be positive before force composition. Nothing here equates that
AV with Health or Willpower, or closes whole contact eligibility, actual original
reaction/ragdoll/save/collision parity. Checkpoint124's milestone entry records
normal-input owned contacts, active-pulse fresh processes and PCM evidence.


### Checkpoint125: native skeleton graph and Havok rotation encoding

The stock human skeleton above has 18 bodies and 17 joints (including seven
malleable limited hinges with empty inner endpoints). The independent fixed-
layout byte oracle `S4/native-ragdoll-stock-byte-oracle-02` and normal/sanitized
comparisons `S4/native-ragdoll-stock-comparison-{normal,sanitized}-03` establish
owned graph agreement without using the production decoder for expectations.
The [NifTools schema](https://raw.githubusercontent.com/niftools/nifxml/master/nif.xml)
defines Havok `hkQuaternion` as XYZW and uses it for rigid-body rotation;
ordinary NIF quaternion encoding is WXYZ. The original reader's mistaken use
of ordinary decoding failed all 18 stock rotation probes and the explicit
parser regression before correction. These are data-format claims only;
original world-space initialization, damping, impulses, collision, animation
binding and recovery parity remain open. Exact hashes, negative controls,
retained failures and test inventories are in checkpoint125's milestone entry.


### Checkpoint126: sphere-motion velocity damping

Original finish-loaded constructor8e96a0 publishes sphere-motion vtablea979a8;
virtual+10 resolves8e96c0. Full execution of that routine and its three real
quaternion/transform callees yields 360 cases without boundary stubs and with
verified stack cleanup. The comparison concerns only damping stores8e9775:
`max(0, 1 - dt * coefficient)`, x87 factor store to binary32, SSE velocity
products. The corpus distinguishes incoming linear velocity after the original
SSE gravity-delta addition from the supplied fixture velocity. Original
executable SHA256a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6,
corpus SHA256e00f25fc4b01464d1f2bc4cd5f09ceaa85de35ebd7b388a7ef71a82d008df34e; complete scope/hashes/controls are in
checkpoint126's milestone entry. Normal and sanitized live Bullet bodies match
all damping cases. Neither final transforms retained by the oracle nor native
velocity caps, gravity, alternate motion types, collision/constraint solver
rows or actual ragdoll gameplay are accepted by this evidence alone.


### Checkpoint127: original ragdoll angular coordinates

Native buildJacobian911480 and all its real initialization/angular/ball-socket
callees execute without boundary stubs for118 cases; modern constructor911000
and Ni adapter8c0b70 establish frame/angle layout. Cone and plane cross-squared
admission is strict >binary32 epsilon. Twist normalizes the sum of axes and
uses body B on the original length threshold. Angle helper8ecbb0 uses a
bounded polynomial with binary32 constants, x87 intermediates and float stores.
The normal and sanitizer coordinate helper matches every recorded parameter
bit exactly, including signed zero. Inputs must be the original transformed
world frames; supplying pre-transform local axes produced a retained harness
failure. Full hashes, controls and scope are in checkpoint127's milestone
entry. This establishes angular coordinates only, not impulse solving,
friction/malleability, complete ragdoll admission or normal-input gameplay.


### Checkpoint129: original joint friction rows

Full original ragdoll911480 and limited-hinge8b2820 builders execute actual
friction builder8f1460 without boundary stubs in96 cases each. Schema store
8f15ba writes the torque*frame-duration product as binary32. Cone axes use
body A's shape basis (three rows); hinge uses its axis (one). Proper hinge
basis layout is pivot/axis/perpendicular1/perpendicular2 for A and
pivot/axis/perpendicular2 for B; the initial reversed fixture was retained
as a failed comparison, then independently corrected. Normal and sanitizer
comparisons match192 cap stores and inspect120 positive-duration actual
Bullet friction-row cases, with four corruption controls each. Zero-duration
checks cover the cap only. Full hashes and bounded scope are in checkpoint129's
milestone entry. Original effective-mass/iteration parity, malleability,
stock graph admission, gameplay and saved continuation remain open.


### Checkpoint130: original malleable parameter scope and one solver sweep

Original9104b0 wraps real cone911480 or hinge8b2820 row builders with schema
opcodes15/16. Complete solver sweep9202a0 executes92197c/9219a5 to replace and
restore solver-info tau/damping fields+4/+8. The values replace defaults rather
than multiplying them. Ball-socket solve9213f0 uses rhs*tau minus relative
velocity*damping, then effective inverse mass. Both complete builder and single
sweep execute without stubs in324 cases per joint type; saved global values are
verified restored. Normal and sanitizer actual-body comparisons pass648 cases
within absolute/relative2e-6 tolerances frozen before implementation and reject
four controls. Full hashes/scope are in checkpoint130's milestone entry.
This is single-sweep evidence in synthetic unit-mass/inertia anchor fixtures,
not full native iterative/frame/collision or gameplay parity. Full stock graph
factory admission and240 finite Bullet steps separately pass, using source
info rest transforms, not current actor pose binding or normal-input reaction.


### Checkpoint131: position stores and ordinary body world-pose boundary

Full original4529e0/43f3e0 execute without stubs in16,384 position conversions,
using separately stored double constants0.1428767293691635 and6.999040126800537
before binary32 output stores. Both x87 words and signed/subnormal/boundary
inputs are covered; native reverse is not an exact reciprocal. Actual normal
and sanitizer production comparisons match these cases and1,080 original
angular damping stores at three length scales exactly. The retained RED test
caught angular radians being length-scaled; production now keeps angular units.
Full hashes and test evidence appear in checkpoint131's milestone entry.

Mode callback tableB2E300 slot5 resolves889d20; independently RTTI-identified
blend-collisionA9643C virtual+70 is88f880. Ordinary-bodyA5605C world sync89eae0
copies current target Ni world transform+64, using actual7150f0/4529e0/4d6830
rotation conversion, position conversion and quaternion normalization.
The216-case full-return probe has collision flag40 and supplies only locking
and final body-mutation boundaries. Corpus `44fcf26089e8162a3df3b51e8a14f16d4f55c3a77e43d30e612c0e49d21a34d9`.
This is not actual body mutation, bhkRigidBodyT or reverse bone writeback.
All live actor, scheduler, recovery and persistence gates remain open.


### Checkpoint132: ordinary current-bone pose adapter

Original89eae0 full-return captures now cover432 cases including stock flag1
with actual native motion-vtableA9AE10 getter911780 returning6, alongside
flag40/type2. Matrix-to-quaternion7150f0, position4529e0 and normalization4d6830
execute; synchronization locks and final body mutation remain boundaries.
Corpus `c694d414276d4349076b678a77013007cdb1525ddaa7f17f70b591e7066b48f8`; complete hashes and scope are in the
checkpoint132 milestone entry. Normal and sanitizer pose adapters match exact
position bits and sign-aligned quaternion components within the predeclared
absolute2e-6 tolerance. Maximum error5.960464477539063e-08.

The graph adapter uses current world bone poses by target record identity,
without reapplying authored body info or bind offsets. It rejects unsupported
bhkRigidBodyT binding and malformed matrices. Transformed stock bind-pose inputs
admit all18 bodies/17 joints and run240 finite synthetic zero-gravity steps in
both builds, then clean up. This is not rendered animated-bone writeback,
actor/world scheduler ownership, original trajectory or normal-input gameplay.
Failed initial API compilation and oracle02 tuple-unpacking are retained.


### Checkpoint134: collision-object bone pose writeback

Pinned original8978d0 supplies1,440 full-return cases without stubbed functions.
Native parent transpose/reciprocal-scale projection and all binary32 intermediate
stores are retained. Collision flag0x8 controls local writes; scale fields are
not copied. World publication compares0.001 rotation and0.01 position component
deltas inclusively; native mask bits are captured directly. Final corpus
`757aa6d64cf2a5b184780846458d0df53aa0a62e2831a68c56f88135a42d80dd`; original helper SHA`2f296bd4f90281f52c2ae95c64c2c7af88a2390bfec2d0fb8951b3d482aecc29`.
Normal and sanitizer C++ projections match every local/world float bit and
publication mask exactly, with five rejected comparator negative controls each.
Seven new tests bring full component inventories to2,111 in both builds.
Renderer application, actor-mode selection, child propagation, controller and
physical save integration remain open. Original mode flags are fixture inputs,
not inferred normal-gameplay behavior. See the checkpoint134 milestone entry
for paths, full executable/helper hashes and reused engine/Python evidence.


### Checkpoint135 — physical snapshots drive renderer bone matrices (S4 open)

`SceneUtil::ActorRagdollPoseBinding` resolves physical graph node-record and bone
name identities in the matching live NIF asset namespace. It borrows renderer
nodes, supports Animation's forced Skeleton wrapper and excludes separately
loaded equipment namespaces. Only physical targets and their ancestors enter
the pose calculation. Expired/detached/ambiguous paths, changed identities,
unsupported transform types, mismatched source hashes and bhkRigidBodyT targets
fail closed. Captures use current renderer transforms, not authored bind poses.
Application validates a complete unique rigid snapshot and every projection
before changing any node. Parents are projected before children regardless of
body-record order; nonphysical connector transforms and authored scales survive.
Physical targets use the previously verified local-write projection with explicit
dynamic snapshot flag9 and forced world publication. Skeleton matrices are
invalidated after publication so skinning updates in the same traversal.

Stock SideWeapon has authored scale0.9999999403953552, whereas unrelated Weapon,
BackWeapon and Quiver helpers have other neighboring-unit scales. Original
NiTransform stores rotation and scale separately; ordinary body sync consumes
unscaled NiWorld rotation and position. The new composition adapter retains
that distinction and native binary32 intermediate stores. Physical world scales
within1e-4 of unit are admitted; broader actor scaling remains unadmitted.
The stock scale audit is retained in `S4/native-ragdoll-stock-scale-audit-01`.
Before implementation, full original53d7a0 and actual7100a0/7101f0 executed
1,494 calls with no stubs, including both x87 precision controls and stock
neighboring-unit scales. Executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`;
private oracle `/tmp/m15_reaction135_bone_compose_oracle.py`, helper SHA
`32585073cf4e560905720290e29f228ad3563aa831dbccd73478ece4cb19724b`; corpus `c42428ea0132f8152e2d08362243ed5854d5a9f4bcda4fdc41e799e53443ef43`.
Normal and sanitizer comparisons in
`S4/native-ragdoll-bone-compose-comparison-{normal,sanitized}-01` match every
output bit exactly. Both also repeat all1,440 original local/world writeback
cases after the shared composition refactor, in
`S4/native-ragdoll-bone-writeback-regression-{normal,sanitized}-01`.
Each comparison rejects five wrong-output/missing/duplicate/malformed controls.

Ten SceneUtil cases cover current poses, child-first graph records, same-traversal
skinning, atomic invalid snapshot rejection, identity/path/lifetime failures,
wrapper/equipment namespaces, untouched scaled helpers, preserved neighboring-unit
physical scales, rotated connectors and actual live body motion feeding renderer
bones. One composition test covers separately retained parent/local scales.
Missing API builds remain in `S4/native-ragdoll-render-bones-red-01` and
`S4/native-ragdoll-render-bone-scale-red-01`; focused green04 passes18 cases.
Actual stock asset `43de349062d2f57b1e581353f1907f8f70bcd3c92b4fe3bd049989b93572d435`
is parsed by NifReader and loaded by NifOsg::Loader. Its18 bodies and17 joints
run240 zero-gravity steps; each resulting body snapshot is published into live
renderer nodes and checked against both captured bone world matrices and
same-traversal Skeleton skinning matrices. Predeclared absolute matrix tolerance
is0.001; both builds pass in
`S4/native-ragdoll-stock-render-bones-{normal,sanitized}-03`, with hashes,
commands and maximum errors in their reports. Failed normal01/02 retain the
scale admission failures; the source fix preserves the asset and tolerance.
Private audit `/tmp/m15_reaction135_stock_render_bones.cpp` and runner v3 are
hash-recorded. This checks actual stock hierarchy/physics/CPU bone matrices,
not pixels, animation/controller transitions or native Havok trajectories.

Full normal and ASan/UBSan runs
`S4/native-ragdoll-render-bones-{normal,sanitized}-01` each pass2,122 component
and815 engine tests, complete inventories, zero failures/skips and stable
fingerprint `62b9e91f938369e310bf41e5226da78b34c57021e732e43852cda93b1f00ed12`. Both rebuild openmw/openmw-tests/esmtool;
ASan leak detection is disabled and UBSan halts on errors. Unchanged243-case
Python evidence remains reused from checkpoint124. The rendering adapter still
needs a production actor lifecycle caller, animation ownership handoff, native
force/mode policy and persistent dynamic body state. Display-dependent runtime
acceptance remains open after checkpoint133's environment failure. No M15 stage
is newly accepted from these component/stock-matrix checks.


### Checkpoint136 — animation and physical pose ownership handoff (S4 open)

Animation can bind, capture, apply and release an explicitly supplied physical
bone projection. Admission resolves and captures the current complete renderer
hierarchy before changing controller ownership. An invalid admission leaves
animation callbacks attached; duplicate admission fails. During ownership,
controller rebuilds remove transform callbacks and retain selected animation
states without advancing their times or returning root motion. Spell effects
still update. Release rebuilds the selected animation controllers; repeated
release is harmless. Model replacement and scene removal drop the borrowed
binding, without reattaching callbacks to a removed root. Native NPC/Creature
XRGD application yields while physical projection is active. The scheduler still
owns bodies and the native service still owns actor lifecycle; Animation owns
only the borrowed renderer projection, not a competing actor simulation.

An actual stock onehandidle KF exposed an overly strict cache-equality check in
checkpoint135: quaternion callbacks retain double precision in the rendered
matrix but cache binary32 NIF rotation components. Procedural rotate controllers
also deliberately leave the cached base rotation unchanged. Capture now reads
the current rendered matrix, removes the separately stored positive scale and
validates the resulting rigid/finite/affine transform. It retains authored scale
and native binary32 composition. This includes procedural pose rotation without
silently substituting a cached bind or animation-base pose. Two new component
regressions cover quaternion/procedural capture and rejecting a sheared rendered
matrix despite valid cached NIF rotation. Four engine tests exercise actual
update traversal, callback exclusion across controller rebuild, failed admission,
invalid snapshots, duplicate/repeated release and scene removal. Green01 retains
a synthetic node fixture whose default scale was incorrectly zero; the fixture
now initializes a proper Ni transform. Missing APIs remain in red01; green03
passes all16 selected component/engine cases.

Private stock audit `/tmp/m15_reaction136_stock_animation.cpp` loads the actual
18-body/17-joint skeleton and73 transform tracks from the winning original
onehandidle KF `d01bf09a3c703ae2f0f4c043abbe47dc1c0e6d3af0fedcf41ed9a50173bc17d5`.
It starts actual Animation playback, advances/traverses the animated skeleton,
captures its current pose, binds physics and runs240 zero-gravity steps. Each
step applies body snapshots, calls runAnimation and scene update traversal,
requires unchanged KF playback time and zero root motion, and verifies captured
and skinning bone matrices against physics under predeclared absolute0.001
tolerance. Release must advance actual KF playback again. Normal and sanitizer
`S4/native-ragdoll-stock-animation-handoff-{normal,sanitized}-04` pass, maximum
matrix error0.000038147. Reports pin skeleton/KF/helper/comparator/binary/library
hashes and build commands. Normal01 retains missing private MyGUI include;
normal02 retains the valid animated-matrix admission failure; normal03 retains
an overly narrow stdout parser which did not account for two setup log rows.
Fresh04 parses those exact setup rows and the final strict summary. No asset or
matrix tolerance was changed to make the check pass.

Full `S4/native-ragdoll-animation-ownership-{normal,sanitized}-01` each pass
2,124 component and819 engine tests with complete inventories, zero failures/
skips and stable fingerprint `bd78d9af002a7085e8f9e6b19bb87bd4d338e3dad1e40e472c99fb3d0cb6ce0d`. Both rebuild openmw,
openmw-tests and esmtool. ASan detect_leaks=0:halt_on_error=1; UBSan
halt_on_error=1:print_stacktrace=1; no leak coverage. Unchanged Python243-case
evidence remains reused from checkpoint124. Native composition/writeback
arithmetic is unchanged since checkpoint135; its independent comparisons are
reused explicitly. Actual stock KF/controller/CPU skinning acceptance is now
covered headlessly; native actor activation/force/mode/recovery policy, dynamic
save integration, pixel/media and normal-input gameplay acceptance remain open.
The display-dependent checkpoint133 environment failure still needs a new
runtime run when a display can launch. No M15 stage gate is promoted here.


### Checkpoint137 — version31 persistent physical body snapshot codec (S4/S6 open)

Runtime schema31 appends an actor-keyed physical snapshot, with matching native
actor/base/lifecycle identity, normalized model path and 32 lowercase hexadecimal
characters encoding the NIF renderer's 16 opaque asset-hash bytes. This identity
is not the independently audited asset SHA-256. Bodies are ordered by original
NIF body record, have unique target node records, row-major rigid world rotation,
world position and linear/angular velocity. All18 floats must be finite; rotation
must be proper orthonormal under fixed1e-4 tolerance. Empty/duplicate/misordered
bodies, invalid paths/hashes, mismatched owners and incompatible legacy versions
are rejected. This stores physical realization without introducing another
lifecycle enum. Older saves decode without physical history and migrate to an
empty physical map; old wire layouts remain unchanged.

Two C++ and two Python regressions check independently assembled wire bytes,
roundtrip, signed zero, every truncated prefix of the new section, empty legacy
migration, identity/path/hash/geometry violations and nonfinite components.
Green01 covered the two C++ cases and243 preexisting Python tests. Full normal01
passed C++ suites but the new Python wire test exposed a scalar writer API being
called with paired IDs; the failed log is retained. The encoder now writes the
two IDs separately. Fresh normal02 passes all245 Python tests plus2,126 component
and819 engine tests. Sanitized02 passes the same full C++ inventories, zero
failures/skips. Both stable tested fingerprint: 3b885d0997d60e0e78088214ea299f21f286c25436955c0be00d2706f12a7492. ASan uses
 detect_leaks=0:halt_on_error=1; UBSan halt_on_error=1:print_stacktrace=1; no leak
coverage. Sanitized01 also passed before the Python-only packing correction.

Private C++/Python interoperability audit
`S4/native-ragdoll-codec-comparison-{normal,sanitized}-02` checks24 snapshots
and432 bodies: complete wire bytes must roundtrip exactly, including signed zero,
and physical JSON fields must agree. Rotations/positions are input fixtures from
the independently captured original bone-world corpus; velocities and asset
identities are explicitly synthetic. Five negative controls reject truncation,
trailing data, nonrigid rotation, duplicate body records and a noncanonical wire
actor. Reports pin corpus, source, comparator, binary and component library hashes
and commands. Normal01 retains the same Python packing failure. This is snapshot
format and interoperability evidence; service/world/physics save capture and
fresh-process gameplay restoration are not yet connected. Lifecycle force/mode/
recovery and normal-input/media acceptance remain open. No M15 gate is promoted.


### Checkpoint138 — native service physical snapshot authority (S4/S6 open)

OblivionCombatService now owns the version31 physical snapshot map. Main-thread
publication compares the caller's expected snapshot against current authority;
stale admission/update/release cannot overwrite a newer pose or recreate released
bodies. Absence is explicit. Before publication, the whole updated snapshot must
validate and match the existing native actor/base/lifecycle. An active binding
cannot change model, asset hash, body count, body or target node records; those
changes require release and fresh admission. Map insertion/candidate preparation
can throw before publication; replacement uses a no-throw swap. Queries return
owned copies. This does not select motion mode, change actor life, emit events or
advance combat randomness.

Capture copies the map before destination publication and rejects v30-or-earlier
downgrades before any destination changes. Validated restore stages it with all
other native authority before swapping. Clear and legacy replacement discard
physical history. The existing World service capture and replacement paths now
carry this map, although per-frame physics capture, realized body creation and
normal gameplay restoration remain to be connected. Four engine regressions
exercise service-to-binary-to-fresh-service storage including signed zero,
independent query copies, stale update/release/recreation rejection, invalid
geometry/owner/assets, atomic invalid restore/downgrade, clear and legacy reset.
This is a new authority object in one test process, not an actual process restart.

Full `S4/native-ragdoll-service-{normal,sanitized}-01` each pass2,126 component
and823 engine tests, complete inventories and zero failures/skips. Normal also
passes245 Python tests. Both stable tested fingerprint `50fe05df8c626835bae2cd4bc0b38a03ac4c8f15714aa19d67aaadfe0eba5199`.
ASan detect_leaks=0:halt_on_error=1; UBSan halt_on_error=1:print_stacktrace=1;
no leak coverage. Snapshot codec/native composition arithmetic is unchanged;
checkpoint137/135 comparisons are explicitly reused.

Independent groundwork for later force policy:
`S4/native-ragdoll-world-gravity-oracle-01` executes96 original-instruction cases
with no stubbed calls. Full generic Havok constructor8a9510 has Y-down -9.8;
full Bethesda cinfo constructor88a4f0 has Y-down binary32 -73.57500457763672.
Actual exterior creation prefix4d5000..4d508a and interior creation prefix
4d4a8b..4d4ad0 overwrite this with Z-down -73.57500457763672 and execute the
actual cinfo and bounds setup callees. Poison-filled inputs, both x87 precision
settings and three quality-byte fixtures preserve gravity bits. Report pins
original executable/helper/corpus hashes. Prefixes stop before allocation and
world construction; the interior begins after eligibility with prior EBX=0
supplied, and exterior SEH uses a synthetic zero environment cell. This does not
admit full world initialization, gravity-modification commands, force stepping or
native gameplay. No gravity policy is changed in production in this checkpoint.
Activation/recovery, realized dynamic save continuation, media/runtime gates and
the display-dependent TES3 regression remain open. No M15 gate is promoted.


### Checkpoint139 — physics/save adapter and stock physical process continuation (S4/S6 open)

MWPhysics converts complete physical shape-world snapshots to native runtime
storage and back, without repeating length conversion or changing lifecycle.
Admission matches NIF source's16 opaque asset-hash bytes, encoded as32 lowercase
hex digits, and actor base/model. Captured bodies resolve their exact target node
by original body record and canonicalize storage order. Restore resolves the
snapshot back into the winning graph's order, checks every body/node identity
and rejects missing/duplicate targets and unadmitted bhkRigidBodyT bindings.
Double-precision Bullet pose and velocity components are rounded to binary32 at
this explicit save boundary; the validated snapshot rejects nonfinite/overflow
and nonrigid values before publication. This adapter owns no bodies or rendering
controllers and does not create another simulation authority.

Four engine regressions cover binary asset bytes, signed zero, units, graph/input
permutations, changed winning assets/model/base/targets, invalid captures and
binary continuation in an independent Bullet world. The continuation runs30
steps, snapshots/serializes/decodes into a second world, then compares60 further
steps against uninterrupted motion. Fixed tolerances: positions1e-4, rotation
components1e-5 and linear/angular velocity1e-6. Green01 passes allfour selected
cases. Full `S4/native-ragdoll-save-bridge-{normal,sanitized}-01` each pass2,126
component and827 engine tests, complete inventories and zero failures/skips.
Normal also passes245 Python tests. Stable tested fingerprint
`dd92309c3d306c27cd91a7a70e512aa5712f687a84eb326b9ee21822e84ca7b0`. ASan detect_leaks=0:halt_on_error=1; UBSan
halt_on_error=1:print_stacktrace=1; no leak coverage.

Private actual-stock audit `/tmp/m15_reaction139_stock_restart_v2.cpp` loads the
winning skeleton SHA-25643de349062d2f57b1e581353f1907f8f70bcd3c92b4fe3bd049989b93572d435,
captures its actual NIF renderer hierarchy, instantiates18 bodies/17 constraints
and applies one synthetic pelvis impulse. The producer simulates60 zero-gravity
steps, writes a populated binary runtime snapshot, then runs60 more steps for
an uninterrupted baseline. A distinct reader process loads the same asset,
validates/restores saved physical state into a new world and runs60 steps. Both
clean up all bodies and constraints. Native actor identity is an explicit
synthetic fixture, not a normal input actor. Comparison tolerances declared
before measuring: rotation components0.0001, positions0.001 and linear/angular
velocity0.001. `S4/native-ragdoll-stock-physical-restart-{normal,sanitized}-03`
passes both process continuations; maximum errors across builds are
{'rotation': 3.277527866885066e-07, 'position': 1.52587890625e-05, 'linear_velocity': 4.85248165205121e-07, 'angular_velocity': 3.1337549444288015e-08}.
Nonphysical runtime fields remain identical, saved input is unchanged and five
controls reject truncated data, wrong asset hash, model, node and actor base
without writing an output. Reports pin original asset, helpers, binaries,
libraries, source fingerprint, inputs/outputs, build commands and process IDs.
Normal01 retains a private C++ vexing-parse compile failure; normal02 retains a
fault fixture with an invalid eight-digit FormKey ID instead of the canonical
six digits. Fresh03 fixes that control while preserving the asset and all
predeclared physics tolerances. This proves separate-process physical storage
continuation for the private bridge, not native Havok trajectory equivalence.
World frame capture/admission/restore callers, gravity/velocity policy, native
activation/recovery and normal combat corpse/loot/media acceptance remain open.
The display-dependent runtime/TES3 regression still needs an environment that
can launch a display. No M15 gate is promoted here.


### Checkpoint140 — original motion delta/damping/velocity-cap kernel (S4/S6 open)

A pure native-unit velocity kernel implements the original sphere-motion stores:
supplied binary32 linear velocity delta first, separately clamped damping factors,
linear speed cap, then angular rotation-step cap. Original SSE squared-length
reduction uses Y+X then Z with binary32 multiplication/addition stores; x87
normalization factors are stored to binary32 before vector multiplication. Angular
step uses half dt, original0.40528470277786255 squared-angle constant and maximum
step0.8999999761581421. Zero duration preserves angular velocity even with zero
angular limit; linear cap remains independent of duration. Inputs require finite
vectors and nonnegative finite coefficients; overflow is diagnosed instead of
publishing nonfinite physical state. The angular-limit argument is explicitly the
raw original motion+B8 field. Conversion from serialized NIF maxAngularVelocity
has not yet been admitted and is not silently assumed. The kernel does not
prepare gravity, integrate transforms or change live actor/scheduler policy.

`S4/native-ragdoll-velocity-step-oracle-01` independently executes6,480 full
original8e96c0 returns with actual889470/4d6830/8b1dd0 and no stubbed calls, both
x87 precision settings. The corpus varies frame duration, two damping channels,
linear and angular limits, ordinary/zero/subnormal velocities and supplied zero,
signed-tiny or gravity-like fixture deltas. It captures damping and both cap
stores, and verifies final returned velocities retain those cap results. The
supplied delta and raw motion limits are explicit fixtures, not proof of original
world-step force preparation or NIF-to-motion property mapping. Three component
regressions pin independent original cases3918/3070, zero-duration/damping/tiny
branches and invalid vectors/coefficients/overflow without input mutation.
Green01 passes the three selected cases.

`S4/native-ragdoll-velocity-step-comparison-{normal,sanitized}-02` match allsix
binary32 output components exactly in all6,480 independently captured cases, with
five rejecting controls for negative duration/angular limit, NaN velocity,
infinite delta and squared-speed overflow. Reports pin original corpus, helper,
comparator, binary and component library hashes, build commands and tested source
identity. Normal comparison01 retains a private standalone helper missing its
cstdint include; fresh02 fixes the helper include without changing production
math or expected tolerances. Full `S4/native-ragdoll-velocity-step-{normal,sanitized}-01`
pass2,129 component and827 engine tests, complete inventories and zero failures/
skips. Normal also passes245 Python tests. Stable tested fingerprint
`9eb2b4ef494a8c5c42ca042b812e6073b7949a01ea2610ab40a1dc2d57c6c13f`. ASan detect_leaks=0:halt_on_error=1; UBSan
halt_on_error=1:print_stacktrace=1; no leak coverage. Existing physical ownership,
snapshot and native pose conversion behavior is unchanged. Live force/property
mapping, actor motion-mode/recovery policy, World frame admission/capture/restore,
normal-input corpse/loot/media and display-dependent TES3 runtime gates remain
open. This kernel is one prerequisite for those integrations; no M15 gate is
promoted and no native Havok whole-trajectory agreement is claimed.


### Checkpoint141 — loaded-body motion limits and atomic owned-body velocity step (S4/S6 open)

The original load prefix8a4513..8a4532 caps finite nonnegative Cinfo linear
limits at250. Construction prefix8a42dd..8a42fc raises them to at least250.
Together these produce250 for loaded bodies, including serialized10000 and
values adjacent to250. The angular limit remains the raw binary32 value:
8a9f50 passes Cinfo+A8 to8a9630, whose full factory returns copy it to motion+B8.
`ragdollLoadedMotionLimits` implements this loaded-body preparation and validates
all four coefficients before returning. It deliberately does not describe the
general motion factory as having a fixed250 limit. Owned bodies retain these
limits; `applyNativeVelocityStep` accepts explicit per-body native velocity
deltas, stages every damped/capped result, then publishes velocities without
changing poses, registration or world clocks. Invalid later-body inputs leave
all live velocities intact. Existing scheduler force policy remains unchanged.

`S4/native-ragdoll-motion-limits-oracle-03` independently executes420 original
cases across seven linear inputs, five angular inputs, six requested motion types
and both x87 precision settings. Both normalization prefixes execute actual
instructions and the motion factory executes through full return, including real
motion constructors and property dispatch. Its only stub is an isolated aligned
allocator with checked allocation size/tag and ret8. Cinfo arguments are synthetic;
original stream deserialization and complete bhk wrapper admission are not
executed. Normal/sanitized comparison01 match all four binary32 returned
coefficient fields exactly and reject12 negative/NaN/infinite controls. Retained
oracle01 records the initially incorrect upper-cap interpretation of the
constructor branch; oracle02 verifies constructor minimum preparation alone;
fresh03 verifies the actual load-then-construction sequence. No original expected
value or production tolerance is tuned to the implementation.

Three new component tests pin loaded thresholds/angular bits, actual owned-body
cap/delta application with unchanged pose, and all-body atomic rejection. Green01
passes the three selected cases. Full normal/sanitized01 pass2,132 components and
827 engine tests, complete inventories and zero failures/skips; normal also passes
245 Python tests. Stable tested source fingerprint `dffc7d561b698a7f8d0670dc2262dac9ed897c9c7e4b00bef171dcd1712ca01e`.
ASan detect_leaks=0:halt_on_error=1; UBSan halt_on_error=1:print_stacktrace=1;
no leak coverage. Original gravity preparation research is continuing separately.
Live scheduler gravity/cap policy, actual World admission/render/save wiring,
native recovery/mode semantics, normal-input corpse/loot/media and TES3 runtime
acceptance remain open. No M15 gate is promoted by these prerequisite checks.


### Checkpoint142 — native gravity and velocity caps in the physics scheduler (S4/S6 open)

The existing serial scheduler step now supplies binary32 native gravity*dt before
loaded-body damping and velocity caps, then advances Bullet exactly once using the
same physics duration. Default native gravity Z is the independently captured
-73.57500457763672. Each owned ragdoll disables Bullet world gravity and sets its
Bullet body gravity to zero, preventing a second application after damping; global
world gravity and unrelated collision objects remain untouched. Per-body delta
storage is allocated at admission and reused at each step. Sleeping bodies retain
their velocities/activation state; explicit impulses wake them. No World actor
admission, raw knocked transition or recovery timer is invented by this change.

`S4/native-ragdoll-gravity-delta-oracle-01` executes48 original world preparation
prefixes8d6e95..8d6f1f, which copy step info and store gravity*dt at world+190, then
actual island dispatch8d70e2..8d7113 through the full sphere-motion return. Both
precision settings cover zero/small/large durations, native/default, mixed-axis
and signed-zero gravity fixtures. No calls are stubbed; world/island/body objects
are isolated fixtures and prior actions/full world collision solving are outside
scope. Normal/sanitized gravity comparison01 match all three delta bits in48
cases and reject five controls. `S4/native-ragdoll-box-velocity-step-oracle-01`
executes6,480 full original8eaff0 returns after actual box construction; damping,
linear and angular cap stores match the sphere corpus exactly, including signed
zeros/subnormals. Both box comparisons01 match every output bit and reject five
controls. Transform integration executes but is not compared; no native Havok
whole-trajectory agreement is claimed.

Two component tests cover independently captured gravity bits/invalid inputs and
sleeping-body preservation/wake-up. Two scheduler tests across workers0/1/2 cover
actual one-step gravity-before-damping, length conversion, unchanged global
world gravity, speed caps and sleeping/impulse behavior. Focused green01 retains
nine failing engine cases from an incomplete call-site edit: Bullet gravity was
disabled but the scheduler still invoked damping alone. The corrected call site
passes green02 (74 component/29 engine cases). Full normal/sanitized01 pass2,134
component and833 engine tests with complete inventories, zero failures/skips;
normal also passes245 Python tests. Stable tested fingerprint
`930a31975de32c5b7eb49538b92eb7154b246eeebc0666e6b1da2b9b2531665b`. ASan detect_leaks=0:halt_on_error=1 and
UBSan halt_on_error=1:print_stacktrace=1; no leak coverage.

The actual stock18-body/17-joint skeleton also runs through the real scheduler,
native gravity, a static floor, finite absolute position bounds10000 and renderer
pose handoff tolerance0.001 in private producer/reader processes. However,
`S4/native-ragdoll-stock-scheduler-restart-{normal,sanitized}-02` FAIL their
predeclared continuation bounds (R0.01, position0.1, linear/angular velocity0.05),
first observed position difference0.24506044387817383. Normal01 retains an earlier
private report parser failure on a real settings-loader startup line; fresh02
whitelists that exact setup line, without changing production or tolerances.
No floor-contact restart acceptance is claimed and bounds are not relaxed.

`S4/native-ragdoll-stock-restart-diagnosis-normal-01` is a diagnostic, not a
passing restart gate. Repeated producers and checkpoints are byte-identical.
Restoring the float snapshot in place while retaining contacts still produces
maximum position difference0.47612762451171875, rotation0.2613157853484154,
linear velocity5.301872253417969 and angular velocity2.594521999359131.
Fresh-process restoration rebuilding contacts gives position0.4741659164428711,
rotation0.255676232278347, linear velocity11.168073177337646 and angular velocity
2.5634031295776367. This isolates substantial sensitivity to the float snapshot
boundary even with retained contacts; reconstructed contacts affect velocities
too. The diagnostic retains finite body/renderer bounds but does not establish
adequate continuation. Snapshot precision and contact continuation therefore
remain active work, alongside World render/save admission, native recovery/mode
policy, normal-input corpse/loot/media and display-dependent TES3 runtime gates.
No M15 gate is promoted by this scheduler prerequisite.


### Checkpoint143 — restore rotated inertia and interpolation velocities (S4/S6 open)

`ActorRagdollPhysics::restore` now assigns the restored velocities before calling
Bullet's `setCenterOfMassTransform`, which refreshes rotated world inverse inertia
and copies the velocities to interpolation state. The previous base
`setWorldTransform` path left those values stale. A focused anisotropic-body test
rotates inertia diag(1,2,3) by90 degrees about Z, uses a nonzero center offset and
applies a unit X torque through an off-center impulse: angular X must be0.5,
with interpolation velocity(2,3,4). The actual prior checkpoint142 sanitizer
library yields angular X1 and interpolation velocity(0,0,0), independently
confirmed in `S4/native-ragdoll-inertia-restore-red-01`. Its report identifies the
old library hash and tested revision; it does not claim to test current sources.
Focused green01 passes the new case. Full inertia-restore normal/sanitized01
pass2,135 component and833 engine tests each, complete inventories and zero
failures/skips; normal also passes245 Python tests. Both engine builds rebuild
openmw and esmtool. Stable tested source fingerprint `91cdd4949e69f427c746ede186960d066625c1de5f0325bff3b96a3bf7d2df68`.
ASan leak checks are disabled; UBSan halts on errors. No leak coverage claimed.

Stock floor-contact restart normal/sanitized03 still FAIL the unchanged declared
bounds(R0.01,position0.1,linear/angular velocity0.05), first position difference
0.24506044387817383. No restart or gameplay gate is promoted. Private precision
diagnosis normal01 retains a linker failure from exhausted temporary storage;
normal02 executes a full-double checkpoint but still fails rotation0.01 with
maximum rotation0.2556762620806694, position0.4741649627685547, linear velocity
11.168063640594482 and angular velocity2.56339693069458. This rejects increased
snapshot precision alone as the proposed correction; no production codec change
is made. It also corrects checkpoint142's provisional attribution to the float
boundary: that experiment changed multiple restore operations and did not isolate
float quantization as the cause.

Private inertia diagnosis normal01 and restore-operations diagnosis normal01
are diagnostic successes only, not passing continuation gates. They exercise
the actual18-body/17-joint stock graph, native-gravity scheduler, static floor,
finite position bounds10000 and renderer handoff tolerance0.001. Repeated
producers/checkpoints are byte-identical. Restoring a full-double shape pose in
place while retaining contacts still yields maximum rotation
0.26131683588027954, position0.4761161804199219, linear velocity5.3017425537109375
and angular velocity2.594456672668457. Separate activation-only, AABB-only,
velocity-assignment-only, exact-center-transform-only and clear-forces-only
controls each produce byte-identical final state to the uninterrupted baseline.
The combined shape/center pose round trip remains under investigation; these
controls do not establish whole-trajectory native Havok agreement. World actor
admission/render/save, native recovery/mode policy, normal-input corpse/media,
and display-dependent Morrowind runtime gates remain open.


### Checkpoint144 — scheduler-owned graph and validated snapshot boundary (S4/S6 open)

The scheduler now owns an immutable copy of each admitted physical graph and
constructs bodies/delta storage from that copy. Main-thread ownership queries,
inspection copies, snapshot capture and snapshot restore wait for workers and
use the collision-world lock. Capture uses the actual owned graph/body state;
restore validates the snapshot against the owned asset and body/node identities
before changing physical state. Callers cannot replace the admitted graph by
mutating the original admission argument or an inspection copy. Reference
rebinding moves the graph/body/snapshot ownership together; stale references do
not capture or remove the new owner. No World admission/lifecycle is claimed.

Two parameterized tests cover workers0/1/2, successful restore, changed asset,
body/node identities and nonfinite velocity rejection without partial state,
external graph mutation, reference rebinding and cleanup. Normal/sanitized01
retain three test-expectation failures: the established codec throws
runtime_error for nonfinite velocity, while identity mismatch throws
invalid_argument. The corrected tests preserve those existing contracts.
Full owned-snapshot normal/sanitized02 pass839 engine tests each, complete
inventories, zero failures/skips; both builds rebuild openmw and esmtool.
Stable tested source fingerprint `f0844c84b3ac8f693c5c7418dcfa617881291b79933e43fe68a42c52f7ab4a8e`. No component/Python
sources changed; their checkpoint143 passing evidence is not relabeled as a
new run. ASan leaks are disabled and UBSan halts on error.

Private `S4/native-ragdoll-stock-exact-com-diagnosis-normal-01` is diagnostic
only: it stores exact Bullet center-of-mass transforms/velocities as doubles
and restores them directly into a fresh18-body/17-joint stock scheduler world.
Even this control fails the existing trajectory bounds: maximum rotation
0.24695831537246704, position0.4578094482421875, linear velocity7.057619273662567
and angular velocity2.702526092529297. It preserves the finite position/renderer
handoff bounds and does not change production serialization or tolerances.
Thus removing the shape/center conversion alone does not establish fresh
contact continuation; the restart gate remains open.

`S4/native-ragdoll-recovery-dispatch-oracle-01` independently executes512
signed-byte dispatch cases and80 mode6 completion cases in the pinned original
executable, with no stubbed game calls. Actual jump table at654c58 maps raw1 to
654913, raw2/raw4 to654803, raw3 to654886, raw5 to654a87 and raw6 to654c3b;
unknown bytes exit unchanged. Mode6 executes actual472ea0 and the update suffix
through its return: completion requires animation+D0 null and either animation+CC
null or its field+10 nonzero with kind3. The earlier actor eligibility and full
method are outside scope; synthetic animation fields do not prove real get-up
timing. These results ground further lifecycle work, not a passed gameplay gate.
World render/save/collider handoff, native recovery/mode policy, normal-input
corpse/media and all other open M15 gates remain incomplete.


### Checkpoint145 — suspend movement capsules during physical ownership (S4/S6 open)

`Actor::suspendCollision` changes registration through the scheduler after waiting
for workers under the collision-world lock. Suspension removes the movement
capsule entirely and clears queued velocity, inertia, grounding and standing-on
state. Repeated suspend/resume is idempotent; a foreign scheduler is rejected.
Mask changes while detached retain the desired flags without dereferencing a
missing broadphase proxy. Immediate/queued capsule AABB refreshes do not publish
a detached capsule. Resume refreshes its transform from the current reference
and registers the current collision mask. The default is unsuspended.
`PhysicsSystem` excludes suspended capsules from movement preparation and both
NPC/player position publication, preventing a second movement owner once the
World ragdoll handoff is connected. No native admission/mode policy is added.

Two engine tests exercise real physics Actors, actual ragdoll gravity, capsule
membership and idempotence, cleared movement/contact state, detached position
updates, updated external-collision/water-walking flags, current-position resume,
destruction and foreign-owner rejection. The physical coexistence/resume case
runs workers0/1/2. Full capsule-suspension normal/sanitized02 pass841 engine tests
each with complete inventories, zero failures/skips; both builds rebuild openmw
and esmtool. Stable tested fingerprint `1755823050f622310f6a1b6f0e2fac1e724b15a6f451cb24d80159893fc59c82`. Components/Python
were not changed or rerun. ASan leak checks disabled; UBSan halts on error.

Normal/sanitized01 retain compiler failures from temporary-file quota exhaustion
(`Disk quota exceeded` writing compiler assembly files under/tmp), before tests.
The unchanged source passes fresh02 with compiler temporary storage redirected:
`TMPDIR=/home/maciek/openmw-oblivion/openmw-oblivion/build/oblivion-compat/m15/compiler-tmp`.
The ignored workspace directory has adequate storage; no unrelated temporary
files or retained evidence were removed. This environment setting should be
used for subsequent large parallel builds in this session.

World admission/release, render/save synchronization, verified native reaction/
recovery policy and the failed stock floor-contact restart audit remain active
work. This tested ownership prerequisite does not promote any M15 gameplay,
media, Morrowind display-dependent runtime or final acceptance gate.


### Checkpoint146 — PhysicsSystem physical ownership facade (S4/S6 open)

PhysicsSystem now admits a scheduler-owned ragdoll only for an existing,
unsuspended movement capsule, then suspends that capsule. Failed admission
preserves capsule ownership; a failed suspension rolls back the new graph.
Release resumes the capsule before removing the physical graph and is
idempotent. The public boundary exposes owned graph inspection, body capture,
validated snapshot capture/restore and impulses. Object removal still destroys
both physical and capsule ownership without unnecessarily resuming the capsule.
No World reaction/lifecycle caller or native collision-group policy is claimed.

An integration test loads an actual editable synthetic collision model through
VFS and ResourceSystem, admits a native actor through PhysicsSystem and covers
workers0/1/2, missing/invalid/duplicate admission, capsule proxy membership,
owned graph/snapshot inspection, impulse and restore, atomic wrong-asset
rejection, repeated release and object removal. Initial normal/sanitized01
failed in headless shader initialization before admission; the retained failures
are corrected by supplying the actual shader path, default/shadow definitions
and LightManager definitions. Full normal/sanitized02 pass842 engine tests each,
complete inventories, zero failures/skips, rebuilding openmw and esmtool.
Stable tested source fingerprint
`95a16fada2e8a0470ca46b83e9cc0e7af692cfba0680b7b5b133e6a41ba27026`.
Component/Python sources are unchanged; no new runs are claimed. ASan leak
checks remain disabled and UBSan halts on errors.

World admission, renderer/save synchronization, native recovery policy and
stock floor-contact restart acceptance remain incomplete. This facade closes
an ownership prerequisite and does not promote a gameplay acceptance gate.


### Checkpoint147 — preserve native filters and verify their ordered predicate (S4/S6 open)

The owned NIF graph retains both world-object and rigid-body-info layer, flags
and group fields independently, without assuming they match or changing current
physics admission. The collision-filter kernel accepts caller-owned layer/bone
mask tables and rejects layers32..63 before lookup. Its initial tables and ordered
branches follow the pinned original initializer008a83c0 and predicate008a7f70.
Zero groups, flag14, the ordered layer29 exception, same/different groups, bone
IDs and flag15 adjacency retain the original branch order. Runtime actor-group
assignment, dynamic layer-mask policy and World admission are separate work.

`S4/native-ragdoll-stock-filter-audit-01` reads actual18 stock bodies; both filter
fields match, layer8/group0 with distinct bone IDs. Asset SHA256
`43de349062d2f57b1e581353f1907f8f70bcd3c92b4fe3bd049989b93572d435`.
`S4/native-ragdoll-collision-filter-oracle-01` executes original initialization,
actual CRT memset and full predicate returns, with no stubbed game calls:
37,836 cases span all32x32 supported layers, group/flag combinations, all32x32
bone IDs and stock18x18 ordered pairs. Original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`;
corpus SHA256 `5789dbeb98a0d2696f0f356c737c7b0550c6d763370bb82e28e41e564202c546`.
Actual C++ normal/sanitized comparison each matches37,836 results and both
complete tables exactly. Always-enabled/disabled controls mismatch23,069/14,767
cases; swapping arguments mismatches726. No gameplay gate follows from this.

Three component tests cover independent field retention, unsupported layers
even on wildcard/disabled paths, ordered flag handling and caller mask changes.
Full filter normal/sanitized01 pass2,138 component and842 engine tests each,
complete inventories, zero failures/skips, rebuilding openmw and esmtool.
Stable tested fingerprint
`b61302c59973a897969136a616048ca638ec53d11b9f29bddcf9ed3bdc929f05`.
Python sources are unchanged; ASan leaks disabled and UBSan halts on errors.

Private stock-filtered scheduler restart normal01 supplies a nonzero same-actor
group and original bone masks; it does not execute actor-group allocation. It
still fails the unchanged continuation bounds, first linear velocity difference
0.05434548854827881 exceeding0.05. Full snapshot maxima are rotation
0.008762143552303314, position0.2649369239807129, linear velocity
2.0861258506774902 and angular velocity0.857350766658783. The private cold-solver
normal01 control disables Bullet warmstarting and also fails (first linear
velocity difference0.32358162105083466 exceeding0.05). Neither changes production
solver behavior, serialization or tolerances. Both failures remain retained;
contact continuation, World lifecycle/render/save and gameplay gates stay open.


### Checkpoint148 — apply resolved internal filters before physical admission (S4/S6 open)

Physical construction accepts optional caller-resolved native system-group and
mask inputs through PhysicsSystem and the worker-barrier scheduler. The graph's
rigid-body-info layer/flags combine with that group. Validation and internal
ignored-pair setup finish before any body/constraint enters the world. Unsupported
layers, including single-body graphs, and asymmetric internal pairs fail without
publication. Connected pairs follow supplied native masks rather than an
unconditional Bullet constraint collision exclusion. The existing unfiltered
callers retain their prior behavior. Inputs are consumed during construction;
no borrowed mask lifetime or later caller mutation changes the admitted pairs.
This boundary governs internal pairs only, not other owners or native World
group assignment/reuse/dynamic-mask policy.

Original allocator00531d80 executes complete returns for all65,536 uint16 prior
counter values, no stubs. It increments modulo65536 and substitutes10 at zero.
The executable's raw initialized counterB2EB3C is10; this is a PE data audit,
not a runtime initialization trace. Corpus SHA256
`6e0754a1658f62d1cae7d9cbc706d53e98aee0da64e0eba79006b89e4f01d663`;
original executable identity is the checkpoint147 pinned hash. Actual C++
normal/sanitized comparisons each match all65,536 outputs; zero-on-wrap and
one-on-wrap controls each fail the wrap case. Actor group reuse branches and
persistent runtime allocation are not claimed from this isolated operation.

Four component tests cover real overlapping sphere contacts, zero-group
wildcards, immutable consumed masks, connected-pair filtering, malformed and
asymmetric admission cleanup, plus allocator boundaries. The PhysicsSystem
integration test exercises the new boundary across workers0/1/2 and verifies
that an invalid filter preserves the movement capsule. Full admission
normal/sanitized01 pass2,142 component and842 engine tests each, complete
inventories, zero failures/skips; openmw and esmtool rebuilt. Stable tested
fingerprint `2544d4359f419fdb6ddb77b6d65bcb56f397fe19303c1f641fbd9da563020c29`.
Python sources unchanged; ASan leaks disabled, UBSan halts on errors.

World lifecycle, renderer/save coordination, native controller group assignment
and the stock floor-contact continuation gate remain incomplete.

Private `S4/native-ragdoll-stock-admitted-filter-scheduler-restart-normal-01`
loads the actual18-body/17-joint stock graph through the production admission
boundary, with allocator result10 from explicit prior9. Checkpoint, uninterrupted
and resumed binary states exactly match the previous manual-filter diagnostic;
`manual-filter-equivalence.json` records all three hashes. It still fails the
unchanged velocity bound at0.05434548854827881>0.05. This comparison verifies
filter integration equivalence, not a passing restart or normal-input gate.


### Checkpoint149 — keep capsule and physical ownership together on reference replacement (S4/S6 open)

PhysicsSystem reference updates now rekey an admitted movement actor together
with its scheduler-owned ragdoll when the live object identity changes. Empty
and occupied actor destinations are rejected before physical rebinding. A new
map slot is allocated first; a failed scheduler rebind removes that empty slot.
Only then does the capsule move to its new key and update its Ptr. Existing
standing-on/projectile owner updates continue afterward. Same-identity updates
retain their previous path. Ordinary CellStore moves retain the live object
identity; this repair concerns replacement references, not a changed cell-move
implementation or a demonstrated normal-input corpse course.

The actual PhysicsSystem/resource integration test now covers a distinct live
replacement retaining the stable FormKey, rejected duplicate/empty owners,
continued suspended capsule membership, new-owner graph/snapshot access, stale
lookup/capture/removal isolation, idempotent rebinding, release/resume and final
removal across workers0/1/2. Full capsule-rebind normal/sanitized01 pass842
engine tests each, complete inventories, zero failures/skips, rebuilding openmw
and esmtool. Stable tested fingerprint `b0122eaddba9eb37da8679f3f7671d3deb502f1d088c7cfd42facf08e7fbc6a6`.
Component/Python sources are unchanged; checkpoint148 evidence is not relabeled
as new runs. ASan leak checks disabled; UBSan halts on errors.

World physical reaction admission/render/save, native controller recovery/group
policy and floor-contact restart acceptance remain open.


### Checkpoint150 — preserve authored blend fields and verify the native entry gate (S4 open)

Owned ragdoll bodies now retain optional bhkBlendCollisionObject record identity,
flags, hierarchy gain and velocity gain. Plain collision records remain absent;
nonfinite authored gains are rejected while finite negative gains are preserved.
The owned copy does not borrow source NIF records or represent live progression.

The native raw2/4 entry gate uses hierarchy gain <=0 when a scene root exists.
A missing blend lookup returns1 and holds entry; a missing root skips this gate.
Original prefix00654803 reaches00654824 (ready) or00654c4d (hold), using actual
0088fa30/0088f200 lookup and actual casts/RTTI initializer/getter with no game
call stubs. All104 cases cover both x87 precision words, four scene placements,
signed zero/subnormal/finite/infinity/NaN gains. Actual normal and sanitized C++
comparators match104 each; always-ready/always-wait/reversed-comparison controls
produce50/54/66 mismatches. Original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`;
corpus SHA256 `b262f81cfde97f97778034a801046fdeeb671b9b3f1086a5d5bd96bae5b142b8`.
Evidence: S4/native-ragdoll-blend-entry-oracle-01 and
S4/native-ragdoll-blend-entry-compare-{normal,sanitized}-01.

Independent bounded stock NIF byte decoding identifies all18 blend records,
flags1 and both gains1, without the production NIF reader. It explicitly admits
this stock flags1 fixture, not arbitrary NIFs. The production NIF reader and
owned graph each match all18 records/gain bits. Stock asset SHA256
`43de349062d2f57b1e581353f1907f8f70bcd3c92b4fe3bd049989b93572d435`;
independent corpus SHA256
`1b0cfaf6813271b06a2796a3fbf49d4bfc9ec41967c709fff153a136d304072c`.
Evidence: S4/native-ragdoll-stock-blend-byte-oracle-01,
S4/native-ragdoll-stock-blend-audit-01 and
S4/native-ragdoll-stock-owned-blend-normal-01.

Three added component tests cover owned identity/flags/gains, lifetime isolation,
finite admission and exact gate boundaries. Full blend-entry normal/sanitized01
pass2,145 component and842 engine tests each, complete inventories and zero
failures/skips; openmw and esmtool rebuilt. Stable tested fingerprint
`13d5cfbeae4519b16600b16a2a25ccd2df5f4717fa7d254af168e423eefbc472`.
Python sources unchanged; ASan leaks disabled, UBSan halts on errors.

This adds the verified gate rule and authored data, not a World caller or live
gain progression. Native controller recovery, coordinated runtime admission,
renderer/save handoff and stock floor-contact continuation remain open.


### Checkpoint151 — native one-shot blend clock and gain interpolation (S4 open)

Checkpoint150 is committed as `8d6c24f27746c5e4ca2959ef9b83cc147419752c`;
isolated-git-progress-128 verifies147 commits, exact shared bytes and a fresh
bundle clone. This chunk adds immutable finite zero/one/two-key gain evaluation
and a mutable absolute speed1/phase0 one-shot clock, matching native flags0xc5.
The clock establishes its origin on the first update, retains unclamped elapsed
and previous time, allows backward time and returns a clamped key time. Invalid
inputs/overflow leave clock state unchanged. These are rules, not scene
controller creation or a persistent World reaction owner.

Original0088aa990 and006d3690 execute full gain-evaluator/interpolation returns
without stubs. Initial1,440 cases and extended4,168 cases cover empty/one/two
keys, exact endpoints, independent signed gains, shifted intervals, signed zero
and2,000 generated fixtures, each under both x87 precision words. Original
007155a0 executes full one-shot clock returns for330 sequential and96 signed-
zero/subnormal/snapshot-state cases. Only Windows EnterCriticalSection and
LeaveCriticalSection are stubbed; no game-function boundary is substituted.
All native corpora use executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Corpus SHA256, respectively:
`b5ed23509e013af61bef64c423d44c1b3f3343dca01d7b663e9e27b9537e16c8`,
`b585d6fcd5a3165f5a4c7b4280cd8ba7ee866da3b14fb414e357e795cf077a1d`,
`bc85a9b652e51b40ade0b24f3844679dadf53bbda4947072ae113fe14a436b40`,
`d8dc49b210ce4def72201c9a0d6f6f345a435477e504d9411a7a455885b24eb0`.
Evidence: S4/native-ragdoll-blend-interpolation-oracle-{01,02},
S4/native-ragdoll-blend-clock-oracle-{01,02}.

Retained compare-normal02 found70 exact mismatches despite initial unit suites
passing: the native evaluator stores the key interval before division, not the
numerator, and interpolates at the first endpoint. Retained compare-normal04
found two signed-zero key-time mismatches: positive-zero phase is added before
clamping while the elapsed negative-zero bits remain stored. Regression tests
preserve both discoveries; no tolerance was used or relaxed. Initial full
normal/sanitized01/02 checks remain evidence of their earlier source revisions,
not evidence for the final corrected code.

Final S4/native-ragdoll-blend-progression-compare-{normal,sanitized}-05 each
match all5,608 gain and426 clock cases exactly. Constant-gain, always-zero-clock
and clamped-stored-elapsed controls disagree4,488/280/154 times respectively.
Full blend-progression normal/sanitized03 pass2,153 component and842 engine
tests each, complete inventories and zero failures/skips; openmw and esmtool
rebuilt. Stable tested fingerprint
`40167aa9f0a78542a554d91c6722a840c4d06136556e14d052f96957acd18772`.
Eight added component tests cover independent gains/entry endpoint, first-update
and backward clock, copied-state continuation, invalid-key/clock atomicity and
three exact-bit regressions. Copied clock continuation is not a save/restart
course. Python unchanged; ASan leak checks disabled, UBSan halts on errors.

Native BlendSettings.ini duration resolution, scene/controller attachment,
blended physical mode/pose/velocity handling and World admission/render/save
coordination remain open, as does stock floor-contact restart acceptance.


### Checkpoint152 — native duration tables and zero-duration controller rules (S4 open)

Checkpoint151 is committed as `ef3b7c5c60d43fa07ea36ceff3fd60af6f2d67e7`;
isolated-git-progress-129 verifies148 isolated commits, exact source bytes and a
fresh bundle clone. New physicalblendsettings owns the independently identified
32-entry duration defaults: body IDs0..24 use get-up1/knockdown0.25 seconds;
IDs25..31 use-1. Configuration updates replace currently nonnegative entries
and preserve negative entries, including on repeated updates. The native body
ID selector uses packed world-object filter bits8..12. Settings are explicit
BlendSettings.ini HIT inputs, not GMST lookups or a completed INI import.
Nonfinite typed settings/table entries are rejected without mutating inputs.

Original data B11A24/B11A2C identify fGetUpTime:HIT/fKnockDownTime:HIT defaults;
B2EE68/B2EEE8 identify the32-entry tables. Original prefix0053a40c..0053a45c
updates the tables and returns, no game calls/stubs. Two x87 words and100
setting pairs yield200 cases, of which128 have finite configured values and
belong to typed admission. Corpus SHA256
`0d8103ec614414bfb1e4eca109d9057cd5af99c1795ab1099470e96d3e014b9f`;
S4/native-ragdoll-blend-duration-oracle-02. Initial oracle01 is explicitly
invalidated: its prefix setup failed to reset ECX (original0053a3ec does so),
causing later cases to retain the loop index128. Both failed duration compare01
outputs are retained; no production change was inferred from that invalid
corpus. Corrected setup supplies ECX0 for every independent case.

A zero-duration one-shot clock is valid and returns key time0. Original gain
evaluation skips interpolation for a zero key interval and retains the first
key's gains, including signed zero; it does not divide by zero or invent an
instant jump to the second key. Additional full original evaluator/clock
returns cover30/66 cases, respectively, with clock Windows critical-section
boundaries stubbed as in checkpoint151. Zero-interval oracle01's incorrect
helper-visitation assertion is retained; corrected oracle02 verifies that the
linear interpolation helper is not called. Corpus hashes:
`9fbfe4089700fd4a01f2c07769090749e7d018d41fa6749b77edb92ede7306b1`,
`a8fb5a4239d24e27b2fa352fce09e482f60616c4751c4b30515040c595d84e31`;
S4/native-ragdoll-blend-zero-duration-interpolation-oracle-02 and
S4/native-ragdoll-blend-zero-duration-clock-oracle-01. Original executable
identity remains `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.

Final S4/native-ragdoll-blend-durations-compare-{normal,sanitized}-02 each match
5,638 gain cases,492 clock cases and128 finite table cases exactly. Constant
gain, zero clock, clamped stored elapsed and ignored settings controls reject
4,488/280/208/126 cases respectively. Four added component tests cover zero
intervals, every packed body selector/default, signed-zero/negative/repeated
configuration and nonfinite rejection; earlier malformed-key/clock tests now
use genuinely invalid descending/negative domains. Full duration
normal/sanitized01 pass2,157 component and842 engine tests each, complete
inventories and zero failures/skips; openmw and esmtool rebuilt. Stable tested
fingerprint `8d70b24f092352f7ba33065f637875d218a60de2e43a3262a76d46a9b4aabe03`.
Python unchanged; ASan leaks disabled, UBSan halts on errors.

Winning BlendSettings.ini import, actual scene key/controller creation,
physical motion/pose/velocity blending, World admission/render/save and stock
floor-contact continuation are still open. Rule/helper evidence does not close
normal-input reaction or fresh-process gameplay gates.


### Checkpoint153: prepared-target physical velocity blending

Checkpoint152 committed as `dac976344efca6435e316f983166dacad9168913`
with exact changed bytes and a verified fresh bundle clone. The new
NifBullet::ragdollNativeBlendVelocities mixes current linear/angular velocities
with supplied already prepared/capped target velocities. Finite gains remain
unclamped. Each native SSE product/sum retains its binary32 store; a present
world additionally subtracts gravityZ*gain/inverseFrameSeconds from linear Z
with only the final x87-equivalent binary32 store. Missing worlds receive no
gravity compensation. Nonfinite inputs, nonpositive inverse time and output
overflow reject without mutating supplied velocities.

Original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`;
S4/native-ragdoll-blend-velocity-oracle-01 executes original prefix
008a37e8..008a388c, stopping before wake/publication, and calls the actual
0089d940 world getter through the original wrapper vtable. Synthetic wrapper,
body, motion and optional world fields supply current and prepared targets;
there are no game/OS call stubs. Both x87 precision words and nearest SSE
without denormal flushing cover19,800 cases:100 vector fixtures,11 gain
patterns including signed zero/subnormal and values outside0..1, three
inverse frame times and three world/gravity choices. Oracle source SHA256
`f14f39f777c44c8afcfb712506cb8932b7cbb2e97d78290dd1dd4aadb64c3459`;
corpus SHA256
`0075280329514bf655d85cc4b7eea2d2b067518bb40a2fb5eca62326cb753131`.

S4/native-ragdoll-blend-velocity-compare-{normal,sanitized}-01 each compare
all19,800 C++ outputs exactly with zero mismatches. Always-current,
always-target, omitted gravity and clamped-gain controls reject
10,836/18,402/3,654/5,406 cases. Comparator/driver SHA256:
`2e936640cb10f5a038cd1e3596561419eb570e28a34d8d7494d1b3cbedf28180`,
`8410192e30e634f0d72782b5db68e0a812312202c69d3785421611555ac36815`.
Four component tests cover exact independent stores, unclamped gains,
optional gravity, signed zeros and invalid/overflow rejection. Full
S4/native-ragdoll-blend-velocity-{normal,sanitized}-01 each pass2,161
component and842 engine tests, complete inventories, zero failures/skips;
openmw/esmtool rebuilt. Stable tested source fingerprint
`0e749004b5d94c82807b2d2c8d0f67d20d609da3bd572aae3ebba56c17b3c211`.
ASan leaks disabled; UBSan halts. Python unchanged.

This closes the prepared-target mix rule only. Pose-to-target velocity
preparation/caps, motion selection, body wake/publication, controller
attachment, World/render/save lifecycle and stock floor-contact continuation
remain open. S4 and all later acceptance gates retain their existing status.


### Checkpoint154: pose-to-target velocity preparation

Checkpoint153 committed as `29d89b5350cba2850ca9e89c3056e6b3e13123cb`,
150 isolated commits, exact changed-byte and fresh-clone verification. New
ragdollNativeTargetVelocities rotates body-local COM into the desired body
pose, subtracts current world COM, multiplies by inverse frame time and caps
linear speed. Desired rotation times conjugate(current) supplies normalized
relative rotation; the original angle threshold selects angular preparation,
quaternion signs select the short rotation and angular speed receives its
separate cap. These caps are motion+B4/B8 vector speed limits, distinct from
the ordinary integrated angular rotation-step cap. Caller supplies resolved
limits; alternate keyframed-motion resolution is not implemented here.
Typed admission requires finite near-unit quaternion inputs (squared norm
within1e-4), positive finite inverse time and nonnegative finite caps; invalid
inputs/overflow reject without input mutation. Portable reciprocal square root
replaces CPU-specific approximate RSQRT. Angular comparison tolerance was
predeclared2e-5 absolute plus2e-5 relative; measured outputs are stronger.

Original pinned executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
S4/native-ragdoll-target-velocity-oracle-01 executes actual prefix
008a34c0..008a37e8 with actual wrapper local-COM/current-COM/quaternion getters
008a2ff0/008a3030/008a2f10, sphere motion type lookup008a98d0,
quaternion composition00889470, normalization004d6830, angle008a2c00,
axis008a2c70 and actual CRT acos; no game/OS call stubs. Synthetic supplied
native fields are not game-world acceptance. Initial1,440 cases span80 pose
fixtures, three inverse times, three cap pairs and two x87 precision words.
Oracle source/corpus SHA256:
`edcb2213e93529c672c70bab8554bc494b2d71a7be7043eb9406965cacd2752d`,
`b564e0d7f873bbad22287015af9824b1b0d3d6310e815e5615bb04f85ee675d1`.

Extended oracle02 includes the initial fixtures plus small-angle/threshold
cases, four rotation axes, equivalent quaternion signs and immediately
adjacent scalar quaternion components near1:3,312 total cases. Source/corpus:
`4b966f958d014134d58ef893cb97bd75cc28f8c8f0fd30be3a416399faf43ffb`,
`9272c7e5df872d031687d7e760f289e10419a997cdcba172483efb997bcc70d0`.
S4/native-ragdoll-target-velocity-compare-{normal,sanitized}-02 each match
all3,312 linear AND angular outputs bit-for-bit; maximum angular error0.
This does not prove every original CPU's approximate RSQRT bit pattern.
Zero-linear/zero-angular controls fail2,100/1,344 cases; a real C++ run with
local COM erased fails2,292 cases. Comparator/extended driver SHA256:
`90a34b25358080272ff6aed483decb310db51d15149edd1992ceeca5dc748770`,
`95586d742847e5071dde53e0afab429e7e0ae7537558fb10a9796deefb765e67`.
Initial comparator01 also matches all1,440 outputs in both modes. The private
driver's initial syntax error occurred before any build/run and was corrected;
no production adjustment was required by these comparisons.

Three component tests cover original rotated COM and independent caps,
quaternion sign equivalence/zero limits, malformed inputs and overflow.
S4/native-ragdoll-target-velocity-{normal,sanitized}-01 each pass2,164
component and842 engine tests, complete inventories, zero failures/skips;
openmw/esmtool rebuilt. Stable tested fingerprint
`71ebcc54551ede22bbb0542430d147fba4cc83c08eb7d8f129bad4c59825da00`.
ASan leaks disabled; UBSan halts; Python unchanged. Actual body publication,
wake/motion mode, scene controller attachment, World/render/save integration
and stock floor-contact continuation remain open. No stage is newly closed.


### Checkpoint155: owned body pose drives and connected activation

Checkpoint154 committed as `51c82c1bcc041db519d985b9a9f014d2f84a1577`,
151 isolated commits with exact bytes/fresh-clone proof. ActorRagdollPhysics
now accepts sparse native target-pose drives keyed by owned body record.
It prepares capped targets, mixes current velocities with the supplied gain,
compensates supplied native world gravity and converts linear units once.
Every result is validated before velocity or activation mutation. Duplicate/
unknown records, bad target quaternion, nonfinite gain/gravity, invalid inverse
time and overflow reject the entire batch. The driver preserves body poses,
forces, contact and interpolation state; it does not restore/step transforms.
Empty selections leave sleeping graphs alone. An explicit zero-gain call still
activates eligible bodies as the original setter does; controller selection
must decide when to call it. No controller selection is claimed here.

Original pinned executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
S4/native-ragdoll-full-velocity-publication-oracle-01 now executes the complete
008a34c0 return, including actual preparation callees,0089d940 world getter,
0089f570 dispatch,008a6410/008cbc00 activation, and0089db90/0089dbb0 velocity
stores. There are no game/OS call stubs. Synthetic existing-index activation
objects avoid allocation; scene motion-mode selection and collision stepping
are excluded. Twelve pose fixtures, two x87 words, three inverse times/cap
pairs/gains, present/absent world, initially active/inactive activation object
and prevent-activation byte yield5,184 full returns. Actual activation occurs
648 times, including216 zero-gain calls;3,240 cases finish active. Source/corpus
SHA256:
`3581fbd09256c0b5a77e4ec5973224c01c765fc9ea1939e8d76fb66350e4268e`,
`1be490ab71572d2b4a3c5ed56346239bb5a833e2e5abd42372f60f67c312a8ab`.
Two private script syntax errors were fixed before any emulation/evidence
creation; setup-failures.txt retains the account.

S4/native-ragdoll-body-drive-compare-{normal,sanitized}-04 each compare432
applicable full-return cases through actual constructed owned Bullet graphs:
world present, activation eligible, loaded max-linear250. Linear/angular bits
match exactly; active state and unchanged transform agree in every case.
Portable angular tolerance remains predeclared2e-5 absolute+relative; observed
maximum error0. Missing publication, omitted activation and forced zero gain
controls reject288/216/288 cases. Comparator/driver SHA256:
`0d1d5708cc40a173d6164133938a616ba53e38205030786641f63ff32132ddc0`,
`4efe1dab78643dd84ecf20e5399510b23ceef64a1299b65e0af67f8882ec19b1`.
Comparator01 compile failures and sanitizer02 failure retain build logs and
failure.json: assumed Bullet include paths were wrong; no cases ran. Corrected
normal02/sanitizer03 passed before the final connected-activation revision.

Native activation changes the shared activation object, not only one setter's
body. Owned constraint-connected groups are reconstructed during construction
and awakened before native gravity/damping. Both pose drives and impulses use
that grouping. Disconnected owned bodies remain asleep. This addresses owned
constraint connections only; contact-based islands across owners remain a
world integration concern. No cross-owner activation acceptance is claimed.

Retained S4/native-ragdoll-pose-drive-baseline-01 executes four failing new tests
against a do-nothing driver. Initial normal/sanitized01 each fail three tests:
Bullet's default vector constructor left staged fourth components uninitialized;
explicit zero vector construction fixes this. Normal/sanitized02 pass2,168
component/842 engine tests, but are superseded: connected-baseline01 then
executes two failing tests showing the undriven connected bone stayed asleep
and skipped gravity after both a pose drive and an impulse. Final owned-group
activation corrects both failures. Six new component tests now cover publication,
force/pose preservation, disconnected selection, whole-batch rejection,
length/principal-COM conversion and connected activation before integration.
Final S4/native-ragdoll-pose-drive-{normal,sanitized}-03 pass2,170 component
and842 engine tests each, full inventories, zero failures/skips; openmw/esmtool
rebuilt. Stable tested fingerprint
`7bbbaf8169285027ba0f9c480f0049a3efeb34c2803c4abac522594be30aabe8`.
ASan leaks disabled; UBSan halts; Python unchanged.

Scheduler/PhysicsSystem drive dispatch, contact-connected world activation,
controller creation/selection, native frame-time resolution, motion-mode and
hierarchy pose blending, actual World/render/save lifecycle and stock floor
continuation remain open. S4 and later gameplay/restart gates remain open.


### Checkpoint156: scheduler and PhysicsSystem pose-drive dispatch

Checkpoint155 committed as `dd66e47fa0e0574e6ad1bb045488aa18035a797d`,
152 isolated commits, exact changed bytes and verified fresh-clone bundle.
PhysicsTaskScheduler now serializes native pose drives with movement workers
and the collision-world mutex, resolves the single owned graph by live
reference and dispatches its staged body driver. PhysicsSystem forwards to
that same authority. Gravity matches the scheduler's existing native world
step (RagdollNativeDefaultGravityZ); shared Bullet world gravity is untouched.
Controller callers must supply resolved inverse frame time. Original frame
preparation/configuration and actual controller calls are not implemented here.

Three added parameterized scheduler tests each execute with0/1/2 workers:
exact native gravity compensation and unchanged pose/shared gravity/owner;
drive after queued worker work and physical movement on the next substep;
invalid target/time, empty/stale/rebound/removed ownership and real snapshot
capture/restore. The existing native NPC PhysicsSystem admission test also
exercises the public drive after actual LiveRef replacement, verifies stale
owner rejection, keeps its ordinary capsule suspended, preserves the body
pose and restores the same snapshot. Its VFS/ResourceSystem/PhysicsSystem are
real; it does not initialize the full World renderer/gameplay pipeline.

S4/native-ragdoll-pose-drive-scheduler-baseline-01 builds the missing adapter
reproducer and executes exactly nine selected cases, all failing. This is a
filtered failing baseline, not full engine acceptance. The actual adapter
corrects it. The native NPC fixture's drive uses its actual initial body Z20,
so its displacement stays on X and tests publication without unintended caps.
Independent expected compensation bit pattern1050473923 is from the original
checkpoint153 prepared-velocity oracle (gain.5, inverse120, gravity default).

Final S4/native-ragdoll-pose-drive-scheduler-{normal,sanitized}-01 each pass
all851 engine tests, complete inventories and zero failures/skips; openmw and
esmtool rebuilt. Stable tested fingerprint
`50354f376d93e87dff5c6653aee3dad8a69b4f848e87faf02e82883de40fcb97`.
ASan leaks disabled; UBSan halts. Components/Python are unchanged; prior2170
component checks retain their original checkpoint155 scope/fingerprint.

Actual controller creation/selection and frame-time resolution, contact-based
activation across owners, physical mode/hierarchy blending, World/render/save
handoff and stock floor continuation remain open. These worker/physics tests
do not close normal-input or fresh-process acceptance. S4 remains in progress.


### Checkpoint157: native physical blend dispatch selection

Checkpoint156 committed as `a12132b5f5aabb6f11c44b8c4ee5d0aed069e627`,
153 isolated commits with exact-byte and fresh-clone proof. New immutable
physicalblenddispatch selects the native requested motion and pose route.
Zero hierarchy chooses dynamic motion with physical-to-scene sync when
velocity is zero or flag0x100 is set; otherwise it chooses pose/velocity blend.
Exact hierarchy1 chooses keyframed motion and scene-to-physics sync. Other
finite hierarchy values remain unclamped and normally choose dynamic blend.
Raw selector1 skips updates at hierarchy0; selector2 skips at hierarchy1 and
selects physical sync at other hierarchy values. Other raw selector values
retain default branches; no guessed configuration names or bounds are imposed.
Nonfinite gains reject. This selector publishes no body mode, pose or velocity.

Original pinned executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
S4/native-ragdoll-blend-dispatch-oracle-01 executes1,024 prefix cases at
0088f3fe, stopping before publication at0088f484 or early0088f6dd/0088f6df.
Synthetic blend fields and the preceding scene lookup's supplied EAX are
explicit boundaries; there are no call stubs. Source/corpus SHA256:
`9fb905b8bd67716d59192eba85a4bb59bb53c95ca05150a04ca5c0442cbae7dc`,
`98b8ae6ed7a7da567a6b4136aecef20cac98ccb50af00c18d7efcac5675726d7`.
Expanded oracle02 covers15,680 cases:14 finite gain patterns including signed
zero/subnormals, adjacent1 and finite extremes; five flag words; eight raw
selectors including signed-boundary/max bit patterns; two x87 precision words.
Source/corpus:
`000d27998bd88c7ba7d6b27049beb5077c9f900c15b068093e2e2fc9c47a2134`,
`c2624f247b92312991819fbfdf36e74fd4663d3e018e01d5171b03ef21e5e4b7`.
Route interpretation is tied to actual branches: route0 enters the008a34c0
prepared-pose/velocity path; route1 calls0089ea70 for body transform to scene;
route2 uses the actual blend vtable+68 target0089eae0 for scene transform to
body. Actual synchronization/motion publication remain separate work.

S4/native-ragdoll-blend-dispatch-compare-{normal,sanitized}-01 each match all
15,680 outputs exactly. Always-driven, always-physical, clamped hierarchy,
ignored flag0x100 and ignored special-selector controls reject
3,892/13,188/5,840/672/1,960 cases. Comparator/driver SHA256:
`7e32e081a13bf71bef2e083ae127f25671e9ca0f9aed3f70a309b52a745968fa`,
`f52912ccdf1ee3fcb503e7bfc1b98324050772bb0e81bfe1d3cf9c34b992ceff`.
Five new tests cover zero/intermediate/endpoint routes, flags, exact adjacent
branches, raw selectors and nonfinite rejection. Dispatch-baseline01 retains
five failing tests against the absent rule. Four existing conditional test
macros now have explicit braces, removing their ambiguous-else warnings.

Full S4/native-ragdoll-blend-dispatch-{normal,sanitized}-01 each pass2,175
component and851 engine tests, complete inventories, zero failures/skips;
openmw/esmtool rebuilt. Stable tested fingerprint
`814fd7e234d7c8630eb0593e557d44bfebb88b8346093239ea28f612f3d13e55`.
ASan leaks disabled; UBSan halts; Python unchanged. Selector/controller wiring,
body motion switching, hierarchy pose blending, frame-time resolution,
cross-owner contacts, full World/render/save and stock floor continuation
remain open. No stage or normal-input/restart acceptance is closed here.


### Checkpoint158: native velocity-drive target pose interpolation

Checkpoint157 committed as `71f8c34e1e4232739ea82d6a82ca34c6213d2ade`,
154 isolated commits with exact-byte and fresh-clone proof. New
ragdollNativeBlendTargetPose prepares the target supplied to native8A34C0.
Position products and sums each store binary32. Rotation uses the original
shortest quaternion path, linear threshold0.9990000128746033, spherical
weights, and two normalization/refinement sequences:8B1C60 normalizes once,
then88F5DF calls4D6830 again. Finite hierarchy gain remains unclamped;
negative/extrapolated gains and signed zero retain their native branches.
Near-unit finite rotations and finite positions are required. Nonrepresentable
results reject before publication. Spherical arguments outside +/-2^63 are
explicitly unsupported/rejected: original FSIN would leave its operand and
set C2, unlike portable libm argument reduction. Quaternion reciprocal-square
root/CRT arithmetic is portable, not a claim about every original CPU.

S4/native-ragdoll-blend-target-oracle-01 executes1,800 cases against original
SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Actual88F5A4..88F5EA prefix executes full8B1C60, actual4D6830 twice and
actual986130 CRT acos on spherical branches; no game/OS call stubs. Live
caller SSE/stack inputs are explicit synthetic boundaries. Corpus covers100
quaternion pairs (equivalent signs, quarter/pi turns, adjacent linear
thresholds and seeded unit pairs),100 position pairs, nine gains including
signed zero, adjacent1 and extrapolation, and two x87 precision words.
Oracle source/corpus SHA256:
`db52c73732abd7a895933f545311142446913406ad40efd50bac01e36ff7fa66`,
`3053a5a1b89ff8c4800ffced8e36b2196ff49c9c964a2d8c7cb5dd6e23e6874b`.

Actual C++ S4/native-ragdoll-blend-target-compare-{normal,sanitized}-01 each
match all1,800 position and rotation outputs exactly, maximum quaternion
absolute error0. Positions require exact bits; quaternion tolerance was
predeclared2e-5 absolute plus2e-5 relative for portable CRT/RSQRT. Always
physical/always animated/clamped-gain controls reject1,382/1,618/792 cases.
Comparator/driver SHA256:
`52ae505120f5a6723f16dfe27736bf914e2128b9af23ef23b6866ac65c560403`,
`2891c517ddc261bec56e61553142e11edd326ca3daee061efc5d56a62325e34d`.
Four new tests cover spherical interpolation, extrapolation and signs,
linear threshold/signed-zero stores, invalid input and overflow. Baseline01
retains four failures against the missing interpolation.

Full S4/native-ragdoll-blend-target-{normal,sanitized}-01 each pass2,179
component and851 engine tests, complete inventories, zero failures/skips;
openmw/esmtool rebuilt. Stable tested fingerprint
`14da7d025af047df41f2bc3c5c8197fb085414b91cfb7f1aef8da38405bce2f0`.
ASan leaks disabled; UBSan halts. Python unchanged.

This helper supplies the velocity-drive target only. Native flag0x100 scene
position selection and subsequent scene writers are separate, uncompleted
semantics; mixed target position must not be called the final rendered pose.
Controller attachment, effective gains/frame clock, body mode switching,
animated renderer blending, actual World/render/save integration, cross-owner
contacts and stock floor continuation remain open. S4 remains in progress;
no normal-input/restart gate is closed by these arithmetic tests.


### Checkpoint159: native physical frame preparation

Checkpoint158 committed as `467db9b7a4beef761eaa0fa6dd54f809853fd655`,
155 isolated commits with exact-byte/fresh-clone proof. New physicalframe
prepares the native frame, substep duration/count and persistent remainder/
smoothing state as distinct fields. Raw mode0 uses fixed substeps and the
stored-remainder subtraction;10 smooths; all other raw modes divide the
prepared frame. The supplied boolean limits count to2 rather than3. Short
frames accumulate rather than pretending a body step occurred. Accumulated
binary32 overflow is still capped. Clock changes stage atomically; malformed
input, unrepresentable output and unsupported maximum/substep ratios>=2^32
reject. Settings expose independently read image defaults, including frame
cap166.6666717529297, threshold.008333333767950535, fixed step.01666666753590107
and smoothing.05000000074505806. These are not a winning configuration audit,
caller mapping or complete simulation authority.

Original SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
S4/native-ragdoll-frame-preparation-oracle-01 executes2,240 full889810 returns
through889A20, no call stubs. Finite nonnegative delta/remainder, prior
smoothing and raw mode/boolean are explicit fixtures. Four raw image settings
are restored before every call. Source/corpus SHA256:
`624293410d75554a21d8c1f052c0665ce95cfa7fc2e994c6488ccf387ce2bc14`,
`f294360c85aae3c36149c01f78e5288a44b878c08b110b39571646c31f179ca9`.
Oracle02 expands to18,240 with100 seeded durations; source/corpus:
`cbcd1d24b25ced412e41c953a37938630ce1b982f79ea400252a11b3b9846a12`,
`35b84e0c95b80de5953f867c3601aee8b739de29356b35d35fce95e57c10df30`.
Oracle03 adds maximum finite delta/remainder and signed-zero remainder:
25,760 cases, both x87 precision words. Source/corpus:
`8271bc3453593ba809c2e61c5553c419c153f7befeec34de385b4ae0c494e1c8`,
`19bbee332b361bdf771668dcf2ee80a0a710769cc65e09322926fad130e9ab35`.
No executable bytes or disassembly are committed.

C++ compare-{normal,sanitized}-01 each match18,240; expanded02 each match25,760
at the additional-test fingerprint. Final S4/native-ragdoll-frame-preparation-
compare-{normal,sanitized}-03 each match all25,760 cases, six output fields
exact (original input delta, prepared frame, step duration/count, remainder,
smoothing). Always-one-step/input-frame/discard-remainder/force-divided-mode
controls reject18,632/18,776/6,528/12,596. Comparator/final-driver SHA256:
`dcb9f23790770ae4ae31965a36c5a4ab66b420dcacdef9870f5db925bacbb247`,
`9363092320389b32eb941f8e1c13bc8750e808ea7acc9c5bce5c26873b44f4bd`.

Baseline01 retains four failed tests against missing preparation. First full
normal/sanitized01 pass2,183 component and851 engine tests. Expanded02 retains
one failed additional overflow test in each build: its supposed overflowing
smoothing product was actually finite. Fix uses delta2 rather than1 to
produce overflow; production arithmetic was unchanged. Five frame tests now
cover modes/float stores, accumulated and zero frames, exact thresholds/count
limits/cap/raw modes, malformed state/settings/duration with no mutation,
accumulation overflow and atomic smoothing-result rejection. One additional
target-pose test covers unsupported FSIN arguments and normalization overflow.

Final S4/native-ragdoll-frame-preparation-{normal,sanitized}-03 each pass2,185
component and851 engine tests, complete inventories, zero failures/skips;
openmw/esmtool rebuilt. Stable tested fingerprint
`c9d126f4db7cf736ab521c95dbd946e0900ebd945f3bde3cbf605f25c4a18214`.
ASan leaks disabled; UBSan halts. Python unchanged.

Actual frame-preparation caller/winning settings, scheduler clock ownership
and persistence, controller attachment/effective drive parameters, physical
motion switching, animated renderer blending, full World/render/save and
stock floor continuation remain open. This arithmetic/clock helper advances
no bodies and closes no normal-input/restart acceptance gate. S4 remains
in progress. Next: independently verified inverse-frame/effective gain
selection, then actual controller and physical/render ownership wiring.


### Checkpoint160: native driven-route inverse time and effective velocity gain

Checkpoint159 committed as `5d83c9293cb427f44bc0041ee0a76eeb32b8ff12`,
156 isolated commits with exact-byte/fresh-clone proof. New
resolvePhysicalBlendDriveParameters consumes an already prepared frame on an
already selected PoseAndVelocity route. Either signed zero uses inverse1;
nonzero frames divide then store binary32. Flag0x100 overrides velocity gain
to1. Other finite gains remain unclamped, including signed zero/extrapolation.
Nonfinite/negative frame, nonfinite gain and nonrepresentable positive inverse
reject before publication. This selects no route or controller and does not
prepare a frame, write a scene pose or publish body velocities.

Original SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
S4/native-ragdoll-drive-parameters-oracle-01 executes1,200 cases: original
88F3FE..88F484 selects the flag byte; original88F656..88F687 stores inverse
and effective gain. Explicit jump over scene/motion publication separates
the prefixes; no game/OS call stubs. Synthetic prepared frame and finite blend
fields use hierarchy.5/raw selector0, which actually selects driven route.
Source/corpus SHA256:
`6c86603edf777a50df380a84411929b41b701bbc34e74b275cb4bd0157ab256d`,
`5d5425ee263fbe3310ac064c5ed4f55f12bb8d763de467aea4d6d2b20b3d4a69`.
Expanded oracle02 covers11,600 cases: adjacent inverse-overflow boundaries,
100 seeded positive finite raw frame patterns, ten finite gains, five flag
words and two x87 precision words. Native nonrepresentable inverses remain
in the corpus as explicitly unsupported publication results. Source/corpus:
`374e6e9e176fba9bf7d0013968c09c37d8faacf0aeb8d211eab69c431832244e`,
`5d67dde69a8b335b0b5c238d2e9f44bf2ef27174b48e00e7a73493a9c3a36850`.

Actual C++ S4/native-ragdoll-drive-parameters-compare-{normal,sanitized}-01
each match11,300 representable outputs exactly and reject all300 unrepresentable
inverse inputs. Always-inverse1/ignored-flag/clamped-gain/zero-inverse-zero
controls reject11,000/4,068/4,068/200 cases. Comparator/driver SHA256:
`d26296ea6e7ba56f02927c11319bf487c0b77a1fc84390517ef4b616f2d17e39`,
`e44756c680550194653aababbe3df54dd72a859aa5beb79fd0410585f347415a`.
Three new tests cover the frame/zero branches, only-flag100 override and
unclamped/signed-zero gains, invalid inputs and finite-versus-overflow inverse.
Baseline01 retains three failed tests against the missing rule.

Full S4/native-ragdoll-drive-parameters-{normal,sanitized}-01 each pass2,188
component and851 engine tests, complete inventories, zero failures/skips;
openmw/esmtool rebuilt. Stable tested fingerprint
`066664d4c10d53c7e01dcda754b570377b2e9ea6741744a7a9402a593f10e973`.
ASan leaks disabled; UBSan halts. Python unchanged. Actual controller/frame
caller and winning settings, scheduler clock/save ownership, body motion
switching, animated renderer blending, full World/render/save, cross-owner
contacts and stock floor continuation remain open. No normal-input/restart
acceptance gate is closed. Next: animate/capture targets while physical poses
own the renderer, without a second scene traversal or overwritten skinning.


### Checkpoint161: exact renderer local checkpoints for animation sampling

Checkpoint160 committed as `d358e3abf69c015217731b4441bf1e10fff355f0`,
157 isolated commits with exact-byte/fresh-clone proof. ActorRagdollPoseBinding
now captures an immutable, ephemeral local checkpoint of the physical targets
and their unsimulated ancestors. It retains each actual OSG double matrix,
separate NIF rotation cache and scale without decomposing/recomposing them.
A private shared authority token ties the checkpoint to this exact binding;
foreign/default/recreated owners reject, including after the old binding dies.
Every selected ancestor record, body name, asset and parent path is checked
before the first restore write. Restoring a validated checkpoint can recover
invalid live numeric fields; it does not require those fields to be valid
before rollback. Same-traversal skinning caches are invalidated afterward.
This is renderer sampling/rollback state, not a serialized physical save.

Four baseline tests fail against missing local capture/restore. Initial full
S4/native-ragdoll-local-checkpoint-{normal,sanitized}-01 each pass2,192
component and851 engine tests. Added cache-baseline01 retains one failure:
a valid rendered matrix previously hid an invalid NIF rotation cache. Capture
now validates both representations; their legitimate differing procedural
rotations remain distinct. Five tests cover exact double matrix/NIF-cache/
scale restoration (including signed-zero matrix bytes), empty/foreign/recreated
bindings, changed ancestor/body/asset/parent identities with atomic rejection,
recovery of nonfinite/zero-scale live fields and skinning cache refresh, and
invalid cached rotation with a valid visible matrix.

S4/native-ragdoll-stock-local-checkpoint-{normal,sanitized}-01 each pass.
Real stock18-body/17-joint NIF, actual73-track onehandidle KF, real Animation,
OSG update traversal, Bullet factory/240 zero-gravity steps. Independent
parent-chain enumeration identifies24 physical/ancestor local transforms.
All24 actual double matrices, nine NIF rotation fields and scale are captured
as expectations, overwritten, then restored240 times with exact byte
comparisons:5,760 node restorations per build. Existing physical world/bone/
skinning agreement tolerance remains.001, observed maximum3.8147e-05 in both
builds. Freeze/resume and graph cleanup checks remain active. Source/driver
SHA256:
`d2160a6d3e0a4190859340d0bed5c77a89ed61470dfc7c9312f7e11e9b935253`,
`7c4a78901ac525d811c3a5ae73d5d5b985db653596ebf6f46c68607c4222dc2a`.
Stock NIF/KF SHA256:
`43de349062d2f57b1e581353f1907f8f70bcd3c92b4fe3bd049989b93572d435`,
`d01bf09a3c703ae2f0f4c043abbe47dc1c0e6d3af0fedcf41ed9a50173bc17d5`.
This is actual headless engine/resource/renderer/physics integration; it
checks local checkpoint preservation, not pixels, native trajectories,
animated physical blending or game-world acceptance.

Final S4/native-ragdoll-local-checkpoint-{normal,sanitized}-02 each pass2,193
component and851 engine tests, complete inventories, zero failures/skips;
openmw/esmtool rebuilt. Stable tested fingerprint
`35f127d4634782bca0f3932bb23bce91f5599b7d0ffc1ace4d2985f8fac5e5ad`.
ASan leaks disabled; UBSan halts. Python unchanged. Animation currently still
freezes during physical ownership; actual target sampling/advancement is the
next bounded integration. Controller/frame caller, clock/save ownership,
body motion switching, full World/render/save, cross-owner contact activation
and stock floor continuation remain open. No stage or normal-input/restart
gate is closed by the checkpoint API.


### Checkpoint162: animated targets while physical bones own the renderer

Checkpoint161 committed as `54f6ef6c2bbe84e7d11dc0de01ae30e2606c0560`,
158 isolated commits with exact-byte/fresh-clone proof. Animation now has an
explicit AnimatedTargets physical mode in addition to the default Frozen mode.
The animated mode advances its existing animation/text-key/effect clock once
through runAnimation. Its explicit target sampler executes detached transform
callbacks in ordinary depth-first node order, preserving registration order on
each node. It copies the update frame stamp, traversal number/masks and node
paths into a traversal-disabled update visitor; it does not traverse scene
children, geometry, effects or bounds a second time. It restores the previous
animated local checkpoint before sampling so missing KF channels cannot inherit
physical transforms, captures new world targets and the latest animated locals,
then restores the exact physical locals and invalidates skeleton matrices.
Invalid frame/placement or callback results reject; callback exceptions restore
the physical targets and their selected ancestors. This does not promise rollback
of unrelated scene nodes or arbitrary callback-owned state. Topology remains
stable during sampling. Ending animated physical ownership restores the latest
animated checkpoint before ordinary callbacks are reattached.

Controller group rebuilds temporarily expose the saved animated locals to
transition-controller construction, keep callbacks detached, and restore physical
locals on success or exception. Embedded resident keyframes are retained ahead
of external tracks: even a keyframe without a source resets the NIF rotation
cache, so leaving it attached can overwrite a procedural ancestor rotation during
ordinary traversal. Discovery includes hidden/masked nodes; actual sampling
still honors update traversal masks. Both frozen and animated physical modes
detach resident keyframes and restore them on exit. Object replacement/removal
clears these ephemeral physical sampling fields. None is a new save authority.

Six new baseline tests fail against frozen target sampling in
S4/native-ragdoll-animated-target-baseline-03. Baseline01 retains five earlier
failures; baseline02 retains a test-fixture compile error from attempting to
modify OSG's const frame stamp, corrected by owning a mutable fixture stamp.
Nine new regression cases cover target capture without renderer/skinning
replacement, missing channels, invalid/throwing callbacks and no child traversal,
parent-before-child ordering despite registration order, finite update frames and
mode/placement rejection, animated handoff, mask/override behavior, controller
rebuild failure, and resident ancestor keyframes in both physical modes.
S4/native-ragdoll-resident-mask-baseline-01 retains the additional failing
hidden-at-entry ownership case before discovery's mask override was implemented.

Stock S4/native-ragdoll-stock-animated-target-normal-01 through -04 retain
actual renderer failures after sampling: immediate physical writeback agreed,
but later ordinary traversal reset an ancestor's cached rotation. Diagnostic
runs02-04 reached maximum error7.37653 with nonzero procedural rotations.
The original .001 renderer tolerance was not changed. After resident-keyframe
ownership was fixed, normal/sanitized runs05 and06 pass; run06 includes the
whitespace-clean source. Final normal/sanitized runs07 each pass against the
final hidden-node fix. Two independently loaded stock18-body/17-joint skeletons
and Animation instances use the real73-track onehandidle KF. One is sampled
under physical ownership; the other uses ordinary scene traversal. Nonzero
head/upper-body/legs/body rotations and a midpoint controller rebuild exercise
ordering and transition construction. All69,120 world-matrix fields match
exactly over240 animation frames in each build; clocks advance identically.
A further ordinary frame after physical handoff also matches. Real Bullet
zero-gravity steps continue feeding physical renderer/skinning transforms,
maximum error3.8147e-05 in both builds against .001.
Cleanup leaves zero owned bodies/constraints. Source/driver SHA256:
`6439195ce1b3e7601a03817b1f5ec85359a169bf5aacdd0c19642434e6036266`,
`ddb613a30c168dab0fb74f50ab64c56bda0e2b1de3352c7d7e606f5d3630b60c`.
Stock NIF/KF SHA256 remain:
`43de349062d2f57b1e581353f1907f8f70bcd3c92b4fe3bd049989b93572d435`,
`d01bf09a3c703ae2f0f4c043abbe47dc1c0e6d3af0fedcf41ed9a50173bc17d5`.
This is headless engine/resource/animation/renderer/physics integration, not
an original-game trajectory comparison, pixels, gameplay or restart acceptance.

Final S4/native-ragdoll-animated-target-{normal,sanitized}-05 each pass860
engine tests, complete inventories, zero failures/skips; openmw/esmtool rebuilt.
Stable tested source fingerprint `4f8a43329351d5b0c899d88f5ed9a9bf9f7c9ab760e67bd7de34ac5a4233130b`.
ASan leaks disabled; UBSan halts. Components/Python implementation is unchanged;
checkpoint161's2,193 component passes retain their separate tested fingerprint.
No stage closes. Native frame/settings ownership and physical controller caller,
body motion switches, renderer blend-route publication, actor scaling, full
World/save lifecycle, cross-owner contact activation and stock floor continuation
remain open. Next: implement and independently verify owned dynamic/keyframed
motion handoff, then connect the complete physical controller update path.


### Checkpoint163: owned dynamic/keyframed body handoff

Checkpoint162 committed as `6e07e2b021da35bbf116f6eaf237b9c075f20827`,
159 isolated commits with exact-byte/fresh-clone proof. ActorRagdollPhysics now
accepts sparse body-record requests for Dynamic1 or Keyframed6 and reports those
requested controller modes. It stages and validates the entire request batch
before changing any body. Unknown records, duplicates and unknown modes reject
without partial mass/activation changes. Keyframed bodies retain their current
pose/velocities/interpolation/forces and become Bullet kinematic objects with
zero inverse mass/inertia. Returning to Dynamic restores the original loaded
mass/principal inertia, refreshes world inertia for the current pose and wakes
constraint-connected owned bones. The same body/shape/constraint objects and
collision/filter identities remain owned. Same-mode and empty requests do not
wake untouched sleeping bodies. Keyframed bodies are excluded from dynamic
native damping/velocity publication; a dynamic pose drive for them rejects.

This boundary does not synchronize an animation pose or reset transition
velocities. Original blend update88F484 separately synchronizes old mode6 or
zeros linear/angular velocity for another old mode before requesting a change.
That controller sequencing remains open. Mode requests are ephemeral at this
step and are not included in RuntimeActorRagdoll snapshots. Full save authority,
world/island/contact-mode publication, frontend worker ownership and the actual
controller caller remain required before gameplay/restart acceptance.

Original executable SHA256 remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
S4/native-ragdoll-motion-archive-oracle-02/-03/-04 each cover200 cases under both
x87 precision words. Actual8CBC60 executes dynamic type2 -> keyframed6 -> requested
Dynamic1, restoring the archived type2 rather than inventing a new type1. Actual
keyframed constructor, type getters,89DF00/89DFB0 motion-state/velocity copies and
destructor execute. Only allocation/free primitives supply a synthetic heap;
no world is borrowed. All44 motion-state words and eight velocity words are
copied, inverse mass/inertia properties stay archived, and the archive is
consumed on return. The actual8CD4E0 dispatcher proves same-mode early return.
Oracle01 retains a harness failure: invoking the internal8CBC60 helper directly
for a no-op bypassed that caller gate. Oracle03 makes updated quaternions valid
for the portable pose bridge;04 additionally includes signed-zero, subnormal,
minimum-normal and maximum-finite velocity fields. Final source/corpus SHA256:
`965f4ef1cc38f9f22da0aca38f097dbc41a6f9d6cf3322fe6606797dae0ba103`,
`908c737ae1f433ff423ffba97b5ce6aaa0169b7da794a0b45ae3ca54d8492e72`.
This does not prove contact/constraint-island publication, scene synchronization,
zero-mass wrapper eligibility, physical stepping or a World/save lifecycle.

Six new component tests cover current-pose/velocity/force/interpolation and mass/
inertia conservation, actual kinematic non-response to impulse and dynamic
response after return, atomic invalid batches and untouched sleeping bodies,
128 repeated switches with stable graph identity and connected activation,
restored anisotropic/center-frame off-center impulse response, and exclusion
from dynamic integration/drives. S4/native-ragdoll-motion-mode-baseline-01
retains five failures;baseline02 retains all six against the dynamic-only stub.

Actual C++ S4/native-ragdoll-motion-mode-compare-{normal,sanitized}-01 each
pass the200-case oracle03 corpus. Final compare-{normal,sanitized}-02 each pass
oracle04:3,600 native position/linear/angular velocity fields match exactly,
plus1,200 current-pose/interpolation, restored Bullet inverse-mass/principal-
inertia, requested-mode and no-op checks. Each case removes its owned graph;
zero bodies/constraints remain. Wrong old-velocity/zero-velocity/always-dynamic
controls differ in200 cases each. Restored loaded physical properties represent
native archived type2 behavior here; Bullet does not expose a Havok type tag.
Comparator/driver SHA256:
`53955d90ea72f5cff8bd6f0a58cdf3385d0f536be16b6bead06d8a8df08e4abd`,
`d34f2482de174b3fc68ec7149bf50fa633ea422854f5c81e67f9fe0dda558882`.

Full S4/native-ragdoll-motion-mode-{normal,sanitized}-01 each pass2,199 component
and860 engine tests, complete inventories, zero failures/skips; openmw/esmtool
rebuilt. Stable tested source fingerprint `7c9d500c6f5648a2d589820fa74b21e3f3cd3eac8a3fc507f77547dd57bb9447`.
ASan leaks disabled; UBSan halts. Python unchanged. No stage closes. Next: connect
mode publication/query through the worker barrier and owned-record scheduler,
then complete the actual blend-route controller update and its persistence.
Native frame/settings ownership, renderer blend-route publication, actor scaling,
full World/save lifecycle, cross-owner contact activation and stock floor
continuation remain open.


### Checkpoint164: worker-owned dynamic/keyframed publication

Checkpoint163 committed as `c4db091f31179501fbe5b18e2109a57a6ea4e336`,
160 isolated commits with exact-byte/fresh-clone proof. PhysicsSystem now exposes
owned sparse native motion-mode publication/query through PhysicsTaskScheduler.
Both scheduler operations wait for outstanding workers and take the collision
world lock before resolving the actor owner and accessing its owned graph.
Stale, null and removed owners reject even an empty setter request. Body-record
and mode validation remains at ActorRagdollPhysics; no new simulation authority
or serialization representation is introduced.

Three new parameterized test bodies provide nine cases across0/1/2 workers:
queued dynamic stepping completes before the keyframed handoff; the next
substep holds that pose; returning to Dynamic resumes falling. Dynamic-only
pose drives reject keyed bodies. Invalid records/modes and stale owners reject
without publishing pose/mode changes. Unselected sleeping bodies, owner pointers,
collision filters, per-body gravity flags and global world gravity survive.
Every graph teardown leaves zero collision objects and constraints. The existing
public PhysicsSystem capsule/snapshot/removal integration test now checks stale
owner rejection, requested modes and exact pose conservation while the capsule
remains suspended. It then exercises the existing impulse/drive/restore/removal
path after returning to Dynamic, in all three worker configurations.

S4/native-ragdoll-motion-scheduler-baseline-01 retains a build failure caused by
a missing request-type forward declaration; baseline02 builds and retains all10
filtered behavioral failures with an inactive scheduler setter. The corrected
setter passes full S4/native-ragdoll-motion-scheduler-{normal,sanitized}-01:
869 engine cases each, exact complete inventories, zero failures/skips;
openmw/openmw-tests/esmtool built in both configurations. Source fingerprint
`cb10ee3512c463e3bf1678379354507a36f78d8b44ac571e5d94b26ba1beea2c`. ASan leak detection disabled; UBSan halts on errors. The component
implementation is unchanged from checkpoint163's2,199 passing component cases
in each build at that checkpoint's separate source fingerprint. Python unchanged.

This closes only the worker/frontend motion-mode boundary. Native blend route
sequencing, animated scene-to-physics synchronization, root blend selection,
mode/clock persistence, actor scaling, actual World/gameplay acceptance,
cross-owner contact activation and the retained stock-floor continuation failure
remain open. No M15 stage closes. Next: independently verify native root blend
selection, then connect the actual blend controller and its save authority.


### Checkpoint165: explicit native root blend selection

Checkpoint164 committed as `65800037d4b94a410a3336c142fcb3a1c499f39d`,
161 isolated commits with exact-byte/fresh-clone proof. The loader now provides
loadActorRagdollRootBlend(FileView, optional explicit scene-root record). Its
owned result retains source hash, selected target-node/body/collision identities,
flags and independent hierarchy/velocity gains. No root assumes file-root order
or body-record order. Absence of the supplied root returns no blend. Root blend,
first-child blend and the selected branch blend have successive priority.
The selected branch is first child's slot0 if its nonnull count is1, otherwise
slot1, bounded by slot extent. The query then scans only that branch's direct
children in slot order. It never recursively searches deeper or visits an
unselected branch. Zero/negative finite gains do not affect selection.

Original full88F200/497420/47FAC0/88EA80 execute with actual NiNode vtable
A7E38C+8 ->7616D0 (return this) and NiAVObject A7DF24+8 ->6F7070 (return null),
identified via actual NiNode/NiAVObject constructors and RTTI getter. Oracle01
covers4,097 synthetic raw-word cases. Final oracle02 covers7,169 cases, including
seven child-slot layouts with holes, both selected object types and512 blend
placement masks plus null root. B6/B8 are independently distinguished by the
full original4B34E0 NiTArray SetAt in96 cases: slot extent versus nonnull count.
Only imported Windows InterlockedIncrement/Decrement use equivalent synthetic
reference-count operations (76 calls); allocation/attachment and original NIF
loading are outside that array probe. Lookup itself has no stubbed calls.
Original executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Final lookup source/corpus SHA256 `8e44080a46a2fd5f4cdb978a1612f24bf701a91ae6969827298b12d87b0a9e9b`,
`32f10e3d7d1b2329074d16bd405aed69eb8ab2561385668e1d4525cf0e41c02d`; array source/corpus
`b48bc247543e036dad5fa8bfac8ccb81f9db95112a9f4ef19dcf7273212abf31`, `a9c0ba0456decb3892b517d75c178c932d5d6dca18f347c80080f8aac2d446c0`.

Six new component tests cover selection priority; holes without compaction;
absence of recursive/unselected fallback; copied metadata and unclamped finite
gains; explicit root selection and missing blend; malformed paths, foreign
identity, wrong target/body, nonfinite gain and unsupported format. Portable
admission safely diagnoses missing/null first children and null scanned entries
where native code would dereference them. Existing full graph validation still
owns cycle/shape/joint admission; the lookup alone does not validate a whole
ragdoll. Baseline01 retains six failures against the empty query. Full normal01
crashes and sanitized01 aborts on test fixtures that assigned unresolved default
NIF references instead of resolved null pointers; the diagnostic log identifies
RecordPtr::empty's pointer-state assertion. Those fixtures now use typed null
pointers, and null scanned entries explicitly reject before getPtr.

Actual C++ compare-{normal,sanitized}-01 each matched all7,168 nonnull-root
oracle cases before the fixture/null-scan correction. Final compare-{normal,
sanitized}-02 each repeat that complete comparison against the final source:
exact original selected node plus controlled owned body/collision/flags/gain/hash
fields. Generic DFS and always-slot0 negative controls differ in392 and222
cases. Comparator/driver SHA256 `f037f2c711d6a014b8eb39696192b715000f98482e3856837abd9b467e864d43`,
`9c5e49f29b08916c13c4410af77f69f61c08f549b0f5f60f4defc22f720c6d97`. Final source fingerprint `88cf51e59303e29bfded48653a9d9ba0f237cd3f4fb99725136d4204b3bc0a57`.

Pinned stock skeleton SHA256 `43de349062d2f57b1e581353f1907f8f70bcd3c92b4fe3bd049989b93572d435` parses/adopts its18-body/
17-joint graph in stock-root-{normal,sanitized}-01. Explicit file root0 selects
node8/body12/blend13 in both builds. Stock-root-oracle01 executes the original
lookup with synthetic original-layout objects populated from that exported
production-parser hierarchy and returns the same collision. Selected target/
body/collision identities also match the independent raw blend-payload audit.
Stock oracle source/layout SHA256 `97ac086de42fa9d173dba470eebf1e8afac4d3650551946b3dda696a1a29bceb`,
`e5dbb19d4c72b09216d56d018e0b3af49adfd800169741c9d28f0f6cad524e07`. The hierarchy remains a declared production-parser
boundary input; this is not an independent original NIF-loader audit. Original
runtime scene wrapping and the gameplay actor's chosen object root remain open.

Full root-lookup-{normal,sanitized}-02 each pass2,205 component and869 engine
cases, exact complete inventories, zero failures/skips; openmw/esmtool rebuilt.
ASan leak detection disabled; UBSan halts. Python unchanged. No M15 stage closes.
Actual animation/root ownership, blend route/mode-transition sequencing,
scene-to-physics publication, mode/clock persistence, actor scaling, World/
contact/restart acceptance and retained stock-floor continuation remain open.


### Checkpoint166: native scene quaternion preparation in the existing bone adapter

Checkpoint165 committed as `ed49df1a124aabd2a0012bc47ff4385d8511f890`,
162 isolated commits with exact-byte/fresh-clone proof. The existing
ragdollNativePoseFromBoneWorld adapter now obtains its position/rotation from
ragdollNativeSceneTargetPose before constructing the Bullet transform. The new
query exposes the prepared native-unit origin and XYZW float quaternion without
losing its representation through a Bullet matrix roundtrip. It retains the
existing rigid/affine/finite matrix admission and signed-zero position conversion.
OSG row-vector matrices are transposed to native NiMatrix3 convention. Rotation
preparation follows the actual native positive-trace/largest-diagonal branches,
strict diagonal comparisons/cyclic indices, float trace/root/reciprocal stores,
unrounded double difference/product intermediates, and one float quaternion
normalization reduction/Newton pass. The old OSG quaternion extractor and extra
double normalization are no longer used by the bone/body pose adapter.

Original executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Full original89EAE0 keyframed scene synchronization completes400 synthetic cases
under flags1/40 and both x87 precision words. Actual motion type getter, NiWorld
transform getter,7150F0 matrix conversion,4529E0 separate position scale,
4D6830 quaternion normalization,8A2FB0 wrapper setter,8A9E20/89DB30/8DD970 motion
transform stores,8B1DD0 quaternion-to-matrix and8A9D10 no-world return execute.
Only Windows EnterCriticalSection/LeaveCriticalSection/GetCurrentThreadId are
boundary stubs; no game-call arithmetic/pose setter is substituted. Source/
corpus SHA256 `2b3ffeca6e1fd1de0b4b62d01bbd0194cfd19b61d055aac6ff0e138d75596afa`, `9a96456ff35e6326775942f27a53d5658ae8bd4b465eb35f3cf53472a84d79a9`.
Native current/previous quaternion and COM stores agree and all eight velocity
words and the local COM survive. World/contact/AABB publication is outside this
no-world probe. The separate original scene scale field is not consumed by this
rotation/position branch; this does not establish shape/inertia scaling or admit
scaled OSG matrices. The cyclic index tableB27120 is the original initialized
(1,2,0) PE data, and position adapterA39088 uses its actual separately stored
0.1428767293691635 double, not an inferred reciprocal of the reverse constant.

Three new component tests retain ten native input/output examples for exact
quaternion branch signs/stores, ensure the existing bone adapter constructs the
same transform from prepared target values, and preserve rigid admission/signed
zero. Baseline01 retains two failures against the old adapter projection; the
third admission case already passes. The range-loop copy warning is corrected.
Actual C++ scene-target-compare-{normal,sanitized}-01 each match all400 original
cases:2,800 exact native origin/normalized-quaternion fields and400 existing-
adapter consistency checks. The old OSG quaternion/double-normalization negative
control differs in305 cases in each build. Comparator/driver SHA256
`361255b06cc3db47dccc8345361805fb4bd355aa80c9a8105c26e83a3c6a248a`, `cd49b1705013550d84d38416638650fad916a4fdd98e512ca2eed01304745fcd`.
Normalization uses portable reciprocal square root; instruction emulation is not
an original-hardware trajectory/RSQRT-approximation gameplay probe. Setter COM/
previous-state/velocity observations are not compared by this arithmetic bridge.

Full scene-target-{normal,sanitized}-01 each pass2,208 component and869 engine
cases, exact complete inventories, zero failures/skips; openmw/esmtool rebuilt.
Stable tested fingerprint `282017e1c4f342348a6ee58e6d0f784dd0dad43bd95f52b4c2058c86f37dd8f8`. ASan leaks disabled; UBSan halts. Python
unchanged. Actual pinned stock18-body/17-joint NIF/KF animated-target regression
scene-target-stock-animated-{normal,sanitized}-01 each pass240 frames,69,120 exact
animated matrix fields, controller rebuild and handoff, zero graph teardown
objects/constraints. Maximum physical renderer/bone error
4.57764e-05 normal/4.57764e-05 sanitized,
within unchanged.001 tolerance. This is a real asset Animation/Bullet projection
check, not a World/gameplay/save continuation.

No M15 stage closes. Next: publish validated scene targets to owned keyframed
bodies at the worker boundary, then complete actual blend-controller sequencing
and mode/clock persistence. Gameplay scene-root choice, actor scaling, contact
activation, full World/restart acceptance and the stock-floor continuation
failure remain open.


### Checkpoint167: owned keyframed scene pose publication

Checkpoint166 committed as `7fc3af23ee6c3519c395ccce3680c92d46133410`,
163 isolated commits with exact-byte/fresh-clone proof. Owned ragdoll physics
now accepts sparse native scene-pose requests identified by body record. It
stages and validates the complete batch, rejecting duplicate/unknown records,
non-keyframed bodies and invalid matrices before writes. Native target preparation
from checkpoint166 precedes one caller-length conversion and the existing
principal-center frame conversion. Publication activates selected owned
constraint groups, updates current and interpolation world poses, and refreshes
AABBs. Body/shape/proxy/constraints, current velocity and accumulated forces
survive publication. Empty batches do not activate or move bodies.

Four new component cases cover exact prepared target pose, nonzero centers and
principal inertia, velocity/force and graph identity preservation, complete
invalid-batch rejection, empty/unselected sleeping bodies, collision queries at
the new pose, and switching back to dynamic at the published position.
All four fail against the retained inactive baseline01.
Full keyframed-scene-{normal,sanitized}-01 each pass2,212 component and869
engine cases with complete inventories, no failures/skips and rebuilt
openmw/esmtool. Tested fingerprint `e38e360fedb82b13ba67362fad72a956265227e12b724921cbec5844ab07511f`. ASan leaks disabled; UBSan halts;
Python unchanged.

Actual owned Bullet publication compares against checkpoint166's full original
89EAE0 corpus, executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`, corpus SHA256
`9a96456ff35e6326775942f27a53d5658ae8bd4b465eb35f3cf53472a84d79a9`. Keyframed-scene-compare-{normal,sanitized}-01 each
pass400 bodies,2,400 exact native velocity fields and2,400 interpolation/force/
identity/mode/teardown checks. Before either run, tolerances were declared as
native origin1e-9, rotation basis1e-6 and world COM.001, accounting for the
Bullet double/principal-center representation. Maximum errors in both builds:
origin `1.1368683772161603e-13`, basis
`2.2590223736074222e-07`, world COM
`0.0004245360994976225`. Unchanged old pose and zero-velocity
controls each differ in400 cases. Comparator/driver SHA256
`5d6731b1a2b3c683818b90b8c36edb3fbb324f2e7adf6cd7d21a25b262ae8bb0`, `f8f9f1859abe8ccd1db1757a39d34b668973a60a589102a61047b34e53e9b67a`.
The original probe executes no-world setter publication with Windows lock/thread
imports stubbed; it does not prove full original-world contact/AABB behavior.

Velocity preservation is proved at publication only: Bullet's later kinematic
saveKinematicState derives velocities from its interpolation poses. Native
keyframed substep semantics still need an independent instruction trace; no
claim of native velocity conservation across a physics step is made. Owned
constraint-group activation is reused, but cross-owner contact-island activation
is open. No scheduler/World caller, controller/mode/clock save restoration,
actor-root/shape scaling or normal-input gameplay acceptance is introduced.
No M15 stage closes; stock-floor fresh-restart discrepancies and S5-S14 remain
open. Next: route scene publication through the worker barrier and PhysicsSystem
before implementing actual blend-controller sequencing.


### Checkpoint168: worker-barrier/public physics scene publication

Checkpoint167 committed as `6414a43697cf1d1ad3b79582afe7618a2b7de270`,
164 isolated commits with exact-byte/fresh-clone proof. PhysicsTaskScheduler now
routes sparse owned keyframed scene targets after waitForWorkers and under the
collision-world exclusive lock. PhysicsSystem exposes the same owner-checked
operation. No body construction, capsule replacement or controller state is
added to this adapter.

Three parameterized scheduler cases run with0/1/2 workers: publication after
queued work and stable pose through the next keyframed step, updated ray queries,
shape/proxy/owner identities and teardown; malformed whole batches, empty/stale/
removed owners and successful owner rebind; empty publication, unselected sleep,
current velocity/force and world/filter preservation. The existing real-resource
PhysicsSystem capsule handoff test additionally verifies stale-owner rejection,
scene publication, capsule suspension, restore and rejection on dynamic bodies.
Baseline01 selects13 tests: all9 new cases fail and4 preexisting native-ragdoll
cases pass. Baseline02 selects the public PhysicsSystem case and fails it.
Both retained baselines use an inactive scheduler adapter.

Full keyframed-scheduler-{normal,sanitized}-01 each pass878 engine tests,
complete inventories, zero failures/skips and rebuilt openmw/esmtool. Stable
tested fingerprint `16b702512226441ad98f9c1902dc8a8974273f734e41374c0fe1124215618a92`. ASan leaks disabled; UBSan halts. Component
implementation is unchanged from checkpoint167's2,212-case passing runs; Python
unchanged. Current velocity conservation is asserted at publication only; the
next-step case proves pose stability, not native keyframed velocity evolution.
These are actual queued scheduler and public PhysicsSystem operations, not
World.init or normal-input gameplay/save/restart acceptance.

No stage closes. Actual controller dispatch, root choice, frame ownership,
motion/clock save state, actor scaling/contact islands and stock-floor restart
failures remain open. Next: native reverse scene target preparation against the
full original physics-to-scene route, then actual controller integration.


### Checkpoint169: native ordinary physics-to-scene target preparation

Checkpoint168 committed as `803a51f8ef70b60cb8861329297aa572e2f235d4`,
165 isolated commits with exact-byte/fresh-clone proof. The new
ragdollBoneWorldFromNativePose query converts native origin/XYZW quaternion to
an OSG world matrix for the ordinary89EA70 physics-to-scene route. It follows
actual47C600: separate float doubled XYZ stores, nine float product stores and
unrounded sums/differences before final NiMatrix3 float stores. It transposes
native/OSG conventions and uses the existing separately stored reverse length
constant. Finite inputs, unit quaternion within1e-4 and representable world
position are required; the quaternion is not renormalized. This is not the
88F5xx mixed-pose renderer branch, which uses a different8B1DD0 matrix routine.

Two new component tests retain ten original input/output examples and invalid
nonunit/zero/nonfinite/overflow cases. Both fail against the old OSG rotation
baseline01. Full physics-scene-{normal,sanitized}-01 each pass2,214 component
and878 engine cases with complete inventories, zero failures/skips, rebuilt
openmw/esmtool and stable tested fingerprint `071352ea3493ae55bb4fa94f89446bcc02b011c74c48bb56187fedabebb6ef1f`. ASan leak checks disabled;
UBSan halts; Python unchanged.

Executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`. Full original89EA70 route,
actual body/motion getters,4D6950 quaternion reorder,47C600 conversion,4D6900/
43F3E0 native length return and8978D0 scene writer complete1,600 cases each:
oracle01 parentless and oracle02 with rotated/translated unit-scale parents.
Flags0/8 and forced/threshold publication, both x87 precision words27F/37F;
no game-call or Windows import stubs in either route probe. Velocity and
separate local/world scale slots survive. Parent02 additionally executes
718A80 inverse and53D7A0 transform product. Source/corpus SHA256:
parentless `9915943faa9820bdaa3c8fd9386da2d14c7dfa736eda4223bd835a9c5bdc6f7e`, `1329b26ce8e85751e90f24ef32f95b72bb028293aa53f3dbdb35ad4f9e0a7274`;
parent `dc235189f88fbef23f2aaa1160895379566ac9848df50af87646b3b14a894fa0`, `f20b4b610d6c763a3073c8e20cb26f7ee2127ca8b03bbe0139d43ac552a8b9ac`.
Synthetic no-world data is not original NIF loading or gameplay acceptance.

Actual C++ target preparation plus existing bone writer in compare03 each
matches all1,600 parentless cases:41,600 exact local/world float fields under
both precision words. Initial compare01/02 in both builds retain a comparator
compile failure from attempting a range loop over osg::Vec3f; indexed access
corrects the harness. Parent compare04 in each build retains four one-bit local
rotation mismatches, all under37F; every world field and all80027F cases match.
This exposes53-bit versus64-bit x87 cancellation in the preexisting parent
composition helper; no tolerance has been relaxed and no arithmetic changed
to satisfy a noncanonical precision experiment.

Independent CRT precision oracle03 runs12 supplied-argument/control-word cases;
oracle04 executes the actual startup987769 XOR/INC creating EBX1, actual9877E0
PUSH/CALL and981C6B floating initializer prefix. Its original read-only PE-header
pointer gate accepts AA3E84 callback982897; actual99076F/99E24F/9A0C8C sets53-bit
precision while preserving rounding for three initial words. Only Windows
GetModuleHandleA returns no external CRT module, selecting the actual floating-
feature fallback. The IAT identity is independently parsed as KERNEL32.dll
GetModuleHandleA; the unused next boundary is GetProcAddress. Intervening
startup calls between the two caller slices are not emulated; EBX is callee-
saved by the x86 ABI. CRT01/02 retain a raw-word assertion failure: reserved
bit6 differs when the emulator loads a reconstructed word;03/04 compare the
precision/rounding/exception fields, excluding that reserved bit. Startup04
source/corpus SHA256 `7289a72d1c0ff16ce8be62f36fba08428d442e9aa5a35ba0f36841b8fee09da4`,
`53bd87ad1f366dfbc7d01b969c6a613b210a9846e29b76a7785ce38b28b551ee`. Neither WinMain/graphics nor live main/worker
control words are proved by these initialization slices.

Parent compare05 in both builds declares the original CRT startup53-bit,
round-to-nearest domain before execution and matches all800 selected cases:
20,800 exact local/world float fields each. The four37F differences remain in
compare04 as precision diagnostics; this is not an all-precision pass. Full
live precision/runtime confirmation remains open before stage acceptance.
Actual stock18-body/17-joint skeleton/idle KF regression in physics-scene-stock-
animated-{normal,sanitized}-01 uses the new native return adapter on actual
Bullet captured poses. Each passes240 frames,69,120 exact animated target fields,
controller rebuild/handoff, unchanged.001 physical renderer tolerance (maximum
4.57764e-05 normal/4.57764e-05 sanitized)
and zero graph teardown objects/constraints. It is not World/save acceptance.

No stage closes. Next: verify the full mixed-pose renderer/mode transition
sequence and connect the actual controller. Live precision, actual actor root/
shape scaling, cross-owner contact activation, native keyframed substep behavior,
mode/clock persistence, stock-floor restart and all remaining gameplay campaigns
stay open.


### Checkpoint170: raw animated target preparation for the mixed blend route

Checkpoint169 committed as `ce5eb2e548a87b9ded0c4252951f0ce33f418af8`,
166 isolated commits with exact-byte/fresh-clone proof. New
ragdollNativeBlendSceneTargetPose follows the separate animated-target boundary
inside88F3D0:539850 transposes/pads NiMatrix3, then8B1B40 extracts XYZW quaternion.
The trace, sqrt result and reciprocal remain unrounded53-bit intermediates
until final output floats; the nonpositive-trace branch subtracts the sum of
other diagonals before its root and uses strict largest-axis/cyclic selection.
It keeps the raw quaternion for later Slerp, without the keyframed path's extra
normalization. The existing rigid/finite/affine admission and native position
conversion are retained. Ordinary keyframed scene sync still uses its distinct
7150F0/4D6830 preparation unchanged.

Three component cases retain14 original matrix/target examples, independently
retain both original raw and original keyframed outputs for one shared matrix,
and cover invalid matrices plus finite maximum-world-position admission.
Baseline01 fails the first two cases against reuse of normalized keyframed
preparation; admission already passes. Full blend-scene-{normal,sanitized}-01
each pass2,217 component cases, complete inventories, zero failures/skips and
stable fingerprint `0f251690e6d888cac89d8321abca98eecb1b4102964f1d50b9a531a71e830ca7`. ASan leak checks disabled; UBSan halts. No engine
caller consumes this new pure query yet; engine/Python were not rerun for this
query-only chunk. Checkpoint169's full engine878-case results are previous-
revision evidence, not tests of this fingerprint.

Executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`. Full original88F3D0 executes19,200
synthetic no-world sphere-motion2 mixed-route updates. Requested dynamic mode1
is unchanged, so this does not prove mode-transition initialization. Animated
matrices/positions vary across400 source inputs; hierarchy gains-.25/.25/.5/1.25,
velocity gains0/.5/1, flags8/108, forced/threshold publication and27F/37F words
are covered. Actual539850/8B1B40,8B1C60/4D6830 blending,8B1DD0 matrix return,
renderer writer,8A34C0 target/mixing and final velocity setters execute. Only
Windows EnterCriticalSection/LeaveCriticalSection/GetCurrentThreadId are stubbed.
Source/corpus SHA256 `8605fc2dc9f2790661a2be3596cd52effe740bbb62cc88ee0a861ff1cfb2365d`, `66544806b64a4a6b73cebd1bc1fac6c37f56ed8cbdc488774bd5912439c83650`. Raw animated
pose, mixed target, desired scene transform, prepared target velocities, final
body velocities and scene stores are independently captured at their actual
boundaries. These latter fields are not compared by this chunk's query bridge.
8B1DD0 is an x87 routine, not an SSE matrix converter; its store sequence is
separate from ordinary47C600. Blend vector/quaternion operations do use SSE.

Full-blend-oracle01 retains an invalid harness assumption that local and world
poses always agree after flag8 publication: the local pose is written while
world publication may be suppressed by its unchanged-position/rotation threshold.
Oracle02 removes that assertion and passes1,600 fixed-gain/identity-animation
cases;03 expands the independent inputs and captures the intermediate fields.
Actual C++ blend-scene-compare-{normal,sanitized}-01 each matches all19,200
raw animated targets under both precision words:134,400 exact position/quaternion
float fields,400 distinct animated input matrices. The normalized-keyframed
preparation negative control differs in12,000 cases in each build.
Comparator/driver SHA256 `67b8fb24d93f5fdc4b262ad68575f363e1a1eb850035c8737eb5091e63cd3fc1`, `34cc8c3701689cd74c130b7be4d2c07bede0d2241871e90e3f4c479c02400626`.
Portable libm and reciprocal-square-root behavior remains distinct from original-
hardware gameplay evidence. Actual startup53-bit precision was checked in169;
live main/worker precision remains open.

No stage closes. Next: prepare the flag-dependent mixed scene target using the
actual8B1DD0 store sequence, compare the coupled renderer/velocity pipeline,
and integrate the actual controller/mode transitions. Root selection, actor
scaling, cross-owner contact islands, keyframed substeps, mode/clock persistence,
World/gameplay and stock-floor fresh-restart acceptance remain open.


### Checkpoint171: flag-specific mixed renderer and coupled native update

Checkpoint170 committed as `5a2f04bf7f8349a281a78811174ff88cdc8687e2`,
167 isolated commits with exact-byte/fresh-clone proof. The new
ragdollBoneWorldFromNativeBlendPose follows original8B1DD0: six XYZ products
and WX are stored as floats; WY/WZ stay at53-bit precision until final matrix
stores. Both it and ordinary47C600 subtract summed diagonal products. A new
near-cancellation fixture independently exposes the former ordinary adapter's
left-associated subtraction: original27F returns bits3019898880 while the
old expression returns3019898881. Shared admission still requires finite,
near-unit quaternion and representable world position; no normalization is
inserted. This correction applies to the existing ordinary renderer adapter.

For an already selected PoseAndVelocity route, ragdollNativeBlendPoseTargets
prepares both the blended velocity-drive target and scene target. Scene rotation
uses the mixed quaternion; scene position uses raw animated native position
unless flag0x100 selects the mixed position. The drive target stays mixed in
both cases. Dispatch still owns route selection; neither helper writes bodies
or renderer nodes. Whole preparation rejects before returning either result.

Four new component cases cover original ordinary cancellation, distinct mixed
matrix stores, four independent flag/gain/animated-pose examples, and invalid
pose/gain/unrepresentable position rejection. Baseline01 retains three failed
cases and one already passing validation case. Full mixed-renderer-
{normal,sanitized}-01 each passes2,221 component and878 engine cases, complete
inventories, zero failures/skips and stable fingerprint `af0b740844c436b3dbfad7b3514178d1bbd71f28d3de5d34fc8dd193b9fb577a`.
ASan leak checks disabled; UBSan halts. Python is unchanged and not rerun.

Executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`. Full original47C600/8B1DD0
oracle executes6,556 cases:1,639 distinct near-unit quaternions, both matrix
paths and both precision words. Quaternion inputs include captured mixed
updates, signed-zero/axis fixtures and33 tiny-axis diagonal-cancellation cases.
Original mixed matrix padding stores are also checked. There are no replaced
game functions or Windows imports in this matrix oracle. Source/corpus SHA256
`84a1bfa458fbbba8a634448fb0e08ce0fa1e1b0c9d6a5e5b02b6cc9c6c390428`, `b5d39945358c6a728adfd334dc2c0dace0cb083f12792be90058baf8ef4d6743`.

Before C++ execution, each comparison declares exact-bit53-bit27F arithmetic,
linked to the independent original CRT startup proof in checkpoint169. Both
mixed-renderer-compare builds match all3,278 canonical matrix cases/29,502
fields. Both also match9,600 complete original88F3D0 mixed updates/624,000 fields:
raw animated preparation, blended drive, desired scene, actual local/world
writer, prepared target velocities and final velocity mixing. Original fixtures
use unchanged requested Dynamic mode1, native sphere-motion2, gains
-.25/.25/.5/1.25, velocity gains0/.5/1, flags8/108, forced/threshold publication
and no World. Only Windows lock/thread imports are stubbed by that full-update
oracle. Native64-bit37F outputs remain diagnostic corpus data, outside the
predeclared C++ domain; live main/worker precision remains open. No tolerances
were introduced for these exact-bit comparisons. Comparator/driver SHA256
`c14895bf174fd342af3c37d28e60e22b3d4f0a8cb704eb7c14e3a3439e1c4d54`, `4def3d357227c8146cc9c6f5892407ba447437bb208e46a1747eb5bbd538c752`.

Mixed-renderer-stock-animated-{normal,sanitized}-01 each runs240 frames using
actual18-body/17-joint stock skeleton and embedded73-track idle animation.
It compares69,120 target matrix fields exactly with an independent ordinary
Animation instance, including controller rebuild and post-physical handoff.
Captured actual Bullet body poses and sampled animated targets feed the new
mixed preparation with explicitly synthetic hierarchy gain.25 and flags8.
Physical renderer and skin projections stay within unchanged.001 tolerance;
maximum error `4.57764e-05` in each build. This is a
coupled Animation/Bullet projection probe, not a native actor controller or
World lifecycle/save/gameplay acceptance. Asset hashes remain those recorded
in checkpoint169; the probe does not issue velocity drives or claim native
frame integration.

No stage closes. Next: publish controller mode transitions and route sequencing
through the physics owner and actual World lifecycle. Root selection, actor
scaling, cross-owner contact islands, keyframed substeps, mode/clock persistence,
World/gameplay and stock-floor fresh-restart acceptance remain open.


### Checkpoint172: staged owned-body blend controller publication

Checkpoint171 committed as `dccaf4edda6b39ef4a6f204090b49fb69f529b05`,
168 isolated commits with exact-byte/fresh-clone proof. ActorRagdollPhysics now
owns updateNativeBlends: a sparse record-keyed batch stages admission, dispatch,
mode changes, scene synchronization, native pose/velocity preparation and
renderer targets before publishing any body. Duplicate/foreign records,
invalid gains/clock/gravity and nonrepresentable preparation reject the batch.
Skipped routes do not inspect unneeded animated matrices or wake bodies.
Returned collision flags/optional renderer target belong to the caller's
controller/node projection; body motion and velocities remain with this owner.
There is no world step or invented frame ownership in this API.

Leaving requested Keyframed6 synchronizes the animated scene before restoring
the current dynamic motion state. Entering6 from Dynamic1 clears both velocities
before scene synchronization. Changed modes clear/set native flag8 according
to the resulting motion; unchanged modes retain caller flags. SceneToPhysics
synchronizes the animated target; PhysicsToScene returns the ordinary native
projection; PoseAndVelocity prepares mixed targets and publishes target/mixed
velocities, returning its renderer target only for selector0. This follows
actual88F484/89ED20 sequencing. Shared mass/inertia/kinematic publication avoids
a second mode implementation and retains body/shape/proxy/constraints/forces.
Selected changes wake their owned constraint group. The flag20-without40
World-driven scene setter remains explicitly unadmitted and rejects before
publication; this API covers the ordinary scene sync path. PhysicsSystem and
World do not call this new coordinated method yet.

Five new cases cover both transition directions, mixed velocity drives and
selector suppression, late invalid-batch rollback, skipped/empty sleeping-body
retention and invalid clock/gravity. Baseline01 retains all five failures.
Full controller-publication-{normal,sanitized}-01 each passes2,226 components
and878 engine cases with complete inventories, zero failures/skips and stable
fingerprint `35d38b157edfaca09694ca038c15cae209c462c96c2540057273aa119afd6941`. ASan leak checks disabled; UBSan halts.
Python is unchanged and not rerun. This is owner implementation plus engine
regression coverage; no public scheduler/World caller is claimed.

Executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`. Original88F3D0 full controller
oracle03 executes57,600 cases:400 source motion/animated inputs, old requested
modes1/6, selectors0/1/2, hierarchy0/.25/1, velocity0/.5, flags0/8/100/108 and
both precision words. Old6 is prepared by the actual8CBC60 archive constructor;
mode setters, archive restore, scene sync, renderer writer and native velocity
paths execute without replacement. Only Windows lock/thread imports and heap
allocation/free primitives are supplied. Native scene-writer inputs are
captured, including their absence for skipped/SceneToPhysics/suppressed mixed
updates. Native requested mode, motion vtable, flags and final origin/quaternion/
velocities are captured. Source/corpus SHA256 `38563fd0293db0a6e919435fc3bb3d916abe78f33ac6abed2f0964509039be74`,
`bfdf230014c5a6de29f98b2e229935055b2cf3a33bf9b357466e1c60ce8980a0`. Oracle01's event lists accidentally share mutable storage
with later fixture setup; its event-history rows are invalid diagnostic data.
Oracle02 copies those lists and passes720 cases;03 expands the corpus and
captures the actual desired renderer input. None proves a native borrowed
World/contact island or actor lifecycle.

Owned-controller-compare-{normal,sanitized}-01 each executes28,800 canonical
53-bit27F cases against actual Bullet-owned bodies. Mode, collision flags,
selection/renderer presence, force/torque retention, shape/proxy identity and
cleanup match in230,400 exact checks. Before execution, the comparator declares
absolute tolerances: renderer.001, native origin.001, quaternion components
1e-6, native linear/angular velocity.001. These cover Bullet's double matrix/
principal-COM representation versus separately stored original float fields;
original no-World fixtures are compared with an owned World and explicit zero
gravity, without stepping. No tolerances were changed after observing output.
Maximum errors in both builds: renderer `3.814697265625e-06`,
native origin `6.0656331697828136e-05`, quaternion
`5.412993220321738e-08`, native linear velocity
`0.00011444091069279239`, angular velocity
`0.0001926422119140625`. Comparator/driver SHA256
`402fc5534ed1baeb49a25263f27bc85f9d439c99389c120e0b45e9a688306325`, `605dbc7075bdc230b8fcdc321070c590cacf994b46b7cd56fe9dc4eea702c127`. Portable libm/RSQRT and live
worker precision remain distinct from original-hardware gameplay acceptance.

Stock controller-stock-animated-normal-01/sanitized-02 each runs240 frames on
the actual18-body/17-joint skeleton and73-track idle animation. Synthetic
hierarchy cycles1/.25 every60 frames, producing eight18-body mode changes;
returned flags are carried to subsequent updates and every body's mode/flag
is checked. Clock1/120 and gravity0 are explicit caller inputs. Detached
animated targets match69,120 matrix fields exactly with an independent normal
Animation instance; physical renderer/skin retain the prepared scene targets
within unchanged.001 tolerance, maximum `5.34058e-05`.
Controller rebuild, animation handoff and zero remaining bodies/constraints
pass. Sanitized01 retained a harness output-layout failure: debug Bullet emits
its exact static-static needsCollision warning for keyframed pairs. Runtime
returned0 with the same small projection error. Sanitized02 admits only that
specific debug line, records it, still validates all expected outputs and
passes source/library hash guards. No arbitrary warning/error is suppressed.
The stock run advances Bullet but does not establish native keyframed-integrator
semantics or cross-owner contacts.

No stage closes. Next: expose this batch through the movement-worker barrier
and PhysicsSystem, then connect actual World controller/lifecycle ownership.
Actor root/scale admission, keyframed integration, native clocks/mode/flag save
projection and fresh-restart/gameplay acceptance remain open.


### Checkpoint173: coordinated controller updates through the worker barrier

Checkpoint172 committed as `b743625abd81c0847286db74678dbeaeb6391bef`,
169 isolated commits with exact-byte/fresh-clone proof. PhysicsTaskScheduler
and public PhysicsSystem expose updateActorRagdollBlends. The scheduler waits
for queued movement workers, takes its exclusive collision-world lock, validates
the current actor owner and calls the staged owned-body controller with the
native default gravity. Prepared frame time and raw selector remain explicit
caller inputs. It returns renderer targets/updated flags without a separate
renderer authority, speculative actor root selection or world step.

Three parameterized tests run with workers0/1/2: publication while queued
physics is active, Keyframed-to-Dynamic sequencing and native gravity; malformed,
unsupported flag20 sync, duplicate/foreign/stale/null/removed-owner rejection
without partial mutation; sparse updates retaining unselected sleep, force,
shape/proxy/user-pointer identity and global World gravity. The existing public
PhysicsSystem/resource/capsule fixture now exercises both controller transition
directions, returned flags/projection and stale-owner rejection while retaining
capsule suspension, snapshot restore, impulse and release/removal coverage.
This is real PhysicsSystem coverage, not World.init/gameplay lifecycle evidence.

Controller-barrier-baseline01 selects10 cases and retains all10 failures against
unimplemented public adapters. Full normal/sanitized01 each retains three failed
new assertions: the fixture incorrectly reused gravity bits1050473923 from an
explicit inverse120 input. The coordinated API derives its inverse from the
stored prepared-frame float1/120; original88F656 stores inverse bits1123024895
(119.99999237060547), and original velocity mixing yields bits1050473924.
The assertion is corrected to that independently observed value; implementation
and tolerances are unchanged. Full controller-barrier-{normal,sanitized}-02
each passes887 engine cases, complete inventories, zero failures/skips and
stable fingerprint `5eef261e307488b0aa8be436905ef882db80a10560b60d67e0cc31c7f40d8834`, rebuilding openmw/esmtool. ASan leak checks disabled;
UBSan halts. Component/Python sources are unchanged and not rerun; checkpoint172
2,226-component results remain explicitly previous-revision evidence.

Executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`. Frame-gravity-oracle01 executes12
original composed cases with no replaced game calls: actual flag/route prefix,
prepared-frame inverse/effective gain stores, then actual world getter and
zero-current/zero-target velocity mix. Frames0/1/120/1/60, flags8/108, gain.5
and both precision words are checked. Explicit jumps bypass mode/scene/target
preparation; this is not full controller/World execution. Source/corpus SHA256
`61aa1bfa713b65ddf3dbc63a9b8876b02a1e9c70022050107777388584bfbbaf`, `b29a90dcd57a44651456e154afd684cd91176de75f72213e0731b5e3933c7e6a`. The exact1/120/flag8 gravity output
is retained in all three queued-worker integration cases.

No stage closes. Next: actual World physical controller/renderer lifecycle,
verified animation-root construction/scale admission, native keyframed stepping,
clock/mode/flag persistence and stock-floor fresh-restart/gameplay acceptance.


### Checkpoint174: preserve authored controller-sequence reaction-root identity

Checkpoint173 committed as `a8d36ece5f8fd3ff2b6127abc240946a32882f5c`,
170 isolated commits with exact-byte/fresh-clone proof. ControllerSequenceMetadata
now retains NiControllerSequence.mAccumRootName verbatim. Loader publication,
shallow-copy preservation and independent copy mutation are covered by the
strengthened OriginalSequenceCoordinatesAndCopies regression. Baseline01
retains its failure against an empty metadata field. Full sequence-root normal
and sanitized01 each pass2,226 component/887 engine cases, complete inventories,
zero failures/skips, rebuilding openmw/esmtool. Tested fingerprint `4e75cb689fdf45487c3aed935bff6cfaf098cc1ad3a12c053f03ccfb166bfa88`;
ASan leak checks disabled, UBSan halts. Python sources unchanged and not rerun.

The native root is NOT inferred from fileRoot0 or Actor+3C. Actual6545E0 calls
actor virtual164: Character/Creature4D8370 selects process+17C for form50/51
and process type0/1, otherwise actual empty ExtraData15 lookup; Player65D720
first checks+5DC, then+5CC only when selector+588 is zero, then that fallback.
Player fields stay opaque. Actor-animation-root-oracle01 runs960 cases through
actual virtual dispatch/getters and, when animation exists, actual654803 entry
gate using animation-object+8 plus full88FA30/88F200. No game-call replacement.
Source/corpus SHA256 `74fee6c8dd517233dcb30983ee808817571021ee833831d6bcd2a8af92b682bb`, `08b473a94f5a181186305b8acb5bed5bc3a4c0cecc1433145d0de3e949ecfc20`.
Eligibility, scene construction and scale remain outside that probe.

Actor model loading4E3A21 calls475D80, which stores the model node in animation+4.
The separate reaction root+8 is assigned at4744AA through471600. Full471600
checks manager+6C, scans array slots through extent+46, chooses its first nonnull
sequence, and returns sequence+60 even if that root is null. Later sequences
are not a missing-root fallback; existing nonnull animation+8 is retained.
Actual sequence binding6C9590 resolves its+5C name through manager+7C's palette
virtual4C and stores+60. Sequence-root-oracle01 covers608 cases using that
prefix, full original palette6C5430/map55E000/hash7DAED0/equality584D10/CRT98262D,
complete471600 and actual lazy assignment prefix, no stubs. Matching is exact,
including case, whitespace and empty strings; absent name preserves prior+60
at this prefix. Source/corpus SHA256 `5be8f8e72c2f6a644c9d689a18eb780aa14bb72f36c47691ddc12dbd5e602e53`, `4696073fa853149259452c9ba2ed1e544de9e829c795609208770f0a7e08a912`.
Registration separately fills a null name from manager target name6C5793;
full binding success, palette population/duplicate names and actor runtime
construction are not claimed by these prefix/declared-layout checks.

Stock-sequence-byte-oracle01 independently decodes the pinned80-record,
73-track idle KF's first sequence header: accumulation string at byte2869 is
Bip01. KF SHA256 `d01bf09a3c703ae2f0f4c043abbe47dc1c0e6d3af0fedcf41ed9a50173bc17d5`, audit source `7d00435d101e0e3269b7c14044eb89a9abcd1404f7be1f3377e735647a21da77`.
Both production stock-sequence normal/sanitized02 preserve that exact string
and resolve the unique skeleton name to record3, then loadActorRagdollRootBlend
selects node8/body12/blend13. Stock-sequence01 in both modes retains a harness
layout failure: the existing loader informational line precedes the two probe
outputs; runtime exited0. Corrected02 admits only that exact line, records it,
validates all outputs and passes source/library hash guards. Full original
stock-sequence-root-oracle01 independently verifies lookup from record3 on the
previous production-parsed scene-layout boundary, with selected blend/body
also matching independently audited raw blend bytes. Source/corpus SHA256
`f91d3f472e784ab4a16ac4dd44bf41d9975082af819c3a9f1ebcc7173a05cda1`, `1d3c5fa75a0f82f73353e55806f9cf2f79749e63a3d09065765446ebbeb30bc8`. FileRoot0 is not substituted;
the synthetic layout is not an independent NIF loader or live actor acceptance.

No stage closes. Next: bind this authored root into renderer lifecycle using
verified native palette/sequence ordering, then actual World controller/save
ownership. Actor scale, keyframed stepping, native clock/mode/flag persistence,
stock-floor fresh-restart and normal-gameplay acceptance remain open.


### Checkpoint175: bind the native reaction root into Animation model lifecycle

Checkpoint174 committed as `a880e81a4f0456225b6dd681c0353a687ec24be6`,
171 isolated commits with exact-byte/fresh-clone proof. Animation caches an
exact-name native record palette from the model before equipment attachment.
Recordless renderer wrappers and serialized empty names do not enter that
palette. Traversal ignores OSG masks, follows scene depth-first order, and the
last equal name replaces earlier entries. Shared movement/bone name maps are
unchanged. KF publication binds the first loaded sequence's authored root;
an absent authored name uses the native model record's name, not the wrapper's.
A missing first sequence root remains absent rather than searching later
sequences. A selected nonnull record is retained across later sources and
clear/reload; actual model rebuild and scene removal release palette/selection.
The optional getNativeReactionRootRecord projection is ready for the native
World owner; it does not create physical bodies or another lifecycle authority.

Five new renderer cases cover duplicate/nonbone/hidden records and case/space
matching; missing first roots with later sequences; empty sequence batches and
parts attached after palette preparation; absent authored-name model fallback;
and retained identity/scene release. Renderer-root-baseline01 retains four
failures and one passing negative case against unimplemented adapters. The
empty-batch final fixture additionally proves late attached record39 cannot
replace original record8. Full renderer-root normal/sanitized01 each pass892
engine cases, complete inventories, zero failures/skips, rebuilding openmw and
esmtool. Tested fingerprint `870bf89db04554cb266108f527cbc661cfad67619f9e4fbfc49fb00d35c7ad7d`. ASan leaks disabled, UBSan halts.
Component/Python sources unchanged and not rerun; checkpoint174's2,226-component
normal/sanitized results are explicitly previous-revision evidence.

Original palette-population-oracle01 executes1,024 cases/5,120 lookups in both
precision words: full716690 clear/populate, recursive7165B0, real NiNode casts,
6C5460/412D30 insertion, preseeded original pool allocation and copied-key
storage, plus full6C5430 lookup. Only Windows critical sections/thread identity
and heap allocation/free boundaries are supplied. Duplicate names are replaced
in last depth-first order; exact case, whitespace and nonnull empty names are
checked. Source/corpus SHA256 `6ff61418c64a15bd3b7a84f54b3fb73cbe83df405ed55d189576e07274ee5938`, `84498b8628a917decae7f817ba9f1f0361bd0df9d9619f8b47e39331580729d7`.
Fallback-oracle01 executes22 actual6C5793 registration prefixes, including full
49F4D0 name copy/CRT984B6A, and zero-length71364F reader-publication branches.
Missing sequence name copies model target name; an existing name is retained;
a serialized zero-length name publishes null. Heap allocation/free only is
supplied; stream reads and full registration/binding are excluded. Source/corpus
SHA256 `b9f9dc1cc38a7b19dbff7f381541fc0734cd10cb5a2124f82ce8d8a0588fda1b`, `622788846b7b1ecc19f21230fb6016a12b23232dd4e7633e16f7ee34ecbfdb62`. Executable identity remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.

Renderer-palette-compare normal/sanitized02 each compare512 original canonical
27F layouts and2,048 nonempty exact-name root queries with production Animation,
without tolerances, including hidden OSG nodes and recordless wrappers. Original
37F duplicate layouts remain in the corpus; nonnull runtime empty-name queries
are predeclared outside the serialized NIF empty-name/null domain and excluded.
Normal01 already passed the same checks but its inherited helper summary
incorrectly printed unused selected_root_record3; its verification.json counts
are correct. Corrected02 preserves that evidence and reports cases/check counts.
No production input, expectation or tolerance changed.

Stock-renderer-root normal/sanitized01 exercise actual setObjectRoot with
ResourceSystem/VFS loading the pinned skeleton, then actual addSingleAnimSource
for its idle KF. Both select native record3, retain it despite a duplicate-named
part attached before KF loading, preserve it across clear/reload, release/rebind
on actual model rebuild, and release on scene removal. Source/library/hash guards
pass. Stock NIF/KF hashes remain those recorded in checkpoint174. This uses an
empty actor pointer and normal animation settings; it is model/KF renderer
lifecycle evidence, not native actor admission, World.init, physics or gameplay.

No stage closes. Next: actual World physical lifecycle/controller ownership,
actor/race scale admission, native keyframed stepping, clock/mode/flag save
projection and unchanged stock-floor fresh-restart/normal-gameplay gates.


### Checkpoint176: reproduce native body and shape property scaling

Checkpoint175 committed as `36193abb0e0cd261928fb583ce8009184bc10f86`,
172 isolated commits, verified exact-byte tree/fresh clone and bundle153.
NifBullet::ragdollBodyWithNativeScaledProperties produces an owned copy of
cached mass, inertia, center and admitted shape properties for a positive finite
resolved actor scale. Original mass scales linearly; inertia uses a stored
binary32 scale cube after two double intermediates, then a separate binary32
product for each element. Center, sphere radius, capsule endpoint coordinates
and distinct endpoint radii scale with individual binary32 products. Convex
hull vertices scale but its collision radius remains unchanged. Inputs and
outputs must remain finite; invalid scale, mass/radius underflow and overflowing
cube/results reject without mutating the caller. Source record identities,
transform, rotation, authored bone bind, coefficients and speed limits are
retained. This property-only function does not scale joint frames, bodyT
translation or renderer transforms, nor does it remove live actor admission
restrictions. It is a prerequisite for the subsequent complete graph adapter.

Four new tests prove mass/inertia distinction, capsule radii, owned hull
vertices with unchanged radius, and atomic rejection. Property-scale-baseline01
retains all four failing tests against the unimplemented copy adapter. Full
property-scale normal/sanitized01 each pass2,230 components and892 engine
cases with exact inventories, zero failures/skips, rebuilding openmw/esmtool.
Tested source fingerprint `d57a3e19f7b23a5a325d749c3fe874b433462ff953560543a716a5fe357b51cb`. ASan leaks disabled, UBSan halts.
Python sources unchanged and not rerun.

Mass-scale-oracle01 executes the complete original8A2D60 without stubs in612
cases: independently byte-audited18 stock body inertia/COM/mass inputs,
17 positive/adjacent-float scale patterns, both x87 precision words and default
SSE rounding. All8,568 stored fields are retained; all unrelated cached-data
bytes and inertia padding are unchanged. Original layout uses three padded
inertia vectors at70/80/90, COM vec4 atA0 and massB0. Source/corpus SHA256
`9347467b6e9ed16320b769450cf6ae2cf9878d89ab427e30a608b6786692a0ad`, `32b475d0c3aca1b583a15fda5bc03a334951048def8571cb22cb1c7ea99b3be6`; stock boundary SHA256
`d39590a10329df8f2fe3133ea93ee3824f56e77100d56ceac37aa9ab08a12161`. This probe excludes the caller/full clone,
cache lifetime, scale resolution, shapes/joints, renderer and live world.

Shape-scale-oracle01 executes408 original sphere8AF4B0, capsule8B68B0 and
hull8C8AA0 copy-member scaling prefixes with real cached-data virtual getters,
17 scales, four coordinate patterns and both x87 words. No calls are stubbed;
each execution stops before generic8A2670 clone publication. Hull plane
normals and collision radius are unchanged; vertices vec4 and plane W scale.
Source/corpus SHA256 `a5ebe5ca41d423974f3e78ebbdf28fc5af90b3473e6c3fb437d89850d551754d`, `6b30dddd7ac3a08efe5d1046fb349f35bce56dc77051be561b38c73a12a8e73b`. Cache
allocation/creation/deletion, native constructors/full clone and gameplay are
outside this probe. Both probes identify original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.

Property-scale-compare normal/sanitized01 each pass all1,020 cases and10,948
exact binary32 checks against these original corpora. Scope is explicitly the
represented fields:13 mass properties; sphere radius; capsule XYZ/end radii;
hull XYZ/radius. COM padding, separate cached capsule global radius and hull
plane arrays are not retained by the owned definition and are not claimed as
implemented. Source/library/input hashes remain stable throughout comparisons.

Bounded caller inspection additionally finds original8A4E30 applying the
resolved cloning-process+10 scale to cached body data before shape cloning.
WorldObject copy89D610 calls source virtual+64 withtrue; actual8BD670 deletes
and clears source+C cached data. This is instruction inspection, not a full
repeated-clone execution proof. NiAV copy707E90 preserves its13 local-transform
words unchanged; native SetScale continuation4DB5FF instead writes root local
scale and updates3D. Do not multiply every node's local translation during
cloning or conflate actor scale with the fixed Havok length conversion.

No stage closes. Next: verified joint/bodyT property and renderer placement
scaling, complete graph/live admission and World physical lifecycle/controller
ownership. Native stepping and unchanged floor fresh-restart/normal-gameplay
gates remain open; all later M15 stages retain their existing pending status.


### Checkpoint177: scale owned bodyT and cone/hinge graph properties

Checkpoint176 committed as `742b24796f5b5b22d774539dd04b28dc4e80c74d`,
173 isolated commits with exact-byte/fresh-clone proof; bundle154 SHA256
`f830ddb575c25978a68f6cc9524d81bb14e121678bdb6ee853b485329a93a3d2`.
NifBullet::ragdollDefinitionWithNativeScaledProperties owns a complete copy and
applies checkpoint176's verified body/shape property operation, additionally
scaling the separate bodyT translation and both cone/hinge pivots. Ordinary
body translation, all rotations/directions, authored bone bind matrices,
angular limits, friction and malleable tau/damping remain intact. Endpoint
identities are validated. Every coordinate is checked before publication;
a nonfinite/overflowing late joint rejects without modifying the authored
source. Repeated calls from that source produce independent scales, without
accumulating prior clone scale. This is a graph property adapter: renderer
placement/binding, native full clone and live actor admission remain separate.

Three new tests cover ordinary/bodyT distinctions, cone/malleable-hinge pivot
and coefficient preservation, independent copies and late atomic rejection.
Graph-scale-baseline01 retains all three failing tests against the unimplemented
copy adapter. Full graph-scale normal/sanitized01 each pass2,233 components
and892 engine cases with complete inventories, zero failures/skips, rebuilding
openmw/esmtool. Source fingerprint `311fc23e62103dfdbd464d410a8308dcb45b0b95360d64dbffe5bc9e1d5534f4`. ASan leaks disabled; UBSan halts.
Python sources unchanged and not rerun.

Joint-scale-oracle02 executes272 complete original cone8C0B70/hinge8B2DD0
copy/scale functions in both precision words using preallocated outputs,
including full8A07B0/8A0200 publication. Only the two pivot vec4s scale;
all copied direction/limit/friction fields remain exact. No calls are stubbed.
Original cached cone/hinge CInfo vtables A98BBC/A56658 dispatch those functions;
live native inner vtables A9CCB8/A97E68 identify cone type7 and hinge type2.
Source/corpus SHA256 `01a84e148b3b08a83f998aff7f0ef55ef4c9054183137e41b46ee3b9c35e3eaf`, `fb5b99f944612ac9017711a126536e81e81331b2cb45d2aadf15d08ba0253c8e`.
Joint-oracle01 retains its initial unmapped FS:0 exception-chain fault at
8C0B7D before any case;02 maps the synthetic Windows exception-chain page.
No native inputs or expectations changed.

Bodyt-scale-oracle01 executes136 original8B8E70 prefixes, including full
8A5980 cached-data getter. Original wrapper quaternion+20 copies to cached+40
unchanged; wrapper translation+30 copies to cached+30 and scales as a vec4.
Every unrelated cached byte remains intact. Each execution stops at8A4E30
before generic body copy, verifying ECX and clone/process arguments. No calls
are stubbed. Source/corpus SHA256 `1a66287c260a65fc1e49d5ff140f8c8b75c2ee8422c253a5ba49568d05e2b66c`, `309ba73fb9a0bf2032b00ff0b0810db2d033e51c11f879660f55add1f857d0a8`.
Allocation, raw-NIF property mapping and the complete clone remain excluded.

Malleable-scale-oracle01 executes272 complete original8BEE20 copies with
preallocated outer output. Full8E7FD0 type dispatch,8E7E60 factory, real
cone/hinge constructors and nested copy/scale/publication execute. The nested
property object is independent; coefficients remain unchanged; all copied
fields match the direct original joint corpus. Only heap allocation/free and
the supplied Havok allocator virtual+10 are boundaries. Source/corpus SHA256
`33c061f5c099c2b0d646a66b1d3386caebe42c901efc1f4234e18139a5483974`, `adec50dea1dce62ccb1569ecd240212b575fb61fb5cb38461a617fe7be34d260`. All three oracles identify original
executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`. General deletion/refcount behavior,
raw-NIF-to-live mapping, whole model clone and normal gameplay are not proved.

Graph-scale-compare normal/sanitized01 each pass680 cases and13,192 exact
binary32 checks: bodyT XYZ/quaternion, direct/malleable cone/hinge XYZ pivots,
represented directions, limits/friction, coefficients and endpoint identities.
Native vec4 padding and unrepresented hinge secondary perpendicular fields are
excluded explicitly; no tolerances are used. Source/library/input guards pass.

Stock-graph-scale normal/sanitized02 use the pinned stock skeleton's actual
NIF parse, renderer unit-placement binding and owned graph adapter for seven
scales(.5/.9/1/1.1/1.3/2/3). Caller-supplied scaled bone translations initialize
real Bullet ownership with18 bodies/17 constraints. All126 inverse masses match
independent original8A2D60 stock outputs. Keyframed-to-dynamic archive restores
mass and principal inertia exactly; every object/constraint is removed on
release.01 retains compile failures from a missing osg::Group include in the
private probe;02 adds that include only, preserving production source and
expectations. No physics step, scale-aware renderer binding, World lifecycle,
save/restart or gameplay acceptance is claimed.

No stage closes. Next: native actor scale/renderer placement and bodyT pose
admission, World physical lifecycle/controller ownership, native stepping and
unchanged stock-floor fresh-restart/normal-gameplay gates. S5-S14 remain pending.


### Checkpoint178: explicit native uniform placement in renderer bone binding

Checkpoint177 committed as `cf1bbb9f2fb961d98d736e750ba66530f8f4c4ac`,
174 isolated commits with exact-byte/fresh-clone proof; bundle155 SHA256
`03b43dec5d757a7aa676d9873b3683ff55f9ee1305e332784681841fce65d454`.
SceneUtil::ActorRagdollPoseBinding now accepts a native NiTransform placement,
retaining uniform scale separately from rigid rotation. Capture composes current
renderer nodes with that scale and returns unscaled physical rotations and
scaled positions. Writeback uses the verified native inverse-parent projection,
preserving authored local scales and unsimulated connectors. Physical bone
scale must stay neighboring-unit relative to placement; nonuniform matrices,
nonrigid rotations, invalid scales, overflow and unsupported independently
scaled physical bones fail before renderer mutation. Existing matrix APIs
retain rigid-placement admission. Callers must scale graph properties separately;
no World caller or race morphology behavior is inferred from this adapter.

Three tests cover scaled/rotated/translated capture, inverse-scale writeback
without repeated accumulation, separate local scale preservation, and atomic
invalid transform rejection. Baseline01/02 retain missing-type include compile
failures; baseline03 compiles and all three tests fail against the adapter
that discarded separate scale. Full render-placement normal/sanitized01 each
pass2,236 component and892 engine cases with full inventories and zero skips
or failures, rebuilding openmw/esmtool. Tested source fingerprint `24c689540b3b0dd1d9c6fe88addab660c63243ea694fafd497df10f7fcf3d62f`.
ASan leaks disabled; UBSan halts; unchanged Python sources were not rerun.

Render-placement-compare normal/sanitized01 each pass36 renderer fixtures and
432 exact binary32 R/P checks against retained complete original53D7A0,
7100A0 and7101F0 composition outputs, both x87 controls, parent scale2 and stock
neighboring-unit parent/local scales. The separate NiWorld scale slot is
excluded from rigid physical matrices. Original corpus SHA256
`c42428ea0132f8152e2d08362243ed5854d5a9f4bcda4fdc41e799e53443ef43`.
Comparator source/driver SHA256 `2493065ca6ba08e352bc2eb6a27c5a325a15d259fcc06b50d1c0d4d561774d30`, `7993e310f6d74724d177d632c32d3c505c8fce7b967c46570edfa9f9985437de`.

Stock-render-placement normal/sanitized01 each pass actual stock NIF parsing,
SceneUtil capture and writeback at seven scales(.5/.9/1/1.1/1.3/2/3), rotated
and translated placement,35 repeated renderer roundtrips within the predeclared
.001 matrix-element tolerance, and actual Bullet ownership18 bodies/17 joints.
All126 inverse masses match independent original8A2D60 outputs. Keyframed to
dynamic restoration retains exact mass/inertia; all objects and constraints
are released. These poses come from live binding capture, replacing the prior
probe's caller-supplied scaled translations. Source/driver SHA256
`cd0c3f8a9682be5020316dd9e966d99db41f45f35bf9294388722c5e782333a2`, `2b906320e126ba517fa8389de8cedb15560d6fa69cdacdf582fa882d1ccd00c2`. Stock skeleton SHA256
`43de349062d2f57b1e581353f1907f8f70bcd3c92b4fe3bd049989b93572d435`.
No Bullet stepping, World lifecycle, save/restart or gameplay gate is claimed.

Root-scale-oracle01 executes864 fixtures/2,592 original paths with no replaced
calls, both x87 controls: complete Character/Creature virtual+EC queries with
actual4D7260,611D00,4D9B40,519D20; creation prefix4E43C6 stops before root
virtual+50; SetScale prefix4DB5C6 includes actual+154 model getter and stops
before707370. Correction to checkpoint176's inspection shorthand:4D7260 is
not universally raw placed scale. For Creature formtype24 it multiplies placed
scale by baseScale+114 with a binary32 store; NPC formtype23 does not take that
branch. Character611D00 then multiplies by selected race height. Creation's
root store uses4D7260; SetScale's root store uses virtual+EC. The whole initial
model, race-weight/morphology, model replacement and physics clone paths are
still open. Source/corpus SHA256 `61fcb9506fd71a0f6284e719f6c44b04facdef0aba23a66a75ddfd9203a07676`, `edf9e0ca5ac145a67f179b99304c710f51df34251c91421f08d3c1ef6290b7dd`;
original executable SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.

No stage closes. Next: bodyT pose admission and actual World physical lifecycle,
caller scale resolution, controller ownership/native stepping and unchanged
stock-floor fresh-restart/normal-gameplay gates. S5-S14 remain pending.


### Checkpoint179: original bodyT forward scene target

Checkpoint178 committed as `4c0d2e4fd951eb196563fc54e000cf15173cbe28`,
175 isolated commits with exact-byte/fresh-clone proof; bundle156 SHA256
`59cfcd74d57a943595730619f0da9b803461ec112c951440c8e81ac3ab9f39d3`.
NifBullet::ragdollNativeSceneBodyTargetPose retains the ordinary current-bone
extraction and adds the verified bodyT-specific forward operation. It rotates
the already scaled local native-length translation by the scene quaternion,
adds native scene position with SSE binary32 stores, and applies ordered
parent/local quaternion composition without an extra normalization. Local
quaternion and translation are validated; ordinary bodies ignore those fields.
The general graph/renderer admission gates remain closed for bodyT until
reverse projection and controller paths are implemented and verified.

Three tests cover local offset units, quaternion order, already scaled offsets,
ordinary-body unused metadata, malformed offsets/quaternions and nonrigid
scene input rejection. Bodyt-scene-baseline01 retains all three failing tests
against ordinary-only extraction. Full bodyt-scene normal/sanitized01 each
pass2,239 component and892 engine tests with complete inventories, no failures
or skips, rebuilding openmw/esmtool. Tested fingerprint `0cae0f08e724be1f752ad97850c1dcf3874e2403ed3095bbe95042ec81182f2a`.
ASan leaks disabled; UBSan halts; unchanged Python sources were not rerun.

Bodyt-scene-sync-oracle01 executes400 complete original89EAE0 synchronizations,
both x87 controls and flags1/40, with real BodyT vtable A980A4 dispatch+ A0 to
8B9400. Full7150F0/4529E0/4D6830 preparation,8B9400 offset rotation/addition,
889470 parent/local quaternion multiplication and ordinary motion setters run.
Only Windows lock/thread primitives are replaced. Local offsets, scene and
local rotations, COM/interpolation state and retained velocities are recorded.
The body-specific target is independently captured at8A2FB0 after the actual
forward transformation. Source/corpus SHA256 `77b371f752150d0f31d4b668b852156db05a154a6f49234a9c428da32cd01931`,
`2755719edee7d5d869f6a51dfc3189fcf082e86509797adf4bd79cd50ddcc6e0`; original executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
NiWorld's separate scale slot is varied but excluded from the already rigid
C++ matrix input; property-clone scaling remains independently owned.

Bodyt-scene-compare normal/sanitized01 each match400 cases/2,800 exact origin
and quaternion fields. Discarding the bodyT offset changes all400 cases,
providing an independently evaluated negative control. Source/driver SHA256
`121d1fdbfa2a9eda9461c244d69eddd8f9f4f72972346ccd469b6957be161f96`, `6672d1b804125b2ed17d35a6df3792833cb47e1f7195e6fb28cdfc10be28f027`. Original motion COM,
interpolation and velocity preservation are recorded but not compared by this
pure forward adapter. No bodyT reverse, full live binding, World lifecycle,
save/restart, stepping or gameplay acceptance is claimed.

No stage closes. Next: independently verified bodyT reverse projection and
owned controller integration before renderer admission; actual World physical
lifecycle, native stepping and unchanged stock-floor fresh-restart/gameplay
gates remain open. S5-S14 remain pending.


### Checkpoint180: reverse bodyT origin and quaternion projection

Checkpoint179 committed as `a65363c86e57deaad50d6bafb0d0681d897b97bc`,
176 isolated commits with exact-byte/fresh-clone proof; bundle157 SHA256
`e9811a58cfaae5432691c4933544ed7954672605e617e3c45815cc4e89534f07`.
NifBullet::ragdollNativeSceneTargetFromBodyPose removes the bodyT local rotation
with original8A2B40 stores and subtracts the rotated native-length local offset.
The reverse W dot uses SSE(X+Z)+(Y+W); forward889470 instead uses an x87 scalar
product minus stored XYZ dot. No extra quaternion normalization is introduced.
Ordinary bodies retain native origin/quaternion unchanged, with malformed
physical input rejected. Both directions share validated local-offset rotation;
all inputs remain immutable on rejection. Graph, renderer and controller
bodyT admission remain closed until complete controller routes are verified.

Three new tests cover identity/local transform removal, noncommuting rotation
and rotated translation, ordinary unused metadata and atomic malformed or
output-overflow rejection. Existing forward rejection also covers overflow.
Reverse-baseline01 retains all three failing tests against identity projection.
Full bodyt-reverse normal/sanitized01 each pass2,242 component and892 engine
cases, complete inventories and zero failures/skips, rebuilding openmw/esmtool.
Tested source fingerprint `8b8dddeca7c5f92df56214c71aa12cfce08756489567a3cf0fcae628a39f6c5d`. ASan leaks disabled; UBSan halts;
unchanged Python sources were not rerun.

Bodyt-reverse-oracle01 executes400 fixtures/800 complete original8B8FB0 and
8B9150 getters with full8A2B40,8A2F10 and8A2ED0 callees, both x87 controls,
using independently synchronized prior native body/local-offset state. No calls
are replaced. Returns, output pointers, stack cleanup and every source byte
are verified. Source/corpus SHA256 `b257f599fe05651d6af5c0dc7435502b3eabe2d7bf22be85957d0acc65d0717f`,
`893c2ef84a87d9ea31514c19d4097824ecda50255304e04c68eb04d71aa12f05`; input corpus `2755719edee7d5d869f6a51dfc3189fcf082e86509797adf4bd79cd50ddcc6e0`;
original executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`. World-length conversion,
matrix/controller publication and actual actor lifecycle remain outside scope.

Reverse-compare normal/sanitized01 each match400 cases/2,800 exact native scene
origin/quaternion fields and reject discarded-offset math in all400 cases.
Source/driver SHA256 `415c1c3f5eabe71930643b184a99d3a1d4caf8eb06160e388e229094511604ba`, `216925632a5e6fb31f4015288575248565744f92f5ffea0e9b842ce7077dcc29`.
Forward-compare normal/sanitized02 repeat400 cases/2,800 exact original fields
after sharing offset rotation, with the same discarded-offset negative control.
Source/driver SHA256 `121d1fdbfa2a9eda9461c244d69eddd8f9f4f72972346ccd469b6957be161f96`, `183b848bd969e9b13dee342814a2c9c26127d0a1e6e4616c2a3e4a021e434f79`.
No tolerance is used by either comparison.

No stage closes. Next: bodyT center-of-mass velocity-drive conversion and owned
controller integration before renderer/graph admission; actual World lifecycle,
caller scale, native stepping and unchanged stock-floor fresh-restart/gameplay
gates remain open. S5-S14 remain pending.


### Checkpoint181: native bodyT center-of-mass projection

Checkpoint180 committed as `337dafb189837b65613e950f327ac02cd8d0d95e`,
177 isolated commits with exact-byte/fresh-clone proof; bundle158 SHA256
`55746ae957af7e1c7db8d713d5d71fb9e4b9e229e67459221f0f6e495194a315`.
NifBullet::ragdollNativeSceneCenterOfMass implements complete8B9050 math:
subtract BodyT local translation rotated by the physical motion8B1DD0 basis,
using88FE00 SSE products and(X+Y)+Z sums. This is distinct from reverse scene
origin8B9150, which rotates translation using the reverse scene quaternion.
The local BodyT rotation is unused by this COM getter; ordinary bodies ignore
unused local metadata. Native input/output lengths stay unchanged. Malformed
used inputs and output overflow reject without changing the inputs.

Three new tests cover physical-basis rotation distinct from scene rotation,
unused local rotation/ordinary metadata and atomic malformed/overflow rejection.
Com-baseline01 retains two failures and one pass against identity projection.
Full bodyt-com normal/sanitized01 each pass2,245 component and892 engine cases,
complete inventories with zero failures/skips, rebuilding openmw/esmtool.
Tested source fingerprint `5f2dea0cd814a639e17a53e9d5c5f96f32b9593cc5f81f61250fc25729da78da`. ASan leaks disabled; UBSan halts.
Unchanged Python sources were not rerun.

Com-oracle02 executes400 fixtures/800 complete original8B9050/8A2FF0 calls,
with real8A3030 and88FE00, both x87 controls and source-byte preservation.
No calls are replaced. Source/corpus SHA256 `12fb1f5c6ae2f57a021e3e583794d92d6d0752c9366a65533552ca65d243bf01`,
`72c7ca4ea69879aa09039cb249ef68717b472116ec6715006785dba192f2adb8`; input corpus `2755719edee7d5d869f6a51dfc3189fcf082e86509797adf4bd79cd50ddcc6e0`;
original executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Com-oracle01/com-compare normal/sanitized01 retain a fixture-layout failure:
8A3030 reads motion+60, which was initially left empty. Bounded original code
inspection identified this; oracle02 maps the independently captured
center at+60 (the prior corpus labels this previous_center_bits). No production change or tolerance relaxation corrected that probe.
The supplied synchronized fixtures have centers at+50/+60 equal; this
cannot close the later native integration/interpolation authority gate.
Com-compare normal/sanitized02 each match400 cases/2,400 exact scene/raw-local
COM fields; discarded-offset negative control differs in396 cases. Four cases
have offsets too small to affect stored centers. Source/driver SHA256
`70a47c519443fef583c56bfe1b658da2e24b4ed7648407680593b36e754d0757`, `9039bbb26135c5411ecdadc23e1e2cc250f8a6723291628319c4d6a80408201d`. No tolerance is used.

Bodyt-controller-oracle01 additionally records57,600 complete original88F3D0
updates over old modes1/6, selectors0/1/2, H0/.25/1, V0/.5, flags0/8/100/108
and400 independently synchronized BodyT states. Actual native archive/restore,
scene getters/setters, mixed pose, drive and renderer publication execute.
Only Windows lock/thread imports and motion-heap allocation/free are boundaries.
Source/corpus SHA256 `8614f2b2449dbe917a420d8718c09fb2b2ea9c57f66279e02ef6a262100129be`,
`3564df901875899ecaef42a8d08946f62f64bb76ca1890e9de82d772d30c2a5b`.
There is no owned C++ controller comparison yet. Velocity drive targets remain
scene poses: original8A34C0 uses raw local COM, adjusted8B9050 center and reverse
scene quaternion. Forward-composing its desired target would change that route.

No stage closes. Next: owned BodyT controller integration/comparison before
renderer/graph admission. Actual World lifecycle, caller scale, native stepping,
unchanged stock-floor fresh-restart and gameplay gates remain open; S5-S14 pending.


### Checkpoint182: owned bodyT controller and scene adapters

Checkpoint181 committed as `1a0dbfd2bbe3fb3af4fbd73690c9ec78e5fef61a`,
178 isolated commits with exact-byte/fresh-clone proof; bundle159 SHA256
`8f64bd43973b05815e2252c594adbb99b466df15de1635e0590c3be4cc43fb07`.
ActorRagdollPhysics now owns a snapshot of each body's local BodyT offset,
validating supported local data before registration. Keyframed scene sync and
controller sync compose scene targets into physical body poses. Physics-to-scene
and mixed routes remove local offsets before preparing renderer/drive targets.
Velocity drive retains a scene target, raw local COM and the physical-basis
adjusted COM, matching original8A34C0 dispatch. Explicit keyframed-pose and
velocity-drive adapters use the same conversions. Physical capture/restore,
shape ownership, mass archives and sparse atomic publication remain in their
existing authority. Graph and renderer binding admission remain closed.

Four new tests cover keyframed/dynamic transitions and scene projection,
scene-target velocity drive with independently derived physical-basis COM,
owned offsets surviving source mutation and both explicit adapters, plus late
invalid offset rejection before any world registration. Baseline01 retains two
failures; baseline02 retains all four failures against checkpoint181 production.
Green01 passes its selected existing/new controller tests. Full owned normal/
sanitized01 each pass2,249 component and892 engine tests, complete inventories,
zero failures/skips and rebuilt openmw/esmtool. Tested fingerprint `5fba4ec05c7691ca23afffb525656272c60e2b590cdb9735eca15b892938e2ca`.
ASan leaks disabled; UBSan halts; unchanged Python sources were not rerun.

Raw owned compare-normal01 retains21,780 failures/28,800 canonical CRT cases
before implementation. Compare-normal02 reduces this to12 failures: maximum
linear velocity error0.0012073516845703125 and angular0.001180887222290039 exceed
the unchanged0.001 limit. These remain failures, not waived acceptance. Native
motion stores origin and COM independently in float; Bullet's principal-COM
matrix cannot preserve both raw fixture fields exactly. Input-projection01
captures the actual owned starting states before controller execution; audit01
verifies200 unique inputs/28,800 repeated preparations preserve raw COM exactly,
with maximum origin error6.103515625e-05 and quaternion error1.1920928955078125e-07,
within unchanged0.001/1e-6 portable representation limits. Projection/source/
audit SHA256 `4b8395db4f9f5158e3e99278dc0162e53be29a37f094ac0445a338f82aaefca7`, `9c93365b855dd394f95f7af9a835c60f8bc1edeaa8b62eb33553c8709a281714`,
`86889f0b9eb17201e5aa53266d7959cfbad14fd06ae7b314b12532ba4ad1ec6d`. No expected controller outputs come from production.

Paired-oracle01 executes28,800 complete original88F3D0 updates from those
physically paired starting inputs, with actual8B1DD0 initial bases and full
motion creation/archive/restore. Old modes1/6, selectors0/1/2, H0/.25/1,
V0/.5 and flags0/8/100/108 remain covered. Only Windows lock/thread imports and
motion-heap allocation/free are boundaries. Source/corpus SHA256
`460b52f45a85f2e9c31579d6918c2418abc696672bc31824c59b680ba4947507`, `9531c17b25ef67bdcbec5819a978538541cdc6d609c254d8bcd9b46a5958dfb3`; original executable
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`. The raw57,600-case corpus remains retained.
Paired compare-normal04/sanitized05 each pass28,800 cases and230,400 exact
selection/mode/flag/presence/retention/identity/cleanup checks. Renderer output
and angular velocities match exactly; maximum body position error6.0931131656616344e-05,
quaternion8.628668690668917e-08 and linear velocity0.0001068115234375 remain within
the original tolerances. Normal03 records the earlier two-test source pass;
normal04 uses the final four-test source. Sanitized04 retains a compiler
ENOSPC failure;05 uses workspace TMPDIR and succeeds. Comparator source/driver
SHA256 `29e3a0350192b847142c9150b48bb8330b7c1f77bad9ce87a1f96c4f60efaf06`, `3bda5da33ea8edc8f8fed1a1f2e2088eed0889fd212bb11071f8bfc459f730d4` (normal04),
`bceee93878320a2de693c9429ea610bc7f1e213055d117f77fc9fabf654c2a42` (sanitized05). This proves controller rules from
matched owned starting states, not raw-native pose/COM fidelity through steps.

No stage closes. Next: graph/renderer BodyT boundaries with current verified
adapters, preserving the raw-state representation/integrator gate. Full native
NIF loader mapping, caller scale, World begin/apply/end, native stepping,
unchanged stock-floor fresh-restart and normal gameplay acceptance remain open.
S5-S14 remain pending.


### Checkpoint183: bodyT graph preparation and renderer scene binding

Checkpoint182 committed as `ff620ad02f33efed8cb09af37e423eba4b9d7110`,
179 isolated commits with exact-byte/fresh-clone proof; bundle160 SHA256
`f60b69b5c65e948925152e111363daf64433ef20ed2810e599f852cf46087f8e`.
Ragdoll graph pose preparation now uses the verified BodyT forward scene
adapter before constructing physical world poses. Local native-length offsets
include caller graph scaling exactly once. SceneUtil renderer binding admits
BodyT targets as scene bones; offsets remain in physical graph/controller
adapters, so writeback does not apply them twice. Existing rigid pose, finite
input, record/hash/topology identity and positive uniform placement checks
remain. Physical malformed local offsets reject during graph preparation and
owned construction. Renderer binding validates the scene identities it uses.

Two new tests cover exact identity-bone physical local offset/rotation and real
renderer capture, graph construction, owned World keyframed/dynamic transitions,
reverse scene projection and writeback/recapture without doubled offset.
Baseline01 retains a test compile failure from using an unavailable translation
getter;02 corrects that assertion and retains both failing admission-gate cases.
Existing rejection fixtures now use malformed BodyT rotation or missing bone
identity instead of expecting every BodyT record to reject. Green01 passes the
selected old/new graph and scene tests. Full binding normal/sanitized01 each
pass2,251 component and892 engine cases, complete inventories, zero failures/
skips and rebuilt openmw/esmtool. Tested fingerprint `0326e847cd25561e66fa4953f3f5d786aa18f12adecac9cdb35db3465649db4d`. ASan leaks disabled;
UBSan halts. Unchanged Python sources were not rerun.

Binding-compare normal/sanitized01 each exercise400 synthetic scene node
fixtures drawn from complete original SceneSync/controller executions across
both x87 controls: real SceneUtil capture, graph BodyT physical preparation,
owned Bullet World PhysicsToScene publication, renderer writeback/recapture
and exact cleanup. Independent expected physical origin/basis are original
89EAE0 BodyT outputs; expected renderer matrices are complete original88F3D0
PhysicsToScene results. Each build matches1,200 exact world-origin fields and
800 exact cleanup fields. Maximum native/Bullet basis error4.2878553419001264e-07
and renderer scene error0.00048828125 stay within unchanged1e-6/0.001 limits;
renderer writeback/recapture matches exactly. Predeclared tolerances, source/
library/input drift guards and failures are recorded in the evidence.
Source/driver SHA256 `28b3b6de301300ccc0307e70bb8c594d5c33f2e006446226e191c7c436527974`, `f8690d52be2bf257d5d1240f062fb3b6e20bd44681fdea58ecaace9a1b450217`;
forward corpus `2755719edee7d5d869f6a51dfc3189fcf082e86509797adf4bd79cd50ddcc6e0`, controller corpus
`3564df901875899ecaef42a8d08946f62f64bb76ca1890e9de82d772d30c2a5b`; original executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
These are assembled scene/physical fixtures. No complete original native NIF
loader, real actor placement authority, physics step or normal gameplay executes.

No stage closes. BodyT graph and renderer boundary gates are now admitted for
the tested supported local-offset domain; the12 raw-native pose/COM velocity
mismatches from checkpoint182 remain open. Native integration/interpolation,
World lifecycle, caller race/scale, unchanged stock-floor fresh-restart,
save/controller clock projection and normal-input acceptance remain open.
Next: resolve the remaining native reaction-entry/caller authority before World
physical integration. S5-S14 remain pending.


### Checkpoint184: immutable native reaction initialization decisions

Checkpoint183 committed as `9cf7d6df6c9d985baa017e8623cfd470012c03be`,
180 isolated commits with exact-byte/fresh-clone proof; bundle161 SHA256
`ea06f084ecda1ffca90049efe59a56c004236019e5166813f6a65fcedc3cdd8f`.
ESM4::resolvePhysicalReactionInitialization models original6545E0 prefix
branches before physical side effects or the existing raw-state dispatcher.
Duplicate Player animation identity bypasses life/stat queries and preserves
the reaction byte. Raw actor life1/2 clears it. Otherwise the existing native
incapacitation rule uses strict Fatigue<0, IntegerAV48!=0 or raw life6. A fresh
process byte0 starts raw3 for nonzero AV48 or raw4 otherwise. Existing signed
bytes remain intact; only active states1/2 request4FBF90(false, ActorExtraData,
0x40). The returned flag request is not proof of an external mutation.
Only used Fatigue is validated finite; skipped/cleared paths ignore it. Actor
and stat identity resolution, effects, actual body changes and clocks remain
caller responsibilities. This is an immutable rule, not a second live owner.

Four new tests cover Player bypass/life clearing, fresh negative-Fatigue and
signed/extreme AV48 cases, both signed zeros and their adjacent values,
existing signed-byte preservation/flag requests and nonfinite used-data rejection.
Baseline01 retains four failures, including initially incorrect zero-Fatigue
expectations. Original probe rows confirm signed zeros do not start raw4 by
Fatigue alone. Baseline02 corrects those assertions and still retains all four
failures against the placeholder. Production reuses requiresIncapacitation's
already correct strict comparison. Full initialize normal/sanitized01 each
pass2,255 component and892 engine tests, complete inventories with zero failures/
skips and rebuilt openmw/esmtool. Tested fingerprint `96477ed8b99a17bdcd0fce476342d00fe59b9f2a265a5f28315d62a5c84e6b37`. ASan leaks disabled;
UBSan halts. Unchanged Python sources were not rerun.

Initialize-oracle01 executes25,600 original6545E0 prefix cases across both
x87 controls, Character/Creature/Player and duplicate-view Player, eight raw
life states (including UINT_MAX), ten signed process bytes, eight finite
Fatigue boundary/extreme values and five signed IntegerAV48 values. Actual
actor virtual164/animation getters, duplicate gate65D750, life predicates
5E33B0/5E0DC0, stat wrappers5E0A60/5E17E0, High-process initial byte getter
64B090, process-byte3/4 stores and optional4FBF90 with empty ExtraData lookup
execute. Actor source bytes and every other process byte are preserved; early
returns verify stack cleanup. Resolved FloatAV10 and IntegerAV48 cache returns
are boundaries; MagicCaster cleanup699DA0 is omitted. Initial process+11D is0,
so mounted/other disruption is not probed. No body-enable/initial velocity,
blend setup, subsequent dispatcher, effects, World/save/gameplay executes.
Source/corpus SHA256 `f3d5104860e0f1ca114a843e3c164577f44285c8085c6d4b3e504912417804a4`, `f27e3ce05af03b6e1f56d29f9fdc7e079761465791bbaa1399a4691d9e1d7cdc`;
original executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.

Initialize-compare normal/sanitized01 each match25,600 cases/76,800 exact
reaction byte, flag request and action fields. Ignoring the Player bypass differs
in6,400 cases; ignoring raw life differs in4,890. No tolerance is used.
Source/driver SHA256 `30658e8dbc17a88167015ccd1136dbd448c61d83d22fe241e4cf8c444cbd3d8d`, `0afec08b9dfe15873ca464be6857cf177614db57d1dd47435c973b590fec9e37`.
The oracle's legacy clears_movement_flag field records the exact0x40 request;
the production API deliberately does not assign its external ownership here.

No stage closes. Native prefix initialization is verified; actual actor/stat
resolution, physical side effects, root blend creation, recovery dispatch,
World lifecycle and the existing native stepping/restart/gameplay gates remain
open. BodyT graph/controller/renderer admission is separate from physical save
projection, which still rejects BodyT records. Next: reconcile that save boundary
with admitted physical BodyT poses, then continue complete reaction side effects
and World integration. S5-S14 remain pending.


### Checkpoint185: BodyT physical snapshot projection

Checkpoint184 committed as `bf90254090f1a333c8c4917367d7457021d0b192`,
181 isolated commits with exact-byte/fresh-clone verification; bundle162 SHA256
`ed833b6b4d4fbf132d68d46c827829fa8e35a2ca6219cf2752d46df66dc01c73`.
The physical snapshot boundary now accepts BodyT graph records. Snapshots store
physical shape-world poses; local BodyT offsets belong to graph/controller
conversion and must not be applied again on capture or restore. Winning asset
hashes, model/base identity, body/node uniqueness, count and finite physical
pose validations remain required. The serialization schema is unchanged.

Three new engine tests fail against the former blanket BodyT rejection in
native-ragdoll-bodyt-snapshot-baseline-01. They cover binary/canonical JSON
round trips, signed-zero preservation,100 exact restore/capture cycles,
duplicate body/node and changed-asset/nonfinite rejection without mutation,
and a binary snapshot resumed in an independent Bullet world. The latter runs
30 steps before saving and60 continuation steps with unchanged position1e-4,
linear/angular velocity1e-6 and basis1e-5 tolerances. It uses the existing
zero-gravity two-body fixture; this is not a floor/contact/native-game test.
The ordinary-body continuation case remains present.

Native-ragdoll-bodyt-snapshot-normal-01 and sanitized-01 each rebuild openmw,
openmw-tests and esmtool and pass all895 engine tests with complete inventories,
no skips/failures and no compiler warnings. Tested fingerprint `216999cdc986c2071d335cb65e800295bdca2cbfe01137f2c0d8dc2ad92b647a`.
ASan leak checks are disabled; UBSan halts on errors. Component/Python sources
are unchanged in this chunk;2,255 components passed both builds at checkpoint184
and were not needlessly rerun.

This closes the blanket BodyT physical-pose projection rejection only. It does
not persist native motion modes, blend clocks, activation/contact state or
establish native integrator equivalence. The original stock floor restart
failure and raw BodyT velocity representation discrepancies remain retained.
Loaded winning-file hashes remain identity authority; no new persisted kind
marker is claimed. World actor physical lifecycle and normal gameplay remain
open. Next: verify original8AB440 down-transition creation before implementing
its caller/controller setup and World integration. No stage closes; S5-S14
remain pending.


### Checkpoint186: native knockdown blend controller creation fields

Checkpoint185 committed as `2cb5deb4b31e338cd7c7984db39411f31dda157a`,
182 isolated commits with exact-byte/fresh-clone verification; bundle163 SHA256
`83d93a4ab38171d8b8a07ab53305efc1e69d7fadd5d983316988c339690023ff`.
preparePhysicalKnockdownBlend creates the selected-controller two-key transition
from current separate H/V gains to0/0. Both signed-zero durations retain both
keys; no instant-completion shortcut is introduced. Native setup preserves
start-key/duration bits and flags outside its mask. Start adds active bit8:
final flags=(old &0xFEF5)|0xCD. Start/previous clock times reset to-FLT_MAX;
stored elapsed time is preserved. Only used finite gains/duration/start-key/
elapsed values are validated; overwritten old clock fields are ignored.
Negative durations must be handled as disabled by the selecting caller.

Down-setup-oracle01 is a retained harness failure: runtime RTTI parent links
were absent and all cases bypassed setup. Corrected02 executes original static
initializer A120C0/70E220 before collision/controller RTTI lookup, then6,048
original8AB440 prefix cases across both x87 words, seven duration boundaries,
four signed/unclamped gain pairs, three old key-buffer capacities, three flag
words, three start-key values, controller present/absent and immediate paths.
Actual497420/47FAC0,700010,8AA7F0,8AA480 constructors/copy/reset,8AB000/8AA710
insertion,8AABE0 key-range updates and715540 Start execute. Allocator401F00 and
free401F20 are boundaries. Synthetic body-filter17 and requested motion0 avoid
physical mutation. Stops precede immediate mutation, initial velocity and
child traversal; no World/gameplay acceptance is claimed. Source/corpus hashes
`886daebfe6c75d6a8f40ad1c7c8c1baf61673340c9940b13781499553f773a2f`, `9daa85502eb1d4b2b4de465e1e50276d1f96fc703bdc658babbe6593b55305a7`; pinned original executable
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`. The1,296 selected transition rows compare exactly
in normal/sanitized03:15,552 fields each, with clearing-elapsed and omitting
Start-bit8 negative controls differing in every selected case.

Three new tests retain failing baseline01, then verify fields, signed-zero/
subnormal durations, initial gain evaluation and malformed used-data rejection.
Full normal/sanitized01 and02 pass but retain an assertion-branch compiler
warning;03 adds explicit braces and passes2,258 component and895 engine tests
each without that warning. Tested fingerprint `a2bcba31e90c0f880d1715ad6ad04373d0a57cf5c8c5f444184e622cfec8c220`. ASan leaks disabled,
UBSan halts; unchanged Python sources not rerun. Compare sources/driver hashes
`fbb356ad8171a12fad855898846ad040e7dec8ac6dc81c480f3106c90799399c`, `1dccb63a32462d4b0dc0fb1a69765008b5bb5b7d1255b27caffc0347b7aa41b4`.

This is an immutable controller-creation adapter, not a live controller owner.
Full controller cache/state persistence, node attachment, nonzero start-key
clock evaluation, immediate blend mutation, initial velocity, traversal and
World actor physical lifecycle remain open. Existing native stepping/restart
failures are unchanged; no stage closes and S5-S14 remain pending. Next:
connect separate native placement scale to Animation's physical ownership and
sampling API, then continue live reaction lifecycle integration.


### Checkpoint187: native placement in Animation physical ownership

Checkpoint186 committed as `6841edae1a21e4afe1a3a97d196317ef31aeb803`,
183 isolated commits with exact-byte/fresh-clone verification; bundle164 SHA256
`5f4a19c6f0b15d974e35eaba7fba4ce8c847ccbf4b92d4b6b5f8936f00dbd20e`.
Its initial staging attempt hit /tmp disk quota; four older review checkouts
moved to ignored S3/relocated-temporary-reviews-01 storage with original paths
preserved by symlinks. Exact staged bytes were checked before commit recovery;
original workspace Git metadata remains unchanged.

Animation now exposes native NiTransform placement for physical begin, capture,
apply and animated-target sampling. Separate uniform scale reaches the verified
SceneUtil binding instead of being packed into a rigid matrix. Shared private
implementation preserves the original matrix API's controller detachment,
resident/external callback ordering, topology validation and local-pose rollback.
Graph property scaling remains the physics caller's separate responsibility.

Three new tests fail against explicit unsupported-overload baseline01. Final
cases cover scale2 frozen world/local round trips; scale0.5 animated targets,
exact current physical matrix restoration, one callback execution and latest
animated locals on ending ownership; zero/negative/nonfinite scale rejection
before binding, and invalid live placement rejection before callback or write.
All previous matrix-placement tests remain present. Native-ragdoll-animation-
placement-normal-01 and sanitized-01 each rebuild openmw, openmw-tests and
esmtool and pass all898 engine tests with complete inventories, no skips/
failures. Tested fingerprint `5e17eb8b7b709caf9a8c1a69436f657eb542327e5218e6505ea39f693dd1373a`. ASan leaks disabled; UBSan halts. Unchanged
component/Python sources were not rerun;2,258 components passed both builds at
checkpoint186. The normal build retains the same CharacterController::
unpersistAnimationState compiler maybe-uninitialized warning seen in this
chunk's baseline before implementation; no new warning is attributed to the
placement overloads.

This verifies renderer API ownership and sampling, not normal World initialization
or native gameplay. Original placement/scaling arithmetic evidence remains in
checkpoints176-178; no new original-executable probe is claimed here. NPC
initial morphology, World begin/apply/end lifecycle, native controller attachment,
clock/contact/activation persistence and original stepping/restart failures
remain open. No stage closes; S5-S14 remain pending. Next: retain authored node
blend-controller identities and timing inputs so reaction setup can distinguish
a real selected controller from absence, then connect the live lifecycle.


### Checkpoint188: owned authored blend-controller identity and timing

Checkpoint187 committed as `d88ff689321ae666e7e50c3d8ed09fee92c2852b`,
184 isolated commits with exact-byte/fresh-clone verification; bundle165 SHA256
`77c9e65be5cec0c7b8b6503a68da00de4f181dfcda19dae81dfca20223539365`.
The actor graph now owns the first bhkBlendController attached to each blend
collision target node: record identity, nullable authored target identity,
flags, frequency, phase and start/stop timing fields. Lookup follows attached
controller order and stops on the first match. Target equality is not a native
selection predicate; missing controllers remain missing. Membership and cycles
encountered before selection are validated. Later unused controllers are not
invented as active authority. Raw authored timing preservation is not simulation;
controller clock caches and keys are not synthesized here.

Controller-lookup-oracle02 executes189 full original700010 cases with actual
blend/velocity-controller RTTI getters: every matching mask at chain lengths0-5
and null/same/other targets. Original RTTI ancestor initializers A09970,A09D90,
A12400,A12640 execute first; no functions are stubbed. Oracle01 omitted these
parent initializations;02 confirms identical output corpus while covering full
ancestry. Original executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`; source/corpus hashes
`33317fa0c786590e6bcc35e517c64cb773fe987902700bdc69c4a8f50de61d9d`, `550432dff0bd03a967b42987fe533ab51ccc9c2e363da26f136dfc6a52a56a81`.

Three new tests fail in baseline02. Baseline01 retains a private script syntax
failure and rejected empty test selection; it is not test evidence. Green01
retains a real null-link getPtr error (normal failure and sanitizer assertion),
plus an unresolved synthetic null-target record pointer. Explicit empty-link
checks and initialized null fixture pointers correct both. Full normal/
sanitized02 each pass2,261 component and898 engine tests, complete inventories,
no skips/failures and rebuilt openmw/esmtool. Tested fingerprint `67b8b51abdceeccdd1ae6e307cde90ec9e8d8ca217416921f9a56e03dc54bb4e`. ASan
leaks disabled; UBSan halts. Unchanged Python sources were not rerun.

Metadata-compare normal/sanitized02 load the hash-identified stock18-body/
17-joint skeleton and attach189 independently enumerated synthetic chains to
node8. Native selected indices match exactly; owned target/flags/timing fields
match explicit fixture inputs:1,323 exact fields in each build. Head-only lookup
would differ in78 rows; selecting the last match would differ in126. Asset hash
`43de349062d2f57b1e581353f1907f8f70bcd3c92b4fe3bd049989b93572d435`; source/driver hashes `9df35f44569ec017ee8ba5d747824784698b57b4236d094b3b04500588488739`,
`a6d7f8ece4e2f8d35bbd3ee7a7267f0eacf58a8ee55037b94f671b86ab0e91a5`. This fixture substitution is not an unmodified-stock
controller lifecycle or original-game acceptance.

No stage closes. Authored controller identification is preserved; nonempty key
parsing, runtime attachment, transition clocks, World physical lifecycle,
controller/contact/activation persistence and existing native stepping/restart
failures remain open. S5-S14 remain pending. Next: verify the original keyframed
motion stepping path, whose velocity semantics currently disagree with Bullet's
kinematic update, and use that evidence for the live physical lifecycle.


### Checkpoint189: original keyframed physical motion step

Checkpoint188 committed as `4579ac43ae3a3db9b1d1c6565056364ad2fbf20a`,
185 isolated commits, exact-byte/fresh-clone verified. Bundle166 SHA256
`e0b2a0697e5332ef1b50b0f7e6b67ed247ab87c65044b269e9683ef21dbdebe0`.
An immutable keyframed motion helper now caps native linear/angular velocity,
advances physical center of mass, applies a world-space angular increment,
normalizes the quaternion and reconstructs physical origin using raw local
center of mass. It returns the original angular increment cache as well.
Keyframed motion does not apply gravity/damping. Untouched velocity signed
zeros survive. Invalid physical inputs, coefficients and overflow reject.

Original-key-step-oracle02 executes3,780 full8EA4B0 cases with actual889470,
4D6830 and8B1DD0 and no stubbed game functions: both x87 controls,20 independent
prior physical fixtures plus an identity fixture, five frames, three velocity
cap pairs and six linear/angular velocity pairs. Executable hash
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`, oracle source `d72b03f7c69d622400e20a6b422fa906736bb9a916ddc97cb9cbd6bf37b29103`,
corpus `cda82cf69ad846ad9681fc7b8a375f7e3b5cec00973da9553e6e19dc6cf31fd4`. Oracle01's3,000 cases remain retained;02 adds
identity/zero-angular fixtures. Actual motion+60 is current physical COM,
+80 current quaternion and+90 raw local COM; older corpus labels do not define
these offsets. Swept time caches, native World scheduling and contacts remain
outside the helper.

Four initial tests fail baseline01. Green01 full suites pass, but original
comparisons correctly reject13,140 numeric fields in each build. The initial
helper multiplied current rotation before angular increment. Caller8EA6A4
passes incremental rotation first: original889470 therefore applies a
world-space increment before current rotation. The corrected helper includes
an additional rotated-body regression from original row116. A local shadow
warning is also corrected. Both comparison01 failures remain retained;
no tolerance has changed.

Final normal/sanitized02 each pass2,266 component and898 engine tests, full
inventories and no skips/failures, with rebuilt openmw/esmtool. Fingerprint
`92c62dcb17bee09dd7e99e6fe5a12fffe7631a5befa4c4143464fdeb59bcadf4`. ASan leaks disabled; UBSan halts. Comparisons02 each match all109,620
numeric fields across3,780 cases with zero observed numeric error;13,482
native velocity zero fields retain their signs. Fixed prior tolerances remain
.001 for physical positions/velocities/angular cache and1e-6 for quaternion/
basis. Zero-velocity and omitted-COM-advance negative controls differ in
1806 and
1344 rows respectively.
Comparator source/driver hashes `8264bf1cab47db2d3fb68a4dad2ca4cc96f2e1beab991df1e088e7716f885835`,
`e1654a4bc8ea5fdd2830e9a25eb95100e9b881bb37f10b7481412c5a82ed7f51`. Unchanged Python checks were not repeated.

No stage closes. This helper does not yet advance owned Bullet keyframed
bodies or suppress Bullet's saveKinematicState velocity replacement. Native
swept time/activation/contact state, physical World lifecycle/save projection,
retained BodyT velocity representation and floor fresh-restart failures remain
open. S5-S14 remain pending. Next: integrate owned native keyframed stepping
at the shared physics boundary while preserving ordinary Bullet behavior and
verifying owner registration/removal and mode transitions.


### Checkpoint190: owned keyframed integration at actual Bullet substeps

Checkpoint189 committed as `0bf4247c59009455071dc41c1a47689194505487`,
186 isolated commits, exact-byte/fresh-clone verified. Bundle167 SHA256
`89f15ab9884707470e469a5808a9db070666317a2c909c77c27e7700d66b4bb4`.
PhysicsSystem now creates a NativeDynamicsWorld. ActorRagdollPhysics registers
its borrowed bodies and a substep callback after admission, and unregisters
before destruction. Each actual Bullet substep stages the active keyframed
batch using the original motion helper, publishes capped velocity and physical
COM/principal frame, and updates AABBs. Invalid batches do not partly publish.
The native owner retains its velocities through Bullet's saveKinematicState;
unrelated kinematic bodies continue deriving velocity from their old/new poses.
Dynamic bodies keep their existing shared physics behavior. An ordinary
borrowed Bullet world still needs the explicit owned-step caller. Neither
renderer/bodyT offset nor actor scale is reapplied here.

Baseline01 retains missing explicit Bullet header build errors. Baseline02
executes five integration tests: four correctly fail without owned integration,
while ordinary dynamic/unrelated kinematic control passes. Final normal/
sanitized01 each pass 2,271 component and 898 engine tests, complete inventories,
no skips/failures, rebuilt openmw/esmtool. Fingerprint `3e31af0dc9e6e2c9152984c865de8d54df83b9044f02b1fcdd1e5e9028618033`. New cases cover
scaled local COM and preserved velocity under World gravity, zero actual
substeps and multiple real substeps, owner destruction/replacement, rejection
before batch mutation, and unchanged unrelated kinematic/dynamic behavior.
ASan leaks disabled; UBSan halts. Python sources remain unchanged.

Owned-compare01 retains 126 velocity-field mismatches in each build. They are
42 high-speed zero-duration cases: directly invoking original8EA4B0 at zero
duration caps velocity, whereas Bullet performs no actual substep and invokes
no motion callback. This is a dispatched-domain distinction, not a tolerance
change or a claimed native World zero-duration acceptance result. The explicit
empty-step test checks no advancement. Compare02 separately selects the 1,008
positive-duration original cases with effective loaded limits 250/31.4159.
Other synthetic cap pairs from the arithmetic oracle are not admitted loaded
body limits. Independent original physical basis/current COM/raw local COM and
velocities are installed in a one-body owner fixture, then an actual Bullet
step runs. Each build compares 21,168 fields (shape origin, COM, velocities,
physical basis), with no mismatches: maximum origin error
6.07310785199e-05, COM error
2.27373675443e-13, linear velocity error
2.84217094304e-14, angular velocity error
0, basis error
3.57627868652e-07. Fixed prior limits remain .001 for positions/
velocities and 1e-6 for basis; 2,520 zero-velocity fields retain their signs.
Original corpus `cda82cf69ad846ad9681fc7b8a375f7e3b5cec00973da9553e6e19dc6cf31fd4`; comparator source/driver
`437b0fd47a33d775a7704a55c190782c61f90317f93d52eada73fec9e656e125`, `f1d35ca51598f72870fc3a7227c9aa9ffdd4879bb73f150a4bc88bd716f508a6`. The Bullet principal-frame/
quaternion conversion is explicitly a representation bridge, not native
quaternion bit ownership. Contact-free single-body comparison does not prove
native contact solver ordering or full native clock/cache equivalence.

No stage closes. The earlier keyframed velocity replacement/absence of owned
physical advancement now has implemented substep ownership and tested behavior.
Native swept clock caches, exact native motion representation/persistence,
controller lifecycle and actual World physical begin/apply/end remain open.
Retained BodyT raw velocity and floor fresh-restart failures remain open;
S5-S14 remain pending. Next: exercise contact velocity use, motion-mode changes,
registration rejection and actual PhysicsSystem/scheduler routing before
connecting actor/controller World lifecycle.


### Checkpoint191: physical-only public scheduler and contact regressions

Checkpoint190 committed as `ef131a7c82ecc2f790f1830186ae0a2f680feb78`,
187 isolated commits with exact-byte/fresh-clone verification. Bundle168 SHA256
`a5f2404855ded7ffd1dd09a8f054f44fe7ef6fe12b9f044c4ec77c4c602966a8`.
PhysicsSystem now constructs weather frame data only when movement jobs exist.
A suspended capsule leaves its native physical owner alive; an empty movement
job list still steps that owner through the serial or worker barrier. There
is no weather consumer in that case. This avoids an unnecessary scene/weather
authority query, rather than assigning weather state to actual movement jobs.

The public PhysicsSystem fixture now performs a real scheduled keyframed step
with 0, 1 and 2 physics worker threads, waits through the public capture barrier,
checks motion and retained velocity/gravity exclusion, and keeps the capsule
suspended. Baseline normal exits -11 before stepping; sanitizer identifies the
null Scene call in World::isCellExterior -> isInStorm -> WorldFrameData ->
PhysicsSystem::stepSimulation. Both failures remain retained. The fixture is a
headless loaded-native-content/resource/synthetic-mesh integration; it does not
initialize the normal gameplay World or replace normal-input acceptance.

Three added component cases verify a moving keyframed body's retained velocity
feeds an actual Bullet contact manifold and pushes a dynamic body, loaded
linear capping to 250 followed by restored dynamic impulse response, and
rejection of null/duplicate/foreign/missing owner registrations without changing
the admitted owner. First full01 runs retain a fixture failure: the contact
setup inherited a hinge whose registration disables linked-body collisions.
Removing that joint makes the assertion require contact rather than constraint
response. Engine-filtered01 additionally retains a wrong fixture expectation:
the preceding dynamic pose drive intentionally supplies positive-Z gravity
compensation. Explicit zero-Z initial velocity isolates the subsequent
keyframed gravity check; no assertion tolerance changed.

Filtered02 each pass three component cases and the one public scheduler case
(with all three worker settings). Final normal/sanitized03 each pass 2,274
component and 898 engine tests, full inventories with no skips/failures and
rebuilt openmw/esmtool. Fingerprint `969a6e93ccd4ad0e228c12ef770cd43df365a64e96353c7d685619333619be66`. ASan leaks disabled; UBSan halts.
The unchanged original keyframed arithmetic/owned single-body comparison remains
checkpoint190 evidence; it was not rerun for this scheduling/test-fixture change.
Python sources are unchanged. Bullet contact response is an integration
regression, not independently paired original-game contact solver acceptance.

No stage closes. Native swept time and exact motion representation/persistence,
controller ownership and actual World physical begin/apply/end remain open.
Retained raw BodyT velocity and floor fresh-restart failures remain open, and
S5-S14 remain pending. Next: admit and preserve nonempty authored blend-controller
keys from the NIF stream, then connect owned keys/clocks to the physical
lifecycle using the already identified original controller path.


### Checkpoint192: nonempty authored bhkBlendController key payloads

Checkpoint191 committed as `af6e77c003adab556bb530690a64c54fab4deeef`,
188 isolated commits with exact-byte/fresh-clone verification. Bundle169 SHA256
`179eba1e0fff1a87485a29a586638dd9784c2eb86a93db3f4f2542e915ef0395`.
The NIF parser now decodes a uint32 key count followed by time/hierarchy/
velocity float triples for both admitted Oblivion versions 20.0.0.4 and .5.
It preserves authored order, duplicate times, signed zeros and raw gain values.
Payload size is bounded through NIFStream's existing typed-vector read before
allocation; overflow and truncation reject without replacing prior keys.
Zero-count reads clear prior keys. Unverified later nonempty layouts retain
unsupported behavior. Parsed source clock/header fields remain authored data;
this parser does not emulate runtime key-range/cache changes. Actor graphs own
an independent key copy from the selected first attached blend controller,
retained after source records are destroyed.

Original-load-oracle03 executes 36 full8AB7B0 cases, nine key fixtures across
both admitted versions and both x87 precision words. Actual7008A0 inherited
NiObject loading,715F40 NiTimeController loading,712A20 deferred-link reads,
8AA480 allocation/constructor,8AA710 insertion and8AABE0 key bounds execute.
Only resolved NiBinaryStream I/O and allocator/free boundaries are supplied;
no game function is stubbed. Next/target links are null independent fixtures,
not a native resource/link-resolution pass. Fixtures include 0/1/2/3/5 keys,
shifted/descending/duplicate times, signed-zero gains, subnormal time and
maximum-finite gain. The original preserves key order and bits. Executable hash
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`, source/corpus hashes `66619df0f5d635f9e352298d5bcbfd55cd08c492374ade212b1e6fe71fd4d71e`,
`44d4c64bd6dc364915539d496c7c3d28c7b9c3717f4a3f12ea3d30b7a145bac2`. Oracle01 retains a harness failure from treating the
NiBinaryStream's direct read field as an indirect vtable slot, before any rule
case completed. Oracle02's 18 cases explicitly stub inherited time loading;
03 removes that stub and tests both supported file versions.

Three of four initial tests fail baseline01 on unsupported keys; the malformed
payload case passes. Baseline retains the temporary missing-vector-initializer
warning, corrected by explicit ownership initialization. Filtered01 retains a
build error from directly calling the private stream-size checker; the public
typed-vector reader provides the existing checked boundary instead. Filtered02
retains three failures from a version guard admitting only .4 while the common
fixture uses VER_OB/.5. The original full-loader03 verifies both versions;
filtered03 passes all six tests in each build, including added zero-count reuse
and later-version rejection. Final normal/sanitized04 each pass 2,280 component
and 898 engine tests, complete inventories with no skips/failures and rebuilt
openmw/esmtool. Fingerprint `18af560914e7f6c4c4eb2f72c159d073805c66c0dc61f60b622fe2e6e6e30406`. ASan leaks disabled; UBSan halts. Unchanged
Python checks are not repeated.

Stock-key-compare02 loads the hash-identified .4 skeleton (18 bodies/17 joints),
sets the in-memory controller decoder version to each fixture's .4/.5 version,
parses independent key payloads at node8 and then destroys the parsed controller.
Both builds exactly match all 240 owned count/key fields across the 36 original
cases. Dropping keys differs in 32 cases, sorting in four, and deduplicating
times in four. Comparator source/driver hashes `62ea1b456bbe7f85b1426a9f8d3b9e07dbb7e42059b9907039c617c045c9d907`,
`fe4c65a35f2309e32b019bfef39d0fde01293825d6ed66def591a767d7721284`; stock asset `43de349062d2f57b1e581353f1907f8f70bcd3c92b4fe3bd049989b93572d435`. Compare01's 18
inherited-load-boundary cases remain retained and passed. This is explicit
fixture substitution, not a modified .5 stock asset or full stock controller
runtime acceptance.

No stage closes. Nonempty authored payload parsing/ownership is now implemented;
keys and clock caches are not live simulation authority. The existing one-shot
evaluator deliberately admits at most two keys, so general authored-key cursor
progression remains open. World physical lifecycle and full physical save/
restart, retained raw BodyT/floor failures and S5-S14 remain open. Next: verify
and implement arbitrary authored-key evaluation with the original cached
segment cursor, retaining the existing one-shot API's behavior.


### Checkpoint193: cached arbitrary authored blend-key evaluation

Checkpoint192 committed as `e2fceedf9e6dac2bbe0de5558a8c38c7754c06b0`,
189 isolated commits with exact-byte/fresh-clone verification. Bundle170 SHA256
`ead28d17239313d3adde6cbd0423170599c461acbab88ddbda5f96dcd1c3769a`.
The general authored-key evaluator now accepts an explicit cached lower-segment
cursor and returns both gains and the next cursor. Backward time before the
cached lower key resets the search; forward search advances only when the next
key time is strictly less than requested time. Equality, duplicate times and
zero-length intervals preserve cursor-dependent behavior. Empty controllers
leave gains absent and preserve arbitrary cursors; constant controllers ignore
unused key time, requested time and cursor while retaining signed-zero gains.
Multi-key inputs validate finite gains/times, ordered keys, admitted cursor and
caller-clamped time before returning a result. The existing two-key one-shot
API retains its supported domain and interpolation store boundaries.

Full original8AA990 and6D3690 execute 11,248 cases with no game-function stubs:
69 key sets, including60 seeded3-9-key arrays, duplicate/zero-time/signed-zero
fixtures, all admitted cursors, endpoints and interior times, both x87 words.
Empty/single paths include unused NaN time and arbitrary cursors. Both precision
words produce identical results. Executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`; oracle
source/corpus `e28264e6db7f79e2020ccc23b48e1a1507d7c2956e80e92876ba5f38bb21f666`, `2b390341e15713c0fe0726fd117938a4abd69891a50dab1dffd75696b67855c5`. Normal and sanitizer
comparators exactly match44,992 result/cursor/gain-bit fields each. Original
corpus negative controls detect500 cases changed by unconditional cursor reset
and8,904 wrong cursors from always selecting the first segment. Comparator
source/driver hashes `deae41ad4d983203f2c719587a5b7ca1df2ff3529011777f4964ffd998237879`, `e4bb639052edcf26a4409a9683691e8d20dcd5d8f619569d1aacf094960ed115`.

Baseline01 runs six selected tests: the existing authored-byte regression passes
and all five new cases fail the placeholder. Final normal/sanitize01 each pass
all2,285 component tests with complete inventories and no failures/skips;
fingerprint `0f6a998c41baafde80284eefdecd74e15edbf98a4321279c89dfc3833a260f15`. ASan leak checks remain disabled; UBSan halts. Sanitizer
recompilation exposes existing missing-owner aggregate-initializer warnings in
inventorymechanics/runtimestate and dangling-else warning in runtimestate2109;
these unchanged tests are not modified in this chunk. Engine and Python checks
are not repeated for this isolated pure new API: the existing API/data layout is
unchanged, with898 engine tests per build verified at192. Runtime integration
will rebuild affected engine targets.

No stage closes. General authored gain/cursor evaluation is implemented, but
native key bounds, general frequency/phase/cycle/reverse clocks, controller
attachment and live World lifecycle remain open. Full physical persistence,
retained BodyT/floor failures and S5-S14 remain open. Next: establish the generic
native controller clock/cache before connecting authored keys to live owners.


### Checkpoint194: general controller clock and explicit shared cache

Checkpoint193 committed as `a6277430d97a044f6509a67058e427b18bc44dd2`,
190 isolated commits with exact-byte/fresh-clone verification. Bundle171 SHA256
`7f8171170441a7ad523beab8c898c44d87a818312b6b6977ed7f1cd55af34d76`.
General controller timing now supports both initial time bases, signed frequency,
phase, all four cycle modes, reverse, shifted/zero finite ordered bounds and
explicitly owned last-result cache. Native ping-pong cycles unshifted key time;
wrap subtracts the start key. Float stores follow the original delta, accumulated
elapsed, phase, remainder and reflected-result boundaries. The original cache
identity omits reverse: two controllers sharing bounds, cycle and uncycled key
time can reuse a result even with different reverse flags. This behavior is
preserved with an explicit cache parameter, without introducing static helper
state. Controller start/previous/elapsed and cache updates publish together only
after validation and successful arithmetic. Existing one-shot API is unchanged.
Active/attachment/update eligibility and scheduler cache ownership remain caller
responsibilities, not inferred from flags by this clock helper.

Full original7155A0 and actual982BFA CRT remainder execute70,226 cases across
both x87 words. Only Windows critical-section enter/leave boundaries are supplied;
no game clock, remainder or cycle function is stubbed. Cases cover both time
bases, four cycles, reverse, five frequencies, three phases, shifted/zero bounds,
sentinel/existing clocks, deliberate shared-cache reuse,512 seeded nonbinary
fixtures and32 signed-zero/subnormal fixtures per precision word. Both words
produce identical fields. Executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`; oracle source/
corpus `9f1e64dbe51784c79f8cb38569ef50ae328bc653e7aaf16b3ba42dab283764a1`, `4a109e458ad979699623a70037eb9493192021eb56920046e7cd8e4b6f538e9b`. Oracle01 retains the initial
69,138-case corpus; the96-case probe retains an inherited one-shot scope string
although its flags were changed to wrap, so only the full correctly scoped
oracle02 is general-clock evidence. Final comparator04 matches632,034 exact
returned-time, clock and cache fields per build. Source/driver hashes
`07ce2b2de7887da74698bb9da1b55421ea753b1ea44018f03c548853ef34753c`, `b59606df8c7cebd83f9f11359c57a1a62bf7d086a7316b9e3160229e7e4bd5a9`. Original-corpus controls detect
14,680 changed returned times when ignoring frequency,20,120 for phase,13,672 for
forcing clamp (whole-state changed cases34,560/46,080/51,840). Control source hash
`e44bb4ea0b2447cf05557f33ef4f7b1b31c7880d35f6de711c267aab8cf6216a`. Comparators01/02/03 retain passing earlier fingerprints.

All five new clock tests fail the one-shot placeholder in baseline01. Initial
normal/sanitized01 each pass2,290 component tests. Added sixth regression checks
11 malformed clock/cache/input or used-overflow variants, comparing all retained
state bits including NaNs after rejection. Normal/sanitized02 retain one failed
fixture: using -FLT_MAX as a previous time correctly selects native sentinel
initialization rather than subtraction overflow. The independent two-case
sentinel probe confirms zero elapsed with maximal finite input; the used-overflow
fixture now uses nextafter(-FLT_MAX,0), with production arithmetic unchanged. Final normal/sanitized03 each pass
all2,291 component tests with complete inventories, no skips/failures; fingerprint
`91fbe95406485bcd8194e611d2fc23f23fc39929b917a0fdfad0003f8047e579`. ASan leak checks disabled; UBSan halts. Existing aggregate-owner and
dangling-else warnings from unchanged tests remain retained in baseline/first
sanitizer compilation; no unrelated warning fixes are included. Engine/Python
checks are not repeated for this isolated new pure API;898 engine tests per
build were verified at192, and affected engine integration still requires rebuild.

No stage closes. General clock arithmetic/cache and arbitrary-key evaluation are
implemented independently, but native load-time key-bound initialization and
live controller owner/attachment/World begin-update-end remain open. Full physical
save/restart, retained BodyT/floor failures and S5-S14 remain open. Next: establish
native loaded key-bound initialization and controller update/target dispatch,
then connect the owned controller to the existing physics/renderer bridge.


### Checkpoint195: native loaded blend-key bound initialization

Checkpoint194 committed as `16a9247885102aec5bda519c6d049035dbb11f1e`,
191 isolated commits with exact-byte/fresh-clone verification. Bundle172 SHA256
`df4cd92d30ee8d05f489bfaf5ff835130edec8fe04b931719958df2cd480b9a7`.
The native bound helper preserves inherited minima, with +FLT_MAX start and
-FLT_MAX stop initialization sentinels. Empty keys reset both bounds to+0 and
ignore unused prior values. Equality retains inherited signed-zero bits.
Only first/last key times and nonempty inherited bounds participate; gains and
middle key times are unused by this primitive. Finite reversed raw bounds are
preserved, rather than treated as admission to the ordered general clock.
Calling the helper once after creating every transition key differs from calling
it after each serialized key insertion: with0/.25 keys and sentinel bounds,
once gives stop.25 while per-insertion loading retains stop0. The NIF parser's
raw authored header/keys remain unchanged; this helper models runtime bound
initialization separately and does not silently rewrite authoring metadata.

Full original8AABE0 executes3,900 cases with no boundary stubs:59 authored time
fixtures, six inherited bound pairs, both x87 words and both once/per-insertion
strategies. Fixtures include empty, single, ascending/descending, duplicate,
shifted, signed-zero, subnormal and50 seeded key arrays. Executable
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`; oracle source/corpus `a1abcfc09eb5161cae976b298057213785f8fc84f16167443dd4edf102414a2c`,
`17df631f888cb177c060e7501e87f8705210259d48de3e2352396d54cc8a2651`. Oracle01's3,192 per-insertion cases remain retained. Both
comparators exactly match7,800 bound fields each. The first/last shortcut differs
in3,020 original cases. Comparator source/driver `e6405eaf8f80a6f8f9fb967023f540bda78e09aba1b45acd2d57ad0cf7bfba79`,
`f5e33c31da9be394ff8b05360f9473cc949ee929eab51ddf853ee683e9a8c0ee`.

All five new tests fail baseline01. Final normal/sanitized01 each pass all2,296
component tests with complete inventories and no failures/skips; fingerprint
`0f3a807301cd18f71c19c61bf7c72b58290301f745d4e1af9d66b71032f4486c`. Tests distinguish serialized versus one-time setup, inherited minima,
sentinels, equality signed zeros, raw reversed bounds, empty unused NaNs and
used/unused malformed fields. ASan leak checks disabled; UBSan halts. No warnings
appear in final focused recompilations. Unchanged engine/Python checks are not
repeated for this isolated pure helper; engine integration still needs rebuilding.

No stage closes. Key-bound initialization now joins independently verified
arbitrary-key evaluation and general timing, but live controller ownership,
update eligibility/target dispatch, finish/reset/detach and World physical
lifecycle remain open. Full physical save/restart, retained BodyT/floor failures
and S5-S14 remain open. Next: execute the full original8AAD60 controller update
including actual clock/evaluator and finish paths before owning those transitions
in the existing physics/renderer bridge.


### Checkpoint196: native controller update/finish publication candidate

Checkpoint195 committed as `ca79810db9bfa3f3d4738c492907b5e0751cfa40`,
192 isolated commits with exact-byte/fresh-clone verification. Bundle173 SHA256
`caff73f382bbd3c29131030bf1f8238c90ac54d6f3043bc68d39c8533bb69ba9`.
The owned value-state transition composes general timing and cached key evaluation
with original eligibility and finish ordering. Missing target, inactive controller
or empty keys leave state untouched and ignore unused input timing; a target
without a blend updates only previous time. Successful evaluation snapshots old
target gains only when cached hierarchy is negative. Clamp completion clears
keys/cache first when requested, then optionally removes a separate velocity
controller, then optionally restores cached gains, then stops. Reset preserves
the segment cursor; Stop clears active bit and resets previous time, resetting
start only for the absolute time base while preserving accumulated elapsed.
The blend controller remains attached. The immutable result includes controller,
shared time cache, optional target gains and a velocity-removal request for the
runtime owner to publish atomically. This value model does not itself write
nodes, remove live controllers or update physical bodies.

Full original8AAD60 executes110,592 cases with actual497420 target lookup,
7155A0 clock/CRT remainder,8AA990 evaluator,8AA7F0 reset,8AA3E0 velocity lookup,
8AA420 optional gain restoration,6FFE90 attached-list removal and715570 Stop.
Actual controller RTTI initializer ancestry executes. Only Windows critical-section
and interlocked primitives are supplied; retained references avoid allocator/
destructor boundaries. Both x87 words, four key fixtures, both time bases/all
cycles/reverse, all active/reset/remove/restore bit combinations, target/blend/
velocity presence, cached negative/zero/positive hierarchy and boundary times
are covered. Shared time cache starts explicitly reset and all five fields are
captured. Executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`; oracle source/corpus
`3ec989bec3eb2bd6bcb736a7dc6aa97ce9ee77d02c2b4dab09364145ea9e6713`, `2cb20ee115136cf16a269a1c7939d2fa0adb721d0b02992447f178ae5e6e6c71`. Both comparators match1,769,472
exact gain/cache/key-count/clock/cursor/flag/velocity-attachment/time-cache fields
per build. Comparator source/driver `fb1aefe834a9c687e241e672a7a68217278e4a56b764981ba674949c0dbbb08b`,
`e34f29650c98d78f32663e1ef7158556c26449e96586350b08f14cebe877e917`. Paired original controls, excluding flag metadata
itself, detect18,144 cases forced inactive,720 omitting key reset,360 omitting
restoration and360 omitting velocity removal. The360 removed-controller cases
all remove velocity controllers; no blend controller is detached. Control hash
`966589c40e9eed4b0da354d712f161c5a0388ac6647e2b92afbfc4afd11ec29b`.

Oracle01's2,304 cases retain a scope string naming detach even though no fixture
contains an attached velocity controller and6FFE90 never executes. This is not
self-detachment evidence: BA7F3C is the blend RTTI, while the removal lookup asks
for BA8000 velocity RTTI. Oracle02 executes12,288 cases with actual initialized
ancestries and optional attached velocity-controller nodes;03 adds all clock
modes and explicit time-cache fields. These retained diagnostic distinctions are
not waived. Original8AB000 separately confirms key insertion resets cursor+3C
to0; finish reset's retained cursor must not be confused with new-key setup.

All five new tests fail baseline01. Final normal/sanitized01 each pass all2,301
component tests with complete inventories, no failures/skips; fingerprint
`e97ae72ff10e74c2e5fc7839afbe88df8503b7bce6ac76b56f08e4cee9117975`. Unit regressions cover snapshot once, finish ordering, retained cursor,
Stop sentinels/elapsed, zero-hierarchy restoration, missing-blend previous time,
unused malformed timing and used rejection. ASan leak checks disabled; UBSan
halts. Final build logs show no warnings. Engine/Python checks are not repeated
for this isolated immutable component transition; live integration still needs
engine rebuilding and owner lifetime/rollback tests.

No stage closes. The full logical controller transition is now independently
verified, but actual runtime ownership/publication, velocity-controller creation/
removal effects, World begin-update-end and complete physical persistence remain
open. Retained BodyT/floor failures and S5-S14 remain open. Next: give this state
an ActorRagdollPhysics owner and compose its staged publication with the existing
atomic native body-blend bridge before wiring World lifecycle calls.


### Checkpoint197: physical owner for authored blend controllers

Checkpoint196 committed as `d86ad329451ab89a075303cd6872df13f95e3360`,
193 isolated commits with exact-byte/fresh-clone verification. Bundle174 SHA256
`13049f7e191a339ba4300e9538fd195188cfb7a8ab6eaa02cb12ef150d09e2ad`.
ActorRagdollPhysics now owns selected authored controller keys/timing, initialized
per-insertion bounds, clocks/cursors/cached gains and current owned target gains/
collision flags. Source records/definitions can be destroyed independently.
Controller target-node identity resolves the physical body, including targets
other than the body where the controller was attached. Target-less controllers
and owned nodes without blend objects retain their distinct native behavior.
The caller supplies the shared time cache across actors; no per-owner/static
cache silently changes native reverse-cache identity. Each request batch stages
all controller and target states, calls the existing atomic physical blend bridge,
then commits owned metadata and shared cache only after all computations succeed.
Actual body mode/synchronization/velocity and renderer publication targets are
produced by that bridge. Direct updateNativeBlends remains an explicit supplied-
gain bridge; this new interface owns its own controller/target metadata.

Supported identity admission is explicit: controller targets must be absent or
owned body nodes, body-node identities must be unambiguous when controllers
exist, selected controller identities and physical targets must be unique.
Cross-graph targets or multiple requested controllers for one physical target
reject rather than pretending an unresolved node has no blend. Velocity
controllers are not created/owned by this API; absence is passed explicitly to
the logical transition. No velocity-removal effect or complete actor lifecycle
is claimed. These admission limits remain open for the full World integration.

Original owned-sequence oracle01 executes768 updates in192 groups over four
successive frames. Actual8AABE0 initializes bounds after each key insertion,
then full8AAD60/clock/evaluator/reset/restore/Stop executes from sentinel initial
clocks/cache gains. Both x87 words, six key fixtures, reset/restore flags and
target/blend presence are covered. Only Windows primitives are supplied;
there are no velocity-controller fixtures. Executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`;
oracle/harness/corpus `a14397cd04a16b431846fe5246682d8f4ac8a9628ee73812a8baabbab2af7228`, `e527a08cd0584d7b145d9dbe88e4f242f23e1c33da1385ac2b6d05d5586452fd`,
`bcc0c03bc2cca3972c39b09cab202259756229be358fa85f8396663bb240d656`. Both owner comparators execute the physical blend bridge
and exactly match15,816 owned gain/cache/count/clock/cursor/flag/bound/time-cache/
key-bit fields. They recreate/remove192 physical owners and verify no leaked
collision objects between groups. This compares logical owner state, not original
physical contacts or World gameplay. Source/driver `1677b21ddd534a9d49ffc34830f56b50cd3d5b50cfe7c980d422bc5e845c4a7b`,
`746e34ab37bdf068c453790a484f764b979faebf550dd736b53d333cfe093890`. Independent paired-input controls:
`{'ignore_missing_target': 320, 'ignore_missing_blend': 160, 'omit_reset': 56}`; source `ab18830b1ad5abb6d167da42ee74373a98d96780b50f8cadb11c54e4c2733fa5`. The reset control also
clears the unused removal bit to match available pairs, with flag metadata
excluded and no velocity controllers. Controls01 retains a missing-pair lookup
failure;02 makes that fixture constraint explicit.

Baseline01 retains a test compile error from an incorrect RagdollBodyState field
name; corrected baseline02 runs all four initial owner tests and all fail the
placeholder. Filtered normal/sanitized01 each pass those four. Three added
regressions cover invalid identity before registration/publication, cache sharing
across actors with differing reverse flags, and missing target/blend with unused
nonrigid animation poses. All seven owner tests pass. Final normal/sanitized01
each pass2,308 component and898 engine tests, complete inventories with no skips/
failures and rebuilt openmw/esmtool; fingerprint `3defc8c56a82dfcf64b0546d909c4c250d5e3f7f73cdfcf5ea0ae2b00f8eb862`. Comparator01 retains
an explicit-btTransform constructor compile error;02 fixes only the private
fixture construction. ASan leak checks disabled; UBSan halts. Existing GCC16
Character.cpp3330 maybe-uninitialized and testoblivionactorstats6843 range-loop
copy warnings are retained unchanged. Python checks are not repeated.

No stage closes. Authored controller state now has a live physical owner with
atomic physical publication, but public scheduler/PhysicsSystem forwarding,
normal World/renderer begin-update-end, generated reaction-controller/velocity
ownership and complete physical save/restart remain open. Retained BodyT/floor
failures and S5-S14 remain open. Next: expose owned controller capture/update
through the existing worker barrier and shared scheduler cache, test all0/1/2
worker configurations, then wire the actual World lifecycle.


### Checkpoint198: public physical-controller scheduler forwarding

Checkpoint197 committed as `84261feffa078d05707f2b636168579ef69ef44f`,
194 isolated commits with exact-byte/fresh-clone verification. Bundle175 SHA256
`c6d8bc2f95cb532938d4ead83a4e14f0e73596d609a475f84ace3bb7068727b3`.
PhysicsSystem now exposes owned controller/current-blend captures and updates
through PhysicsTaskScheduler. Every operation waits for movement workers and
uses the existing collision-world lock policy. The scheduler owns one shared
physical-controller time cache across actors, initialized before workers start;
its lifetime is independent of individual ragdoll removal. This is the shared
cache for these owned physical controllers, not yet all renderer NiTimeControllers.
Update passes the reviewed native gravity constant and leaves controller/target/
cache publication to the atomic component owner.

The existing public physics ownership integration case now checks owned authored
keys/timing, capture identity, stale-pointer rejection, actual KEY publication,
continued capsule suspension, bad-pose rollback of physical/clock/cache state,
and shared reverse-cache behavior across two physical actors. The case runs
through all0/1/2 worker configurations. Baseline01 fails its first0-worker size
assertion against the placeholder; the fatal assertion does not exercise later
worker configurations in that baseline. Filtered normal/sanitized01 each pass
the aggregate case including all three configurations. Final normal/sanitized02
each rebuild openmw/openmw-tests/esmtool and pass all898 engine tests, complete
inventories with no failures/skips; tested fingerprint `ca22708e6af94d814c4851c58bdd91d754d95fd0ef9e1a691b39f32756321c79`. ASan leak checks
remain disabled; UBSan halts. Components are unchanged since checkpoint197's
2,308-test normal/sanitized checks, so those and Python checks are not repeated.

No stage closes. Public forwarding now reaches the live component owner, but
normal World/renderer begin-update-end, generated reaction/velocity-controller
ownership, the full renderer clock producer/cache scope, and complete physical
save/restart remain open. Retained BodyT/floor failures and S5-S14 remain open.
Next: connect renderer target sampling and physics publication with lifecycle
ownership, starting with a rollback-tested binding rather than assuming a
headless public API test establishes normal gameplay acceptance.


### Checkpoint199: atomic complete animation frames

Checkpoint198 committed as `eca9fc7362536a8ec31c938fce7c958b5f9a6a13`,
195 isolated commits with exact-byte/fresh-clone verification. Bundle176 SHA256
`ec24cf1bc8906cfd3642cc29dd449eac0fcc0b7171e30ebcf42f62ea0ef3924c`.
ActorRagdollPhysics now accepts a complete bone frame and an explicit order
containing every owned controller once. It matches node identities independently
of attached-body identity, advances controllers using the shared cache in the
supplied order, then publishes every owned blend body in supplied bone order.
Uncontrolled blend bodies use current owned gains/flags; non-blend bodies remain
untouched. The sparse controller API and complete-frame API share one staged
implementation, so failures in later uncontrolled body computations also leave
controllers, cached gains, shared clock cache, body modes and poses unchanged.
Complete/unique node/controller admission is validated before publication. Graphs
with ambiguous body nodes reject this complete-frame interface even if the legacy
controller-free constructor admitted their synthetic metadata.

Three initial tests all fail placeholder baseline01. Five final regressions cover
redirected controller targets and unordered bone input, actual KEY publication
for uncontrolled bodies, late uncontrolled failure rollback, malformed identity
coverage, explicit forward/reverse controller order with native shared-cache
identity, and zero-controller frames leaving non-blend bodies unchanged. The
first expected-pose fixture uses the native length scale when comparing the
world-length graph adapter; baseline exits at its placeholder size assertion.
Final normal/sanitized01 each pass2,313 component and898 engine tests with full
matching inventories and no failures/skips; fingerprint `d97e8bc08706d9bbda32dfada7ed78640f92af8bf6a179c1e78a78cc7014e679`. ASan leak checks
disabled; UBSan halts. Python is unchanged and not repeated.

Both complete-frame comparators reuse checkpoint197's independently generated
original corpus, execute768 updates/192 owner lifetimes through the new full-frame
API, and match15,816 exact owned gain/key/bound/clock/cache fields per build.
Original corpus SHA256 `bcc0c03bc2cca3972c39b09cab202259756229be358fa85f8396663bb240d656`; comparator source/driver
`2a138274aa513bb4fe7d62f4506b2e6ebffb9c832d5603ea1ac6654af82f4f81`, `262dfefe993ec2d7901594de6abc8f0a73be04c81f29432d84f1af6a6061c0eb`. This checks logical progression
through the new physical publication boundary; original physical contacts,
complete scene traversal ordering and normal World gameplay are not compared.
The caller must obtain actual controller/bone traversal order; owned record
insertion order is not asserted to be the original renderer traversal order.

No stage closes. Complete frame publication is now available at the component
owner, but public scheduler forwarding for that frame and renderer/World
begin-update-end remain to connect. Generated reaction/velocity-controller
ownership, full renderer clock scope and complete physical save/restart remain
open, alongside retained BodyT/floor failures and S5-S14. Next: expose full frames
at the worker barrier and test a joined renderer/physical binding with begin
failure cleanup before adding automatic World lifecycle calls.


### Checkpoint200: renderer publication before physical commit

Checkpoint199 committed as `a7140028ee1007cad5c5cb025e08a7e654105cce`,
196 isolated commits with exact-byte/fresh-clone verification. Bundle177 SHA256
`8c7a1bf409f57fc556814950deee0b04122fff0f3cc9f5c8ffb7f2521215add2`.
Complete native blend frames now support an optional atomic renderer publication
hook. It receives the borrowed prepared body publications exactly once after all
physical computations and owned controller/target/cache staging, before any
physical body or owned metadata changes. A throwing callback leaves the physical
owner and shared cache unchanged. The callback must publish its own scene batch
atomically, must not mutate the physical owner or reenter scheduler operations,
and must not retain the publication span. The existing renderer binding validates
and projects its whole batch before writing. Scheduler/PhysicsSystem forwarding
now exposes complete frames and their hooks under the existing worker barrier/
collision-world lock, retaining the cross-actor clock cache and reviewed gravity.
This provides a joined publication boundary; automatic World lifecycle is still
absent, and caller-supplied traversal/time are not claimed as normal renderer
clock/traversal producers.

Three component cases all fail baseline01 when the hook is ignored: rejected
scene publication incorrectly commits physical/controller state, no callback is
observed before physical commit, and the joined SceneUtil renderer binding is not
updated. The component failure stops that helper before its engine mode, so a
separate scheduler baseline01 rebuilds/runs the public ownership case and fails
its first0-worker full-frame size assertion. No later worker baseline coverage is
claimed. Final tests cover once-only preparation/publication order, bad physical
targets not invoking the scene callback, callback failure retaining pose/mode/
gain/cache/clock state, complete real renderer projection rejection without
partial node writes, and successful retry rendering desired physical poses.
The public integration case exercises full-frame forwarding, scene callback,
stale actor rejection, callback-failure rollback and capsule suspension through
all0/1/2 worker configurations.

Final normal/sanitized01 each rebuild openmw/openmw-tests/esmtool and pass2,316
component and898 engine tests, complete matching inventories with no failures/
skips; fingerprint `7eeac77fbb8fceacab3695d4fd01dc15fb30e79730a56b1c29834c7104869d8f`. ASan leak checks disabled; UBSan halts. Baseline's new
GTest dangling-else warning is fixed with explicit braces before the final builds.
Python is unchanged and not repeated. Both updated comparators execute768
successful publication hooks and192 owner lifetimes over the independently
captured checkpoint197 corpus;15,816 exact owned gain/key/bound/clock/cache fields
match per build. Corpus SHA256 `bcc0c03bc2cca3972c39b09cab202259756229be358fa85f8396663bb240d656`; comparator source/driver
`7de2c6b3c1ba3ab54acb4a67e238c88a7ec563f39b7b309b0d719d5c5a22203e`, `faa83b4e20f138b789c68e0a29ae914e012f1d4046f63cd3940d7c01e5ae6c73`. No original physical contacts,
normal scene traversal timing, World gameplay or full save/restart comparison is
claimed by these metadata comparisons or the synthetic joined renderer fixture.

No stage closes. The renderer/physical frame publication boundary and public
forwarding are now joined. Next: bind renderer and physical lifetimes with
failure cleanup at begin, then connect actual World reaction begin/update/end.
Generated reaction/velocity-controller ownership, actual clock/traversal
producers, initial NPC morphology and full physical persistence remain open;
retained BodyT/floor failures and S5-S14 remain open.


### Checkpoint201: joined renderer/physical lifetime entry and cleanup

Checkpoint200 committed as `b68c124296f79f9198b289c48d45f02f8c59fe06`,
197 isolated commits with exact-byte/fresh-clone verification. Bundle178 SHA256
`d56c329028110db841957a9db3d6dd3a149ca7a0fa1066d859e01414cbd50a23`.
The World-layer beginNativeActorPhysicalPose adapter joins Animation's detached
animated-target ownership with public PhysicsSystem body/capsule ownership.
It rejects missing/already-bound actors, owns a native property-scaled graph
from caller-resolved uniform placement, captures actual live bone world poses,
converts native/world lengths once, then admits physical bodies. Conversion/body
admission failures end renderer ownership; capsule suspension is published only
on successful physical admission. Authored graph properties remain unchanged.
The caller supplies resolved collision policy and uniform morphology/placement;
this helper does not infer NPC morphology or select/install reaction controllers.

End joins renderer release and physical/capsule release, is repeatable, and still
releases physical bodies/capsule when renderer rebuilding reports a failure.
Animation::endPhysicalPose now releases borrowed physical state even when the
hierarchy no longer admits local restoration. It restores controller ownership
and reports the original restoration error, preserving it over a second rebuild
error. Teardown of damaged renderer topology is cleanup with a diagnostic, not
successful gameplay recovery or a claim that nonexistent nodes were restored.

Entry baseline01 runs the public ownership case and fails the placeholder's
missing admission rejection/renderer rebuild and bound-state assertions at its
first0-worker iteration. Separate end-baseline01 runs the new renderer regression
and proves prior endPhysicalPose retained physical ownership after detached-bone
restoration failed. Final public integration covers all0/1/2 workers: deliberately
invalid inertia rejects after renderer binding, leaving original locals and
capsule collision; successful uniform-scale2 entry owns mass4/radius1 and correct
world position from authored mass2/radius.5; duplicate entry preserves ownership;
repeated teardown is safe; controller rebuild and detached-hierarchy errors still
release renderer/physical ownership and restore capsule collision. The new
renderer case also verifies teardown remains repeatable and callbacks traverse
again after topology repair.

Final normal/sanitized01 each rebuild openmw/openmw-tests/esmtool and pass all899
engine tests, complete matching inventories with no failures/skips; fingerprint
`bb9a17164c374cc016b58308b06ce58ca5f3255bc75bfff582951963dcd54c50`. ASan leak checks disabled; UBSan halts. Component rules/libraries and Python
are unchanged; checkpoint200's2,316 component checks and independent controller/
clock comparisons remain the applicable evidence, with no unchanged repeats.
Uniform property/graph scaling uses the independently verified checkpoint176/177
rules; this adapter adds lifecycle composition, not new native scaling formulas.
The synthetic fixture owns real renderer and physical objects but does not call
automatic World reaction orchestration or establish normal-input gameplay.

No stage closes. Joined frame publication and lifetime entry/cleanup are now
available to World orchestration. Generated reaction/velocity-controller setup,
actual native clock/traversal producers, initial NPC morphology and full physical
save/restart still need implementation/verification before that orchestration
can be accepted. Retained BodyT/floor failures and S5-S14 remain open. Next:
resolve/install owned generated reaction controllers and connect the actual
World begin-update-end callers using independently admitted timing and placement.


### Checkpoint202: owned selected knockdown curve setup and native setup state

Checkpoint201 committed as `df4ab6653f932473bffdbef50b8f2eb9fcee228d`,
198 isolated commits with exact-byte/fresh-clone proof. Bundle179 SHA256
`239d1f8a0dae2e7715db17f54c4b1d083e937a3612c006e352a804616a76ecb0`.
ActorRagdollPhysics now owns each selected controller's attachment-node identity
separately from its target node. prepareNativeKnockdownBlends stages a complete
caller-resolved node/duration batch and configures existing selected controllers
from the attachment node's current gains. Missing blend/controller and negative
duration skip setup; missing controllers are not invented. Unknown/ambiguous or
duplicate nodes and nonfinite used durations reject without publishing an earlier
curve. Unused durations on missing paths remain ignored. Zero duration retains
both keys. Setup owns keys {0,current gains},{duration,0/0}, frequency1, phase0,
explicit start0/stopduration, native flag mask plus Start, sentinel clock reset
with elapsed retained, cursor0, cache gains-1 and raw controller+60 state2.
Full controller finish resets the raw setup state to0 only when resetting keys.

This implements the existing-controller normal setup boundary, not whole8AB440:
physical motion, velocity-controller creation/removal, immediate setup, special
nonblend parts and recursive traversal remain open. The caller must resolve the
duration from the actual constructed native body filter; no unverified choice
between raw NIF filter fields is introduced. Shared time-cache and body/gain/mode
state are unchanged by setup. Actual updates still resolve controller target,
which can differ from attachment or be absent.

Independent original prefix8AB440 then full8AAD60 yields3,456 captures in576
sequences over both x87 controls, null/self/other targets, attachment/blend
presence, negative/zero/.25/1 durations, reverse/reset flags and warmed clocks.
Actual RTTI lookup, allocation/key insertion, range updates, Start, clock,
evaluator, reset/restore/Stop execute; Windows primitives and fresh key-buffer
allocation/free are boundaries. Prefix stops before velocity/traversal/unsupported
physical effects; requested motion0 fixture. Original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`, oracle source `15f52f6c1ed27c57d9ac5609e416225b9079cd641d8df3efd31c91f0198a5242`,
corpus `b54d595a9969c351011e9d3b974291506199f26e219ef70e9027712deb92a0db`. Final normal/sanitized comparator02 each match
all3,456 captures exactly, including both node gains, keys, bounds, frequency,
phase, flags, clocks/cache/cursor and controller+60. Paired original observations
and actual setup/finish markers reject six negative controls; no production
outputs generate native expectations.

The unmodified stock skeleton's independently decoded raw type table contains
18 bhkBlendController instances. Parsed18-body/17-joint graph selects exactly
those18; every controller targets its attachment node and initially owns an
empty curve. Asset SHA256 `43de349062d2f57b1e581353f1907f8f70bcd3c92b4fe3bd049989b93572d435`. This resolves the previous
uncertainty about controller generation for this stock asset: setup fills its
existing curves; an absent blend controller is skipped by the original prefix.
This content/metadata audit is not original-loader or game-lifecycle acceptance.

Three initial regression tests fail against the placeholder. Final tests add
owned warmed-clock setup, attachment/redirected-target distinction, zero-duration
two-key retention, missing/disabled paths including unused malformed duration,
absent target, late malformed/duplicate/unknown batch rollback, and setup-state
finish reset/preservation. Normal/sanitized01 each execute2,321 component tests
and fail one mistaken test expectation: native preserves reverse bit0x10, so
setup from0x1d yields0xdd, not0xcd. Comparator01 already matched all original
captures; assertion corrected without changing the implementation. Those failed
runs stop before engine mode and remain retained. Final normal/sanitized02 each
pass all2,321 component and899 engine tests, exact inventories without failures
or skips; fingerprint `4f62f58cb719894eee4c65761eedfe554f6c2ea8ed84bec538939726a628b0e4`. ASan leak checks disabled; UBSan halts. Python is
unchanged and was not repeated.

No stage closes. Automatic World callers, public setup worker barrier, velocity
controllers, actual timing/traversal producers, initial NPC morphology, full
physical save/restart, retained BodyT/floor failures and S5-S14 remain open.
Next: expose selected-controller setup through the existing public scheduler
barrier and verify it against live capsule/body ownership with0/1/2 workers,
then connect admitted setup/update/cleanup boundaries to World reactions.


### Checkpoint203: public selected knockdown setup behind the worker barrier

Checkpoint202 committed as `63869ec0758097d31d935d6bb1c5d4623141c97f`,
199 isolated commits with exact-byte/fresh-clone proof. Bundle180 SHA256
`f736475546ab94545318b5cbc98aaeb5d1e052fe8d7426eb48c2e7166e501946`.
PhysicsSystem::prepareActorRagdollKnockdownBlends delegates to its existing
PhysicsTaskScheduler, which waits for movement workers, takes the collision-world
lock, resolves current physical owner identity and calls the atomic component
setup. It does not reset the shared frame cache, alter physical body pose/motion,
or change capsule ownership during curve preparation. Duration/filter resolution
and complete physical/velocity reaction effects remain caller-owned.

The public aggregate placeholder baseline01 fails stale-owner rejection,
late unknown-node rejection and Started disposition at its first0-worker
iteration; its fatal assertion prevents later1/2-worker iterations, which are
not claimed as baseline coverage. Final normal/sanitized01 each pass all899
engine tests, exact full inventories with no failure/skip, fingerprint `021de1af6f141626b8844b719d4d291707ac01e224f60e45ab463be8ea0f4660`.
ASan leak checks disabled; UBSan halts. The final aggregate exercises0/1/2 workers:
stale identity rejects; a valid first request followed by unknown999 leaves the
one authored key/setup state0; successful setup owns attachment8/two keys/state2,
resets clock sentinel and preserves body pose/cache/capsule suspension; public
updates advance and finish the curve, clear keys/state and return physical motion
to Dynamic while capsule remains suspended until explicit owner removal.

Component/Python implementation is unchanged, so checkpoint202's2,321 component
checks,3,456 exact native setup/update captures and stock18-controller audit
remain applicable without repeated unchanged suites. This uses real public
physics, body and capsule objects with configurable workers; it is not automatic
World reaction orchestration or normal-input game acceptance. No stage closes.
Next: implement independently verified native velocity-controller update,
creation/attachment and force/removal ownership, then complete admitted World
reaction callers and full persistence. Actual timing/traversal producers, initial
NPC morphology, retained BodyT/floor failures and S5-S14 remain open.


### Checkpoint204: native velocity-controller clock and force intent

Checkpoint203 committed as `4b9ebc9329dc1d0f01cf3774ff9dcb8dd26c7b54`,
200 isolated commits with exact-byte/fresh-clone proof. Bundle181 SHA256
`ba840caaf5fccd927111f814f75282ff234d86bb896428234a8ae66d9acf5cd5`.
New ESM4 physicalvelocitycontroller module models full8B8770 controller update
and8B8380 force eligibility/preparation as immutable state and publication intent.
Every update resets raw frame delta to the native A96CFC value0.016, even when
inactive or missing its target. Active targeted updates store nonnegative
input-minus-previous time as delta; sentinel/negative differences retain default,
and repeated time uses zero. The general native shared clock advances independently
of target blend/body availability. A present blend with hierarchy gain strictly
below1 and a physical body yields native force intent100 times each stored vector
component. Completion at stop key clears active directly for any cycle mode;
it does not call Stop, reset clock sentinels, clear stored vector or detach.

The caller resolves identities and stored vector, owns the shared cache and must
apply force using returned delta. No controller creation/attachment/removal,
body activation/force mutation, source vector generation, World reaction,
persistence or physical contacts are implemented by this pure boundary. Validate
finite used inputs/force results and representable deltas; unused vector/time/gain
values on early paths are ignored. Invalid computations cannot mutate input state
or caller cache because results are staged in an owned candidate.

Independent original full8B8770 plus8B8380 runs49,152 captures in12,288 sequences,
both x87 controls, active/inactive and absolute/relative clocks, all four cycles,
reverse, target/blend/body availability, hierarchy0/.5/1/2, frequencies.25/1,
phases-.125/0, zero/.25 stop keys, forward/backward/repeated frames. Actual RTTI
lookup/shared7155A0/CRT clock execute. Windows primitives are boundaries;5377B0
captures native force/delta arguments without applying them. Exactly1,332 force
calls observed. Original executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`, oracle source
`99bc11e3d134d05440ea2061ffd462b0e7da52e476837f07a23c304f57d4806e`, corpus `0df403d25a05d259242295d664bffbf69e5cb81d952b208bf1e8637846799f55`. Final comparator02
matches all49,152 captures and547,332 fields exactly in normal and sanitizer
builds. Six independent paired/observed negative controls distinguish target,
blend/body/hierarchy gates, missing100x force scaling and incorrect Stop reset.
Earlier original01/02 corpora and comparator01 outputs remain retained.

Six baseline tests all fail the placeholder. Eight final regression tests cover
force preparation and non-Stop completion, inactive/missing target default delta,
clock advancement without force target, backwards/equal frames, cyclic finish,
used/unused malformed inputs and overflow, strict1 threshold with adjacent floats,
and first relative-clock initialization. Final normal/sanitized02 each pass all
2,329 component tests, complete matching inventories without failures/skips,
fingerprint `d718d733b7bbd5ac87fd613b225d33f05f07022c6dac42a4b334853ce24be1b0`. ASan leak checks disabled; UBSan halts. Normal/sanitized01
each execute2,329 tests and fail one incorrect extra test expectation that relative
first update retains elapsed/start sentinel. The original captures already match
the implementation: previous-time sentinel resets elapsed, first relative delta
uses input, and missing start origin becomes input. Only that assertion/name was
corrected; both failed directories remain retained. Engine adapters/Python are
unchanged and were not repeated for this pure module; checkpoint203's899 engine
checks remain the latest public integration evidence, not a claim of velocity
controller runtime wiring.

No stage closes. Next: realize native force application and own velocity-controller
creation/attachment/update/removal together with selected blend curves; then join
actual World reaction callers and persistence. Clock/traversal production, initial
NPC morphology, retained BodyT/floor failures and S5-S14 remain open.


### Checkpoint205: native immediate force stores and atomic physical owner batch

Checkpoint204 committed as `517a66df114496f535ee28339bec6c543a26e9b2`,
201 isolated commits with exact-byte/fresh-clone proof. Bundle182 SHA256
`9aecfa308a4d42ed6227b35fd358839617f3253b6909da1781f8e30f12a00721`.
Native inverse-mass preparation models89DA50/89DAC0: zero/signedzero store+0;
finite positive mass stores the native reciprocal; unsupported negatives,
nonfinite or overflowing inverse results reject. Native dynamic linear force
stores frame*force, inverseMass*that product, then current+delta separately as
binary32, matching8EAC80 SSE. It does not combine products, accumulate a Bullet
force, integrate pose, damp/cap or change angular velocity.

ActorRagdollPhysics::applyNativeForces stages a complete sparse identity batch.
Dynamic bodies resolve the reciprocal from their owned original dynamic mass,
convert live linear velocity to native units, prepare all force results, and
convert world lengths once. Keyframed bodies preserve velocity and ignore unused
force/time as the native virtual no-op does. Unknown/duplicate bodies or late
malformed/overflowing dynamic requests reject before any velocity/wake change.
Only after the whole batch validates are owned constraint-connected groups woken
and dynamic linear velocities published. Pose, angular state, modes, body/shape/
constraint identity, accumulated Bullet forces and unselected velocities remain
unchanged. Unconnected sleeping bodies remain asleep; connected peers wake
without gaining the selected body's force. Native contact-island activation
beyond these owned constraint groups is not established or claimed.

Independent complete original5377B0 executes8A6410 and actual dynamic/keyframed
motion virtual+6C8EAC80/8EA060 in1,600 cases with no boundary stubs. body+91=1
explicitly skips unresolved activation; both x87 controls and default MXCSR,
zero/signedzero, five frames, five inverse masses, four force/velocity fixtures.
All other motion bytes including angular velocity remain unchanged. Executable
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`, force source `8e14db8cf3c5578f8080d9a145adea9bb9a446aef04427142da40e7f78f87bc5`,
force corpus `ff7db56cc44697d77f7ea3f0cf4968642e42d41643117701a1cfeda933db447e`. Independent complete89DA50 plus actual
virtual89DAC0 stores inverse mass in4,056 cases: audited18 stock masses and
adjacent values, zero/signedzero, normal/extreme positives and2,000 deterministic
bit patterns, both x87 controls. Mass source `f7cd507bcfbc030bda4ff7b8d18c1bd79908fb5612c0c8de24e252b0055bde3b`, corpus
`41cc2a628f0a2a5b5fbb3d2607ec2a669cd7b7006eefac1bf5ec76ca8998fccb`. Explicit native motion fixtures; these are not full
body-factory/inertia/contact/World acceptance.

Final normal/sanitized comparators each match all5,656 mass/force cases and16,376
fields exactly.1,440 force cases additionally execute actual ActorRagdollPhysics
with native length conversion, verifying XYZ velocity and preserving pose,
angular state/modes, plus zero collision objects between owners. Dynamic inverse0
has no admitted positive-mass graph and remains pure-only; keyframed inverse0
uses an unused positive graph mass because the actual native force branch is a
no-op. Five independent original paired/observed negative controls distinguish
keyframed dispatch, frame/inverse mass, absent force and use of mass as reciprocal.
No production output generates original expectations.

All five initial tests fail the placeholders. Six final regression tests cover
native reciprocal/force stores, malformed input and arithmetic overflow, real
scaled-body immediate velocity with preserved pose/angular/Bullet force/shape,
unclamped force before the later world velocity cap, keyframed ignored inputs
and restored dynamic response, late batch rollback before wake/publication,
connected peer wake and separate unconnected sleeping-body preservation. Final
normal/sanitized01 each pass all2,335 component and899 engine tests, exact full
inventories without failure/skip; fingerprint `fc3992a5e9a86d80fb2d8fa723c56a5a3497ab4d8feecb03fd48e48f79eed623`. ASan leak checks disabled;
UBSan halts. Rebuilt openmw/openmw-tests/esmtool. Python is unchanged and was not
repeated. Sanitizer compiler notes reduced debug variable tracking for the large
public fixture; no compiler warning/error or runtime sanitizer finding.

No stage closes. The physical force boundary now exists for owned velocity
controllers. Public force forwarding, velocity-controller creation/attachment/
update/removal, atomic joined controller/frame publication, actual World reaction
callers and complete persistence remain open. Actual clock/traversal producers,
initial NPC morphology, retained BodyT/floor failures and S5-S14 remain open.
Next: independently execute velocity creation/attachment and source-vector
preparation, own its lifetime and join it to admitted blend/force/frame state.


### Checkpoint206: original velocity-controller creation/reuse and stored-vector rules

Checkpoint205 committed as `70dba23ceead56d8605c2945fd33f4ee5cb8f704`,
202 isolated commits with exact-byte/fresh-clone proof. Bundle183 SHA256
`209e87216656b006f91493a235c52c5c0c634e8f106bc55aba9c7f46501daf22`.
Complete original8B8590 executes actual NiTimeController constructor715990,
SetTarget715CE0 and attached-list prepend6FFE60 for a fresh allocated controller;
existing controllers retain null/self/other targets without retargeting. Generic
collision47FAC0 admits nonblend collision objects too. Actual535AC0 reads stored
mass through8A98D0 and89DA90, using archived motion inverse mass for keyframed
current motion, while linear damping comes from current motion+C8. The stored
vector rounds mass*source and (.75*linearDamping)*source separately before adding.
No collision/wrapper copies the source unchanged. Frequency1, phase0, bounds
0/duration, flags(old&FFF5)|D, sentinel Start resets with elapsed/delta retained
on reuse; fresh elapsed/delta0. New controller prepends; existing list is retained.

Original01 retains5,184 cases. Final02 expands to10,368 cases with fractional
and extreme finite vector components, both x87 controls, fresh and three reused
target identities, collision missing/object-without-wrapper/body, dynamic and
keyframed archived mass, three inverse masses/damping values/durations, old1D/
FFFF flags and four vectors. Executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`,
source `f76efd4687ce5ccca20ee75fd962b750cfe8f5189bbcb51c8144d6d0b23b232e`, corpus `127f951a6205820dbfe54c4fa1056c1c1710dcf022d21ccbbeb23bb6e9287819`.
Only fresh allocation401E20 and Windows critical/interlocked primitives are
stubbed; constructor, attachment, lookup, mass/damping and Start run original
instructions. These synthetic original scenes do not establish later force,
activation, full Down recursion, World or normal gameplay acceptance.

preparePhysicalVelocityController now models the independently verified finite
nonnegative Down setup boundary. Its immutable result retains prior elapsed/
delta and unmasked flags; missing prior state initializes native constructor
flags/delta. Caller supplies resolved current/archived inverse mass and current
linear damping. Missing bodies ignore unused coefficients. Used invalid duration,
vector, mass/damping or arithmetic overflow rejects without modifying prior
state. Target/list ownership remains with caller and is not fabricated as a NIF
record identity. Four placeholder tests fail; final tests cover creation,
reuse, weighted/zero mass and malformed/overflow rollback. All2,339 component
tests pass in full normal/sanitized01 inventories without failure/skip. Normal
and ASan/UBSan comparators each match134,784 exact captured fields in10,368 cases.
Eight independent negative controls distinguish wrong attachment/target, reset
clocks, missing-body weighting, unweighted body source, omitted damping and
combined coefficient (288 original cases reject that rounding shortcut).
Tested fingerprint `a3b34e6f605af23e140054a093f480c6a6ca9ad06f9d9a90c2e01f73dbebe8fc`. No compiler warning/error or sanitizer finding;
ASan leak checks disabled. Engine/Python sources unchanged and not repeated.

No stage closes. Owned velocity creation/reuse/list lifetime and removal, joined
atomic blend/force/frame publication, automatic World reaction/traversal/clock
producers and complete persistence remain open, as do initial NPC morphology,
retained BodyT/floor failures and S5-S14. Next: use the proven setup rules in the
physical owner, then integrate controller order, force and removal atomically.


### Checkpoint207: owned generated velocity-controller setup, identity and attachment order

Checkpoint206 committed as `74fd7b9aa0928c3fd9c268288786b1b07f346f00`,
203 isolated commits with exact-byte/fresh-clone proof. Bundle184 SHA256
`cc58581e57dcae51e4c61d2bb223a607045bdd4afaf14e7688ab4b75c2c331ed`.
ActorRagdollPhysics owns generated velocity-controller state separately from
selected authored blend controllers. Generated identity is the unique owned
attachment node, with a typed Blend/Velocity reference; no authored NIF record
is fabricated. Atomic setup reads inverse mass from owned dynamic mass (the
native archive source even while keyframed), current loaded linear damping,
and invokes verified206 setup. New controllers target their attachment node
and prepend ahead of the selected blend; reuse retains target and list position.
An entire late-invalid request batch rejects before replacing controllers.
Setup does not mutate body pose, velocity, modes, activation or shared time cache.
Nonblend collision bodies admit setup independently of blend metadata.

Capture/restore of the generated state validates unique unambiguous owned
attachment/target identities, finite clocks/delta/vector and ordered timing
before atomic replacement; null and redirected owned targets are preserved.
This is a component restoration boundary, not a complete serialized actor save.
Selected-controller order consumes explicit caller node traversal and retains
velocity-before/after-blend position. Missing/duplicate/ambiguous nodes reject.
Unrelated renderer controllers and original recursive scene traversal are not
included or inferred from graph body order.

Five placeholder tests fail. Five final tests cover nonblend/keyframed archived
mass with current damping, retained redirected target/clock/list order on reuse,
late vector/duration/identity batch rollback without wake, restore malformed
state/identity rollback and null target, fresh prepend and ambiguous nodes.
Initial normal/sanitized01 each run2,344 component tests with one retained
fixture failure: keyframed DISABLE_DEACTIVATION cannot be replaced by ordinary
setActivationState(ISLAND_SLEEPING). Corrected the test to compare activation
before/after setup; implementation remained unchanged. Comparators01 already
match the original. Final02 normal/ASan+UBSan each pass all2,344 component and
899 engine tests, exact inventories without failure/skip; rebuilt engine/tools.
Final comparators02 each execute3,456 body-present original206 cases and match
58,752 exact fields: timing/flags/clocks/delta/vector, target and selected-list
position. Generated-owner count matches original allocation intent, not the
native allocator implementation. Validate the derived native inverse mass
before every owned case; preserve body poses/velocities/modes and leave zero
registered objects after each owner. Original corpus remains206 oracle02,
`127f951a6205820dbfe54c4fa1056c1c1710dcf022d21ccbbeb23bb6e9287819`.
Tested fingerprint `db4a30f685f81412da512738922926b4cadc86620e318b695e8c3d46f99ae535`. No compiler warning/error or sanitizer finding;
ASan leak checks disabled. Python unchanged, not repeated.

No stage closes. Owned controller advancement, target-node removal, atomic
force/clock/gain publication and public worker barriers remain open. Full scene/
collision frame ordering and World lifecycle/traversal/clock producers require
independent integration; component state restoration is not full save/restart.
Initial NPC morphology, retained BodyT/floor failures and S5-S14 remain open.
Next: join selected physical-controller updates in verified original47C930 order
with actual owned immediate force stores and blend-driven velocity removal.


### Checkpoint208: joined selected-controller clocks, gains, force and removal

Checkpoint207 committed as `a768301b61d07d797a04c23881d6aa98f3be911a`,
204 isolated commits with exact-byte/fresh-clone proof. Bundle185 SHA256
`1ecc40d1249d9b69aa986cc7fbdff9ddb59aecdbed6eedd7632fba300b2292dc`.
advanceNativePhysicalControllers admits a complete unique typed controller order
before staging owned blend/velocity controllers, target gains and shared cache.
Advance in caller order; velocity sees gains from earlier updates. Blend finish
removes the velocity attached to its TARGET node, independently of the blend's
attachment. Original47C930 reads the next link after each Update, so a removed
later velocity is skipped; an earlier velocity's already-prepared force remains.
All immediate force results validate before physical velocity/wake publication;
then controller/gain/cache candidates swap together. Late input, order or force
failure rolls back all of them. Distinct redirected controllers may apply forces
to one body sequentially, preserving each native binary32 store; the explicit
public sparse applyNativeForces still rejects duplicate body requests. Preserve
pose/angular/modes/flags/shape, and do not run collision/renderer pose publication.
Owned constraint-connected activation scope remains unchanged from205.

Independent full original47C930 executes actual blend8AAD60 and velocity8B8770,
shared7155A0 clock, direct5377B0/8A6410/8EAC80 dynamic or8EA060 keyframed force
and6FFE90 velocity detach. No force boundary stub. Original body+91=1 explicitly
skips unresolved activation; Windows critical/interlocked primitives only.
Both x87 controls/default MXCSR, two attachment orders, both motion modes,
three initial H gains, D/CD/1CD blend flags, D/1D velocity flags and forward/
backward/equal sequences. Final oracle03 includes actual per-insertion8AABE0
key bounds before controller traversal:1,152 captures/288 four-frame groups.
Executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`, source `1882e5d9e47fbaa9a59e6959507cf745894e01f3d410a009a13aa8e021a71aa9`,
corpus `a8d7623311cfcb5b3ef6912ac4c0b504f1ea4c32c69bcbb400a10042df87b76e`. Original01/02 manually prepared stop.25;
these remain valid separately prepared-bound observations. Comparators01 against
those fixtures retain1,152 mismatches each because owned graph admission applies
verified per-insertion bounds, producing stop0. No production bounds/clock rule
was changed to fit them. Final03 original uses the same admission boundary.

Five placeholder tests fail. Seven final tests cover actual scaled force and
clock/gains without pose publication, removal before force vs retained earlier
force, late overflow/order rollback before any wake/cache/clocks, keyframed force
no-op with clock advance, redirected target-node removal and sequential forces
from two controllers targeting one body. Initial full normal/sanitized01 each
run2,351 component tests with one retained assertion failure: the two-key fixture's
native per-insertion stop0 immediately calls Stop, leaving the previous sentinel.
Use a single-key.25 fixture for the intended advancing-clock assertion; production
implementation unchanged. Final02 both builds pass all2,351 component and899
engine tests, exact full inventories without failure/skip; engine/tools rebuilt.
Final comparators02 each match27,648 fields exactly in1,152 original03 captures:
blend clock/flags/keys/cursor/cache/setup/gains, velocity presence/clock/flags/
delta/vector, dynamic/keyframed actual owned XYZ velocity and shared time cache.
Raw motion W is compared only in the zero-W fixture; full raw-W persistence is
not established. Preserve pose/angular/modes and zero objects after owners end.
Five final original controls reject omitted force256, wrong keyframed dispatch256,
ignored order248, retained detached velocity768 and reset completion clock264.
Controls01 retain an unobservable-control failure (identical stop bounds);02
replace that control with an observable retained-clock prediction;03 passes the
final initialization corpus. No production expectations generate originals.

Tested fingerprint `a117f886108cda4b07d1287823ec6959dfb8725d6f4c937b5a13367205749c4e`. No compiler warning/error or sanitizer finding;
ASan leak checks disabled. The sanitizer compiler notes a debug variable-tracking
budget retry for the large public fixture, not a runtime sanitizer issue. Python
unchanged, not repeated. No stage closes: public controller/force worker barriers,
atomic complete scene/collision frame integration and automatic World reaction/
traversal/clock producers remain open. Initial NPC morphology, full native state
persistence, retained BodyT/floor failures and S5-S14 remain open. Next: public
worker-serialized ownership/phase/force APIs and real0/1/2-worker owner checks.


### Checkpoint209: public worker barriers for generated controllers and native forces

Checkpoint208 committed as `83e0d9091e35fd6ef62835761e89a9fb9ac7998c`,
205 isolated commits with exact-byte/fresh-clone proof. Bundle186 SHA256
`653a741a2aaa7f84ec51ab6d6539a44e72b9b64075cd02bec9f6b914a9580791`.
PhysicsSystem/PhysicsTaskScheduler expose generated velocity setup/capture/
restore, selected typed controller order, joined physical-controller advancement
and explicit native force batches. Every scheduler entry waits for movement
workers, holds the existing world lock, resolves the current owned actor and
uses the scheduler-owned shared physical time cache for joined advancement.
Stale/removed pointers reject consistently before component state mutation.
These are the verified component boundaries from205/207/208, not yet automatic
World reactions or a complete renderer/collision frame transaction.

The real public ownership aggregate runs with0/1/2 threads. All six stale-owner
paths reject. Late-invalid setup leaves no generated controller; valid setup
resolves owned mass and creates the head velocity before selected blend without
changing cache. Round-trip null target and retained elapsed/delta, then reuse
without retargeting; invalid restore preserves prior target. Late explicit-force
identity rejection preserves velocity, successful force stores native immediate
velocity while preserving pose. Bad typed order preserves clock; joined update
publishes first native default-delta force, then completes velocity at stop.
A subsequent zero-duration blend setup removes that stopped velocity. Actor
capsule remains suspended until explicit owner removal. Existing queued-worker,
owner remap, shared-cache, renderer/lifetime and snapshot checks remain in the
same aggregate and all three thread configurations complete.

Placeholder baseline01 executes the aggregate's first0-thread iteration and
fails stale-owner, setup and ownership assertions; its fatal empty-controller
assertion prevents reaching1/2, which are not claimed as baseline runs. Final
full normal/ASan+UBSan01 each pass899 engine tests, exact full inventories with
no failure/skip, rebuilding openmw/openmw-tests/esmtool. Fingerprint `d5ddc7da9801675896c6daf46ce4786e84e858d9f074a24b1a230ebe0f3febde`.
Components/Python and native formulas unchanged, not repeated;208 full2,351
component inventories and exact native corpus comparisons remain the relevant
component verification. ASan leak checks disabled. The sanitizer rebuild exposes
an existing range-loop-copy warning in testoblivionactorstats.cpp:6843 (unchanged
by this chunk); no new compiler error or runtime sanitizer finding. Compiler
warnings are not claimed absent.

The public fixture repeatedly exceeded GCC debug variable-assignment tracking
and retried the whole large function. Apply existing teststore.cpp source option
-fno-var-tracking-assignments also to testoblivionworld.cpp, scoped to GCC engine
tests. Actual generated normal/sanitizer flags retain -g and the sanitizer build
retains address/undefined instrumentation and frame pointers. No variable-tracking
retry appears for that fixture in these final builds; no matched compile-time
benchmark or game performance improvement is claimed. Preserve all other cached
build options and fetched double-precision Bullet configuration.

No stage closes. Automatic World begin/update/end, full renderer/collision frame
ordering/atomicity, normal knockdown composition and requested-motion handling,
complete physical/controller/contact persistence and initial NPC morphology
remain open. Retained BodyT/floor failures and S5-S14 remain open. Independent
work on full8AB440 normal leaf now establishes that an existing attached velocity
is skipped entirely, while a new one uses its own compiled1.2-second duration
and vector preparation, independently of body Down curve duration. The public
setup is generic8B8590; it does not silently impose that higher-level entry rule.
Next: compound the normal controller setup from the full original leaf captures,
then finish scene/World lifecycle and persistence integration.


### Checkpoint210: compound normal Down setup with resolved pass-out settings

Checkpoint209 committed as `f8161fe7bc14bd014193ccc71fa0887e0739db27`,
206 isolated commits, exact-byte and fresh-clone proof. Bundle187 SHA256
`4d095056de2d4292140f63e4ee88972af2319cd7e9fe3e92ad52adb2b9a990b7`.
ActorRagdollPhysics now stages normal selected blend setup and optional new
velocity attachment together. It preserves the existing blend-only boundary.
Missing blend/controller and negative body Down duration skip unused inputs;
unknown/ambiguous/duplicate nodes and late-invalid used inputs roll back both
controller collections before wake or body mutation. Existing attached velocity
is completely untouched, including target, list position, flags, clocks, vector
and frame delta. New velocity targets its attachment node and precedes blend.

Important correction to209's preliminary 'compiled1.2' claim: the full original
initializer53AC14..53AC2C copies configured DEFAULT fPassOutForce/fPassOutTime
from B11C0C/B11C04 to cached B2EC5C/B2EC60. The initial PE cache contains20000,
but the identified setting default is -10; initial cached data is not initialized
runtime configuration. The API therefore requires caller-resolved force/time,
with finite signed force and supported nonnegative time, validated only for new
velocity creation. Actual configuration import/winning override remains open.
Body curve duration and pass-out velocity duration are independent, including
zero. Original4707B0 stores each world XYZ product as binary32, then4529E0
stores conversion with the separate .1428767293691635 constant. Generic8B8590
mass/damping preparation follows. Source W retains the second blend key time
from the native stack scratch, rather than source W or zero.

The independent oracle04 executes the actual settings-copy prefix then full
8AB440 normal blend leaf, including actual controller construction, lookup,
attachment, owned-mass/damping preparation, Start and security-cookie return.
15,360 cases cover both x87 control words, four existing-target states, body
Down -1/0/.25/1, controller flags, two source vectors, two inverse masses/two
damping values, configured force -10/0/.125/1/20000 and time0/.25/1.2/2.
Only Windows critical-section/interlocked primitives and three allocation/free
boundaries are stubbed. Executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`;
oracle source `943099a2fc2df423367ecb3cc544171cfaa31c40f2bc65a89f186797bb428c48`,
corpus `8a2f44c9943caa07651b7084674d5b93cf3e1fb77ab5cf26cc1c9b75e39d4463`.
The native stored requested-motion field is0 here, explicitly skipping conversion.
88F040 uses that collision field and actual motion type; Down does not establish
an unconditional Dynamic conversion. Immediate/nonblend/child recursion and
force/activation/contact/World remain separate unverified boundaries.

Initial-cache oracle01/02/03 captures remain evidence, not initialized-default
proof.01's key-field read offsets were corrected in02.03 uses finite ordered
existing-controller bounds admitted by owned restore (0/6 instead of unused
native malformed7/6). Initial normal/sanitizer01 each passed2,356 components and
899 engine tests but did not cover settings initialization; they do not close
that gap. Placeholder baseline01 failed all five initial regression tests.
Final02 adds explicit configured settings and signed/zero/fractional force/time
cases, unused invalid settings, used invalid/overflow rollback; each normal and
ASan+UBSan run passes2,357 components and899 engine tests, exact full inventories
with no failures/skips. ASan leak detection disabled. Fingerprint `84da792c7d64d3d0c2622faf14338e0b69e82e6491c395d5f71f7cd42376d73d`.
Each native comparator02 passes15,360 cases/528,960 exact fields, resolving native
inverse mass from actual owned mass before every case. Each legacy blend-only
comparator04 passes3,456 cases/62,496 exact fields after the common refactor.
Eleven independent negative controls reject incorrect constants, reused-velocity
restart, skipped blend setup, duration coupling, missing source conversion,
zeroed fourth lane, wrong insertion/target and disabled/zero body behavior.

No stage closes. Automatic World reactions, configured settings production,
stored requested-motion synchronization, full scene/physical ordering and atomic
publication, complete physical/controller/contact persistence and initial NPC
morphology remain open. Retained BodyT/floor failures and all S5-S14 gates remain
open. Next expose this compound setup through public worker barriers, then
continue scene/World lifecycle and persistence integration.


### Checkpoint211: public atomic normal Down controller setup

Checkpoint210 committed as `4dcec1839bef6d798b830516b9f13d0b10e4e099`,
207 isolated commits with exact-byte/fresh-clone proof. Bundle188 SHA256
`1522a60798baaebacedc06fdd72bb192cbbf756cf8e3416b0b0ae488716f468e`.
PhysicsSystem and PhysicsTaskScheduler now expose compound normal Down setup
with explicit resolved pass-out settings. The scheduler waits for queued movement
workers, holds the existing collision world lock and resolves the current actor
owner before forwarding the atomic210 component operation. The caller still
resolves per-body Down duration, configured force/time and stored requested-motion
synchronization; automatic World reaction dispatch is not claimed.

The real public ownership aggregate runs all0/1/2-thread configurations. Stale
owner rejects; invalid force and late unknown node preserve empty velocity and
reset blend state. Successful signed force-10/time2/body duration.25 creates
velocity before blend with self target and original-oracle04 exact four-lane
mass2/damping0 vector bits. Pose/shared time cache remain unchanged during setup.
Restore a null target and retained elapsed7/frame delta99, then repeat compound
setup with invalid unused settings: existing velocity vector/target/duration/
clocks remain untouched while the blend curve restarts. Restore the self target
and advance through the actual public joined controller phase: first frame stays
active; completing the body Down curve removes the velocity before its own2-second
stop and clears raw blend setup state. Capsule stays suspended until removal.
Existing queued-worker/remap/shared-cache/renderer/lifetime/snapshot checks run
in the same aggregate in all three configurations.

Placeholder baseline01 runs the first0-thread iteration and fails invalid-force,
late-node and missing-velocity assertions; the fatal size assertion prevents
later1/2 iterations, which are not claimed as baseline coverage. Final full
normal/ASan+UBSan01 each pass899 engine tests, exact inventories with no skips or
failures, rebuilding openmw/openmw-tests/esmtool. Fingerprint `2db9dfffe62f6dae13ad4877141a0d0774e0281680ae3c5c633dcb384b83bd70`.
ASan leak detection disabled. Component/Python/native arithmetic unchanged;
210's2,357 component inventories and configured full-leaf/legacy exact comparisons
remain the applicable verification rather than repeating unchanged checks.

No stage closes. Automatic World lifecycle, configured settings production,
requested-motion ownership/synchronization, recursive scene/controller/collision
ordering and complete atomic publication/persistence remain open. Retained
BodyT/floor failures and S5-S14 remain open. Continue those integration boundaries
before claiming normal-input knockdown or physical restart acceptance.


### Checkpoint212: native initial collision constructor and post-link state

Checkpoint211 committed as `f60f9253e7ae1aeaa3dd9b59209d036d45fe6ea2`,
208 isolated commits, exact-byte/fresh-clone proof. Bundle189 SHA256
`e720c555e312f556981d6b42f1fcf4845425fdd6000b0011c62816b675be3761`.
Immutable PhysicalBlendCollisionState models actual88EB60 constructor flags41,
gains0/1 and requested-motion8. resolvePhysicalBlendCollisionAfterLink models
88ECD0's gain/flag result after caller-resolved ownership: OR flag8, replace both
authored gains from resolved table[(packedFilter>>8)&31], or table0 when wrapper/
body is absent. Preserve requested motion. Finite gains remain unclamped and
signed zeros remain exact. Validate only selected table gains; unused authored
and other table entries do not affect the result. Input state/table are immutable.
InitialPhysicalBlendGainTable records the pinned PE's32 pairs1/1; this is initial
data, not proof of winning runtime settings. Caller supplies the resolved table.

Original88F040 reads requested motion separately from actual body motion; its
constructor value8 does not request conversion. Full88ECD0 is the original
post-load virtual slot20, not a guessed body setter. It executes actual inherited
link resolution, scene flag update, body assignment/refcounts and collision-owner
property insertion before overwriting authored gains. C++ helper deliberately
returns only gain/flag/requested-motion state; those ownership/scene operations
remain explicit caller boundaries, not simulated C++ runtime acceptance.

Independent oracle01 captures13,824 full constructor88EB60/load88F2D0/link88ECD0
and initial88F040 skip cases: both x87 words, scene present/absent, wrapper/body
presence, all32 body IDs, four flag words, three authored gain pairs and three
resolved tables including signed zero and unclamped values. Actual property
insertion uses preallocated capacity; no allocator/free/destructor or game-function
stub. Only resolved stream fixture reads and Windows critical-section/interlocked
primitives are stubbed. Native stream+4=8 is a declared fixture, not yet a proved
header-field producer or winning configuration. Oracle03 adds1,920 full load/link
captures with requested-motion0/1/6/8/FFFFFFFF supplied after construction and
retained by link; it does not call88F040 on those changed states.
Executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Oracle01 source/corpus:
`b2540243f987d3e1f7b1acac486d0243ef2b13d92c93209c87d182fac1a7d166`,
`3c1ad661bc8a27e4ad193d1c0619b459492276c61b663450007b71ece10f9b2e`.
Oracle03 source/corpus:
`c71f6691f0015c42a6d2d937d6cacf01723f8aff54d3174098576717bf0f240b`,
`2ca320127007c892941fb65f7790fdf3a011574c513d63da8f096baca0a24cdf`.

Both normal/sanitizer comparator01 match15,744 cases/125,952 exact constructor/
post-link fields, also asserting loaded input remains unchanged. Eight independent
controls reject retained authored gains, omitted flag8, requested reset, table0
for every body, wrong absent-body selection, clamping, discarded negative zero
and a Dynamic constructor request. Four component regressions cover constructor,
all masked filters, missing-body/unused-input paths and selected malformed gains.
Placeholder baseline passes constructor and fails the other three. Final full
normal/ASan+UBSan01 each pass2,361 component tests, exact full inventory, no
failure/skip. Fingerprint `7d80cdfea6a4f6e66e6b6b21f372a478956e32f4d73fefa83cc631fc387cd66f`. ASan leak checks disabled. Engine/Python
unchanged;211's full899 engine inventories remain applicable, not repeated.

Additional oracle02 has384 full-load/link captures with declared stream+4 values
0/7/8/11 and file-version fields20.0.0.4/5. Original raw-load branch reads its two
extra floats strictly below8. Local raw decoder currently uses BethVersion<=8;
no parser change is inferred until the original stream+4 producer/semantic identity
is established. This retained discovery is not described as a passed parser
comparison. Raw authored decoding remains distinct from linked runtime gains.

No stage closes. Runtime owner/World initialization from resolved gains, requested-
motion persistence/synchronization, configuration producer, initial morphology,
full recursive frame ordering/atomicity and physical restart remain open. Retained
BodyT/floor failures and S5-S14 remain open. Next integrate this initial state into
the renderer/physics lifetime boundary and retain requested motion during owned
collision updates before completing automatic World reactions.


### Checkpoint213: resolved collision state at physical-pose admission

Checkpoint212 committed as `ec0f605adaee86957a4f8c0c7446a42f24ad179c`,
209 isolated commits with exact-byte/fresh-clone proof. Bundle190 SHA256
`36ba46ba5d85ab169354f4cec136cf74b6168f37db248affa40ca8e366b4f7da`.
ragdollDefinitionWithNativeLinkedBlendState copies the authored graph and applies
212's linked flags/gains using an explicitly supplied resolved packed-filter
array and gain table. Require a complete filter array; validate only selected
blend gains. Preserve raw source identity, shapes/body properties/controllers
and unrelated nonblend bodies. The prepared graph remains separate from raw
NIF data and caller-resolved native filtering; no inferred Info/World filter
substitution or winning configuration is claimed.

beginNativeActorPhysicalPose now requires those resolved filters/gains and
prepares linked state before native property scaling or animation detachment.
Invalid used gain input rejects before callbacks, owner creation or capsule
suspension. Valid prepared graph feeds the existing joined renderer/physics
admission and cleanup boundary. These free helpers are compiled/tested but are
still not automatically invoked by World reactions. Uniform placement/morphology,
configuration and filter production remain explicit caller responsibilities.

Owned blend-state capture adds requested-motion, initialized8 independently of
actual Dynamic body mode, as in88EB60. Owned controller/collision frame updates
stage the selected native requested motion1/6 before the scene hook and publish
it with clocks/gains/flags after physical preparation succeeds. Selector skips
preserve the old request, and a failing scene hook preserves request and actual
body motion. The original88F484 compares collision+1C with the selected mode;
88F4C5 stores it after conversion. This chunk tracks that metadata using157's
independently verified selector; full mode-conversion coupling when requested
and actual body modes disagree, legacy manual body-update reconciliation,88F040
pre-knockdown synchronization and requested-motion persistence remain open.

Three component regressions verify selected resolved-filter preparation, source
property/controller preservation, invalid/unused table entries and pre-admission
world cleanup. Down captures the resolved gains. A separate owned frame test
checks skipped request8, throwing-scene rollback, Key request6 and completed Down
request1 with corresponding body modes. Real public renderer/physics aggregate
runs0/1/2 threads: bad selected gain causes no rebuild/owner/capsule change;
an unused NaN table entry is ignored; valid part17 gains.53125/.875 and flags9
are captured after scaled admission, with raw graph flags1/gains.9/.8 preserved.
Existing mass4/radius1, placementX102, duplicate/bad-inertia, repeat/end-failure,
worker/owner-remap/controller/force and cleanup checks remain and all configurations
complete. Caller-supplied filter17/settings values are an independent fixture,
not a live winning-data producer.

Placeholder baseline01 failed compilation because four new test calls omitted
required native gravity. Test-only correction retains that failed evidence.
Baseline02 then runs/fails all three new component tests against no-op graph
preparation and absent requested-motion tracking. No engine baseline is claimed.
Final full normal/ASan+UBSan01 each pass2,364 component and899 engine tests,
exact full inventories with no failure/skip. Fingerprint `908bed9b50bf57229170006bac9c1f99350958367741b8c7a0aaad45a8099140`. ASan leak
checks disabled. Python unchanged. Each actual linked-graph/ActorRagdollPhysics
admission comparator matches4,608 body-present oracle212 cases/18,432 exact
flags/gains/requested fields. It also preserves raw source identity/properties,
body pose/mode/velocities, empty controllers and zero world objects at destruction.
The original ownership/refcount/scene operations remain original-only observations,
not inferred C++ runtime acceptance.

A follow-up original converter audit executes200 full Dynamic-type2 -> Key6 ->
archived-type2 transitions with only allocator/free primitives supplied. Current
Key motion+C8/CC are+0, while archived dynamic coefficients are retained for
restoration. Oracle S4/native-keyframed-current-coefficients-oracle-01 source
`e457f95ed8aee0214dd9eae165ef9970b47c28aebe5c13c410b554ed211e0b6e`,
corpus `2e6dc79722b77cb0ddfdbc2ed80f1c8d06e0f4a5d23256b97e946c7fb604a8df`.
207's generated-controller owner still supplies loaded Dynamic linear damping
after Key conversion;206's pure helper explicitly accepts resolved current damping.
Correct this producer boundary next using original conversion plus full controller
creation captures. The passing explicit-input arithmetic captures do not close
that owner-mode gap.

No stage closes. Automatic World reactions, current coefficient production,
stored-mode conversion/synchronization, recursive scene/controller/physical
ordering and atomic publication, full physical/contact/controller save/restart,
configuration and initial NPC morphology remain open. Retained BodyT/floor failures
and S5-S14 remain open; normal-input physical acceptance is not claimed.


### Checkpoint214: current-motion damping at generated-controller setup

Checkpoint213 committed as `ff384797491c337c59dcbbe9e2c9b60f3f019928`,
210 isolated commits. Bundle191 SHA256
`5965197908928df57fb22f64cea0c3176afe49389422d0c5eed6013eb25ca1a2`.
The original generic8B8590 reads current motion+C8 damping. Actual8CBC60
Dynamic-type2 -> KEY6 construction zeros KEY+C0/C4/C8/CC; requested Dynamic1
restores the archived actual type2 and its loaded coefficients. The owned generic
and compound normalDown producers incorrectly supplied archived Dynamic damping
while keyframed. Both now select current owned motion: zero for KEY, loaded
coefficient for Dynamic. Dynamic archived mass remains the mass source for both
modes. Explicit-input pure206 arithmetic remains unchanged and valid. This
coefficient accessor models this owner's conversion lifecycle; it does not claim
arbitrary external native KEY coefficient mutation or setter support.

Two new regressions cover generic reuse through Dynamic/KEY/Dynamic with loaded
linear damping.1, null target, elapsed7, delta99 and list position preserved;
and configured normalDown while KEY, including four original captured vector
lanes, independent velocity duration and existing-velocity skip. Setup preserves
pose, velocity, activation and mode. An older KEY test expected archived damping;
its expected vector is corrected to the actual converted-KEY producer result.
Baseline01 runs both new tests and both fail. A quoting error in the private edit
command prevented its mutation; full normal/sanitized01 therefore retain the
unfixed producer and fail exactly the two new tests and corrected older test.
After applying the correction, final full normal/sanitized02 each pass2,366
component and899 engine tests, exact inventories, no failures/skips. Tested
fingerprint `a863eedfb29636d19d2c3ab7a20e22d6ab35f3f8c723137a483f632fdfaab4a6`. ASan leak checks disabled; unchanged Python not repeated.

Original keyframed coefficient oracle01 executes200 constructor/archive/restore
cases, only native allocator/free primitives supplied. Source SHA256
`e457f95ed8aee0214dd9eae165ef9970b47c28aebe5c13c410b554ed211e0b6e`,
corpus `2e6dc79722b77cb0ddfdbc2ed80f1c8d06e0f4a5d23256b97e946c7fb604a8df`.
The historical dynamic_inverse_mass_inertia_bits label includes damping C8/CC;
derived Dynamic inertia occupies F0/F4/F8. It is not a new inertia finding.
Original current-motion generic oracle03 executes1,728 complete conversion +
8B8590 setup cases (both x87 words, new/existing null/self/other targets, source
vectors, inverse mass, loaded damping, duration and flags). Source SHA256
`5d32e3f2b60fa91bb13694ebe907efa7eb1ce768515e5002722a7b9beb3b9c5c`,
corpus `68a740a9e21940390a4c8ff542f0a9ef7c405ed8ad14df69f81d44d2804cd59b`.
Failed generic01/02 retain a harness stack-reset error after conversion;03
resets ESP before each cdecl call. These failures establish no production rule.
Original current-motion fullDown oracle01 executes9,216 actual conversion ->
53AC14..53AC2C settings-copy -> complete normal8AB440 leaf cases. Source SHA256
`b3739c83ba2b6375be02eec6e89681d8504f892a0e7ca42dd18402ecfa2eed42`,
corpus `7401866dd7c0e6dd7c7a45ea24bead80f9dc72491c7640f26d616e05c6332ca4`.
All probes verify original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Windows primitives, native HK allocation/free and Ni key/controller allocations
are declared boundaries; actual constructors/lookup/attachment/Start/mass and
damping instructions execute. Stored collision requested-motion0 explicitly
skips88F040 conversion; preprepared current KEY is not full requested-mode sync.

Actual owned normal/sanitized comparators each match1,728 generic cases/29,376
exact fields and9,216 normalDown cases/317,376 exact fields. They check current
mode conversion/restoration, archived mass, all controller clock/vector fields,
null/self/other targets and ordering, untouched existingDown state, body
pose/velocities/modes and destruction cleanup. Generated-owner count compares
allocation intent, not native heap behavior. Eight independent captured-pair
controls reject KEY archived damping, lost restored Dynamic damping, zero-current
KEY mass substitution and existingDown refresh. The original normalDown probe
still excludes stored requested-mode conversion, immediate/nonblend/child
traversal, force update, activation/contact and World/game acceptance.

Evidence under S4: native-keyframed-current-coefficients-oracle-01;
native-current-motion-velocity-creation-oracle-01/02/03;
native-current-motion-full-down-oracle-01; current-motion-damping-baseline-01;
current-motion-damping-normal/sanitized-01/02;
native-current-motion-velocity/full-down-compare-normal/sanitized-01;
current-motion-damping-controls-01. No stage closes. Next: couple stored requested
collision motion with actual conversion and pre-knockdown synchronization;
automatic World lifecycle, recursive ordering, complete physical/controller/
contact persistence and retained BodyT/floor failures remain open. S5-S14 remain
pending; normal-input gameplay acceptance is not claimed.


### Checkpoint215: stored requested-motion synchronization before normal Down

Checkpoint214 committed as `dbd4a9334dc494f6b98770378c37260092034646`,
211 isolated commits, bundle192 SHA256
`a911b243f14c2ec5f12273dd2bd92643eb134fdd4b90b1af2eb2c3cd8ea0d188`.
Original88F040 reads collision+1C: requested6 invokes wrapper conversion when
actual native motion is not6; requested1 when actual native type is signed>=6;
other stored values skip. Missing wrapper skips; missing body invokes setter
which cannot create that body. Owned bodies admit Dynamic/KEY modes with positive
archived Dynamic mass, so1/6 select Dynamic/KEY and only mismatched modes convert.
This is actual pre-knockdown synchronization, distinct from the88F484 per-frame
comparison of stored request with a newly selected request. That latter coupling
remains open. Original Dynamic fixture type2 is mapped to logical Dynamic1;
no actual native-type1 restoration is claimed.

The common normalDown setup stages this conversion immediately after resolving
the blend target, before controller lookup and finite/disabled duration handling.
Missing controllers and negative durations still synchronize, but do not create
controllers or consume unused source/settings. Stored request, collision flags,
pose and velocities are preserved. New velocity preparation reads the staged
current mode's damping before conversion publishes. All request validation and
controller allocations precede body mode publication, so a late unknown node,
invalid settings/vector or invalid duration leaves every body/controller intact.
Dynamic conversion uses the existing owned-group activation boundary; native
borrowed World/contact island effects remain excluded. Legacy blend-only normal
setup now also synchronizes stored request, as the complete original entry does.
Its old header comment predates this admitted motion effect and will be updated
with the next frame integration change; it does not override runtime evidence.

restoreNativeBlendStates restores complete raw collision metadata by body ID,
requiring every owned target exactly once and finite gains. It preserves arbitrary
uint32 requested values, unclamped finite/signed-zero gains and all flag bits.
Cloned targets publish only after complete validation. Actual body modes, pose,
velocities, controller attachment/clocks and scene remain separate. This is an
owned metadata restore boundary, not automatic World save/load or full physical
persistence. Three regressions cover request1/6 conversion before missing or
disabled controllers, staged KEY coefficient/vector with late-batch rollback,
and complete restore/duplicate/unknown/nonfinite/incomplete rejection. Baseline01
fails all three against no-op restoration and absent synchronization. Final full
normal/sanitized01 each pass2,369 component and899 engine tests, exact inventories,
no failures/skips. Tested fingerprint `4daa14225c89c53f46d09d6c19fb11e25e4951cdcfdc82257d0c8e8695f6cca9`. ASan leaks disabled; unchanged
Python not repeated.

Full original normalDown oracle01 executes46,080 cases across both x87 words,
actual Dynamic/KEY/restoredDynamic and independent requests0/1/6/8/FFFFFFFF,
new or null/self/other-target existing velocity, disabled/zero/positive durations,
blend flags, world vectors, inverse mass/damping and configured force/time.
Actual88F040 -> wrapper8A3420 ->8A9AB0 ->8CD4E0 ->8CBC60 conversion executes,
including archive restoration before vector generation. Body+8 World pointer is
null; no contact/constraint-world publication is inferred. Source SHA256
`e3ebd8be3d65d52b3cd94195b993c007b6bfd6abcff6f54f617dde54c60c0411`,
corpus `ace9cc3b7f568864a4a56568c5208fa1ad12df9c6ca67b6be816ca25134c4575`.
Additional full-entry missing-controller oracle01 executes240 cases with node
controller-chain absent/present, confirming sync precedes lookup and duration.
Source SHA256 `5b99d9486b39fbc1ba87f6098679867fa1d6541c386792b71f43a8a8b3920c1e`,
corpus `566cd797d430331e20d96fd3b685e7a419d9b0ad04caeca397610942bb3a5efc`.
Each checks original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Only Windows primitives, controller/key-buffer and HK allocation/free boundaries
are supplied. Actual constructors, motion getters/setters, lookup/attachment,
configured settings copy, native vector conversion, mass/damping, Start and
security-cookie paths execute. No immediate/nonblend/children or World acceptance.

Actual owned normal/sanitized comparisons each match46,080 cases/1,725,120 exact
fields plus240 missing-controller cases/5,040 fields. Compare all existing
blend/key/clock/vector/target/order fields plus final request, flags and mode,
with archived mass, pose/velocity preservation and zero world objects after
cleanup. Eight original capture controls check preserved request/flags,
synchronization before disabled/missing-controller exits and unchanged existing
velocity during conversion. Generated-owner allocation intent is compared, not
native heap implementation. Evidence under S4: native-requested-motion-full-down-
oracle-01; native-requested-motion-missing-controller-oracle-01; requested-motion-
down-baseline/normal/sanitized-01; native-requested-motion-full-down/missing-
controller-compare-normal/sanitized-01; requested-motion-down-controls-01.

Follow-up original full collision frame probe01 already executes5,760 cases with
stored request independent of current Dynamic type2/KEY6, eight prior scene
inputs across both x87 words, selectors/gains/flags. This is evidence for the
next chunk, not a passing production comparison. Source SHA256
`d0641aad1b473222d1abd0af8ae9885fa3d7c9a06c55974604ab64358fbc9181`,
corpus `9299b5fe15a614b8a5ebfba7d2a6d178d83b81faadcc8f59632fc3fe43dea24e`.
No stage closes. Next couple independent requested/actual modes in frame
publication and reconcile legacy/manual metadata updates. Automatic World
lifecycle, full physical/contact/controller persistence, configuration/morphology,
recursive ordering, retained BodyT/floor failures and S5-S14 remain open.


### Checkpoint216: independent collision request and actual mode in frame updates

Checkpoint215 committed as `9d6aab88abd9795c9a912c2a3d9b75b68b20b5bc`,
212 isolated commits; bundle193 SHA256
`9aca235ab1bfb1be10c65f9d7aeb55bfc7734670beb00bb6d850709216cc5398`.
Owned collision frame dispatch now compares the previous stored request with the
new selected request, as88F484 does. Equal requests preserve an inconsistent
actual mode. Changed requests zero velocities when the previous request is not6;
previous request6 synchronizes before conversion. The original admitted Dynamic
archive getter returns2, distinct from requested Dynamic1: its setter/flags and
activation path can run without a Dynamic/KEY handoff. Actual KEY6 already equal
to requested6 skips setter flags. This archive-type2 evidence does not establish
arbitrary unmodeled native Dynamic subtypes or zero-mass admission.

Manual updateNativeBlends now stages owned collision gains/flags/request metadata
with physical candidates, including skipped selector inputs retaining requested
motion. Scene hooks run before body and metadata publication. Owned controller
frames retain their staged gain/clock/request publication and rollback. Bodies
without owned blend targets retain the earlier explicit body-update contract;
they do not acquire synthetic collision state. Updated the legacy normalDown
header to describe215's synchronization and wrapped the staged damping argument.

Independent original full88F3D0 oracle01:5,760 cases, eight prior scene inputs
across both x87 words, independent actual Dynamic type2/KEY6 and stored
0/1/6/8/FFFFFFFF, selectors/gains/flags. Source SHA256
`d0641aad1b473222d1abd0af8ae9885fa3d7c9a06c55974604ab64358fbc9181`,
corpus `9299b5fe15a614b8a5ebfba7d2a6d178d83b81faadcc8f59632fc3fe43dea24e`.
Only Windows and native heap primitive boundaries; full original constructors,
archive/restore and scene/velocity setter paths execute. Body's native World
pointer is null.89EAE0 chooses8A3900 for actual Dynamic; that setter returns when
World or World+2B0 authority is absent. This owner models that no-native-World
boundary and preserves Dynamic pose, rather than applying the KEY direct pose
setter. Actual KEY uses the direct setter before/after appropriate conversion.
Live World-driven8A3900 behavior and flag20-without40 remain open. A Bullet World
borrowed by the owner is not proof of a resolved original World authority.

Canonical original/C++ comparison retains172's predeclared53-bit domain and
unchanged tolerances: scene/position/velocity.001, quaternion1e-6. Baseline normal
comparison fails2,016/2,880 cases. Two new unit tests fail baseline02; baseline01
first failed compilation due to a nonexistent named unit-conversion constant.
Initial implementation passes full normal/sanitized01 (2,371 components and899
engine each), but its original comparison fails224 cases: old request6 with
actual Dynamic incorrectly received a direct pose write. Original89EAE0/8A3900
inspection establishes the distinct no-World return. Corrected production setter
selection and the unit test's original incorrect pose expectation; retain all
failed evidence. Final full normal/sanitized02 each pass2,371 components and899
engine, exact inventories, no failure/skip. Fingerprint `7ae3f328451d27642ad23c84a7ebe183fdd761271a52382d8da6951af20bb031`. ASan leaks
disabled; Python unchanged. Final comparisons each pass2,880 cases/25,920 exact
checks; max scene/linear/angular errors0, position.000058675159, quaternion
.000000028951562. Six original captured controls pass over all5,760 cases.

S4 evidence: native-independent-requested-motion-frame-oracle-01;
native-independent-frame-compare-normal-baseline, normal-01, normal/sanitized-02;
independent-requested-frame-baseline-01/02, normal/sanitized-01/02;
independent-requested-frame-controls-01. No relaxed thresholds or missing cases
in the declared canonical domain. Emulator arithmetic is not game acceptance.

Follow-up full original8A3900 World-bound probes:01 fails after24 cases because
KEY conversion cleared body+91, enabling an unprepared activation traversal;
02 sets activation suppression after conversion and passes72 cases, but its
XYZW fixture inputs actually enter a WXYZ argument boundary.03 corrects the
input labels/order and passes72 cases with explicit nonnull World+2B0 authority,
Dynamic/KEY, frame0/.016/1/120, position and rotation. Source SHA256
`36421399f3e9797b1220dbc7bdaad2a9907d81c13033af1ddd29ce43ebce1443`,
corpus `a53b93194ff14b8c0fcb9e7e363f074edd2bf703f8224320116c000f0c1eb2fc`.
No live World authority producer or contact step is proved; this is next-chunk
input. Original executable hash remains the pinned1.2.0416 image.
No stage closes. Next implement and independently compare the World-bound scene
setter, then wire its actual owner/World authority. Automatic reaction lifetime,
recursive ordering, full controller/contact persistence, morphology/configuration,
retained BodyT/floor failures and S5-S14 remain open.


### Checkpoint217: independent World-scene packed velocity rule

Added immutable `ESM4::preparePhysicalWorldSceneVelocities` for original8A3900,
with explicitly resolved World/authority, native target position including its
scratch fourth lane, XYZW rotations, current origin and actual getter fields.
Absent World/authority returns before reading unused fields. Near-target checks
use strict native thresholds and signed quaternion dot; near targets clear both
four-lane velocities even at frame0. Far frame0 retains both vectors. Positive
frames produce uncapped center-of-mass drive and angular drive, including the
original nonzero fourth angular lane. Validation rejects used invalid inputs and
nonfinite results before publication. Supported prepared frame domain is finite
and nonnegative; this does not claim original8A3900 rejects negative frame time.
Portable reciprocal-root estimation/Newton normalization and libm acos remain
explicit limitations. No body pose, mode, activation or owner publication yet.

Original8A2F10 reads motion+80,8A2FF0 reads+90,8A3030 reads+60; these differ from
historical start/previous labels in older fixture files. Expanded original cases
separate+50/+60 and+70/+80. Exact offsets are proved; packed center fourth-lane
clock semantics are not. Original World argument quaternion is WXYZ; captured
prepared input is XYZW. Target scratch W is captured at8A399D; zero in selected
full-caller cases does not establish its general live producer.

Original direct oracle05 passes1,728 cases; full88F3D0/89EAE0 World-driver oracle03
passes256. Boundary oracle01 passes544 cases across both x87 control words,
Dynamic/KEY, frame0/.016, adjacent binary32 distance/signed-dot/angular thresholds
and positive/negative equivalent quaternions. Boundary cases all have a present
World+2B0 marker; its inherited report description of absent-World fixtures
refers to earlier expanded tests. Six captured controls pass, including both
near/far outcomes, absent authority, angular fourth lane and distinct getters.
All use the pinned original executable and only previously documented Windows/
HK heap primitives; body+91 suppresses activation. World+2B0 is a presence
fixture, not a live authority producer.

Original source/corpus hashes:
- direct05: a379f2176033c9170d8532ad23a5352060f6335e3c7fe74f54c9ee7e820d4d8a /
  8f1d17b1a79811de2d73caba4b94bf2887369f85b716508172c9d2a51b5d8a35;
- full-caller03: aee549863639c39bc6258d913ca09276b218fa0d2b079d8de23fee733e7de671 /
  870e95e6bae1a751bf327b16154040a12d12f81e6f5b77275920d6e8cf8d9fa1;
- boundaries01: c145ded86cd1a937b0368019e3488548465bd71c9f61c9cee571e02850e62bd6 /
  c776aae5856a75869658664d3d6e4de31d7eec5ddf420ac6e8ad1d3068bb460f.

Four unit tests cover unused fields/guard order, zero-frame behavior, uncapped
packed output and used-invalid/overflow rejection. Stub baseline01 fails3/4.
Full normal/sanitized world-scene-rule-01 each pass2,375 components, exact
inventories, zero failure/skip, no build warning. ASan leak checking disabled;
engine/Python unchanged since216. Tested source fingerprint
`f9e641a938fccf24dd7fd9694fcaa041149a7d91ae3ed068110ef2a3e2129aab`.
Actual production comparator normal/sanitized02 each pass992 canonical53-bit
cases; expanded03 each pass1,264 cases and9520 exact checks. Declared write
presence, linear four-lane bits and eight-lane no-write retention are exact;
angular four-lane tolerance.001 remains unchanged from172. Maximum observed
angular error0 in both builds; no claim of general bit-exact angular equivalence.
Comparator01 compile failure (missing private-driver<cstdint>) is retained.

Retained original full-World-frame01 fails after8 cases at8A9D71: KEY direct-pose
World branch requires unprepared native broadphase state. The passing full-frame
corpus selects flags20/28 without40, exercising8A3900 without claiming that direct
pose branch, activation islands, contacts or actual game World acceptance.
Expanded direct04/05 and full-frame02/03 are retained with their prepared-input
provenance. S4 evidence uses native-world-bound-scene-sync-oracle-04/05,
native-world-bound-full-frame-oracle-01/02/03,
native-world-scene-boundaries-oracle-01,world-scene-captured-controls-01,
world-scene-rule-baseline-01,normal/sanitized-01 and
native-world-scene-rule-compare-normal-01/02/03,sanitized-02/03.

No M15 stage closes. Next wire owned packed state and staged World-scene velocity
publication, then resolve actual World authority/lifetime and complete the
remaining automatic reaction, ordering, persistence/contact, gameplay and
S5-S14 acceptance gates. Earlier retained BodyT/floor/runtime failures remain.


### Checkpoint218: atomic owned World-scene velocity publication

Added ActorRagdollPhysics sparse `synchronizeNativeWorldScenes`, with explicit
caller-prepared217 input snapshots and owned body identity resolution. It stages
all native computations, output validation and world-unit projection before an
optional caller publication hook. A late invalid identity/input or throwing hook
leaves bodies and activation unchanged. Actual writes publish XYZ plus separately
owned binary32 fourth linear/angular lanes, then wake the owned connected group.
Absent World/authority and far frame0 preserve all eight lanes without waking.
Pose, body mode, accumulated forces, collision shape and broadphase identity stay
unchanged. This invokes the actual production217 rule; it does not infer native
World authority or original getter/time/scratch fields from a Bullet World.

Added complete ordered capture/restore of native packed velocities. Restore
validates the entire snapshot before writing and preserves pose, forces, mode
and activation. Signed zeros and both fourth lanes survive. Initial owned fourth
lanes are zero, matching this owner's initially zero XYZ velocities; this is not
a new proof of every native factory/loaded-stream velocity initialization.
The existing world-unit RagdollBodyState remains an XYZ/pose projection, separate
from this packed snapshot. Requested-motion reset in updateNativeBlends now
clears both owned fourth lanes alongside XYZ, consistent with216's original
reset capture. Other motion/force/damping routes need their own packed-lane
integration; this checkpoint does not claim they already consume these fields.

Three WorldScenePacked baseline tests each fail against publication/restore
stubs. Four final owner tests pass, including uncapped output, all-lane snapshot,
signed zero, late invalid batches, hook rollback, near-frame0 clearing, sleeping
no-write paths and requested-motion reset. Full normal/sanitized01 each pass
2,379 components and899 engine, exact inventories and zero failure/skip. Both
builds have no new warning. ASan leaks disabled; Python unchanged. Tested source
fingerprint `fdd13bf09fe4c6aa09be543dee82bb7c06a23b265a362466681ed0721ef8ede8`.

Actual owner comparison normal/sanitized01 each passes1,264 original cases and
9520 exact checks from217's unchanged corpus/domain/tolerances, maximum angular
error0. Driver alternates Dynamic/KEY and scales1/7/6.999040126800537; asserts
prepublication hook order, retained pose/mode/forces/shape/proxy, and complete
object/constraint deregistration at destruction. These are owned Bullet tests
using resolved native input fixtures, not an automatic World-frame/game probe.
S4 evidence: owned-world-scene-baseline-01,normal/sanitized-01;
native-owned-world-scene-compare-normal/sanitized-01. Shared original source and
corpus hashes are recorded in217 and copied into comparator verification.

No M15 stage closes. Public scheduler/World-frame coupling, actual authority and
getter producers, full packed motion/force/serialization and earlier acceptance
gates remain open. Follow-up original packed-key-step-oracle-01 captures17,010
complete8EA4B0 cases with both fourth velocity lanes -8/-0/+8; this is input to
the next keyframed-integration chunk, not current218 implementation evidence.


### Checkpoint219: packed keyframed caps and substep publication

Extended the189 keyframed motion rule with explicit fourth linear/angular inputs
and outputs. Both lanes preserve untouched signed zeros and use the same
XYZ-derived native cap factors as their three spatial lanes. They do not enter
length reductions, quaternion integration or COM position calculations. Used
nonfinite lanes reject before publication. The owned keyframed substep passes
its packed state through that rule and stages capped fourth lanes with the
physical pose/XYZ result before writing. NativeDynamicsWorld's existing actual
substep registration now reaches these stores; zero-duration World steps still
have no actual substep. Center/time and raw quaternion persistence remain
separate requirements; this is not a full native motion snapshot implementation.

Original packed-key-step-oracle-01 passes17,010 complete8EA4B0 cases, no boundary
stubs:21 physical fixtures, both x87 words, five frames, three cap pairs,
independent linear/angular XYZ and shared fourth-lane -8/-0/+8 controls.
Source SHA256 `f45cfecb8cd1d60380f03924ccf9b05ce36d44e74a81436857a700fb52d8e6db`;
corpus `4e2b618f755504dbe4d7f5f6ca2e3e9132e94bb822c834af5c2a85d418b8ff2a`.
Changing only fourth lanes leaves every other captured motion field bit-identical
across5,670 three-way groups; packed-key-step-captured-controls-01 verifies this.
The two W inputs are varied together in that corpus, not independently of each
other. Existing independent XYZ velocity combinations remain covered.

Three new tests fail baseline01 (pure caps, invalid/signed-zero admission,
owned capped publication). Before the fix, actual pure comparison fails10,332
fields across17,010 cases; actual World substep comparison fails2,016 fields
across4,536 cases. All mismatches are fourth lanes; earlier output fields retain
prior passing behavior. Full normal/sanitized packed-key-step-01 each passes
2,382 components, exact inventories, zero failures/skips and no build warning.
ASan leaks disabled. Engine/Python code unchanged;218 full engine checks remain
the preceding integration evidence. Tested source fingerprint `278306bf8604868f8d74e36020a1c05e69b07d1538ab2a7d95ba0ff82491d372`.

Final pure normal/sanitized01 each passes17,010 cases/527,310 fields, numerical
errors0 and70,308 exact signed-zero checks. Final owned normal/sanitized01 each
passes4,536 positive-duration actual substeps/104,328 fields,12,096 signed-zero
checks. Original declared position/velocity tolerances.001 and basis1e-6 are
unchanged, including the new packed lanes. Maximum fourth-lane errors0; owned
position error.000060731079, COM about2.3e-13, XYZ velocity about2.9e-14 and basis
about2.99e-7. Owned domain uses loaded linear cap250, angular0/1/31.4159, excludes
1,134 frame0 cases and other pure-only linear caps. Baseline owned report copied
old190's exclusion count/limit wording; final report corrects that description.
Prior zero-duration World comparison failure is retained, not waived. S4 evidence:
packed-key-step-baseline-01,normal/sanitized-01;
native-packed-key-step-compare-normal-baseline,normal/sanitized-01;
native-packed-key-owned-compare-normal-baseline,normal/sanitized-01.

Follow-up World binding probe executes original889BB0 using constructor-identified
wrapper vtableA95CF4, actual getter452A60 and full89D430 reference operations,
then full8A3900:1,728 cases pass. World+2B0 is assigned the wrapper pointer during
reference acquisition and cleared during release. It is an owner backreference,
not a simulation-enable boolean. The count2 fixture avoids final destruction;
the full World constructor and game-level lifetime producer remain unexecuted.
Source SHA256 `7a4e5d9054025ee7e0dd0ee2acdf052e5a1397a077211579eab451e52085d119`;
corpus `f8bde8a0bd8430ede67e8208a104f2a13dd14e7ffadef28b45b25439da07f9cb`.
S4 native-world-wrapper-bound-scene-oracle-01 is input to the next public-owner
integration, not proof that this scheduler already supplies that authority.

No M15 stage closes. Continue public World-scene ownership/coupling, packed
force/damping/persistence, automatic reaction lifetime, full ordering, retained
physical/runtime failures and all remaining S5-S14 acceptance.


### Checkpoint220: public packed World-scene service and owner binding

Exposed packed native velocity capture/restore and sparse World-scene publication
through PhysicsSystem and PhysicsTaskScheduler. Each operation waits for workers,
holds the collision-world lock and resolves the current actor owner. The World
publication copies prepared requests, resolves their guard bits from the owned
World/scheduler binding, then invokes218's staged all-lane publication. Caller
World booleans cannot substitute for the scheduler's live binding. Native getter,
converted-target/scratch and prepared-frame values remain explicit caller inputs;
this service does not yet produce them during automatic collision traversal.
The hook retains218's atomic/no-reentry contract while the scheduler lock is held.

NativeDynamicsWorld now supports one scene-owner binding. Scheduler acquires it
through a RAII object before starting workers; destruction clears it after worker
shutdown and ragdoll removal. Normal shutdown is verified; thread-start/allocation failure paths have not
been exercised by these tests. Null/duplicate/ambiguous acquisition rejects; mismatched release cannot
clear another owner. Ordinary borrowed Bullet worlds have no native binding and
therefore no World-scene authority through this public operation. This is the
engine ownership translation of the original backreference guard, not an
implementation of arbitrary Havok reference counts or simulation-enable flags.

Independent original full889BB0 probe recorded in219 passes1,728 cases through
actual452A60 and89D430, followed by full8A3900. Added captured controls: absent
World skips reference operations; releasing clears+2B0 and changes reference2
to1; acquiring installs the actual wrapper pointer and changes2 to3. Original
vtableA95CF4 is independently confirmed as `.?AVbhkWorld@@` by complete-object
locatorAD1988 and type descriptorB2E4BC, not a borrowed header label. Constructor
88AEB0 writes that table; its4C pointer setter89D400 invokes50's custom889BB0 when
switching held objects. Actual full constructor/final-reference destruction and
game-level lifecycle are still outside these fixtures. Wrapper RTTI and binding
controls are retained beside native-world-wrapper-bound-scene-oracle-01; original
source/corpus hashes are in219. No activation island or contact acceptance claim.

New component test exercises binding ambiguity, mismatched release and rebinding.
Extended the existing public physical-owner fixture for worker counts0/1/2 with
packed restore/signed-zero validation, stale actor rejection, late invalid sparse
requests, hook rollback, real binding despite false caller guard bits, uncapped
original velocity outputs, fourth angular lane, unchanged pose/modes/cache,
far-frame0 retention and near-frame0 all-lane clearing. This uses explicit native
getter fixtures alongside real scheduler ownership, not live automatic getters.
Full normal/sanitized public-world-scene-01 each pass2,383 components and899 engine,
exact inventories, no failed/skipped cases or build warning. ASan leaks disabled;
Python unchanged. Tested source fingerprint `87b19a8bdee832f40732c4de84b3582392f284b0ec2c5172cc19224c828744f2`.

Follow-up native-packed-body-force-oracle-01 passes48,000 complete5377B0/8EAC80/
KEY8EA060 cases, no stubs: independent force/current fourth lanes, both x87 words,
zero/signed-zero/frame/inverse-mass boundaries. Source SHA256
`690b3374d3395dd55239625777ea3500cdbc69668d89118da0a087c4538ac360`;
corpus `f4d2abd110aafe46b8784e966ce378fd8d7251fa45f2a30b81c2f5a1af58f459`.
Activation is explicitly suppressed; this is input to221, not current force-owner
integration evidence. No M15 stage closes. Continue packed force/damping/save,
collision-frame getter/ordering integration and automatic World reactions,
then complete the retained physical/runtime and all S5-S14 acceptance gates.


### Checkpoint221: packed native force and controller-force publication

Extended sparse force requests with an explicitly supplied fourth force lane
(default0 preserves existing spatial callers). Dynamic force staging now reads
owned fourth linear velocity, passes all four lanes through205's native force
primitive and publishes the staged fourth result with XYZ. KEY ignores unused
force/time inputs and retains all lanes. Shared-target controller forces read the
preceding staged fourth update, preserving sequential binary32 stores. The owned
velocity-controller phase now carries its existing four-lane force intent through
the body request; it previously dropped W. Angular lanes, pose/modes and force
accumulation remain unchanged. Late used-invalid requests reject before any
body/activation/controller/cache publication.

Two initial tests fail baseline02 after baseline01's compile failure from an
incorrect test field name (`mVector`, corrected to existing `mForceVector`). Added
a shared-target sequential test; baseline03 fails all3. Actual original comparison
baseline fails10,208/48,000 cases, each exclusively the owned fourth linear lane.
No pure or spatial field mismatch. Final tests cover direct fourth-lane force,
late invalid W rollback, KEY unused-invalid inputs, controller source W×100 and
sequential shared-target updates. Public fixture for0/1/2 workers also applies a
nonzero fourth force through PhysicsSystem and captures its result.

Original48,000-case source/corpus is recorded in220. Final actual production
normal/sanitized packed-body-force-compare-01 each passes48,000 pure and43,200
owned positive-mass/KEY cases,412,800 exact bit-pattern checks, no mismatch.
Dynamic inverse0 remains4,800 pure-only cases because positive-mass owner
admission cannot represent that factory domain. Neither tolerances nor inputs
were changed to obtain these passes. Full normal/sanitized packed-force-01 each
passes2,386 components and899 engine, exact inventories, zero failed/skipped
cases and no build warning. ASan leaks disabled; Python unchanged. Tested source
fingerprint `91dc81ab14b613d32a5c95283269957ac91ac865d29d30e2978bfdef24ba7362`.

Additional original sequential-packed-force-oracle-01 executes300 fixtures/900
full5377B0 calls (three consecutive Dynamic/KEY force calls), no stubs, explicit
inverse mass.5 and both x87 words. Source SHA256
`720c87ff8c217a117b44eefc789ca3ea7259bfa38d523431f0c304eed6de01f2`;
corpus `f1bc867944361bdc8657dead844d209b6622682db53d077ce7be0fef149b6353`.
Fixture48 independently supplies10.399999618530273 then16.799999237060547 for
frame.016/forceW800/currentW4; the single-controller and shared-target tests use
those captures. This raw sequential corpus is not a complete native linked-list
controller/World-step probe. Earlier204/208 controller traversal/source evidence
and new owned tests cover the joined controller path at their declared scopes.
S4 evidence: packed-force-baseline-01/02/03,normal/sanitized-01;
native-packed-body-force-oracle-01,compare-normal-baseline,normal/sanitized-01;
native-sequential-packed-force-oracle-01. Activation remains suppressed in
original arithmetic captures; native islands/contact acceptance is still open.

Clarified220's binding lifetime report: normal shutdown/worker variants are
verified; thread-start/allocation failure paths have not been exercised. No M15
stage closes. Packed dynamic damping/caps, full raw motion/controller persistence,
automatic collision-frame getter/ordering and World reaction integration, prior
physical/runtime failures and S5-S14 acceptance remain open.


### Checkpoint222: packed dynamic damping and motion caps

Implemented a single four-lane dynamic velocity rule. It adds the supplied
four-lane linear delta, applies native linear/angular damping, and scales all
four lanes when the spatial linear or angular-step reduction admits a cap.
Only XYZ participates in those reductions. The three-lane rule delegates with
zero fourth inputs; the existing owned scheduler phase delegates with zero
fourth deltas while retaining current owned fourth velocities. The unused
angular half-step W is not computed or validated. Used packed values and stored
results must remain finite within the declared supported domain.

The owned packed phase stages every active Dynamic body and world projection
before writing any velocity. KEY and sleeping bodies ignore unused deltas;
pose, modes, force accumulation, collision objects and activation remain
unchanged. Explicit standalone damping updates all four lanes of Dynamic bodies,
including sleepers, without waking them; KEY retains its original values.
It multiplies directly, preserving negative zero when its damping factor is zero.
Four new tests fail the retained packed-dynamic-velocity-baseline-01 before
implementation. They cover independent original cap outputs, used-invalid W,
late invalid batch rollback, inactive unused-invalid deltas, legacy phase joining,
standalone signed zero, pose/modes and sleeping retention.

Original native-packed-dynamic-velocity-oracle-01 passes34,560 complete sphere
8E96C0/box8EAFF0 motion calls with actual889470/4D6830/8B1DD0 and no stubbed
calls. Executable SHA256 remains the pinned original. Probe source SHA256
`62ca1d3dbbc10a7e9dd10d6565388e923cc36397f7a2affcbe01b199943d33ad`;
corpus `05534719046a03d539d40e1cc5a4f2526c3cbbdb8ad199015f26201b4091752b`.
Fixtures independently vary linearW/angularW/deltaW, both x87 control words,
frames0/1/120/.016/.05/.5, damping0/2, linear limits1/250, angular limits1/31.4159,
spatial zero/subnormal/large velocities and zero/gravity deltas. StepInfo values
and raw angular-limit fields are explicit fixtures, not proved World producers.
Final pose/cache captures are recorded by the full return. Captured controls
find1,920 groups of18 W variants with bit-identical final origin/center/rotation/
angular-delta fields. This proves independence only within those fixtures.

Actual production normal/sanitized native-packed-dynamic-velocity-compare-01
both pass34,560 pure cases and17,280 owned factory-linear-limit250 cases,
449,280 exact field checks including admission indicators, no mismatches.
Owned scales1/7/native alternate, with shape/proxy, pose/mode/forces/activation
retention and collision-object removal checked. Original box cases use the same
owned sphere velocity phase, which does not inspect shape. Raw linear limit1
is pure-only because admitted loaded bodies have limit250. Frame0 is a direct
phase fixture, not an engine World-step acceptance case. All eight velocity
outputs match bit-for-bit; no tolerances or inputs were changed after comparison.
Full packed-dynamic-velocity-normal/sanitized-01 each passes2,390 component and
899 engine tests, exact inventories, zero failures/skips. ASan leak checks remain
disabled. Python unchanged. Tested source fingerprint `a13eaae5180816a3e12325b3693891726fd6d76759f02ed305247cff14f27f03`.

No M15 stage closes. Full packed/raw motion and controller persistence, actual
World getter/clock/collision-frame ordering, automatic reactions, retained native
activation/contact/save failures and all S5-S14 gameplay acceptance remain open.


### Checkpoint223: atomic pose and packed-velocity restoration

Added a complete ordered pose/packed-velocity restore overload. All packed
records, used lanes and world projections are staged before the existing full
pose validation and publication. Native velocities override the spatial velocity
projection; restored interpolation receives those authoritative values. Fourth
lanes publish without further allocation after the existing pose restore.
Motion modes are retained. The original pose restore's force clearing, activation,
inertia/interpolation and AABB refresh behavior is unchanged. The existing
spatial-only API does not gain invented fourth values.

Both new tests fail packed-pose-restore-baseline-01: packed velocity loss and
late invalid input admitting partial pose changes. Final tests cover independent
original case20122 velocities, signed-zero W/XYZ, KEY mode retention,
interpolation/current agreement, and complete rollback for late nonfinite W,
identity/count mismatches and invalid pose. Rollback also preserves existing
force and sleeping state. These are supported-domain validation guarantees;
Bullet internal allocation failure during publication is not exercised.

Actual normal/sanitized native-packed-pose-restore-compare-01 each restores
34,560 original222 captured velocity snapshots,276,480 exact bit-pattern checks,
zero mismatches. Alternate Dynamic/KEY and scales1/7/native; deliberately
conflicting spatial velocities verify native authority. Checks also require
requested pose, retained mode/shape/proxy, interpolation agreement, force clearing,
wakeup and collision-object removal. Both original sphere/box and raw-linear-limit1/
250 outputs are snapshot inputs admitted by this velocity restoration boundary;
this is not original save/restoration or full motion integration evidence.
Original executable/corpus identity remains222's pinned dataset. No tolerance
changes. Full packed-pose-restore-normal/sanitized-01 each passes2,392 component
tests, exact inventories and zero failures/skips. ASan leaks disabled. This lower
owner API has no new engine caller yet; engine and Python unchanged and not
repeated. Tested source fingerprint `df03686b74c4730fbb509126486de094f74d70badb4dff203c1454b4d53b24a2`.

No M15 stage closes. The versioned save schema and public scheduler snapshot
must carry this data before packed save/restart can be claimed. Full raw modes,
COM/time/cache/controller persistence, automatic World getter/frame/reaction
integration, retained contact/restart failures and S5-S14 acceptance remain open.


### Checkpoint224: versioned packed-velocity snapshots and scheduler save boundary

Runtime-state version32 extends each physical body with a strict one-byte
presence marker and, when present, eight binary32 native velocity lanes.
All bodies in an actor snapshot must consistently carry or omit packed state;
finite lanes, count/identity and winning asset checks remain mandatory. Current
snapshots carry packed velocities separately from the legacy world-unit spatial
projection. Native velocities are authoritative during combined restoration.
Versions1-31 retain their existing wire layout and absence semantics; a version31
payload cannot silently discard a populated packed field. Canonical JSON emits
the optional field only when present and preserves negative zero. C++ and Python
codecs share this explicit format and reject malformed markers, partial native
snapshots, truncation and nonfinite lanes.

The capture adapter optionally accepts a complete ordered native velocity span,
resolves body/node identities, and stores it in canonical record order. Restoration
returns complete native state in asset order, or absence for legacy snapshots.
The actual Scheduler/PhysicsSystem snapshot path now captures both projections
under its worker wait and lock. It prepares both restored projections before
calling223's combined atomic restore. Legacy snapshots retain the old spatial
restore path. Packed restoration does not imply restoration of modes, activation,
contact caches or controller clocks; those remain separate/open.

Two C++ wire/malformed tests fail packed-save-baseline-01. Python baseline01's
runner failed import before executing tests; corrected baseline02 executes both
new tests and both error because the baseline codec rejects the new field.
Final tests manually assemble version32 bytes from the unchanged independently
checked version31 layout, exercise absent/present markers and signed zero, and
reject downgrade, mixed presence, nonfinite lanes, unknown marker and every
truncated byte of the new33-byte extension. Engine continuation serializes a
snapshot, deserializes it, resolves wire order12/24 back to asset order24/12,
restores a separate owner and compares60 packed velocity phases bit-for-bit.
This is contact-free phase continuation, not fresh-process gameplay acceptance.
Public World fixture0/1/2 workers checks captured packed data and invalid packed
rollback. The original spatial-only adapter signature remains supported.

Full normal/sanitized packed-save-01 each passes2,394 components but fails the
same3 scheduler snapshot tests (0/1/2 workers). Those tests altered world-unit
velocities without updating the now-authoritative native snapshot. Corrected
fixtures set both projections consistently, retain original expected spatial
velocities and add invalid packed-lane rollback. The keyframed public fixture
uses its actual admitted scale1 and sets native60 alongside world60, retaining
its existing expectations/tolerances. New optional-member initializer warning
and six old test aggregate initializer warnings in runtimestate.cpp are removed
without behavior changes; old inventory-test, loop-copy and GCC animation-state
warnings from the wider rebuild remain recorded in01 rather than hidden.

Actual native-packed-save-wire-compare-normal/sanitized-01 each passes34,560
C++ serializer/deserializer payloads using original222 captured velocity values,
276,480 exact lane checks. The independent Python codec parses and re-encodes
every complete payload byte-for-byte, zero wire failures. Executable/corpus
identity remains222's pinned original dataset. These values are snapshot inputs;
this does not establish the original game's save protocol. Comparisons ran at
source fingerprint `8e418bdb34aae6adfa58c57de4197ff0f353af41ec5f6af44aebe6a7b39b5b26`. Subsequent changes touch tests only;
both final component archives are verified byte-identical to those comparers.
Full packed-save-normal/sanitized-02 each passes2,394 components and900 engine,
exact inventories, zero failed/skipped cases. The normal run additionally passes
247 Python tests. ASan leak checks remain disabled. Final tested source
fingerprint `1c8b12d10595a4ba91a4cc0e78d8b4cf25d6ddf756c30613e611aa38e42b72e6`.

No M15 stage closes. Full native modes/request/flags/gains/controller/clock/raw
COM-time and activation/contact persistence, automatic World getter/frame/reaction
integration, retained physical/restart failures and S5-S14 acceptance remain open.


### Checkpoint225: complete logical motion/pose/packed restoration

Added a complete ordered logical Dynamic/KEY mode, pose and native packed-velocity
restore overload. Mode count, identity and enum validation, velocity conversion,
pose validation and buffer allocation all precede the first physical handoff.
Restoration reuses the existing archived dynamic mass/inertia and KEY transition
primitive, then publishes authoritative native velocities and pose/interpolation.
Body, shape, proxy and constraint identities are retained. The logical owner mode
is not the original Havok motion class-kind: Dynamic's owner value1 must not be
mistaken for a raw native getter/archive type. Activation/contact persistence is
not added by this API.

Extracted shared pose validation/publication and packed preparation/publication
helpers so legacy, packed and mode-inclusive restores use the same reviewed
paths. No allocation occurs between mode handoff and packed/pose publication.
Bullet-internal allocation failure during publication remains unexercised; the
batch guarantees concern supported-domain validation and owned staging.

Baseline01 fails compilation because a new test named a nonexistent public
damping getter. Replaced it with actual damping behavior: after restoring both
handoff directions, Dynamic W damps and KEY W retains its value. Baseline02
executes both new tests and both fail. Final tests verify inverse mass, kinematic
flags, signed-zero/native lanes, interpolation, pose and constraint/shape identity;
late invalid mode/record/count, packed W and pose all leave prior modes, pose,
velocity, force and sleeping state intact before any handoff.

Actual normal/sanitized native-packed-motion-restore-compare-01 each changes
34,560 cases to the opposite initial logical mode, alternates Dynamic/KEY and
scales1/7/native, and retains all eight immutable original222 captured velocity
lanes exactly (276,480 bit-pattern checks). Additional checks require desired
mode/inverse mass/kinematic flag, pose, shape/proxy, interpolation, force clearing,
wakeup and destruction cleanup. This is owner state restoration using original
velocity outputs as inputs; it is not a full original save/handoff experiment
or raw native motion-class persistence. No tolerance/input adjustments.
Full packed-motion-restore-normal/sanitized-01 each passes2,396 components,
exact inventory and zero failures/skips; no new compiler warnings. ASan leaks
disabled. Python unchanged; no new engine caller for this overload yet. Engine
integration checks will accompany the next versioned scheduler mode boundary.
Tested source fingerprint `76af79a836750db715a2bc0eb317a6cdb28206fb415875f742660a09ca19b302`.

No M15 stage closes. Versioned logical-mode persistence is still required before
fresh owners restore KEY correctly. Raw COM/time/cache, controller clocks/flags,
activation/contact persistence, automatic World getter/frame/reaction wiring,
retained physical/restart failures and all S5-S14 acceptance remain open.


### Checkpoint226: versioned logical motion modes and fresh-owner restoration

Runtime-state version33 appends a strict optional logical-mode marker/value to
each body after32's packed velocities. Values1/6 mean the owner's Dynamic/KEY
mode, not original Havok motion class/getter tags. Unsupported0/2/255 reject.
A populated mode requires packed velocities; all bodies in an actor must
consistently carry or omit modes. Legacy versions1-32 retain their existing
layout and mode absence. A populated mode cannot silently downgrade to32.
Current JSON emits native_motion only when present. C++/Python validate the
same typed fields, marker, count/identity, complete presence and version domain.

The capture adapter optionally consumes complete ordered logical modes together
with packed velocities. It maps enum values explicitly and stores canonical
body-record order. The restoration adapter resolves modes back to asset order,
or reports absence for legacy snapshots. Actual Scheduler/PhysicsSystem capture
now includes modes under its worker wait/lock. Restore stages all three projections
before225's complete mode/pose/packed restore, retaining legacy32 packed and31
spatial branches. This restores logical KEY into a fresh Dynamic owner with
zero current inverse mass and correct dynamic-phase exclusion. Raw original
archive/class-kind, requested blend motion and activation/contact state remain
separate and unpersisted.

Both C++ motion-save-baseline-01 tests fail. Python motion-save-python-baseline-01
executes both new tests and both error on the baseline codec's unknown field.
Final manual v32/v33 wire tests cover Dynamic1/KEY6, absent mode and unchanged
legacy prefix, exact bytes and JSON. Negative controls reject downgrade,
mode without packed data, mixed per-actor presence, unknown enum/marker and
every truncated byte of the extension; Python additionally rejects bool/string
values. The engine serializes/loads a mixed-mode snapshot in a separate world,
restores a fresh Dynamic owner to KEY, resolves12/24 wire order to24/12 asset
order, checks inverse masses and compares60 packed velocity phases bit-for-bit.
KEY deliberately receives unused NaN deltas while Dynamic consumes gravity.
This is contact-free phase continuation, not an actual process/World-step or
normal-input gameplay acceptance claim. Public0/1/2 worker fixtures restore
KEY snapshots and reject malformed native getter2 without pose/mode changes.
Capture also rejects wrong identities/counts/enums or modes without packed data.

Actual native-motion-save-wire-compare-normal/sanitized-01 each passes34,560
payloads:276,480 exact velocity lane checks,34,560 exact logical modes, and
independent Python parsing/re-encoding of every complete C++ payload byte-for-byte.
Original222 velocity corpus/executable identity is unchanged; logical modes
alternate as explicitly supplied software snapshot inputs. No tolerance/input
changes. Full motion-save-normal/sanitized-01 each passes2,398 components and
901 engine tests, exact inventories, zero failures/skips. Normal additionally
passes249 Python tests. ASan leak checks disabled. The wider rebuild retains
previous inventory-test initializer, loop-copy and GCC animation-state warnings;
none originates in the new mode format/adapter. Tested source fingerprint `5682837b95d51fe97d177dc7c91edd28c65bd5289b4a2ca7c780eac655a729f5`.

Read-only existing-display query found DISPLAY=:0/Wayland wayland-0 but
xdpyinfo cannot open :0 (exit1), recorded in motion-save-display-probe-01.
No unchanged Xvfb retry or game launch was performed. Required normal-input
runtime availability remains unresolved; requested existing-display information
while continuing implementation. This is not a newly failed gameplay course.

No M15 stage closes. Requested motion/flags/gains/controllers/clocks, raw COM/time/
quaternion/cache, native activation/contact persistence, automatic World getter/
frame/reaction integration, retained physical/restart failures and S5-S14
acceptance remain open.


### Checkpoint227: atomic blend request/flags/gains with physical restoration

Added a complete blend-target snapshot to the logical-mode/pose/packed restore
transaction. Count, unique known body IDs and finite unclamped gains are validated,
then a complete metadata buffer is allocated before physical staging/publication.
The buffer swaps only after successful physical restoration. Raw uint16 flags
and arbitrary uint32 requested motion remain independent of actual logical body
modes. Standalone blend restoration shares the same validator and retains its
complete-target semantics. Controller state and shared clocks remain separate.

Both packed-blend-restore-baseline-01 tests fail. Final tests preserve signed-zero
and unclamped gains, raw flags/FFFFFFFF requests, packed lanes and an inconsistent
actual KEY/stored request1. Its next H0/V0 update retains KEY and saved fourth
velocities, matching216's independent original rule: matching stored request skips
conversion/reset. An un-restored constructor request8 would incorrectly hand off
and clear velocities. Late nonfinite gain, unknown/duplicate target and incomplete
blend snapshot reject before pose/mode/force/activation or metadata publication.

Actual native-packed-blend-restore-compare-normal/sanitized-01 each passes5,760
composed snapshot inputs,74,880 exact field checks: eight native velocity lanes,
logical mode, raw flags/request and two gain bit patterns. Inputs use all216
original full-caller final flags/requests/vtable mappings with its explicitly
supplied fixture gains, paired cyclically with222's original velocity outputs.
Gains are original fixture inputs, not newly captured post-return getter values.
Checks include opposite initial mode handoff, scales1/7/native, shape/proxy/pose/
interpolation, force clearing/wakeup and destruction cleanup. This composition
verifies restoration of independently identified values; it is not one original
save/restart/controller/World/contact experiment.216 original source SHA256
`d0641aad1b473222d1abd0af8ae9885fa3d7c9a06c55974604ab64358fbc9181`,
corpus `9299b5fe15a614b8a5ebfba7d2a6d178d83b81faadcc8f59632fc3fe43dea24e`;
its previously declared Windows/heap boundary stubs and no-World scope remain.
222 executable/velocity corpus identity is unchanged. No tolerance/input changes.

Full packed-blend-restore-normal/sanitized-01 each passes2,400 component tests,
exact inventory, zero failures/skips and no new compiler warnings. ASan leaks
disabled. No new engine caller or Python change yet; versioned scheduler blend
persistence will carry accumulated engine verification. Tested source fingerprint
`810d6ddf98d21a1c71fd61d80619d820c456a3d228cf10f560a1384fb031224f`.

No M15 stage closes. Blend metadata still needs a versioned save boundary;
controller/order/global clock and raw COM/time/quaternion/cache, native activation/
contacts, automatic World getter/frame/reaction integration, retained physical/
restart failures and S5-S14 acceptance remain open. Existing-display information
is pending while implementation continues.


### Checkpoint228: version34 complete blend metadata wire boundary

Version34 adds an actor-level optional complete blend-target list after its body
records. A strict byte presence marker distinguishes absent legacy data from a
current complete empty list; present lists have a uint32 count and18-byte entries
(body uint32, flags uint16, opaque request uint32, hierarchy/velocity binary32).
Versions1-33 preserve their previous layouts and absence. Populated lists reject
older-version serialization. Entries must be increasing unique known body IDs,
finite unclamped gains, at most the body count, with complete packed velocities
and logical body modes. The schema permits a proper subset of bodies because
not all asset bodies have blend collision targets. Exact winning target-set
validation belongs to the next adapter/scheduler checkpoint. Arbitrary uint32
requests remain independent of logical actual modes1/6; all uint16 flag bits and
signed-zero gains survive binary and canonical JSON output. No archive mode tag,
controller keys/clocks, native raw motion or World service persistence is invented.

Both blend-save-baseline-01 component tests execute and fail; both
blend-save-python-baseline-01 Python tests execute and error before implementation.
Separate C++/Python golden checks construct the version34 suffix byte-for-byte
from an unchanged version33 payload, includingFFFFFFFF/F123 and negative-zero/
unclamped gains. Tests cover legacy absence/current empty, proper body subset,
downgrade, incomplete body state, duplicate/unknown/reversed IDs, finite gains,
strict markers, excessive counts and every truncation in the metadata extension.
Python additionally rejects booleans/strings/out-of-range flags and requests,
invalid list/entry shapes and nonfinite values.

Full blend-save-normal/sanitized-01 each passes2,402 component tests; normal
passes251 Python tests. The first builds exposed a new local-count shadow
warning; renamed it to blendCount after terminal runs. Final
blend-save-normal/sanitized-02 each passes2,402 component tests with exact
inventories and zero failures/skips, no new compiler warnings. First builds also
retain preexisting inventory-test missing mOwner initializer warnings. Python
code/tests were unchanged by the C++ local rename, so unchanged passing Python
checks were not repeated. ASan leak checks disabled. Tested final source
fingerprint `b999a39e7dd8c6561e7d19f8058d6a7ac165e5046b0b4a742af87266dbc184a3`.

Actual native-blend-save-wire-compare-normal/sanitized-01 each passes5,760
complete C++ encode/decode inputs and independent Python decode/reencode byte
identity:46,080 exact native velocity lanes,5,760 logical modes and23,040 raw
flag/request/gain fields (74,880 total). The composed input set pairs all216
original full-caller final flags/requests/vtable mappings and its explicit input
gains with222 original velocity outputs cyclically. Gains are fixture inputs,
not post-return getter captures. Original216 source/corpus identity and declared
Windows/heap/no-World boundary are unchanged;222 original identity is unchanged.
This is codec interoperability, not a single original save/restart/controller/
World/contact experiment or gameplay acceptance. Comparer source and binary,
archive, input corpus and source fingerprint hashes are recorded in each report.

No M15 gate closes. The actual scheduler must still capture/resolve/publish these
fields; controller/order/shared clocks and raw COM/time/quaternion/cache,
activation/contact continuation, automatic World/reaction wiring, retained
physical/restart failures and S5-S14 acceptance remain open. Existing X11 display
information is pending while implementation continues.


### Checkpoint229: scheduler blend snapshots and winning-asset restoration

The actual scheduler captures pose, native packed velocity, logical body modes
and complete raw blend metadata under its existing worker barrier/shared lock.
The capture adapter distinguishes omitted legacy metadata from explicitly present
empty metadata. It sorts target entries by body identity and validates the entire
winning blend-bearing body set. Restoration stages all four projections under the
worker barrier/exclusive lock, resolving canonical saved IDs back into winning
asset order before the existing atomic owned transaction. Absent metadata retains
version31/32/33 legacy restoration branches; a present empty set is accepted only
for assets with no blend targets. Count-complete sets also reject substituting a
known body that has no blend collision target. No raw request is inferred from an
actual logical mode. Updated the stale scheduler comment to describe actual saved
logical modes.

All three worker-count cases in blend-scheduler-save-baseline-01 execute and fail
because old capture omits metadata. Final worker0/1/2 tests queue a real movement
frame before capture, resolve reversed saved/asset order, restore into a newly
created owner, preserveF123/FFFFFFFF/unclamped and signed-zero gains, reject late
malformed/incomplete targets before pose/mode/metadata changes, then retain an
actual KEY/stored request1 and both fourth lanes on the next dispatch. The actual
PhysicsSystem fixture exercises complete snapshots and malformed metadata at
worker0/1/2. A separate adapter/serialized two-world owner fixture preserves the
same inconsistent mode/request across60 explicit subsequent dispatches; velocity
bits match uninterrupted ownership each frame. Winning subset/complete-empty,
known nonblend substitution, wrong asset and incomplete capture fields are tested.
These are software owner/scheduler tests, not normal-input gameplay, automatic
World stepping/contact or a fresh-process/original-game save acceptance.

Both blend-scheduler-save-normal/sanitized-01 builds retain a test-only vector/
array equality compilation failure; no engine test execution is claimed there.
Corrected the assertion container type. Full normal/sanitized-02 each passes906
engine tests, exact inventory and zero failures/skips. A new GCC optimization
warning in the malformed fixture's copy/clear optional-vector path was eliminated
by explicitly emplacing its intended complete empty list after terminal runs.
Final normal/sanitized-03 each rebuilds the engine targets and passes all6 affected
engine cases (including the public fixture's three internal worker courses), exact
filtered inventories and no new compiler warnings. Broader earlier builds retain
preexisting character.cpp maybe-uninitialized and actor-stats loop-copy warnings.
ASan leaks disabled. Components/Python are unchanged since228's2,402 full component
checks per build and251 Python tests; unchanged passing checks were not repeated.
Full-suite source fingerprint `aef8090f41f546a8bac5de8ebdda5bf3c7567179814cae76784834be2d79f729`; final test-only cleanup fingerprint
`b7a6b54dab8cb1886bcb851501927388f44b6750b6006deaabd6a7d4b8956879`.

Actual native-blend-adapter-wire-compare-normal/sanitized-01 each passes5,760
composed inputs and74,880 exact fields through real engine capture adapter,
version34 C++ serialization/deserialization/reencoding, all winning-asset restore
adapters and physical owned restoration. Opposite initial modes, scales1/7/native,
shape/proxy/pose/interpolation, force clearing/wakeup and deregistration are checked.
Inputs retain all216 original full-caller final request/flags/vtable mappings and
its explicit fixture gains, cyclically paired with222 original velocity outputs.
Fixture gains are not newly captured final getters; original216 declared Windows/
heap/no-World scope and both corpora/executable identities remain unchanged. This
is a composed software snapshot pipeline, not one original save/World/contact
experiment. Comparers use the full-suite fingerprint; both actual component and
engine archive hashes remain identical after the final test-only cleanup. Reports
record source/corpus/archive/binary hashes and commands; no tolerance/input change.

No M15 gate closes. Controller keys/cursors/cached gains/clocks/generated list
position and shared cache persistence are next. Renderer-global Ni ordering/cache,
raw COM/time/quaternion/cache, activation/contact continuation, automatic World
reaction/getter/frame/save wiring, retained physical/restart failures and S5-S14
acceptance remain required. Existing display information remains pending.


### Checkpoint230: complete owned controller and physical restore transaction

Added complete authored blend-controller and generated velocity-controller spans
to the physical pose/packed/mode/blend restoration transaction. Both controller
vectors are allocated/validated before the existing physical handoffs; successful
publication only swaps their prepared buffers. Authored record count/uniqueness
and winning attachment must match the existing owned controller set. Nullable
cross-target nodes remain independent of attachment and record; complete input
can be in another order and resolves back to the existing authored order.
Raw uint16 flags, finite timing/clock sentinels, ordered duplicate-time keys,
unclamped cached gains, cursor and opaque uint32 setup state are preserved. Raw
finite reversed bounds remain restorable (e.g. a missing-target/inactive native
controller); active clock admission remains separate. Empty/single-key cursors
are retained; the supported multi-key cursor must be below key-count minus one.
Generated restoration reuses the existing unchanged finite/vector/ordered-timing/
owned-attachment validation; nullable target, per-node head/tail position and
clock/delta are retained. An empty generated set clears it in a complete snapshot.
Shared clock caches remain with the runtime authority, outside this owner API.
Bullet-internal handoff allocation failure injection remains unexercised; complete
owned validation/buffering occurs before any physical publication.

All3 complete-controller-restore-baseline-01 tests execute and fail. Final tests
preserve bit patterns, duplicate keys, sentinels/cursors/setup, reversed raw bounds,
record/attachment/target independence and per-node list position; late incomplete/
duplicate/unknown/mismatched authored identities, targets, timing/clock/cache/key/
cursor and generated target/vector/clock/delta inputs reject before pose/mode,
force/sleep, old controller or raw blend metadata changes.60 explicit physical
controller phases compare fresh and uninterrupted owners, their retained cursor/
clock progression, generated list state and all packed force output bits; the
first phase explicitly checks elapsed2, previous12 and duplicate-time cursor2.
Both owners share an unstepped Bullet world; this is not a World/contact test,
serialized restart, renderer-global traversal/cache or original-game continuation.

Actual native-complete-controller-restore-compare-normal/sanitized-01 each
passes110,592 original196 full8AAD60 controller outputs through complete owned
restoration,1,216,512 exact captured fields: target/cached gain pairs, key count,
three clocks, cursor, flags and generated-controller presence. Original fixture
keys are supplied unchanged or cleared using captured key count; their contents
are inputs, not new post-return getters. Node/record mapping, logical Dynamic,
zero packed velocity, setup0/2/FFFFFFFF and generated head/tail are declared
software snapshot inputs, separately checked. Original shared-cache capture is
not restored by this owner API. Original196 source/corpus SHA256
`3ec989bec3eb2bd6bcb736a7dc6aa97ce9ee77d02c2b4dab09364145ea9e6713`,
`2cb20ee115136cf16a269a1c7939d2fa0adb721d0b02992447f178ae5e6e6c71`;
its full-clock/evaluator/reset/stop/list execution and declared Windows/interlocked
boundary remain unchanged. No new original next-phase/save/World/contact claim.
Comparator source/corpus/binary/archive and source fingerprint hashes are recorded.

Full complete-controller-restore-normal/sanitized-01 each passes2,405 component
tests, exact inventories, zero failures/skips and no new compiler warnings. ASan
leaks disabled. No new engine caller/Python format yet; the controller codec and
scheduler bridge will carry accumulated integration verification. Tested source
fingerprint `d30e018ca9919ab1f3bd7c70e3e141053cd075332f41b4bafdd93066cd61a8a5`.

No M15 gate closes. Versioned controller fields and shared physical clock-cache
save boundaries, actual scheduler/World capture/restore, renderer-global Ni order/
cache, raw COM/time/quaternion/cache, activation/contact continuation, automatic
World reactions/getters/frame, retained physical/restart failures and S5-S14
acceptance remain required. Existing display information remains pending.


### Checkpoint231: version35 complete owned controller wire metadata

Version35 adds an actor-level optional complete authored/generated controller
snapshot after version34 blend metadata. Absent legacy data differs from present
empty arrays. Authored entries carry record/attachment/nullable target, raw uint16
flags, four binary32 timing fields, three clock fields, cursor, two cached gains,
opaque setup uint32 and ordered time/gain keys. Generated entries carry attachment/
nullable target, strict head/tail bool, timing/clocks, all four native force lanes
and frame delta. Fixed authored entries are59 bytes without a target/63 with,
plus12 per key; generated entries56/60 bytes. Counts/keys are bounded and presence/
boolean markers strict. Versions1-34 retain their layouts/absence; populated data,
including a complete empty snapshot, rejects downgrading. Present controllers
require complete packed/mode/blend projections, with canonical authored record
and generated attachment order, known unique attachments/nullable targets,
finite state/keys/gains, ordered duplicate-time keys and supported multi-key
cursor. Empty/single cursors are retained. Raw reversed finite authored bounds are
preserved; generated timing remains ordered, with finite nonnegative delta and
four finite force lanes. Exact winning authored records/attachments/counts will
be resolved by the next engine adapter. No controller defaults are invented for
legacy data. Shared caches remain outside per-actor metadata.

Added value equality to the six existing timing/clock/gain/key/controller value
types so nested snapshots can compare normally; no layout or rule change. Signed
zero is tested by binary bit patterns/whole wire bytes, not ordinary float equality.
Canonical JSON emits complete nested timing/clock/key/cache/setup and generated
force/order fields only when present, preserving negative-zero values.

controller-save-baseline-01 retains an incorrect golden-size assertion (136 vs
actual140) alongside missing-code failures. Corrected golden count and nullable/
order marker offsets after terminal run. Both controller-save-baseline-02 C++
tests then execute and fail on missing version35 logic; both Python baseline
cases execute and error. Independent manual wire fixtures append a140-byte
controller suffix to an unchanged version34 prefix, including single-key opaque
FFFFFFFF cursor/setup, missing target/reversed bounds, signed zero, sentinels and
generated order/target. Separate tests cover legacy absence/current empty, every
extension truncation, strict markers/excessive counts, downgrade, incomplete body
metadata, dangling/duplicate identities, timing/clock/cache/key/cursor and force/
delta admission. Python additionally rejects booleans/noninteger identity/flag/
setup fields, invalid nested shapes and nonboolean list order.

Full controller-save-normal/sanitized-01 each passes2,407 component tests; normal
passes253 Python tests. Exact inventories, zero failures/skips; no new compiler
warnings. Broader recompilation retains preexisting inventory-test missing mOwner
initializer warnings. ASan leaks disabled. Tested fingerprint `283fcb42340bb2adb948d06ca0aa8418e0d0debddae711b6cbd4e3d8459880ee`.

Actual native-controller-save-wire-compare-normal/sanitized-01 each passes110,592
composed snapshots:1,216,512 exact original196 captured flag/clock/cache/gain/
key-count/cursor/generated-presence fields. C++ encode/decode/reencoding and
independent Python decode/reencoding preserve the entire payload byte-for-byte.
C++ canonical JSON is independently parsed and encoded by Python to the exact
same payload for every input, covering its actual nested JSON shapes and all
supplied field bit patterns. Original fixture key contents, node/record mapping,
setup0/2/FFFFFFFF, generated head/tail/state, logical Dynamic mode and zero packed
velocity are explicit software inputs, not newly captured final original getters
or original save data. Original196 executable/source/corpus identity and declared
Windows/interlocked/full-instruction boundary are unchanged. Shared cache and
original next-phase/save/World/contact/gameplay remain excluded. Reports identify
source/corpus/binary/archive hashes and commands; no tolerance/input changes.

No M15 gate closes. Actual controller capture/restore adapters and scheduler,
separate global shared physical cache persistence, automatic World save/reaction/
getters/frame, renderer-global Ni traversal/cache, raw COM/time/quaternion/cache,
activation/contact continuation, retained physical/restart failures and S5-S14
acceptance remain required. Existing display information remains pending.


### Checkpoint232: actual scheduler controller snapshot capture and restore

The physical snapshot adapter now accepts a borrowed complete authored/generated
controller view, copies all typed state into version35 actor metadata and sorts
by authored record/generated attachment. Present controllers require complete
packed velocity, logical motion and blend projections. The winning-asset restore
adapter checks the exact authored count, record and owning attachment node before
returning owned vectors in asset body order; generated targets/attachments must
resolve to known owned nodes. Mutable keys, timing, clocks, cursors, cached gains,
setup, nullable targets, force lanes and generated head/tail order are preserved,
not compared against initial authored values. Complete empty snapshots and legacy
absence remain distinct. A partial authored set is complete only when it matches
the winning asset's actual controller-bearing subset.

Actual PhysicsTaskScheduler capture waits for workers and holds the shared lock
while collecting all body/packed/mode/blend and both controller projections.
Borrowed controller spans refer to live local vectors through the adapter call.
Restore waits/holds the exclusive lock, resolves every projection and publishes
with checkpoint230's six-span transaction. Legacy absence retains the prior
four/three/two/one-span paths; no controller state is invented. PhysicsSystem's
existing forwarding path exercises this actual ownership boundary.

controller-scheduler-save-baseline-01 executes all three worker-count cases and
fails because capture omits controllers. Full normal/sanitized-01 are retained
build failures from a new adapter-test aggregate initializer in the wrong field
order; neither suite executes. After correcting it, full normal/sanitized-02 each
executes911 cases:907 pass, four new expectations fail. Those fixtures explicitly
save previous velocity time11 and advance at12, so native delta is1 rather than
the sentinel fallback.016; native force lane8 *100 *delta1 *inverseMass.5 is400.
Corrected only test expectations and added an explicit delta1 assertion after
both runs terminated. No production rules or tolerances changed.

Final full controller-scheduler-save-normal/sanitized-03 each passes911 engine
tests, exact inventory, zero failures/skips and no new compiler warnings. The
broader sanitized-02 compilation retains the preexisting actor-stats range-loop
copy warning. ASan leak checks remain disabled. Tested fingerprint `098937ca04e638c4b24d7a5a24301a5aa84ebeb5cdfbf839a0a4142e540e92fe`.
Tests cover queued-worker barriers for0/1/2 workers, exact current capture and
fresh-owner restore, signed-zero cache/opaque setup/cursor, duplicate-key cursor,
generated list position, incorrect authored identity/count, unknown target and
nonfinite late clock rollback before changed body poses publish. Actual
PhysicsSystem tests loop0/1/2 workers and check complete controller state plus
invalid restore rollback. Adapter fixtures cover complete-empty/no-authored,
proper winning subsets, wrong known attachment, wrong asset, missing prerequisite
projections and legacy absence. A serialized two-world fresh-owner fixture checks
60 explicit physical-controller phases, restored authored/generated clocks/state
and all eight velocity lane bits, with separately supplied identical cold shared
caches. No Bullet world/contact step is implied by those explicit phases.

Actual native-controller-adapter-wire-compare-normal/sanitized-01 each passes
110,592 composed original196 controller outputs:1,216,512 exact captured fields
through winning-asset capture adapter, C++ version35 encode/decode/reencode, every
winning-asset restore adapter and the complete six-span owner restore. Full
production engine/component archive hashes match the final successful builds;
the two intervening corrections affect test construction/expectations only.
Original fixture keys, owned node/record identities, setup0/2/FFFFFFFF, generated
head/tail/state, logical Dynamic mode and zero packed velocities are declared
software inputs, not new original post-return getters or save data. Original196
hash/boundary scope is unchanged. Shared-cache/original next-phase/native-save/
World/contact/gameplay evidence is excluded. Previous version35 independent
Python binary/JSON interoperability remains unchanged and was not rerun.

No M15 stage closes. Next is separate global shared physical-cache metadata and
transactional multiowner restoration at the scheduler authority boundary, then
automatic World save/reaction/update/getter/frame wiring. Renderer-global Ni
traversal/cache, raw COM/time/quaternion/cache, activation/contact continuation,
retained physical/restart failures, normal-input display acceptance and S5-S14
remain required. Original checkout metadata is protected; this chunk is committed
as exact matching source bytes in the existing writable progress repository.


### Checkpoint233: version36 global physical clock-cache metadata

RuntimeState now optionally retains the scheduler-wide PhysicalBlendTimeCache,
separately from every actor snapshot. This is the physical controller cache;
renderer-global Ni caching is a different unresolved authority. No owner is
required for present data: the scheduler can retain its cache after the last
ragdoll is removed. Legacy versions1-35 preserve their exact layouts and do not
invent cache state. Populated state, including the default raw sentinel, rejects
downgrading to35. Version36 appends a strict presence byte to the complete
version35 payload, then raw uint32 cycle and binary32 stop/start/key/result fields
when present:21 bytes present or1 byte absent. All four floats must be finite;
cycle remains an arbitrary raw uint32, and finite reversed cached bounds are
preserved rather than treated as active controller timing. JSON emits the exact
five-field native_physical_blend_time_cache object only when present; scalar
negative zeros are retained. Python validates exact shape/int-not-bool cycle,
range, finite binary32 values and downgrade. Added cache value equality only;
signed-zero proof uses actual bits and independent whole-payload byte fixtures.

Both physical-cache-save-baseline-01 C++ cases execute and fail on unsupported
version36; both physical-cache-save-python-baseline-01 cases execute and error on
the same missing codec support. Manual21-byte suffix fixtures use FFFFFFFF raw
cycle, reversed1/-1 bounds, negative-zero key time and-.25 result, appended to an
unchanged version35 prefix with its version header updated. Tests cover all raw
cycle boundaries, legacy/current absence, every extension truncation, marker2,
all four float nonfinite fields through encoding and C++ decoding, populated
legacy downgrade, Python bool/noninteger/range violations and extra/missing or
incorrect nested shape. Existing older-schema goldens remain unchanged.

Full physical-cache-save-normal/sanitized-01 each passes2,409 component tests;
normal passes255 Python tests, exact inventory with zero failures/skips. Broader
recompilation retains only the preexisting inventory missing-mOwner initializer
warnings. ASan leak checks disabled. Tested fingerprint `6ece135ec4a38373b6f0b90bf6a0ff8cdd29ade8e1a9255df1c69ae4fd5aa61b`.

Actual native-physical-cache-save-wire-compare-normal/sanitized-01 each passes
110,592 original196 captured cache outputs,552,960 exact fields:raw cycle and all
four cached binary32 values. C++ version36 encode/decode/reencode, independent
Python decode/reencode and C++ canonical JSON to Python encoding preserve the
whole payload byte-for-byte for every input. Optional global location/presence
and player envelope are software snapshot policy, not original save output.
Original196 executable/source/corpus identity and full-instruction/declared
Windows/interlocked boundary remain unchanged. No original native-save/next-phase,
World/contact/gameplay or renderer-global Ni cache claim. Sources/commands/archive
hashes are recorded; unchanged engine integration was not rebuilt in this codec
chunk.

No M15 gate closes. Global cache publication and consistent multiowner snapshots
at the scheduler/World authority barrier still need implementation and engine
verification. Automatic World lifecycle/reaction/update/getter/frame wiring,
renderer-global traversal/cache, raw COM/time/quaternion/cache, activation/contact
continuation, retained physical/restart failures and S5-S14/normal-input acceptance
remain required.


### Checkpoint234: publish saved shared physical cache through scheduler authority

PhysicsTaskScheduler now exposes restoreNativeBlendTimeCache, forwarded by the
actual PhysicsSystem. Restore waits for workers, holds the exclusive physics
lock, checks all four cached float fields for finiteness before publication and
then assigns the complete cache. Raw uint32 cycle, finite reversed cached bounds
and scalar signed zeros retain version36's raw snapshot contract; no controller
or body state is changed. Active clock admission remains the prior helper rule.
The cache can be restored without any actor owners, and removing the last owner
does not discard it. This operation publishes only global cache state; it is not
a transaction covering the separate per-actor restore calls.

physical-cache-scheduler-baseline-01 executes three worker-count cases0/1/2 and
fails exactly at the fresh scheduler's unchanged default cache after no-op
restore. The fixture first advances a reversed producer controller at time11
with start/previous10, ordered keys0[1,0],4[0,1] and interval0/4. This produces a
cache with cycle2, key1 and result3. It saves that cache in version36 with no
ragdoll owners, destroys the scheduler, starts a new one, restores a distinct
consumer's initial normal controller state, queues a movement frame and then
restores the saved cache. The consumer at11 reuses result3 because native cache
identity omits the reverse flag, yielding hierarchy/velocity gains.25/.75;
a cold recomputation would yield.75/.25. Each late nonfinite cache field rejects
without changing captured cache/body/controller state. No-owner restoration of
FFFFFFFF cycle, reversed finite bounds and negative-zero key time also passes.
Actual PhysicsSystem's existing fixture loops0/1/2 workers and exercises saved
cache publication, signed-zero bits, nonfinite-result rollback and restoration
of the prior cache before its remaining checks.

Full physical-cache-scheduler-normal/sanitized-01 each passes914 engine cases,
exact inventory and zero failures/skips. Tested fingerprint `ba0557e2a7dd5c788eb3b756e3aca7545c37597505f2ee5479c869e85fee1e52`. ASan leak
checks remain disabled. The baseline's broader recompilation retains existing
character.cpp maybe-uninitialized and actor-stats range-loop copy warnings;
final run warning inspection is recorded in the evidence. No physics arithmetic
rule or tolerance changes. Version36's unchanged2,409 component/255 Python and
110,592 original-cache binary/JSON interoperability comparisons remain recorded
at checkpoint233 and were not repeated solely for this forwarding integration.
The original194/196 cache identity and instruction-boundary provenance remains
unchanged; this explicit engine phase is not an original fresh-process restart
or contact/world/gameplay acceptance course.

No M15 gate closes. Consistent multiowner capture/publication and automatic World
save/lifecycle/reaction/update/getter/frame wiring remain to implement. Renderer-
global Ni traversal/cache, raw COM/time/quaternion/cache, activation/contact
continuation, retained restart/physical failures, normal-input display acceptance
and S5-S14 remain required.


### Checkpoint235: separate complete owner restore preparation and publication

ActorRagdollPhysics now exposes an opaque noncopyable PreparedRestore token.
prepareRestore validates and owns the complete body projection, authoritative
packed velocities, logical modes, blend target metadata and authored/generated
controller buffers without publishing any physical state. Caller buffer lifetime
ends at preparation: later edits cannot change the prepared snapshot. It retains
the original owner/implementation identity. commitRestore rejects a foreign or
consumed token before mutation, applies changed modes, publishes the prepared
physical/packed state, swaps the already prepared metadata/controller vectors
and consumes the token after successful publication. The original owner must
stay alive and synchronized between preparation and commit; the token does not
permit use after destruction or a concurrent lifetime change. The existing
complete six-span restore now prepares and commits through this same path.
Legacy one/two/three/four-span routes retain their prior behavior.

All owned buffer allocation and validation precedes publication. This allows a
caller to prepare every actor before changing the first, instead of invoking
each complete restore sequentially and discovering a bad later snapshot after
earlier owners changed. The scheduler-wide cache remains a separate authority.
This lower API alone is not a complete multiowner or World save transaction.
Actual Impl::setMotion changes mass/flags/activation/inertia in place; it does not
remove/re-add bodies or allocate owned buffers. Physical publication calls
updateSingleAabb and therefore the Bullet broadphase. Internal Bullet allocation
failure was not injected or audited as a no-throw guarantee; no OOM rollback or
noexcept publication claim is made.

prepared-controller-restore-baseline-01 retains an executed first-test crash:
after expected no-op restore failures, the new test indexed the empty generated
controller list. Added a size assertion after the terminal run; no implementation
change. All three corrected prepared-controller-restore-baseline-02 cases execute
and fail on missing staging validation/publication and token ownership/consumption.
Tests then cover untouched pose/mode/controllers after preparation, caller-buffer
edits before commit, actual prepared pose/packed signed-zero/mode/raw requested
metadata/controller fields, six late-invalid categories in a second owner while
neither publishes, successful later publication of both and rejection of a wrong
owner or already consumed token before changes.

Full prepared-controller-restore-normal/sanitized-01 each passes2,412 component
tests, exact inventory, zero failures/skips. Tested fingerprint `41002b0fc3c633f6f9a0b9ae7ce3a87cd56f49bfc949cb2e8cd4415f4a91ca6c`.
ASan leak checks disabled. Unchanged Python codecs and engine forwarding are not
rerun in this lower owner transaction chunk.

Actual native-prepared-controller-restore-compare-normal/sanitized-01 each passes
110,592 original196 composed controller outputs through the explicit preparation
and commit API,1,216,512 exact captured fields. Captured gain/cache/clock/cursor/
flags/key-count/generated-presence fields and original executable/source/corpus/
Windows-interlocked boundary are unchanged. Original fixture keys, owned node/
record mappings, setup0/2/FFFFFFFF, generated head/tail/state, logical Dynamic
mode and zero packed velocity remain declared software inputs, not new original
save/getter captures. Shared cache, original next-phase/native-save/World/contact/
gameplay are excluded. Component archive hashes and comparator commands/sources
are recorded. Existing complete restore tests also exercise the refactored
six-span wrapper and its prior60 explicit fresh-owner controller phases.

No M15 stage closes. Next is scheduler/PhysicsSystem coherent multiowner capture
and restoration with global cache at one worker barrier and lock, followed by
automatic World lifecycle/save/reaction/update/getter/frame wiring. Renderer-
global traversal/cache, raw COM/time/quaternion/cache, activation/contact
continuation, retained physical/restart failures, normal-input display acceptance
and S5-S14 remain required.


### Checkpoint236: prepared legacy restore variants preserve absent metadata

PreparedRestore now supports all existing one/two/three/four-span legacy
projections as well as the complete six-span controller snapshot. Its optional
owned packed/mode/blend/controller buffers distinguish absent fields from a
present complete-empty set. Spatial-only preparation validates and owns the
world pose/velocity projection. Packed preparation replaces spatial XYZ with
native authoritative velocities before validation, retaining the prior rule
that unused conflicting spatial velocity fields do not override packed state.
Mode preparation validates complete ordered logical1/6 requests; blend
preparation validates and owns the complete winning blend-target set. Complete
controller preparation adds both validated authored/generated vectors. None
publishes during preparation.

Commit applies only present metadata. Without packed state it publishes the
legacy spatial projection and retains the owner's separately stored native
fourth velocity lanes. Absent modes, blend metadata or controllers leave existing
owner fields untouched; no defaults are invented. Present complete-empty metadata
is still published. Wrong/consumed token admission and original-owner lifetime/
synchronization contract remain unchanged. All five existing restore overloads
now use the same appropriate prepare-and-commit path, preserving prior spatial,
packed-authority, mode, force/activation/interpolation and metadata semantics.
This removes the legacy preparation gap before aggregate scheduler restoration;
no runtime schema, JSON field or World save format changes.

All three prepared-legacy-restore-baseline-01 cases execute and fail because the
new legacy preparations return no token. Added tests cover spatial-only
restoration at length scale2 with world XYZ7/8/9 becoming native3.5/4/4.5,
preserved native W8/angular negative zero, existing Keyframed mode, raw requested
motion FFFFFFFF and authored/generated state; packed-only/packed+mode/
packed+mode+blend variants preserve absent controllers and publish only selected
metadata; late invalid pose, packed fourth lane, logical raw2 mode and blend gain
reject without physical publication. Existing lower restore tests continue
checking authoritative packed XYZ, late rollback, force clearing/activation,
interpolation, BodyT, identities and complete controller continuation.

Full prepared-legacy-restore-normal/sanitized-01 each passes2,415 component tests,
exact inventory and zero failures/skips. Tested fingerprint `ef4b9328fe31ec3871ec7d1af3e8ec97c83f04f564f2b860443afa03549b3c24`. ASan leak
checks disabled. Unchanged Python codecs and scheduler forwarding are not rerun.

Actual native-prepared-legacy-controller-restore-compare-normal/sanitized-01 each
passes110,592 original196 composed controller outputs through complete explicit
preparation/commit after the optional-buffer refactor,1,216,512 exact captured
fields. Captured gains/cache/clock/cursor/flags/key-count/generated-presence and
original executable/source/corpus/declared Windows-interlocked boundary are
unchanged. Input key contents, owner IDs, setup0/2/FFFFFFFF, generated order/state,
logical Dynamic mode and zero packed velocities remain explicit software
fixtures, not newly captured original save/getter outputs. Shared cache and
original next-phase/native-save/World/contact/gameplay remain excluded.
Internal Bullet broadphase allocation failure remains unexercised; preparing
owned buffers is not a noexcept/OOM rollback claim.

No M15 gate closes. Next is coherent scheduler/PhysicsSystem capture and
restoration of every bound owner plus the global cache at one worker barrier and
lock, choosing matching legacy/current preparations. Automatic World lifecycle/
save/reaction/update/getter/frame wiring, renderer-global traversal/cache, raw
COM/time/quaternion/cache, activation/contact continuation, retained physical/
restart failures, normal-input display acceptance and S5-S14 remain required.


### Checkpoint237: coherent scheduler group snapshots and shared cache

Added NativeRagdollSnapshotGroup with canonical actor-key map and optional shared
physical time cache, and borrowed call-lifetime bindings carrying Ptr/stable actor
key/base/model. PhysicsTaskScheduler capture waits once, holds one shared physics
lock and captures every bound owner's complete body/packed/mode/blend/authored/
generated projections plus the shared cache. Bindings must cover exactly every
owned ragdoll, including the zero-owner case, with unique known nonempty Ptr
references, unique canonical nonnull actor keys, canonical nonnull base keys and nonempty model labels.
Actor/base/model labels come from the winning caller context; this physics API
cannot infer the World profile store's base/model from a Ptr. The existing
winning-asset adapter retains hash/record/node binding validation. Controller
span inputs remain backed by live local vectors through each capture call;
per-actor public methods are not recursively called under the group lock.

Restore waits once and holds one exclusive lock throughout. It validates the
complete bindings, exact saved actor set and finite present global cache fields,
resolves every winning-asset projection and prepares all owner tokens before the
first physical publication. Matching legacy/current branches choose one/two/
three/four/six-span preparation. Only after all owned validation/storage succeeds
does it commit each owner and publish a present shared cache. Absent legacy cache
preserves the existing one. Current empty-owner capture retains a present cache.
Unknown, missing, duplicate or incomplete bindings, bad later actor data or late
cache admission cannot publish earlier actor poses. Internal Bullet broadphase
allocation failure remains unexercised; this is not a noexcept/OOM rollback claim.
Actual PhysicsSystem forwards both operations through this scheduler boundary.

ragdoll-group-save-baseline-01 retains an insertion-script failure: the
PhysicsSystem header names updatePtr rather than updateActorRagdollPtr. Only the
group DTO was inserted before that exception; its subsequent helper builds but
selects no tests. It is not an executed group baseline. After correcting the
marker and guarding the already inserted DTO, all nine ragdoll-group-save-
baseline-02 worker0/1/2 cases execute and fail on missing group capture. Correct
late-owner rollback tests use the last binding, not the last canonical map key:
bindings are deliberately unsorted20/10, so key ordering must not turn a purported
late failure into the first prepared actor.

Tests cover queued-worker group capture, canonical actor ordering, complete
two-owner/cache restore, first-owner changed pose with bad second-owner authored
identity/packed lane, nonfinite cache, missing or unexpected actor, incomplete/
duplicate/empty bindings, legacy absence and zero-owner retained/restored cache.
Actual PhysicsSystem's fixture loops0/1/2 workers and checks group forwarding,
changed pose/cache capture, invalid cache rollback and original-group restoration.
Three additional worker-param cases serialize the two-owner group and global
cache in a valid software version36 envelope with linked reference/value/life
companions, restore into a fresh scheduler in a different Bullet world, and compare
60 explicit controller phases against the source. Each phase compares complete
group state and all eight packed velocity lane bits per owner. Initial cache
cycle2/key1/result3 makes a normal consumer reuse reversed-producer result3 and
gain.25; a cold recomputation would give.75. Neither Bullet world is stepped.
The envelope is a software fixture, not automatic World saving or original-game
fresh-process/contact/gameplay acceptance.

Full ragdoll-group-save-normal/sanitized-01 each passes926 engine cases but
retains a new dangling-else warning from an unbraced if around a GoogleTest macro.
After both terminal runs, added braces only in that test. Final full ragdoll-group-
save-normal/sanitized-02 each passes926 engine cases, exact inventory, zero
failures/skips and no new compiler warnings. Broader sanitized-01 recompilation
retains the preexisting actor-stats range-loop copy warning. ASan leak checks
disabled. Final tested fingerprint `cf605484016d8205d6f1b0398b54b7c378b486cb776082fc5b3b9b465c243670`. Unchanged lower2,415 component,
255 Python and original110,592 controller/cache comparisons remain recorded at
preceding checkpoints; no new native arithmetic or runtime codec rule changed
in this scheduler grouping chunk.

No M15 stage closes. Automatic World capture/restore and actor lifecycle/reaction/
update/getter/frame wiring remain required, including joining physical projections
with retained unloaded actor/reference/value/life authorities. Renderer-global Ni
traversal/cache, raw COM/time/quaternion/cache, activation/contact continuation,
retained physical/restart failures, normal-input display acceptance and S5-S14
remain required.


### Checkpoint238: enumerate current physical owners through the worker boundary

Added the read-only actorRagdollOwners query to PhysicsTaskScheduler and actual
PhysicsSystem forwarding. It waits for queued movement workers and reads the
complete owned ragdoll map under one shared physics lock. The returned vector
contains each owner holder's current Ptr, including its current cell binding;
it owns no actors and promises no hash-map ordering. It does not include normal
capsule actors or synthesize stable native identities/model labels. Callers must
use these borrowed references on the main thread and resolve native bindings;
the existing group snapshot API separately checks complete ownership coverage.

All three worker0/1/2 ragdoll-owner-enumeration-baseline-01 cases execute and fail
because the baseline stub returns an empty list after two owners are admitted.
The implemented tests enumerate after queued movement, check two distinct owners,
rebind one reference, verify the stale reference disappears, prove stale removal
cannot remove the replacement, remove each current owner and confirm an empty
list and zero owned collision objects. The actual PhysicsSystem fixture also
checks zero owners before admission, exclusion of a normal capsule, current owner
after physical handoff and the rebound reference. An intermediate private edit
helper failed on an overly narrow test insertion marker after implementing the
scheduler method; no build ran on that partial attempt. Corrected the marker
before the final test runs.

Full ragdoll-owner-enumeration-normal-01 and sanitized-01 each pass929 engine
cases with exact inventory agreement, zero failures/skips and no compiler
warnings. ASan leak checks disabled. Tested fingerprint: `07f4495e249f925ab6eb6eee93c66713508d4e10616ed47680aaa91c891b3eea`.
No native arithmetic, lower component code or binary/JSON codec changed, so
unchanged component/Python/original-instruction comparisons are not repeated.

No M15 stage closes. This supplies the complete physical owner discovery needed
by World save/lifecycle joins; automatic World capture/restore and reactions
still require implementation and acceptance. Retained unloaded authority,
winning base/model/asset identity, renderer-global traversal/cache, native raw
pose/time/activation/contact continuation, retained numerical failures,
normal-input runtime gates and all pending S5-S14 requirements remain open.


### Checkpoint239: actual World save refreshes loaded physical projections

World::captureOblivionRuntimeState now invokes a physical join after capturing
the combat service and before final RuntimeState validation. With initialized
physics it enumerates every current owner, resolves the projected Player to
dynamic(player,1) and other native actors to their reference keys, and requires
matching cached pose, native actor values/life and current base. It resolves the
current corrected class model; a projected Player's explicit custom model uses
the actor-model resource correction path. A stale model label cannot be passed
back to physics as if it were current. Models live in a reserved owning string
vector throughout borrowed group bindings. The coherent scheduler group captures
all loaded physical projections and the shared clock. World checks every loaded
asset hash, body count and record/node identity against the admitted authority
before overlaying the local save map. Other retained poses remain in that map;
no cached combat service pose is mutated by saving. Final state validation checks
the reference/value/life joins. Empty initialized physics still saves its global
clock. Without initialized physics, a retained T4ST clock is preserved; a fresh
headless World has no invented clock.

Factored the existing World-owned physics construction into initializePhysics,
returning its borrowed PhysicsSystem reference and rejecting duplicate explicit
initialization before replacing any owner. Normal World::init calls that method.
This permits headless actual World-owned physics tests without introducing a
test-only friend or injecting an unrelated subsystem. It is not full renderer/
navigator/scene initialization or automatic native reaction admission.

Two new engine tests loop worker counts0/1/2. The live-owner test uses editable
synthetic mesh/VFS/resource inputs, an actual World/PhysicsSystem, native NPC and
projected Player physical owners, native reference/value/life companions and a
retained resident NPC pose with no physical owner. It changes NPC/Player body
positions, native packed W lanes and global cache after service capture, proves
World save refreshes both loaded poses and preserves the other pose, checks a
negative-zero angular W bit, verifies binary version36 roundtrip and confirms
service caches remain unchanged. Missing authority, valid-but-wrong hash/model,
body record, node and count all reject capture. The retained actor in this test
is resident/nonphysical, not a demonstrated unloaded-cell gameplay continuation.
The empty-owner test checks fresh absent cache without physics, readT4ST retained
cache preservation without physics, initialized physics superseding that clock,
duplicate initialization ownership, empty ragdolls and binary clock roundtrip.

world-physical-save-baseline-01 retains a compile failure caused by an incorrect
assumption that World exposes getPhysicsSystem; its actual getter is the
RayCastingInterface. Returning the initialized subsystem corrected the fixture.
Baseline-02 executes both tests: empty-owner capture fails on absent cache;
live-owner setup first fails on a duplicate projected Player record ID. After
using a distinct managed record, baseline-03 retains a headless fixture crash:
Player capsule scale construction accesses the camera when the Player already
has a cell. The fixture now constructs its capsule before assigning the cell,
then binds the current Ptr before physical handoff/saving. It does not exercise
camera-dependent initial scaling. Baseline-04 reaches stale NPC/Player snapshots,
missing cache and missing-owner rejection but stops its live loop at a malformed
wrong-hash fixture. Changing only the final hash digit retains the required hash
format. Baseline-05 executes both tests without setup exceptions: the live case
runs all three worker loops and fails on stale poses, packed negative-zero W,
cache omission and accepted invalid bindings; the empty case demonstrates
retained clock loss and initialized cache omission at worker0 before its fatal
presence assertion. These are actual missing-behavior failures, not skipped cases.

First full normal/sanitized implementation attempts retain a compile failure
from spelling REC_NPC instead of this repository's REC_NPC_. Corrected that
constant, with no expectation/tolerance changes. Final full
world-physical-save-normal-02 and sanitized-02 each pass931 engine cases with
exact inventory, zero failures/skips. Normal broader recompilation retains a
GCC16 maybe-uninitialized diagnostic in unchanged CharacterController queue
restore; unchanged-character-warning.json compares both character source/header
bytes against checkpoint238 and records their hashes. All queue entry fields
are assigned before insertion on inspection; no new World-source diagnostic is
reported. Sanitized build has no compiler warnings. ASan leak checks disabled.
Tested fingerprint `05c6771acc0b0c7055727739256c68786249dd1824333f41a0b4e7ad959ad725`. No lower component/native arithmetic/codec changed,
so unchanged component/Python/original-instruction comparisons are not repeated.

No M15 stage closes. This is automatic World saving of already admitted physical
owners, not automatic World begin/update/end, reaction/getup implementation,
winning initial morphology or INI producers, fresh-process physical restoration,
renderer-global Ni cache/traversal or raw COM/time/quaternion/activation/contact
continuation. True unloaded-cell lifecycle joins, retained numerical failures,
normal-input display acceptance and pending S5-S14 remain required. Next connect
the saved physical cache/owner projections to World restore and lifecycle before
claiming resumed simulation or gameplay acceptance.


### Checkpoint240: import the saved physical clock at World initialization/apply

World::initializePhysics now constructs its candidate subsystem locally, imports
a present retained Oblivion T4ST physical time cache through the scheduler's
validated barrier, and only then publishes mPhysics. The duplicate-initialization
guard remains before construction/publication. Fresh or legacy absent cache
leaves the candidate's initial clock intact. World::applyOblivionRuntimeState
imports a present cache only after existing detached validation and World/script/
AI/combat restoration have accepted the state. An absent legacy cache preserves
the current scheduler-owned clock. Existing RuntimeState reading validates the
saved cache, and the scheduler validates all four float fields before assigning
the complete clock. These changes do not create or restore actor body graphs.

world-physical-cache-restore-baseline-01 executes two engine tests, both looping
worker0/1/2 without fixture exceptions, and fails on missing clock import at
initialization and state apply. The existing empty-owner World save test now
expects its retained T4ST clock to be imported before replacing that live clock.
The new WorldApplyRestoresPhysicalCacheAfterValidationAndPreservesLegacyAbsence
test restores a raw cycleFFFFFFFF/reversed-bounds/signed-zero clock, changes the
live clock and applies the saved World state, checks the imported clock and its
negative-zero bit and confirms World resave captures it. Applying version35 with
no cache preserves the live clock. A finite but unsupported native Player integer
modifier causes detached restore rejection before changing native values, World
time or the physical clock, even when the incoming state asks for changed time
and cache. Tests use actual World-owned PhysicsSystem without physical actors;
they do not prove loaded body/controller/contact continuation.

Full world-physical-cache-restore-normal-01 and sanitized-01 each pass932 engine
cases with exact inventory, zero failures/skips and no compiler warnings.
ASan leak checks disabled. Tested fingerprint `8c29b7e66ad98ca812ca32d1dde539861d72048dfd6bb33c5ae61bc8b0715006`. No lower component,
native arithmetic or runtime codec changed, so unchanged component/Python and
original-instruction comparisons remain at their prior evidence checkpoints.

No M15 stage closes. World capture and global physical clock import are connected,
but automatic reaction/admission/update/end/getup, body/controller restoration,
true unloaded-cell lifecycle joins, renderer-global Ni traversal/cache, native
raw pose/time/activation/contact continuation, retained numerical failures,
normal-input runtime acceptance and all pending S5-S14 remain required. Next
stage complete body/controller restore before World publication and validate
physical owner lifetime across that staged boundary.


### Checkpoint241: staged physical group restore with lifetime validation

Added an opaque noncopyable PreparedNativeRagdollSnapshotRestore token and actual
PhysicsSystem prepare/commit forwarding. Scheduler preparation waits for workers,
takes a shared physics lock, validates complete unique bindings and optional
cache, resolves every winning-asset projection and owns every lower one/two/
three/four/six-span prepared restore plus a copy of the optional cache. It does
not publish bodies/controllers/clock or retain borrowed caller spans/models.

Scheduler and each ActorRagdoll have an independent shared lifetime marker;
prepared data holds weak markers and borrowed Ptr identities, not strong body
owners. Commit waits and takes an exclusive lock, rejects consumed or foreign/
destroyed scheduler identity, requires the exact current owner count and checks
every pending Ptr membership/lifetime marker before invoking any lower commit.
Removed/readmitted actors cannot be accepted by recycled owner addresses or
unchanged raw reference keys; rebinding cannot silently redirect an old token.
Weak markers do not retain removed Bullet bodies. Only after all markers accept
does commit publish every owned projection and a present global clock, then
consume the token. Legacy absent clock remains unchanged. Prepared storage
cleanup does not dereference a destroyed physical owner. Internal Bullet
broadphase allocation failure remains unexercised; this is not a noexcept/OOM
rollback guarantee.

Extracted locked preparation/publication helpers so the existing immediate group
restore retains its single worker wait/exclusive lock across all staging and
publication. It does not call separately locked public prepare/commit methods.
The separate deferred API supplies the World prevalidation boundary; World does
not yet invoke this body/controller token in this checkpoint.

All nine ragdoll-prepared-group-baseline-01 worker0/1/2 cases execute and fail
because preparation returns no token. Implemented tests verify read-only
preparation, owned caller-buffer copies, complete body/cache commit and a raw
negative-zero angular W bit, consumed-token rejection, late owner removal/
readmission/rebinding rejection before changing an earlier owner, immediate
collision-object release despite a pending token, malformed later packed data,
foreign scheduler, destroyed scheduler and fresh replacement, empty owners,
legacy cache absence and all four invalid cache fields. A supplementary test
admits a third owner after preparation while both original owner markers remain
alive; the old two-owner token rejects the expanded set without publishing any
body or clock. The actual PhysicsSystem fixture loops worker0/1/2, prepares its
changed authored-controller group, confirms no publication, commits and rejects
a second commit.

Full normal/sanitized-01 each executes941 cases with938 passing and three
failures: the new invalid packed-body assertion expected invalid_argument, but
the existing RuntimeActorRagdoll validator correctly throws runtime_error.
Changed only that assertion to the established exception type and added the
expanded-owner-set regression. No production algorithm, numeric tolerance or
rejection policy changed. Final full ragdoll-prepared-group-normal-02 and
sanitized-02 each passes941 cases with exact inventory, zero failures/skips and
no compiler warnings. ASan leak checks disabled. Tested fingerprint `3b9fec742e42b4aae6c57d024e99a68c28d1c6716531f0e61c202284fe8125be`.
Unchanged lower component/native arithmetic/codec/Python comparisons remain at
their prior checkpoints; this change only stages and validates engine ownership.

No M15 stage closes. Next connect this group token to World restore before
globals/player publication and join restored native actor lifecycles with live
physical projections. Automatic reaction/admission/update/end/getup, true
unloaded-cell continuation, winning morphology/INI producers, renderer-global
Ni cache/traversal, raw pose/time/activation/contact continuation, retained
numerical failures, normal-input runtime acceptance and pending S5-S14 remain
required.


### Checkpoint242: stage loaded physical projections before World restore publication

World capture and restore share resolveNativePhysicalBindings with a noncopyable
owning context: reserved strings back borrowed model views, and vector moves
retain that backing storage through the call. It resolves current native bases/
models, Player identity and saved pose/value/life ownership without adopting a
stale label. Capture retains its prior complete asset/body validation and local
map overlay. World apply uses detached preparedCombat with the incoming saved
state and collects only the currently owned physical actors into a group; other
saved poses remain in the complete restored native authority.

After existing World preflight and before global/player/reference publication,
World prepares the complete loaded group through checkpoint241. Winning-asset
hash, body identities, authored controller identities and every controller/body/
cache buffer are resolved without physical publication. At the end of accepted
World/script/AI/combat installation it commits that group, including a present
shared clock. Zero-owner and legacy absent-clock paths preserve the preceding
clock contract. Native authority and the physical projection no longer resume
with different loaded pose/controller snapshots merely because save used a
fresher physics capture than the service cache.

Existing live graphs cannot be migrated by bare CellStore/reference metadata
mutation. Preflight requires the loaded Player's current cell/race/gender and
stored Player model to match the pending context; loaded NPCs require the same
resident binding/cell, an enabled nondeleted saved reference and unchanged
explicit reference scale. Changes requiring scene unload/readmission reject
before World publication. This prevents retaining an old physical pointer or
geometry across a metadata move; it does not implement automatic readmission.
Normal scene/lifecycle reconstruction and unowned saved actor admission remain
required for full gameplay/save-load acceptance.

world-physical-owner-restore-baseline-01 executes the new actual World test
through all worker0/1/2 loops without setup exceptions. It fails on missing body/
authored/generated controller restoration and accepted late hash/controller/
model mismatches that change World time and physical cache. The fixture owns
native NPC and projected Player ragdolls plus a retained resident/nonphysical
NPC pose. The projected Player's managed model is installed under the actual
Player record so normal World metadata restore retains that model; it is still
an editable synthetic fixture, not a stock asset/runtime oracle.

The test saves changed positions, native packed lanes (including negative-zero
angular W), requested blend/gains, authored clocks/cached gains/opaque setup and
single-key cursor, generated velocity attachment/order/clock/force/delta and the
shared clock, then changes the live projections and applies T4ST. It compares the
complete physical group and World resave and verifies the other pose survives.
Late mismatch uses the actual scheduler owner's last binding, not canonical map
ordering. Added six readmission cases: NPC scale/disabled/cell, Player cell/race/
gender, along with the three hash/controller/model cases. Each incoming state
also requests changed time/cache/pose and native Player damage; all nine failures
retain physical group, World time, native values, reference position/scale/
enabled state. Saved restoration is repeated between cases.

Full normal/sanitized-01 each passes942 cases. Added the readmission coverage;
normal/sanitized-02 each also passes942 but retains a new test variable-shadow
warning. Renamed only that loop variable after both runs terminated. Final full
world-physical-owner-restore-normal-03 and sanitized-03 each passes942 with exact
inventory, zero failures/skips and no compiler warnings. ASan leak checks
disabled. Tested fingerprint `80779b9297cc4e8a3f390d577c20b087c10fb2350fc17507f9314f8ae3b92157`. No lower native arithmetic or codec changed;
unchanged component/Python/original-instruction comparisons are not repeated.

No M15 stage closes. This restores already admitted physical graphs in a
compatible existing context, not automatic body creation, scene readmission,
reaction/update/end/getup or renderer pose publication. True unloaded-cell and
fresh-process admission, original initial morphology/INI producers, full Ni
traversal/cache, raw pose/time/activation/contact continuation, retained numerical
failures, normal-input gameplay acceptance and pending S5-S14 remain required.


### Checkpoint243: atomic multi-owner native physical projection retention

Checkpoint242 committed as `adec77814922d1b92d52b9c779df89946c7b017c`,
239 isolated commits, tree `8ce4250c120fd1b5e85e543438794a6544b50eb0`;
bundle220 SHA256
`8aad5b45eb04cd4349ef6d51d8e4196e34cee4e56e287607f5992fd6e181c3d5`.

OblivionCombatService::syncActorRagdolls accepts an ordered group of expected/
updated optional poses. It checks every expected live pose before validating
updates, then validates every proposed pose and native value/life/base/asset/
body identity using the same helper as single-owner publication. All candidate
map copies, erasures and insertions finish before one nonthrowing map swap.
Stale input returns false; invalid input throws without changing any owner.
Unmentioned actors, native values/life, action IDs, events and RNG remain in the
existing authority. Null updates release cached projections; null expected
poses admit only absent owners. Empty groups succeed without allocating.
The existing single-owner path retains its previous publication and performance
contract; it does not copy the complete map.

The new baseline test executes seven later-owner stale/invalid inputs against
an intentionally sequential group wrapper. Each produces an earlier-owner
mutation, demonstrating the missing atomic behavior rather than a setup fault.
Implementation coverage includes expected absence/stale position; wrong base,
asset hash, body record, quaternion and model; valid two-owner updates; an
unmentioned third owner; empty groups; mixed release/admission; and binary
restart. Each failure compares the whole service save bytes and verifies an
allocated pending action remains pending, beyond merely checking body positions.

Full native-physical-group-publication-normal-01 and sanitized-01 each pass943
engine tests with exact inventory, zero failures/skips. Tested fingerprint
`7be279a0f2ce765da557e48b38eab0083d5b5bfaac6543460ee821b07406560e`; ASan leak checks disabled and UBSan halts. Baseline broad recompilation
retains the earlier CharacterController maybe-uninitialized and actor-stat test
range-copy diagnostics; sanitizer broad recompilation retains the range-copy
diagnostic. Warning source/header bytes are verified unchanged from242 in each
final run's unchanged-warning-sources.json. No new-source warning is present.
Lower component arithmetic, codecs and Python sources are unchanged and their
passing comparisons are not repeated.

This is the native authority transaction needed before retaining current
physical state across scene teardown. It does not yet wire cell unload or object
removal, recreate unloaded physical owners, drive reaction/controller/render
updates, or pass original-game/normal-input/fresh-process gameplay gates.
No M15 stage closes; pending S5-S14 and earlier retained failures remain open.


### Checkpoint244: retain physical projections before scene teardown

Checkpoint243 committed as `2c37b41f43d4d1b7596e8d2b9b007ded3b89594f`,
240 isolated commits, tree `a8a9a590197c4673ba0057c00841964adb3e86f0`;
bundle221 SHA256
`073cfde7d593cf713a82b2288efffd81fe598faf9b72dda556869841e39b0729`.

World::retainOblivionPhysicalState captures the native combat authority into
local storage, captures and validates all currently owned physical projections,
then uses checkpoint243 to publish the complete loaded-owner group atomically.
It resolves current bases/models/Player identity using the existing save join,
checks all asset/body identities before mutation, and updates even numerically
equal negative-zero lanes. Nonphysical retained poses remain in authority.
No physical owner or actor value/life/action/event state is released by this
operation. Missing physics or zero physical owners returns before requiring a
Player; TES3 returns without entering the native authority. The global clock
remains in the existing PhysicsSystem across cell teardown.

Ordinary Scene::unloadCell invokes retention before the visitor clears renderer
nodes or removes any body/capsule. Scene::removeObjectFromScene invokes it for
physical actors before mechanics/Lua/navigation/render/physics removal.
Scene::clear explicitly disables retention during whole-World reset: that path
intentionally discards native state and must not be blocked by a stale old
binding. Ordinary cell transitions keep the default retention behavior.
This does not add admission to the new scene or migrate a physical graph across
teleport/model/race/scale changes. Existing World callers may already have
changed reference metadata before entering Scene removal; no transaction for
those earlier caller operations is claimed.

The actual World regression loops worker0/1/2 with native NPC/projected Player
physical owners, authored and generated controller state, packed W lanes,
blend metadata and the shared clock, plus a nonphysical retained actor.
A valid-but-wrong hash on the scheduler's last binding rejects retention;
native pose maps and the whole physical group remain unchanged. Corrected
bindings retain the fresh complete snapshots for both owners. Actual
PhysicsSystem::remove releases bodies and capsules; a later World save and
binary decode retain those poses/controllers/cache with negative-zero angular W.
The second test uses no Player and verifies no-physics/empty-physics retention
does not invent native projections or change the current raw-cycle/negative-zero
clock.

world-physical-retention-baseline-01 retains a fixture compile failure caused by
a snapshot variable conflicting with an existing Ptr. Renamed only the snapshot
variable. Baseline-02 executes all three worker loops and fails on accepted bad
binding retention, stale service projections and stale saves after body release.
First implementation normal/sanitized-01 retains compile failures from incorrect
new empty-test API names. Corrected only the test names to the existing
captureNativeBlendTimeCache/restoreNativeBlendTimeCache methods. Final full
world-physical-retention-normal-02 and sanitized-02 each pass945 cases with
exact inventories, zero failures/skips and no compiler warnings. ASan leak
checks disabled; UBSan halts. Tested fingerprint `f4b3a76e7f36c95de50dabd57de295967d8d74d93801d2c47c771804cc96362b`.
Lower native component arithmetic, codecs and Python sources did not change;
their previous passing comparisons are not repeated.

The tests exercise the actual World retention operation and physical release,
not a normal-input Scene cell transition, renderer/controller reattachment or
fresh-process restored-owner admission. Cell unload/object removal call sites
compile and await those runtime gates. Automatic reaction/admission/update/end/
getup, morphology/configuration producers, full native scene traversal, contact/
activation continuation, retained numeric failures and pending S5-S14 remain
open. No M15 stage closes.


### Checkpoint245: independent HIT gain configuration producer

Checkpoint244 committed as `dcc322cd37ff03b500de4e7f64b12996a7181554`,
241 isolated commits, tree `1167c3b4d588f74d8b296a51950f31fab647fdbb`;
bundle222 SHA256
`f584fc50a7b3f754f16281b46f18cc5b12aa49673fe5031a455a7c1005e32d33`.

Original53A1B0 configures the HIT gain table at B2EC68, which is distinct from
the post-link table B2E660. The latter's initial32 pairs are all1/1 and the
existing InitialPhysicalBlendGainTable remains correct for that purpose.
The HIT initial PE table uses mostly.2/.9, with native variants at IDs1-7,
11-13 and22. Configuration changes ten body IDs: Head1, Body2, Spine1=3,
Spine2=4, left upper/fore/hand5/6/7 and right upper/fore/hand11/12/13.
Unconfigured IDs retain their previous entries, including ID22=1/1.

PhysicalHitBlendSettings owns already-parsed typed HIT inputs. Compiled settings
produce Head.4/.6, Body1/1, Spine1.6/.8, Spine2.5/.7, upper arms.2/.5 and
forearms/hands1/1, with minimum hierarchy.3 and velocity.95. The distinct
InitialPhysicalHitBlendGainTable records the independently read PE data.
resolvePhysicalHitBlendConfiguration validates all typed previous/settings/
minimum floats, updates an owned table and returns minima without clamping.
Finite negative/outside-unit gains and negative zero survive. Source settings
and previous table are immutable. This API does not parse strings, discover or
merge BlendSettings.ini, choose an actor reaction, or replace post-link gains.

native-blend-gain-configuration-oracle-08 verifies original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`,
executes complete CRT callback initialization982837, then full53A1B0 with actual
sscanf98629E/995D33 and numeric callback98FDAD. All300 fixtures return explicitly:
both x87 control words, ten settings and15 strings, including valid comma pairs,
negative zero/negative values, exponent notation, empty/invalid strings, partial
second conversions and missing comma. Before each scan native code initializes
both outputs1/1. Partial first conversion retains the second1. No numeric parser
or table producer is replaced. Declared boundaries are CRT current-thread locale
acquisition with pinned initial C-locale pointers, Windows GetLastError/
SetLastError and the CRT OS decoded-pointer boundary using initial plain
pointers. File discovery, initial whole-game startup and gameplay are excluded.
Corpus SHA256
`32962e85ba6949607b441a0befb547e11893773fd1b2276bc6a742c5a8e7a033`;
source SHA256
`6abf4820d3f87858f4b4c17bdf89c2cd5def9a78b0cd6987fb8fa3941f4562af`.

Attempts01-07 remain failed setup evidence, with zero completed cases.
Unresolved Windows IAT entries contained RVAs and jumped into unrelated mapped
game code. Supplying declared OS/locale boundaries reached the CRT's initial
floating trap callback: the fixture had not executed982837 to install actual
98FDAD. The correction executes the original initializer; it does not stub the
trap/abort or numeric conversion. Bounded instruction/literal-reference reads
identified the callback producer before changing the fixture.

Two new component tests execute and fail against a placeholder that returns the
previous table: missing ten configured gain updates, lost negative-zero/raw
settings and accepted nonfinite input. Implementation coverage checks compiled
body mappings, all unconfigured IDs, immutable input, distinct unchanged
post-link defaults, raw finite settings and nonfinite last-setting/minimum/
previous-table rejection. Full native-hit-configuration-normal-01 and
sanitized-01 each pass2417 component tests with exact inventories, zero failures/
skips. Normal final has no warnings; sanitizer broad recompilation retains
preexisting missing-initializer diagnostics in unchanged inventorymechanics.cpp,
whose bytes match244 in unchanged-warning-source.json. ASan leak checks disabled.
Tested fingerprint `d7e059474162354837ac378a6dd97579f16faa5cfeaa31fcc801822271b190a9`. No engine writer or Python codec changed; the
preceding945 engine tests are not repeated for an unused new typed API.

Standalone comparison-normal-01 cannot load bundled Bullet and executes zero
cases; its failed launch is retained. Add the existing bundled-library search
path, with no production change. native-hit-configuration-compare-normal-02
and sanitized-02 each compare64 initial PE HIT lanes plus all64 configured
gain lanes and two minima for300 original cases:19,864 exact binary32 checks
with zero mismatches. Already-parsed selected inputs use actual native scanf
outputs; the other nine use compiled setting strings. Expected complete tables/
minima come from original instruction outputs, never production C++. The
comparison therefore proves typed mapping/preservation, not a C++ string parser.
Archive hashes and source fingerprints remain stable during both comparisons.

No M15 stage closes. Winning configuration file discovery/merge and typed
runtime import, initial morphology, actual automatic physical admission/
reaction/update/getup, full Ni traversal/time/contact continuation, retained
physical numerical failures, normal-input restart/gameplay and pending S5-S14
remain required.


### S5 actual Player bow hold-fatigue frame writer (checkpoint263)

Actual native frame resource updates now debit Player bow hold fatigue between
running expenditure and regeneration. Process action5 selects the debit in
every phase; integer Marksman chooses mastery, novice rate uses winning typed
TES4 fFatigueAttackBase/multiplier settings, and positive current Fatigue caps
the debit. NPCs and god-mode expenditure are excluded. Both fatigue/frame
settings resolvers refresh winning typed GMSTs; TES3 names cannot supply them.
The service prepares all native channels and shared projections before
publication, so a later invalid regeneration setting leaves resources and
the actor frame clock unchanged.

Three new engine tests cover compiled and replacement settings, record typing,
Player/NPC action/mastery boundaries, fractional integer-AV composition,
nonpositive/depleted Fatigue, unchanged unrelated modifier channels, canSpend
exclusion, regeneration order, and late-failure atomicity. The actual World
frame writer uses the saved service clock rather than the supplied arbitrary
duration; restore plus actor rebinding prevents same-clock replay and continues
the next debit. God mode and leaving action5 stop expenditure. This is backend
World/service continuation, not a fresh-process or normal-input game course.

Original oracle01 executes 5FACC8 through the bow hold branch and complete
5E07D0 common expenditure, then 65E530 Player god-mode guard. It stops at
65E565 before original magnitude adjustment/storage, or5FAD1B on skipped
expenditure. All19,200 cases complete across Player/NPC, process absence,
actions, mastery ranks, current Fatigue, duration, rate, god mode and both x87
control words. Declared boundaries supply process action, integer Marksman,
mastery rank and current Fatigue getters; an observer asserts the actual action
return, and native eligibility virtual977C50 executes. The corpus records
requested deltas, not a claim that native actor-value storage was emulated.
Final normal05 and sanitized05 comparers exercise the real Player service
writer plus shared Fatigue projection against4,800 Player/present-process
fixtures each, with zero mismatches. Plain Fatigue fixtures predict their
result from current plus the captured original delta. NPC exclusion is tested
separately in the engine suite.

Final bow-hold-frame-normal-03 and sanitized-03 each pass967 full engine tests,
matching exact inventories with zero failures/skips, and build openmw,
openmw-tests and esmtool. ASan leak checks remain disabled; UBSan halts.
Components and Python sources are unchanged from262's passing2461/260 checks.

Retain failed01 compilation (ambiguous test initializer), failed02 normal
segfault/sanitizer assertion (level-scaled NPC fixture without a World), and
comparer01 missing Settings initialization,02 missing MyGUI include path,
03 unlabeled parsing of Settings stdout. Correct fixture initialization and
use actual engine include flags plus indexed result labels. Expectations and
production mechanics are unchanged by those fixture corrections. Final03
retains the existing actorstats range-loop-copy warning: recorded prefix bytes
match262 before the newly appended tests.

Tested fingerprint: f9118962d7480e4f10f3dd34eab857bdac6a52b09ac81a4c6f59ab27911f8f06
Original PE SHA256: a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6
Oracle source SHA256: dfa4d32bad5906b1b61735db9a549d5613ed8a579f96c54b53487ba9d306f7ab
Oracle corpus SHA256: df5bc294d770b6fc306bb7244fbe71a07a2b0dd173d7254b7b2162fe63dda570

Actual bow controller/renderer integration, release ammunition/shot fatigue/
weapon wear, projectiles/impact/recovery/in-flight save and normal-input courses
remain open. S5 remains in progress; no other M15 stage closes.


### S5 equipment KF binding refresh in both actor renderers (checkpoint264)

Loaded KF sources previously dropped tracks whose targets were absent from the
cached skeleton map. Equipping a bow after loading its animation therefore left
Bow:0/ArrowBone targets unbound; replacement could retain detached equipment
through node/controller caches. Animation::refreshAnimationBindings now prepares
a fresh hierarchy map and complete per-source/mask bindings from immutable
loaded KF controllers before publication. Previously missing targets become
eligible; unchanged controller bindings retain their clones. Detached target
entries leave smoothing caches, and active callbacks rebuild without restarting
selected groups, changing their time/speed/loop ownership, or reloading assets.

TES4 NPC assembly refreshes after body construction and weapon attachment/
removal. Equipment refresh clears stale node lookup before rebuilding parts.
Repeated showWeapons(true) still replaces equipment even when visibility is
unchanged. The distinct Player NpcAnimation path refreshes native bindings
after weapon/quiver updates and native Player part replacement; TES3 call-site
behavior remains selected by its existing profile. Native physical node
metadata/admission and missing controller implementations are not claimed by
this ordinary KF binding operation.

The existing258 hash-identified stock asset audit records ArrowBone, Bow:0 and
ArrowHelper01 tracks in both third- and first-person bowattack KFs; this is why
late equipment targets matter. No new original gameplay or graphics capture is
supplied here. One synthetic editable KF fixture loads the real NIF parser and
Animation source, starts playback before Bow:0 exists, then attaches, refreshes,
repeats, replaces, removes and reattaches it. It checks exact unchanged playback
time, callback identity/count, old-target callback removal and final disable.
Actual update traversal samples a linear keyed translation: x2 at time.5,
x2.5 at time.625, and the replacement samples x2.5 at the retained time.
This exercises controller output rather than only map membership.

Initial01 normal/sanitized each pass968 tests before the numeric sampling
extension. Retain normal02 SIGSEGV and sanitized02 invalid-downcast report:
the new fixture used plain osg::MatrixTransform for a NIF controller whose
contract requires NifOsg::MatrixTransform. Correct the test nodes to initialized
NIF transforms, matching actual loader output; keyed expectations and production
binding code do not change. Corrected03 normal/sanitized each pass968. Final04
includes the Player call-site integration and each passes968 full engine tests,
with exact inventories, zero failures/skips and no emitted compiler warnings.
All final checks build openmw, openmw-tests and esmtool. Initial01's broad
normal rebuild retains the old GCC16 Character animation-queue warning, with
unchanged implementation/header hashes against263. ASan leak checks remain
disabled; UBSan halts. Components/Python are unchanged from262.

Tested fingerprint: aec8c17474debd2c3582c2f286921495d000700125028dab0f4fff42e07ac957

Normal-input first/third-person bow rendering, held-arrow geometry, draw/release
input, prepared ammunition/fatigue/wear publication, flight/impact/recovery/save
and remaining mastery courses still require their own integration/acceptance.
S5 stays in progress, and no other M15 stage or runtime gate closes.


### S5 native held-arrow geometry (checkpoint265)

The original hash-identified nock path5FCF8C..5FD0FD looks up exact Arrow:0
and calls700900 to clone that selected object before attaching it. The clone
wrapper constructs its cloning process, invokes source virtual18/38, and returns
the clone; original destination storage and action publication are not emulated
here. The renderer now clones only exact Arrow:0 through the actual SceneManager
instance API, preserving authored transforms and the immutable ammunition
template. Missing Arrow:0 never falls back to the complete quiver model.

Both TES4 NPC and native Player animation producers select actual equipped,
positive-count bow/ammunition instances and winning typed TES4 WEAP5/AMMO
records. They load the winning native ammunition model, prepare a PartHolder
before graph publication, and replace the previous held clone only on success.
Missing targets/items/geometry leave existing ownership unchanged; detachment,
equipment replacement, object-root replacement and scene removal release it.
These producers do not debit ammunition, fatigue or condition and do not
publish a logical bow event or projectile. Player native release still requires
its own implementation; ordinary TES3 release cannot supply native semantics.

Two renderer tests cover exact-name selection, hidden source nodes, independent
clones, preserved authored transforms/template, absence of quiver and actual
PartHolder attachment/cleanup. One World fixture checks actual equipped
instances with intentionally misleading shared TES3 model/type projections,
winning native model replacement, missing ArrowBone, zero ammunition, nonbow
native type, missing Arrow:0 retaining the previous clone, repeated detach and
scene removal without ammunition debit.

Final normal and ASan/UBSan each pass971 full engine tests with exact inventories,
no failures/skips and no emitted compiler warnings. Initial01 also passes971;
the final02 fixture adds negative admission cases without changing production.
Initial01 retains the existing GCC16 Character queue warning: implementation
and header bytes match264. ASan leak checking is disabled, not leak coverage.
Components/Python remain unchanged from262.

The stock asset audit loads the original Iron Arrow through actual NIF and
SceneManager code: seven source geometries, one selected Arrow:0 geometry,
221 vertices, no quiver in the held clone and clean parent removal. Normal and
sanitized final03 both pass, loading isolated original diffuse/normal textures
without missing-image diagnostics. Mesh SHA256:
35286b4cb6638d8de87d1a25fe1e5a677379172115fe43cd900d3f6330f53a63.
Texture archive SHA256:
d6e6a2155bea688052862df471c631e93572d525fb81b4358da328bd34760bd2.
Inputs and their individual hashes are retained in held-arrow-stock-input-02/
inputs.json. Proprietary extracted assets remain ignored evidence.

Retain initial stock audit01 shader initialization failure, corrected by
matching the actual headless engine shader setup. Intermediate02 passes
geometry checks but reports a missing texture; final03 supplies hash-identified
textures and checks that diagnostic is absent. Retain stock-input-01's metadata
path-normalization failure; corrected02 verifies normalized extracted paths.
Evidence lives under S5/held-arrow-geometry-{normal,sanitized}-02 and
S5/native-held-arrow-assets-{normal,sanitized}-03.

Tested fingerprint: 6734488639f413294e37fa7e1dbe753e3969b2074e0d167e97d62238d18f6d37

This proves scene geometry selection/ownership and stock material loading,
not graphics capture or normal-input first/third-person acceptance. Bow input,
successful release resources, flight/impact/recovery/save, mastery and remaining
M15 gameplay gates stay open. S5 remains in progress.


### S5 prepared collision publication (checkpoint266)

PhysicsSystem now prepares an opaque projectile token containing the actual
detached Bullet collision object, its prepared ownership map node and an owner
identity. Preparation validates finite position and positive finite radius,
allocates without registering collision and consumes no projectile ID.
Commit rejects foreign, consumed and expired-owner tokens before writes,
registers collision, inserts the allocated map node and publishes the next ID.
A replacement PhysicsSystem at the same address has a distinct identity and
cannot accept the previous owner's token. Cancelling an uncommitted token
remains safe after its owner has been destroyed.

The existing addProjectile producer uses this preparation/commit path after
its real shape lookup. Collision transforms are initialized before registration;
the destructor only unregisters a registered object. Scheduler registration
rolls back its world/set insertion if Bullet registration throws a C++ exception.
This does not claim coverage of allocator abort/null failure modes.

Four real PhysicsSystem tests check ray invisibility before commit and after
cancellation, publication-order IDs, duplicate commit rejection, retained live
collision after consumed-token destruction, ghost-free removal, invalid input,
foreign owners, destruction before cancellation and same-address owner reuse.
Initial normal/sanitized01 each pass974 full engine tests. Final02 each pass975
with exact inventories and no failures/skips or emitted compiler warnings.
ASan leak checking is disabled; components/Python sources are unchanged.

Evidence: S5/prepared-projectile-{normal,sanitized}-02.
Tested fingerprint: fcfc127e7c1bd842d55a4de756d9b50c6aae7545c2a5b592a5894acfe7a0798a

This is collision preparation and ownership integration. Compound native bow
release, resource debits, flight/impact/recovery/persistence and normal-input
acceptance remain open. The explicit caller radius is not yet a verified native
arrow collision rule. S5 and the full M15 goal remain in progress.


### S5 projectile-manager publication rollback (checkpoint267)

The existing ProjectileManager launch path now prepares its scene model,
effect-time/controller/texture/glow setup, mesh-derived collision token and
ownership vector insertion before publishing the scene node or collision.
False or throwing scene publication removes any attached node and the staged
ownership entry. Collision commit is the final fallible publication operation.
PhysicsSystem's mesh preparation overload retains the actual BulletShapeManager
lookup and existing radius rules; addProjectile delegates through it.
createModel's ordinary callers publish after its fallible controller/texture
setup, while the projectile launch caller requests detached preparation.

The real World fixture uses a synthetic triangle mesh, actual ManualRef items,
SceneManager and PhysicsSystem. A model without collision geometry, explicit
scene rejection and an injected exception after attachment each leave zero
scene children, ray hits, saved projectile records and consumed projectile IDs.
Successful publication creates one visible and collidable projectile with ID1;
clear removes scene and collision, and the next launch receives ID2.
The producer does not debit ammunition.

Retain initial normal/sanitized01 compiler failures: ProjectileState is not
nothrow-movable. Corrected02 moves the state before world publication and
rolls ownership back on failure. Final normal and ASan/UBSan each pass976 full
engine tests, exact inventories, no failures/skips and no emitted compiler
warnings. ASan leak checks are disabled. Components/Python are unchanged.
Evidence: S5/projectile-manager-publication-{normal,sanitized}-02.
Tested fingerprint: 5eb6157239b5a2303a184a88dbe16f463e4a09c852a1743263ff9edd664fe6c0

This fixes the existing shared producer's publication failure path. It does
not establish native TES4 flight, native resource release or normal-input
ranged acceptance; those S5 gates remain open.

### S5 checkpoint268: original Player bow-hold input tail

playerBowHold implements the bow-present Player input tail65EF38 through
65F04A/65F070. Held or newly pressed action4, process readiness, crossbow
exclusion and blocked-input state decide latch preservation versus clearing.
Pause requires animation category4..7 excluding category5, zero input gate,
Hold phase and a live latch. Original AttackBow group19 is category7, and
actual native setters pause both views. Category5 is casts, not Bow; the
earlier reverse-engineering interpretation is corrected here.

The independent hash-identified executable oracle executes actual group,
category, sequence, phase and pause instructions. Input403520 and process
virtual readiness304/crossbow13C/bow138/action2D0 are declared stable fixture
boundaries. Both x87 control words and all3840 factorial cases agree with
production C++, exactly7680 pause/latch fields in both normal and sanitizer
comparisons. Earlier admission/latch creation, nonbow input, process creation
and gameplay are excluded. Invalid typed phases are rejected explicitly.

Normal and ASan/UBSan each pass2463 full component tests, exact inventory,
zero failures/skips. Both incremental builds emit20 existing missing-owner
initializer warnings in inventorymechanics tests; no changed-source warnings.
Leak detection is disabled. Engine/Python are unchanged.
Retain comparison-normal01 failure: the harness accidentally linked the
older playback driver. Corrected normal/sanitized02 use the intended driver.
Evidence: S5/native-player-bow-hold-oracle-01,
S5/native-player-bow-hold-compare-{normal,sanitized}-02,
S5/player-bow-hold-{normal,sanitized}-01.
Tested fingerprint: 0cbb084455afbddb42e8b6c14a698d681c70bf642d5e1e5f48291cb9e5cea168

This closes the typed input rule only. Owned saved latch/controller wiring,
resource release, native flight and normal-input ranged acceptance remain open.

### S5 checkpoint269: saved Player bow input and native timer selection

Runtime-state39 adds a canonical boolean Player hold latch to each owned bow
record. Only the actual Player can carry a true latch. Version38 draws decode
with a false latch; current binary/JSON/Python agree, and populated latch
downgrades are rejected. Migration preserves older draws without inventing a
held input. Accepted Player draw admission creates the owned latch; NPC draws
do not. advancePlayerBowPlayback selects pause from the verified native tail
and prepares timer/latch before publishing a successful playback frame.
Invalid/stale/nonrunning frames leave the whole state unchanged. Cancellation
removes ownership; fresh admission receives a fresh action identity/latch.

The independently executed original input selection65EB57 observes actual
Player+640 timer writes through65EF38/65EC2A/65ED9F. Input403520, process
readiness/bow/action, setting403C00 and draw-weapon request5E6D70 are stable,
declared boundaries; actual raw-action and phase queries execute. All5760
factorial cases agree in exact binary32 bits with normal and instrumented C++.
Eligible new presses preserve an active draw timer, eligible ordinary held
frames accumulate it, inactive/End draws reset, and ineligible input leaves it
unchanged. The old caller-selected timer API remains available for its distinct
supported prefix; the owned Player input adapter uses this verified selection.

Actual service tests compare full saved state for held playback, button
release and a resumed press that cannot recreate the old latch. They verify
the timer remains1.5 across released/pressed frames, and invalid frames,
lossy capture, cancellation and renewed admission. C++/Python independent
golden wire tests reject truncation/noncanonical markers. Both cross-language
drivers pass52 exact binary/JSON cases: all39 empty schemas, ten phase/latch
combinations, two pending-Release latch cases, and a populated legacy38 draw.

Final normal and ASan/UBSan each pass2465 full component and978 full engine
tests, exact inventories, no failures/skips and no emitted incremental-build
warnings. Normal also passes261 Python tests without skips. Leak detection is
disabled. Retain both initial engine01 failures: the new service-comparison
fixture used an empty Player identity; initialize it from the real fixture
state. Retain Python precheck01's NPC latch fixture error and both wire01
drivers' legacy character-generation fixture error. Corrected02 evidence is
S5/player-bow-input-{normal,sanitized}-02 and
S5/player-bow-input-wire-cross-language-{normal,sanitized}-02.
Timer evidence is S5/native-player-bow-timer-input-oracle-01 and
S5/native-player-bow-timer-input-compare-{normal,sanitized}-02.
The standalone comparisons preceded the final engine-test-only correction;
production archives are unchanged and hashed by each comparator.
Final tested fingerprint: 1a875ccccd9539ab2340b4139af42d368269ace172d4cb95c9f21f1fa31dc0d5

Additional independent flight research executes original interior-world setup
4D4A8B through4D4AD0, including actual88A4F0/8A9510 constructors and8A9460
bounds setup, with no game-function stubs. Both x87 modes store the exact
gravity quad[0,0,-73.57500457763672,0] in world-info+10; original89A230
copies that quad to Havokworld+20. Evidence:
S5/native-world-gravity-setup-oracle-01. These are Havok coordinates; the
coordinate-conversion caller, arrow motion-state selection, other world
producers and trajectory acceptance remain unverified here.

Character input/renderer wiring, release resources/projectiles, flight and
normal-input ranged acceptance remain open. This does not close S5 or M15.


### S5 checkpoint270: prepared ammunition publication and native actor dispatch

InventoryStore now prepares a one-item debit of the actual equipped ammunition
instance without changing count, slots, caches or observers. Commit validates
the owner lifetime, exact instance and positive raw projected count, then
publishes count, last-item slot/selected-enchantment clearing and cache
invalidation without allocations or callbacks. Deferred notification is
one-shot even when an observer throws. Identity checks after callbacks avoid
dereferencing items destroyed by inventory replacement. Copy/move assignment,
clear and prepared-content swaps invalidate old tokens before list destruction;
strong lifetime tags reject same-address owner reuse. Existing public item
removal and CellRef setters retain their normal notification behavior.

Four actual-world tests cover two-to-one-to-zero counts and weight, cancellation,
last-item slots and notifications, foreign/stale/replaced inventories, owner
destruction and placement-new reuse, and observers that throw or clear the
inventory. Normal and ASan/UBSan each pass982 full engine tests with exact
inventories and no failures/skips. Leak checks are disabled. Components/Python
are unchanged. Normal emits ten existing Lua/Character/test warnings; sanitizer
emits eleven existing console-regex/test warnings. Warning-bearing sources
match checkpoint269, recorded in unchanged-warning-source.json in each run.
Evidence: S5/prepared-ammunition-{normal,sanitized}-01.
Tested fingerprint: d9dd857256bb67d327bb39f4776bca5aa323c964b4ff777ab4523e65cf5e5321

Independent original release dispatch5FD4A4 through5FD4B0 reveals an actor
distinction: NPC/Creature vtablesA6FC9C/A710F4 dispatch ammunition removal to
60D0A0, a RET; PlayerA73A0C dispatches to662590. Player requests exactly one
item unless god mode is active. All192 actor/god/count/ExtraCount/x87 cases
pass, including original41E860 signed-word/default-one reads and60D020 process
entry stores. Stable declared boundaries are process entry getter/clear,
ExtraCount lookup/setter request, inventory removal4D8760, UI request5C1900
and absent ContainerChanges4D6D40. This observes requests and entry writes,
not full inventory/ContainerChanges storage or a compound shot. The quantity-one
release policy applies to the native Player caller; do not debit NPC ammunition.
Evidence: S5/native-arrow-ammo-dispatch-oracle-01.
Oracle source SHA-256:
715b7fb05e1efb3951d7b4bca8d67953f11d1f6aa39077da081e8a35ab3ef5e0

Compound resources/projectile release, Character input, native flight, bow
break consequences and normal-input acceptance remain open. Existing display
connections are unavailable; authenticated TCP Xvfb also fails to open its
listening sockets. Headless checks do not close normal-input gates or S5/M15.


### S5 checkpoint271: prepared bow resources and coordinate/shape provenance

OblivionCombatService prepares immutable shot inputs before resource changes,
then stages actual fatigue channels, shared actor projections, bow condition,
ammunition and owned action consumption. Commit validates service/inventory
lifetimes, exact equipment and unchanged authority before callback-free
publication. Player god mode preserves resources; NPC ammunition is preserved
as verified at checkpoint270. Deferred ammunition observers run only after
the resource/action state is coherent and cannot replay after failure.
Cancellation, stale or foreign owners, assignment/restore, invalid settings
and equipment changes publish nothing. The owned launch data remains safe to
read and discard after World destruction.

Five new actual-world tests cover coherent last-arrow observers, NPC/god-mode
policy, nine stale-state branches, cancellation/foreign owners, late failures
and token destruction after World destruction. Normal and ASan/UBSan each
pass987 full engine tests with exact inventories, no failures/skips and no
incremental compiler warnings. Leak checks are disabled. Components/Python
are unchanged. Retain both initial01 failures: the fixture advanced only0.1
after native Hold rewound to1.0, never crossing Release1.25. Corrected02
advances0.3. Evidence: S5/prepared-bow-release-{normal,sanitized}-02.
Tested fingerprint: e6b85e52e5cc0bffb642417b5a088f69727ee2b5004d3c95d2d4d09beee7ac07

An independent130-case original-instruction oracle verifies64 coordinate
vectors in both x87 modes plus two actual arrow sphere callers. Original
43F3E0 converts xyz to world units with double6.999040126800537 and float
stores; arrow initialization60A363 uses double0.1428767293691635. These
constants are not exact reciprocals. Actual60A3A4 sphere construction passes
world radius0.10000000149011612 and conversion flag1; original532090 requests
Havok radius0.014287672936916351 (bits1013585624) at8AF550. Original base
constructors execute; a declared Windows InterlockedIncrement import and the
shape publication boundary are isolated. Retain initial01's unbound-import
failure. Evidence: S5/native-arrow-coordinate-shape-oracle-02.
Oracle source SHA-256:
cbb8a6b8cad73b4a595267ce61f17c7d7f45cf8e8e0d289da3d416fda491ab69

Projectile publication is not coupled to this transaction yet. Character bow
input, complete native flight/controller state, impact/recovery, saved
projectiles and normal-input acceptance remain open. Wear that would break
the bow is rejected before publication until native break consequences are
implemented. Coordinate/shape evidence does not establish trajectory or full
Havok allocation. S5 and M15 remain in progress.


### S5 checkpoint272: native projectile coordinate and gravity stores

Typed projectile rules now preserve the original distinct world/Havok double
conversion constants and each binary32 coordinate store. Collision radius
uses the original converted sphere radius in scene units. The gravity tail
keeps Havok velocity/gravity units and rounds factor*gravity, duration*product
and velocity+increment separately. Nonfinite vectors, negative multipliers
and arithmetic overflow fail before returning any changed value.

Three new component tests compare exact bits against independently executed
original coordinate outputs for64 vectors and gravity outputs for128 inputs,
including signed zero and subnormal coordinates. They cover every axis,
invalid unused operands and overflow at each gravity store. The original
gravity oracle executes8CF9B3 through8CFA1B, including actual Arrow vtable
A6FA64+58 getter8B9C80 and8AC0C0, with no game-function hooks. Both x87 modes
agree in all256 cases. Evidence: S5/native-arrow-gravity-store-oracle-01.
Oracle source SHA-256:
5ca993a830d5f715e25bd1e123c9bc67b250129885ff5604b721a8e2819a4502

Normal and ASan/UBSan each pass2468 full component tests, exact inventories
and no failures/skips. Leak checks are disabled. Both builds emit20 existing
inventory-test missing-initializer warnings; that source is byte-identical
to checkpoint271, recorded in unchanged-warning-source.json. Engine/Python
behavior is unchanged; no live caller invokes the new rules yet.
Evidence: S5/projectile-vectors-{normal,sanitized}-01.
Tested fingerprint: 03f536a27fbde0b7bf91c6ec5d1798533bb03279e9193c7a0932950360f9b37b

Further isolated research executes the complete original State2 update,
including91F430 resolution and890740 timer tail, in64 cases with declared
fixture fields and no game-function hooks. Retain initial01's missing
motion-state pointer failure; corrected02 initializes the actual8AC070
pointer path. Evidence: S5/native-arrow-state2-oracle-02.
Oracle source SHA-256:
c973a644366aa9a337b803d1b1f3f27c9f297685a689b09c48aeb4cf08a3629a
These fixtures do not establish actual Arrow controller field producers,
per-frame support/collision inputs or trajectories. Full native flight,
resource/projectile coupling, Character input, persistence/impact and
normal-input acceptance remain open. This does not close S5 or M15.


### S5 checkpoint273: staged equipment removal for compound resources

InventoryStore now prepares exact-instance unequipping without changing
quantity or slots. Commit validates the inventory lifetime, slot identity and
count, then clears the slot/selected enchantment and invalidates caches
without allocation or observers. Restacking, OnPCEquip handling and equipment
notification are deferred and one-shot, including failures. Lifetime checks
protect stale item pointers after replacement and callback-driven clearing.
Existing public unequip behavior is unchanged.

Four actual-world tests exercise Player bow/ammunition cancellation, quantity
and weight conservation, coherent slot/selection state before observers,
invalid slots, foreign/count-stale/cleared/assigned/swapped owners, moved
tokens, throwing or clearing observers and same-address owner reuse. Tokens
can be discarded after inventory destruction. Normal and ASan/UBSan each
pass991 full engine tests with exact inventories and no failures/skips.
Leak checks are disabled. Components/Python are unchanged. Normal emits two
existing Character/actor-test warnings; sanitizer emits one actor-test
warning. Sources match checkpoint272 in unchanged-warning-source.json.
Evidence: S5/prepared-unequip-{normal,sanitized}-01.
Tested fingerprint: 8988ab9bf28fb98e874b310a77f517bc61d1e35622ae879b7f4fd7f170fa1c83

Independent original break-dispatch research passes48 admitted-consequence
cases and720 expanded zero-condition/process/readiness cases. Original
5F39D3 through5F3B25/5F3B31 executes all four process vtable+304 dispatches:
Low/MiddleLow69D990 return false; MiddleHigh/High64AD30 read byte115.
With null enchantment argument, positive remaining condition, missing
process or false weapon-out emits no equipment request. A broken drawn weapon
requests unequipping for the actual Player or quest items; other NPC/Creature
weapons request dropping exactly one item. Actual WEAP vtableA45354+78
getter4D7030 tests form+8 bit0x400, matching the TES4 header quest-item flag.
The Player bypasses that getter. Original711440 rotation conversion executes.

Declared boundaries are actor+168 query, process+118/+120 node getters,
unequip5F2E70, drop5FC440 and notification4DC000 requests. This establishes
dispatch/request arguments, not full wear admission, inventory storage,
dropped-reference creation or gameplay acceptance.
Evidence: S5/native-broken-weapon-dispatch-oracle-{01,02}.
Expanded oracle source SHA-256:
1499d2716bf16927606d2519e29642467ca1a981af5e231fe87ab58536335e5b

The bow service does not invoke staged unequipping yet; it still rejects a
wear-to-zero transition. Native drop publication, release/projectile coupling,
Character input, flight/impact/persistence and normal-input acceptance remain
open. S5 and M15 remain in progress.


### S5 checkpoint274: atomic Player/quest bow-break consequences

Prepared bow release now snapshots the actual saved shared draw view. A
wear-to-zero transition with a drawn weapon stages exact slot removal for
the Player or a winning quest-item bow. Condition, ammunition, fatigue,
shared actor projection and action consumption publish before equipment
observers. Deferred notifications remain one-shot when an observer throws
or clears inventory. A later ammunition notification rejects the retired
inventory identity before dereferencing destroyed items.

With the supported null-enchantment wear context, a non-weapon draw view
retains the already admitted shot's now-zero-condition equipped bow, matching
the original no-equipment-request branch. Further release preparation still
requires positive condition. Player god mode preserves condition and ammunition.
Drawn ordinary NPC bows still reject wear-to-zero before any publication
until an atomic native dropped-reference transaction is available. NPC quest
bows unequip and retain their ammunition. The existing saved shared draw view
supplies readiness here; full normal-input linkage remains open.

Six new actual-world tests cover Player last-arrow break conservation and
complete native/shared resource state before both equipment observers, NPC
quest versus unsupported ordinary-drop policy, unready Nothing/Spell views,
god mode, stale draw view, cancellation, throwing/clearing observers and a
clearing observer that returns normally before the next notification.
The existing late-failure fixture now checks ordinary drawn NPC drop rejection
alongside invalid winning settings; immutable tokens still survive World
destruction safely. Independent break dispatch/readiness evidence is recorded
at checkpoint273; no dropped-reference or full original inventory execution
is claimed.

Final normal and ASan/UBSan each pass997 full engine tests, exact inventories,
no failures/skips and no incremental compiler warnings. Leak checks are
disabled. Components/Python/schema are unchanged. Both initial01 runs passed
996 tests but emitted a new fixture enum-conversion warning; corrected02 uses
an explicit unsigned conversion and adds the returning-clear lifetime case.
Production is unchanged from those initial passing runs.
Evidence: S5/prepared-bow-break-{normal,sanitized}-02.
Tested fingerprint: aa7fbd134262780cc7068cb6b36c9e2d414f3905574f7570aac403323f89fab9

Native NPC ordinary-bow drop, compound projectile/resource publication,
Character bow input, flight/impact/recovery, saved projectiles and normal-input
acceptance remain open. S5 and M15 remain in progress.


### S5 checkpoint275: native placed-item condition and charge storage

Live native REFR instances now retain exact binary32 condition and enchantment
charge independently of their winning authored record. Missing condition reads
the supplied base maximum, rather than an invented broken value; missing charge
keeps the existing full/default sentinel. Copy, move and swap retain these extras
with the reference's stable identity. Legacy integer health views use the same
ceil/clamp projection as native inventory health. Explicit reset restores absence.

Native condition and charge reject invalid values before changing the instance.
The native charge setter also preserves the sign bit when replacing positive
zero with negative zero. Existing projected TES3/inventory charge-reset and
remainder behavior remains unchanged. Actor references cannot acquire item
condition through this API.

Three engine unit tests directly exercise CellRef storage, exact float bits,
zero/subnormal/maximum values, positive-to-negative-zero charge replacement,
copy/swap/base-record independence, default/reset behavior, malformed inputs
and legacy projected health. They do not exercise World drop or pickup.

Normal05 and sanitized04 each pass1000 full engine tests with exact inventories
and no failures/skips. Passing incremental builds emit 1/1
existing range-copy warnings in the unchanged actor-stats test; byte equality
against checkpoint274 is recorded in normal05/unchanged-warning-source.json.
ASan leak checks are disabled. Components, Python and runtime schema39 are
unchanged.

Initial normal01 exposed two new fixture compile mistakes; sanitized01 was
interrupted before tests. Both02 were interrupted for another fixture
initialization correction, and both03 for the signed-zero charge regression.
Normal04 then failed linking because an interrupted build had left a zero-byte
generated weather binding object. Recovery removes only that empty object;
normal05 has identical source to normal04. Failed/interrupted evidence and the
object-recovery metadata remain in their original directories.

Evidence: S5/native-placed-item-extras-normal-05 and
S5/native-placed-item-extras-sanitized-04.
Tested fingerprint: d5acc292e55cafc315d0b10e05457bc2b300ab2ee426a804bdf55bb379a88303

Native loose-item save capture/restore, dropped-reference recreation and atomic
pickup remain open. Drawn ordinary NPC bow breaks still reject before resource
publication; full native drop consequences, compound shot publication,
Character input, flight/impact/recovery and normal-input/restart acceptance are
not closed by this storage prerequisite. S5 and M15 remain in progress.


### S5 checkpoint276: schema40 loose-item extras and World restoration

The custom OpenMW TES4 runtime envelope now stores nullable binary32 loose-item
condition and enchantment charge on native references. Absence selects the
winning base defaults; zero retains a broken or discharged instance. C++ binary,
canonical JSON and the independent Python codec preserve exact float bits,
including negative zero, subnormal values and the maximum finite binary32.
Readers reject malformed presence flags, nonfinite/negative values and extras
on actor references. Older envelopes retain absence without inventing damage.
This is not a claim about the original game's save format.

Actual World capture reads native placed-item extras. Restore preflights the
winning item category and prepares a complete CellRef candidate before any
clock, actor, inventory or reference publication. The final no-throw swap
retains the native FormKey and RefNum. Older saves clear previous item extras
and restore default selection. A changed winning category rejects the entire
restore before earlier resources change.

Two component tests, two World integration tests and one Python method cover
81 nullable condition/charge pairs, all39 legacy envelopes, malformed wire and
value inputs, signed-zero preservation, actual World capture/apply/default
migration, and rejection before resource publication. Normal checks pass2470
full component,1002 full engine and262 Python tests. Sanitizer checks pass2470
full component and1002 full engine tests. C++ selections match exact inventories
with no failures/skips; ASan leak checks are disabled. The final incremental engine
builds emit 0/0 warnings. Initial01 builds passed the same
suites but exposed two new assertion-macro dangling-else warnings; explicit
braces remove them in02. Initial01 also rebuilt unchanged inventory tests and
Character/actor-stats sources, with baseline initializer/range-copy/compiler
warnings. Byte equality against checkpoint275 is recorded in
normal02/unchanged-warning-sources.json. Initial evidence remains retained.

Separate normal and instrumented comparers each verify121 fixtures: all40
envelopes and81 item-extra pairs. C++ files and canonical JSON independently
round-trip through Python, and Python-written files round-trip through the
production C++ decoder/encoder, byte for byte. These checks establish custom
protocol agreement, not fresh-process game restart or normal-input acceptance.

Evidence: S5/native-loose-item-state-normal-02,
S5/native-loose-item-state-sanitized-02 and
S5/loose-item-extras-wire-cross-language-{normal,sanitized}-02.
Tested fingerprint: 1c3b0db4ae2715a0e280acd66c58b7b782fc69fb169f79df2287b404201a1186

Unprojected dynamic references are still retained without recreation. Atomic
drop/pickup, compound shot publication, Character input, native flight/impact/
recovery, and normal-input/in-flight restart acceptance remain open. Drawn
ordinary NPC bow breaks continue rejecting before publication until their
native drop path is implemented. S5 and M15 remain in progress.


### S5 checkpoint277: detached native cell-reference publication

CellStore can now prepare a new reference in a privately owned list node.
Preparation requires a loaded target and an unassigned reference without
registry, scene-node or local-script ownership. It leaves the cell's reference
list, state and caches untouched. The borrowed detached node can participate
in the existing prepared WorldModel registry replacement to reserve a RefNum
without consuming the live generated-reference counter.

After all compound preflights, the caller can commit the registry and immediately
splice that same node into the cell without allocation or callbacks. The node's
address, native FormKey, binary32 item extras and base identity survive this
publication. Cancellation leaves cell and registry state unchanged. Foreign,
moved-from, consumed and replaced-cell handles reject before publication;
an independent lifetime tag prevents an old handle from matching a new cell
constructed at the same address.

Four engine integration tests cover actual native REFR/WEAP condition negative
zero and charge7.25, cancelled identity reservations, registry/list publication
with the same node address, foreign/moved/repeated handles, registered/assigned/
scene-owned inputs, unloaded targets, owner address reuse and preserved projected
inventory count/condition/charge. Normal and ASan/UBSan checks each pass1006 full
engine tests, exact inventories and no failures/skips. ASan leak checks remain
disabled. Components, Python and schema40 codecs are unchanged from checkpoint276.

Incremental builds emit 2/11 warnings in unchanged actor-stat
tests, Character code and standard-library regex instantiations from unchanged
console code. Source equality against checkpoint276 is recorded in
normal01/unchanged-warning-sources.json. The new insertion API and cases emit
no warnings.

Evidence: S5/prepared-native-cell-insertion-normal-01 and
S5/prepared-native-cell-insertion-sanitized-01.
Tested fingerprint: 86f8798fb56f2a83742b04e5f96e04b3629faa35e4af26c3ad15817afdce3cc7

This is logical cell/registry staging, not a drop or pickup caller. It neither
publishes scene/loose-body physics nor recreates dynamic references during save
restore. The registry-then-cell interval must remain synchronous and callback
free. Full drop consequences, compound shot publication, Character input,
flight/impact/recovery and normal-input/restart acceptance remain open. S5 and
M15 remain in progress.


### S5 checkpoint278: native drop ExtraOwnership selection

The original drop prelude selects whether to preserve existing ExtraOwnership,
remove it, or set Player-base ownership. The new pure rule preserves the raw
native actor-state branches1/2/6 and compares signed integer prices directly
against the binary32 threshold without rounding the integer to binary32.
Cell ownership comes from the original cell owner getter; it is not a generic
loaded-cell predicate. This selection does not establish the final ownership
of a newly created dropped reference.

The native typed setting builder uses the independently verified absent
initializer45 for fValueofItemForNoOwnership. A profile-gated World resolver
reads current winning TES4 settings on each call and rejects wrong types and
nonfinite values, without falling back to legacy TES3 settings. Finite negative
overrides remain valid native comparisons. The installed-content audit confirms
the setting is absent after reverifying all11 plugin hashes.

Three new component cases cover exact integer/binary32 boundaries, all early
branches, malformed values and strict typed settings. One actual World/store
case covers absent settings, fresh overrides, deletion and legacy/profile
rejection. Each build passes2473 full component and1007 full engine tests with
exact inventories and no failures/skips. Normal and ASan/UBSan builds each
also match10692 independently executed original-instruction cases. Expected
selections derive solely from original owner remove/set requests. Both x87
precisions are represented. Leak checks remain disabled; Python/schema40
codecs are unchanged from checkpoint276.

Initial normal and instrumented engine attempts failed in the new fixture before
calling the resolver because its legacy TES3 Variant had no numeric type.
The fixture now constructs an explicit float Variant; both original failure
directories remain retained. Production logic did not change.

The original path executes actor-state, actor-cell, cell-owner and Player-base
getters. ExtraData owner storage/query and price queries remain declared
boundaries. Inputs model the nonnull ExtraData list used by the verified
broken-bow caller. Initial null-list fixture failures remain retained in
oracle01/02; oracle03 used a declared cell predicate, refined in oracle04 to
the actual cell-owner getter. No full inventory removal, reference creation,
physics or normal-input acceptance is inferred from these instruction cases.

Final incremental builds emit no warnings (0/0).
Initial broader builds emitted22/21 warnings in unchanged inventory/actor-stat
tests and existing Character template instantiations. Byte equality to
checkpoint277 is recorded in normal02/unchanged-warning-sources.json.

Evidence: S5/native-drop-extra-owner-normal-02,
S5/native-drop-extra-owner-sanitized-02,
S5/native-drop-owner-selection-oracle-04,
S5/native-drop-owner-selection-oracle-03/winning-threshold-audit.json and
S5/native-drop-owner-cross-comparison-{normal,sanitized}-01.
Original executable SHA256:
a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6.
Oracle source SHA256:
b12196b8cbca437b24a8a0c417bdd0df9641c7f6b42e278ee2c40cd8600750c3.
Tested fingerprint: d87bccd6c8d73fad2db86557402333a3c6b995ef5e21630c3b6253b3f964c923

Native drop publication, full dropped-reference ownership, loose-body physics,
pickup/restart, compound arrow publication and Character input remain open.
S5 and full M15 remain in progress.


### S5 checkpoint279: deferred native body admission and rollback

The owned body graph can now construct and restore its complete private bodies,
shapes, full inertia, materials and constraints before registering anything in
Bullet. Deferred construction requires a NativeDynamicsWorld lifetime identity.
Publication preserves the prepared body addresses and collision masks and
registers the native motion callback only after all bodies and constraints
succeed. Existing immediate callers retain their default behavior.

Partial admission is now rolled back even when a World inserts the current body
or constraint and then throws. The attempted object is included in cleanup
before entering the virtual admission call. Failure leaves no bodies,
constraints or native callback and permits a retry with the same private graph.
Successful publication is once-only. An externally registered body causes
rejection without removing its caller-owned proxy.

Native World markers retire explicitly during destruction. A retained strong
observer of the marker cannot prolong World validity, and a replacement World
at the same address cannot admit a stale graph. Native graphs may be discarded
after World destruction without dereferencing that World. Ordinary Bullet
Worlds retain the existing requirement to outlive their immediate graphs.
Detached restore avoids updating live AABBs; detached World steps do not advance
the private body. Graph destruction unregisters the native callback before
removing bodies while its World remains alive.

Six actual Bullet integration cases cover cancellation/restored detached state,
real impulse integration only after admission, exact mass/material/masks,
foreign and repeated publication, failures after body1/body2 and constraint
insertion, retry, immediate-constructor rollback, replaced World addresses,
strong marker observers, post-World discard and externally registered proxies.
Each normal and ASan/UBSan build passes2479 full component tests and1007 full
engine tests with exact inventories, no failures/skips and no compiler warnings.
ASan leak checks remain disabled. Python/schema40 codecs and native arithmetic
rules are unchanged.

Evidence: S5/deferred-native-body-publication-normal-01 and
S5/deferred-native-body-publication-sanitized-01.
Tested fingerprint: 47f9316b0ad319ddeb84d1d9e6e0e253392050b5ccd0f7f9c83e3c6ca0831712

Development input for the next drop integration is retained in
S5/native-loose-bow-body-audit-01: the hash-pinned stock iron bow has one convex
hull body with authored mass5; the arrow model has distinct arrow/quiver bodies.
That inspection used the checkpoint278 component library and is production NIF
metadata inspection, not independent raw-record verification or drop dynamics.

This admission API does not yet create a dropped reference, route ordinary
NPC bow breaks, own loose bodies in the scheduler, publish their rendered pose,
or restore/pick up dynamic items. Compound projectile/drop publication,
Character input and full flight/impact/recovery and normal-input/restart gates
remain open. S5 and full M15 remain in progress.


### S5 checkpoint280: separate loose-body scheduler ownership

The native physics scheduler now owns a prepared single non-actor body separately
from actor ragdolls. Cancellation registers no physics or collision routing.
Admission reserves routing before the final fallible body publication, then
inserts the prepared map node without allocation. A World that inserts a body
and throws leaves no physical or logical owner and permits retry. Foreign,
consumed, duplicate and changed-count preparations reject before admission.
Actor-body admission and rebinding also reject references already owned by
loose physics; the two owner maps cannot claim the same live reference.

Loose bodies receive native gravity and damping through the existing worker
barrier, even when no actors or movement jobs exist. Mixed actor/loose owners
share exactly one DynamicsWorld step. Per-body Bullet gravity stays disabled
and global gravity stays unchanged. Removal and scheduler unload/destruction
remove collision routing, physics and motion registrations. Clearing a scheduler
retires pending preparations; a replacement scheduler at the same address
rejects stale handles before accessing their borrowed reference.

The caller must keep the reference alive through preparation and registration
and remove its physics before deleting it. This scheduler API does not establish
a general live-reference lifetime guard. Its captured pose and native packed
velocities support validated whole-state restoration for the registered body.

Six parameterized integration cases run with zero, one and two workers:
private preparation/cancellation and ownership guards, a real collision floor,
one shared actor/loose step with independently pinned native gravity stores,
invalid snapshot rejection and physical free-flight continuation, post-insertion
admission failure/retry, and retired/reused schedulers after deleting the
borrowed reference. These use live abstract Static references and real Bullet
bodies; they are not actual WEAP drop or normal-input gameplay fixtures.

Both final normal and ASan/UBSan runs pass1025 full engine tests with exact
inventories, no failures/skips and no compiler warnings. Leak checks remain
disabled. Earlier 01 runs also passed1025; subsequent review added the cross-map
admission/rebind guard and regression assertions, then both 02 runs passed.
Component/Python/schema40 and native arithmetic sources are unchanged since
their preceding passing checks.

Evidence: S5/native-loose-body-scheduler-normal-02 and
S5/native-loose-body-scheduler-sanitized-02 (01 retained).
Tested fingerprint: f560ee038e1e9e57a5e0b759a30702300cf5e167a2ec68a9615a7764c5604e4d

PhysicsSystem/World drop integration, rendered pose publication, exact source
inventory transfer, pickup and dynamic-reference save recreation remain open.
The continuation case is an in-process physical test, not process restart or
normal-input acceptance. Compound projectile/drop publication, Character bow
input, native flight/impact/recovery and all required M15 campaigns remain open.
S5 and full M15 remain in progress.


### S5 checkpoint281: loose-body PhysicsSystem lifecycle and collision queries

PhysicsSystem now exposes detached loose-body preparation, validation,
publication, whole-state snapshot restoration and removal through its native
scheduler. A unique system identity rejects foreign and replaced-owner tokens
before their borrowed reference is accessed. Existing static, trigger and
movement-actor collision rejects preparation/publication; ordinary static and
actor admission also rejects a reference that already owns loose physics.

The ordinary removal path clears ignored collision pairs before removing the
loose body. Reference transfer reuses the existing physical body, shape,
velocity and collision identity while updating its PtrHolder and owner-map
key without allocation. Invalid or occupied destinations preserve the prior
owner. Transfer retires pending scheduler preparations so an old reference
cannot subsequently gain a second body; stale handles reject even after that
reference is deleted. Collision rays report the transferred reference and can
explicitly ignore loose bodies.

Three new parameterized cases run with zero, one and two workers. They exercise
actual PhysicsSystem admission/cancellation and foreign/repeated publication,
ray hit/ignore routing, validated snapshot restoration, body-preserving reference
transfer, deleted old references, ordinary removal and duplicate static/actor
admission. A placement-new system replacement rejects an old preparation after
its borrowed reference has been deleted. These use abstract live Static
references and real collision bodies; they do not create a native dropped WEAP.

Both normal and ASan/UBSan builds pass1034 full engine tests with exact
inventories, no failures/skips and no compiler warnings. Leak checks remain
disabled. Component/Python/schema40 and native arithmetic sources are unchanged.

Evidence: S5/native-loose-body-physicssystem-normal-01 and
S5/native-loose-body-physicssystem-sanitized-01.
Tested fingerprint: a610a0020825f712aa5b70c2b04a6bcb5385b12e28be1d0c6f8b2b5f1f937504

The caller still owns reference/cell lifetime, profile/model selection and
rendered pose publication. The adapter does not yet connect Character bow input,
create a dropped reference, transfer source inventory, recreate dynamic items
on load or complete pickup. Reversible rendering admission is the next bounded
prerequisite for compound projectile/drop publication. Native flight/impact,
normal-input/restart campaigns and full M15 acceptance remain open.
S5 remains in progress.


### S5 checkpoint282: detached and reversible model admission

Objects can now prepare a non-actor, non-animated model on an explicitly detached
parent without changing the reference base node, live cell scene graph or
animation registry. The real ObjectAnimation constructor builds that private
model and preserves ordinary light/particle/glow behavior. Prepared animation
and optional cell registry nodes allocate before publication. Strong owner
identity plus exact count, cell, placement and scale checks reject foreign,
replaced or stale preparations before scene admission.

Publication checks the parent addChild result and rolls back an attempted
attachment even if the parent inserted the child before returning false or
throwing. Registry insertion and base-node assignment follow successful scene
admission without allocating. A failed attachment can retry the same private
model. Successful publication remains owned by Objects after the preparation
is discarded and uses ordinary removal/unref behavior thereafter.

A synchronous rollback operation removes only the exact published model,
clears its base node and removes a newly created empty cell root. It preserves
existing cell siblings, refuses to remove a replacement animation and consumes
the rolled-back preparation. It uses no deferred unref allocation and emits no
game observers. Callers must use it before publishing compound resources or
allowing another scene mutation. Borrowed reference/cell lifetime remains the
caller's responsibility.

SceneManager now supports strict template loading. It rethrows an uncached
load failure before creating an error marker and rejects a previously cached
fallback marker. Ordinary callers retain their default fallback behavior.
Prepared model admission strictly checks its source template before cloning it,
so a missing or malformed model cannot silently become a successful native drop
represented by an error marker.

Two component cases cover missing/malformed source bytes, actual test-owned
scene-file loading, cache identity, strict rejection and preserved ordinary
fallback behavior. Six engine cases use actual OSG graphs and ObjectAnimation:
private construction/cancellation, source/owner guards, rejected and
post-insertion failed attachment/retry, normal removal, synchronous rollback,
existing cell preservation, replacement models and reused Objects addresses
after deleting the borrowed reference. Their authored OSG scene fixtures are
not stock native NIF assets or rendered GL captures.

Both final normal and ASan/UBSan runs pass1040 full engine tests with exact
inventories, no failures/skips and no compiler warnings. Leak checks remain
disabled. The initial normal run passed1040; the first sanitizer engine run
aborted on the first new rendering fixture because it had created a reference
without a cell, violating Ptr.getCell's debug assertion. The fixture now uses
a real loaded CellStore, and preparation explicitly rejects orphan references
before calling getCell. The next sanitizer02 attempt exposed a second fixture
lifetime error: its CellVariant borrowed an ESM::Cell record local to a helper.
The source record is now owned by the fixture and outlives CellStore. Both
final03 runs pass the corrected fixtures and orphan-rejection assertion.
Both earlier01/02 attempts remain in their original evidence directories.

The unchanged strict-loader component implementation/tests passed2481 full
component cases in both initial01 builds with exact inventories and no failures,
skips or compiler warnings. Their fingerprint is 0408fc9a56954aef52bc87ab8a70921b12dc122c1596cf6496d63b6c75d01df1; the correction
changes only Objects preparation and the engine fixture. The broader initial
engine rebuild emitted two normal and one sanitized warnings in existing
Character/actor-stats test sources byte-identical to checkpoint281, verified
by normal-01/unchanged-warning-sources.json. No changed file produced a warning.
Python/schema40 and native arithmetic are unchanged.

Evidence: S5/prepared-model-publication-normal-03 and
S5/prepared-model-publication-sanitized-03; both01/02 directories retained.
Latest engine-tested fingerprint: 828a2de7a0ccd8526e226c75e8efbaa803e6d513257c55dbda5d6293464c9533

This reversible admission primitive does not yet publish an actual native
dropped reference, couple source inventory/weapon wear with arrow and dropped
body admission, update loose-item rendered poses or recreate/pick up dynamic
items. Character bow input, native flight/impact/recovery and normal-input,
restart and visual campaign acceptance remain open. S5 and full M15 remain
in progress.


### S5 checkpoint283: final equipped-weapon removal and original base-gold query

InventoryStore can prepare removal of exactly one projected weapon instance
from CarriedRight without changing slots, quantity, extras or observers.
An opaque preparation captures the inventory lifetime identity and exact item.
Foreign, replaced, cleared, reassigned, swapped, reused and changed-count
inventories reject publication before dereferencing their former item.
A weapon claimed by another slot is rejected.

The callback-free commit changes only quantity, CarriedRight, selected
enchantment and inventory caches. It deliberately preserves final item
condition and enchantment charge written by the compound caller immediately
before removal; swapping an older CellRef would restore stale wear or quantity.
Unrelated ammunition retains its identity and count. The compound caller must
validate all original resource snapshots before its first resource write.

Deferred notification removes the reference script, notifies equipment and
reports one item removed. It marks notification consumed before invoking any
observer, checks the inventory identity between observers and cannot replay
after a throwing observer or an observer deleting/replacing the inventory.
Discarding a preparation after owner destruction does not touch its old item.

Three new actual native-world/shared-inventory integration cases exercise
cancellation, final zero condition with retained fractional enchantment charge,
selected-slot clearing, ammunition conservation, source/owner invalidation,
owner-address reuse and reentrant/throwing observer deletion.
Both corrected normal and ASan/UBSan builds pass1043 full engine cases with
exact inventories, no failures/skips and no compiler warnings. Leak checks
remain disabled. Both initial01 attempts ran1043 cases with one failure in the
new fixture: it used the integer condition charge field for a fractional
enchantment charge and then correctly zeroed that condition. The fixture now
uses setEnchantmentCharge/getEnchantmentCharge; no production behavior changed
for that correction. Failed01 evidence remains retained.

An independent original-executable query audit also establishes the native
WEAP price source for drop ownership. Original cdecl470520 executes its actual
CRT RTTI against original WEAP vtable A45354, casting TESForm to TESValueForm
B05C24 at object offset112 and returning the unsigned gold DWORD at offset116
unchanged. All192 cases pass:12 gold bit patterns across4 base health values,
2 header flags and both x87 control words. No game-callee stub is reached.
The mapped raw WEAP fixture and Win32 FS exception-chain head are explicit
fixture boundaries; original RTTI and SEH restoration execute. This is not a
native weapon constructor, inventory removal, drop factory or gameplay probe.
The downstream verified ownership selector interprets EAX through signed
int32 FILD, so high-bit gold values preserve that signed interpretation;
condition-adjusted shared item value is not this query's input.

Evidence: S5/prepared-equipped-weapon-removal-normal-02 and
S5/prepared-equipped-weapon-removal-sanitized-02, with failed01 retained.
Original query: S5/native-weapon-drop-value-oracle-01/verification.json and
cases.json; pinned original PE SHA256
a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6.
Oracle source SHA256:
31a60563edcce12a1bfe08d4bfd70f810bb0b24a51d0902f334f758176eeba02.
Latest engine-tested fingerprint: 1314116165908e5f9bab9dff7b23ab1cfa85691e48e314827d37081d7cf41255

This primitive does not create a dropped native reference, admit its model or
body, reserve a dynamic identity, couple arrow publication with wear/removal,
persist loose-body motion or implement pickup. Ordinary NPC broken-bow release
continues rejecting preparation until the complete transaction is wired.
Native Drop ObScript context and normal-input/restart campaigns remain open.
S5 and full M15 remain in progress.


### S5 checkpoint284: compound native loose-weapon admission

WorldModel now owns a preparation that stages a native WEAP cell node,
its registry entry, an actual rendered ObjectAnimation and a native loose
physical body. Preparation leaves the cell, registry, scene and physics
unchanged. Cancellation destroys the private preparations before their
backing reference. Publication adds the model and body before the guarded,
callback-free registry replacement and same-address CellStore splice.
Scene failure or a scene callback changing/clearing the registry rolls back
the exact published model before any logical or physical item escapes.

Admission requires an enabled, undeleted single native instance, exact
winning WEAP base and managed native cell identities, validated dynamic
namespace, unresolved legacy RefNum, unit scale, resolved model, valid native
condition/enchantment capacity and one valid loose body. Existing stable
identities reject duplication. The caller still owns dynamic serial
reservation, final ownership/placement resolution and authored asset/body
binding.

Registry preparations now reject retired WorldModel instances before touching
borrowed references; clear retires that identity before cell destruction.
Rendered-model and physical preparations expose weak owner-lifetime guards,
allowing the compound transaction to reject destroyed managers safely.
Seven new actual WorldModel/Objects/PhysicsSystem cases cover cancellation,
same-instance publication, exact negative-zero condition and fractional
enchantment charge, invalid identity/extras/body, scene rejection and
insert-then-throw, reentrant registry replacement and world clear, manager
destruction and WorldModel address reuse with a deleted borrowed reference.
These fixtures use an owned OSG model and synthetic spherical body; they do
not establish stock NIF rendering or gameplay acceptance.

Corrected normal and ASan/UBSan builds pass1050 full engine cases with exact
inventories, no failures/skips and no compiler warnings. Leak checks remain
disabled. Both initial01 runs stopped in the new fixture constructor because
its Environment/store context was missing. A retained fixture-only ASan
diagnostic against normal production archives identified native Cell's store
lookup; it is not full sanitizer coverage. The fixture now owns Environment
for its full lifetime and registers the actual store before creating Cell.
The failed full runs and partial diagnostic remain retained. Warnings in the
initial broad rebuild came from unchanged existing sources, with byte hashes
recorded in native-loose-weapon-admission-normal-01/unchanged-warning-sources.json.

An independent original-executable audit executes full ownership-claim query
4CAAC0 across3392 cases and10104 actual CRT RTTI casts. Original actor/base/
faction vtables, NPC faction-list lookup and cached ExtraOwnership/ExtraRank
queries execute, with Win32 FS/TLS and raw object/cache layouts as explicit
fixture boundaries. No game-callee stub is reached. Unowned cells return
false from this positive ownership-claim query. An absent required rank or
only -1 normalizes to0; other signed required ranks remain unchanged.
Player faction raw flag08 filters membership to absent rank-1, whereas NPC
membership ignores that flag. Absent rank-1 can satisfy required ranks<=-2.
The semantic name of raw08 is not established. Slow ExtraData cache/list
locking, native construction, the whole drop factory and gameplay remain
outside this audit; no C++ cross-comparison is claimed.

Evidence: S5/native-loose-weapon-admission-normal-02 and
S5/native-loose-weapon-admission-sanitized-02; failed01 runs and
S5/native-loose-weapon-fixture-diagnostic-01 retained.
Original audit: S5/native-drop-cell-access-oracle-01/verification.json and
cases.json. Original PE SHA256:
a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6.
Oracle source SHA256:
950ddcc8ae718ceffbeeac8048d7fd81b28c6c89122505ed3c53095516d8c108.
Latest engine-tested fingerprint: 67443d5835dca9f42559fa694cd197735af58267e3ce6372fdd9839c644cfe95

No World drop frontend or arrow/source-inventory transaction invokes this
admission yet. Dynamic serial allocation, loose-body world-frame rendering,
save/recreation, pickup, Drop script context and normal-input/restart courses
remain open. Ordinary NPC broken-bow release retains its explicit preparation
rejection until that complete transaction exists. S5 and full M15 remain in progress.


### S5 checkpoint285: exact bow-release extras and condition-only publication

Prepared native bow release now snapshots the instance's enchantment charge
as binary32 bits, owner, faction, permission global and required faction rank.
Changes to these inputs invalidate release before ammunition, fatigue,
condition, process action or action ownership is written. This includes
negative-zero versus positive-zero charge. Discarding a stale preparation
and retrying captures the new extras and consumes the pending release once.

Final wear publication no longer swaps the whole CellRef. A checked,
allocation-free and noexcept condition-storage swap changes only the optional
native condition and change flags, preserving identity, placement, ownership,
enchantment charge and every unrelated field. It supports projected inventory
and native placed-item variants, including swapping absent/negative-zero
conditions across them, and rejects actor variants before either changes.
Bow validation checks both supported condition storage variants before the
first ammunition write.

Three new engine cases exercise twelve actual Player/NPC stale-extra changes and
fresh retries, independent placement/scale changes surviving final wear, and
native/projected condition-only swapping with actor rejection.
Both corrected normal and ASan/UBSan suites pass1053 full engine cases with
exact inventories, no failures/skips or compiler warnings. Leak checks remain
disabled. Both initial01 attempts are retained: a new fixture placed two
SCOPED_TRACE macros on one line, producing duplicate generated variable
names and a compile failure before tests. The fixture now uses separate
lines. Review also found its signed-zero change would be ignored by the
shared TES3 setter; it now passes through absent-charge sentinel-1 before
setting+0, preserving the shared TES3 setter behavior. Initial broad-rebuild warnings from
unchanged sources are retained rather than attributed to these new APIs.

Evidence: S5/bow-release-extra-snapshot-normal-02 and
S5/bow-release-extra-snapshot-sanitized-02, with initial01 retained.
Latest engine-tested fingerprint: 1df651092488642ff3481dd64cffee0fe019478057ac28ecc531950b26da44c3

This closes resource-snapshot and condition-publication defects in prepared
release. It does not attach the bow frontend, admit or move native arrows,
publish an ordinary NPC broken-bow drop, persist loose objects/projectiles,
execute enchantment effects or establish normal-input/restart acceptance.
S5 and full M15 remain in progress.


### S5 checkpoint286: optional signed native CELL ownership ranks

Native TES4 CELL loading now preserves XRNK as an optional signed int32.
Absence, explicit zero and negative ranks remain distinct, and each load
clears the previous value before applying a replacement record. TES4 XRNK
must contain exactly four bytes and appear once; malformed sizes and duplicate
zero/nonzero ranks fail with a CELL XRNK diagnostic. Other supported formats
retain their previous compatibility behavior. Typed esmtool CELL dumps expose
the canonical header key and rank presence/value.

Five new component cases cover both TES4 header versions, compressed and
uncompressed records, signed extremes, malformed lengths, duplicate ranks,
reused loaders and following-subrecord alignment. One actual ESMStore load
case covers master-order owner remapping, rank replacement, omission and
deletion. Both normal and ASan/UBSan runs pass2486 full component cases and
1054 full engine cases with exact inventories and no failures/skips.
Leak checks remain disabled. Rebuild warnings are retained; relevant warning
sources match checkpoint285 byte-for-byte.

Both initial01 attempts are retained. The new positive CELL fixtures lacked
the real GRUP context required by Cell::load and failed before rank parsing;
negative fixtures could then pass from that unrelated exception. Corrected
fixtures supply real CELL groups and assert the CELL XRNK diagnostic for
negative cases. The focused five-case normal fixture check is retained
separately and is not counted as a full suite.

An independent raw audit verifies all11 pinned official plugins and counts
35787 physical CELL records,35565 live winners,33 physical explicit ranks and
32 winning explicit ranks. Both actual typed esmtool binaries independently
match every physical rank presence/value and every winning rank after the
pinned load order. Per-plugin uniqueness, plugin hashes, binary hashes and
unchanged before/after source fingerprints are verified. This is typed
content readback, not a gameplay ownership query.

Evidence: S5/native-cell-ownership-rank-normal-03 and
S5/native-cell-ownership-rank-sanitized-03; failed01 and focused fixture-normal-01
retained. Independent input: S5/native-cell-ownership-rank-raw-audit-01.
Typed comparisons: S5/native-cell-ownership-rank-typed-normal-03 and
S5/native-cell-ownership-rank-typed-sanitized-03.
Initial typed-comparer01 startup failed before any dump because the skill helper
import lacked its sibling module search path. Both failures are retained;
corrected02 uses the actual skill scripts directory. The corrected normal02 comparison passed; sanitized02 found a real existing
stack-use-after-scope in typed record filtering: recordType viewed a temporary
ESM::NAME. A named ESM::NAME now owns that storage through filtering and printing.
Both02 attempts are retained; fresh03 builds and comparisons cover the correction.
Raw audit source SHA256: 9c362c182f85630abdf9bc5de7041122aec813421ad475dc8a8ec659e4930683
Typed comparator SHA256: cf565922dc502fd34d41f939b12dd9ba2fd8622dd6fb2d03c89d48e62f952f72
Latest tested fingerprint: 81a850a6b84b60348e8298e3df9d4b9b2170ff9974e4733566dd0afe3b19b4e1

This closes the missing static CELL rank input. It does not implement mutable
native faction membership, final drop ownership, a native loose-weapon factory,
bow frontend/arrow admission or crime/runtime acceptance. S2 and S5 remain in
progress. The next integration task is the compound native broken-bow release,
using the prepared inventory, cell, model and physical admission foundations.


### S5 checkpoint287: provisional World-owned native reference identities

The actual native World now prepares fixed-namespace native-reference keys
from the same serial captured/restored by T4ST. Preparation and destruction
advance nothing. A checked noexcept commit publishes the next serial once;
competing reservations become stale after the first commit. This is serial
publication only: the compound item transaction must validate all participants
before a callback-free reference/inventory/serial commit.

Weak World lifetime identity and the actual WorldModel preparation guard
reject handles after destruction, registry changes, clear or successful
restore, including restoring the same serial. The guard precedes borrowed
World reads. Resident identity collision checks include disabled, deleted and
unregistered loaded nodes; cached save identities are also checked. Exhaustion
rejects before uint64 wrap. Native restore rejects a high-water serial that
would reuse any saved native-reference key before changing global time or
serials. An invalid restore leaves existing preparations valid because it has
not published logical state.

Six new actual World cases exercise cancellation, competing/repeated commits,
binary serial roundtrip, registry and clear invalidation, same-serial restore,
continued allocation from restored serials, saved/resident collisions,
exhaustion and owner destruction. Both normal and ASan/UBSan full engine suites
pass1060 cases with exact inventories and no failures/skips. Leak checks remain
disabled. Relevant rebuild-warning sources match checkpoint286 unchanged.
Components, schema layout and Python codecs did not change; no fresh component
or Python result is claimed for this checkpoint.

The committed checkpoint286 World header independently fails compilation of
the new reservation API as expected. This characterizes API absence only, not
a baseline gameplay assertion. The saved next_dynamic_serial field already
existed; the new allocator adds no wire fields or schema version.

Evidence: S5/native-reference-identity-normal-01,
S5/native-reference-identity-sanitized-01 and
S5/native-reference-identity-api-baseline-01.
Latest engine-tested fingerprint: cf6b73b2adce8735f91418d2c92eb695bd4da9c34eb1d0ea1f0a9c2773076159

Identity reservation does not create a loose WEAP, remove source inventory,
construct asset-bound physics, publish an arrow or prove pickup/restart
acceptance. The ordinary NPC broken-bow safety rejection remains until the
complete transaction is installed. S5 and full M15 remain in progress.


### S5 checkpoint288: last validation and compound loose-weapon publication

Prepared native loose-weapon admission accepts paired noexcept validation and
publication hooks. It checks source inputs before scene admission, rechecks its
own lifetime/registry/cell guards after validation, completes fallible model
and physical admission, then validates source inputs and guards again before
any source inventory, serial, cell or registry write. Late rejection rolls the
admitted model/body back. Incomplete hook pairs reject before admission.
Rollback checks external owner lifetime before cleanup. The publication hook
runs once in the no-callback/no-allocation interval before the existing
prepared registry/cell commits. It must leave those guards unchanged.

Three new engine cases exercise incomplete hooks, initial/final rejection,
actual model/body rollback, hook ordering and repeated-commit suppression.
One case composes actual NPC InventoryStore equipped-instance removal and
the real World's saved serial reservation with actual Objects/PhysicsSystem
and managed native CELL admission. Success publishes one native dropped
instance with zero condition and charge7.25, removes one projected source bow
with matching final wear/charge, clears its slot and advances the serial once.
Failures cover changed source condition, an externally committed serial,
scene rejection and insertion followed by a throw. They leave source count
and slot intact and publish no drop/model/body. External condition/serial
changes remain intact; rollback does not reverse someone else's changes.

The integration uses an owned OSGT and synthetic one-body sphere at the asset
boundary. It does not exercise the stock NIF factory or normal bow release.
Both normal and ASan/UBSan full engine suites pass1063 cases with exact
inventories and no failures/skips. Leak checks remain disabled. Relevant
rebuild-warning sources match checkpoint287 unchanged. Component/Python/schema
code did not change; no fresh component/Python pass is claimed.

A syntax attempt initially failed on the nested aggregate default argument;
an explicit no-argument forwarding overload keeps prior callers compatible.
Both initial01 full runs failed in the new World fixture during setup:
normal segfault and sanitizer WindowManager-null assertion. Direct inventory
add invokes UI updates; corrected setup uses the existing actual staged actor
inventory path and explicitly registers the projected weapon class. These
failures and fixture-only diagnostics are retained; partial ASan diagnostic
coverage is not counted as a full sanitizer pass.
The retained committed checkpoint287 header independently lacks the new
compound API. That is API absence characterization, not a gameplay baseline.

Evidence: S5/native-loose-compound-publication-normal-02,
S5/native-loose-compound-publication-sanitized-02,
S5/native-loose-compound-syntax-01 and
S5/native-loose-compound-api-baseline-01.
Latest engine-tested fingerprint: 841df5268eb295f11426aa56dce119ca56f2e589174eaa31959d75b587663a9a

This provides the compound publication interval. The actual native drop
factory, original ownership/post-shot state mapping, bow frontend/arrow
publication, loose-body model synchronization/recreation and exact pickup
remain open. Ordinary NPC broken-bow release remains safely rejected until
its complete transaction is installed. S5 and full M15 remain in progress.
