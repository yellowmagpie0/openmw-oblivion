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
