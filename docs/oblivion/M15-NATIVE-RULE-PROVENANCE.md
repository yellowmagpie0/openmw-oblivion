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
