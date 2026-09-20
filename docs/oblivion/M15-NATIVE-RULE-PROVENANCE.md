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
installed master override `0287A3` is 35 degrees. A live console lookup is still
pending and is explicitly marked so in the setting inventory. Fourteen supplied-
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
