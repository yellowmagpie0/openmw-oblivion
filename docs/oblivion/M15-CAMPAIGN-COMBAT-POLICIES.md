# M15 selected campaign combat inputs

Status: reviewed data inputs; native gameplay acceptance remains pending.
This supplements the actor/reference selection in [the case inventory](M15-CASE-INVENTORY.json).
All short hexadecimal IDs below belong to `Oblivion.esm`, not a load-order slot.
Winning inputs use the eleven-plugin hash/order lock in
`scripts/data/oblivion_compat/m15_count_lock.json`.

## Evidence and interpretation

`build/oblivion-compat/m15/S2/audit-12/m15-audit.json` resolves every selected
actor's style, AI values, inventory and spell links. The independent C++
decode/default/resolution comparison in `S2/policy-audit-01/comparison.json`
checks all 7,540 fields across 130 policies. `campaign-records.json` in that
directory independently retains winning raw spell, race, class, leveled-list,
equipment and effect records. These are data checks, not observed AI choices.

Two resolved styles cover the selected combat opponents. A null CSTY invokes
the native default getters; it does not mean passive or unspecified behavior.
The numbers below are style inputs, not unconditional per-frame probabilities.
The AI controller must apply the reviewed context/skill/fatigue rules before
selection, respect weapon availability, and preserve the source of random draws.

| Input | Native default | `0ca0d7` NPCBanditMissile |
| --- | ---: | ---: |
| Attack / block / dodge chance | 40 / 30 / 75 | 75 / 10 / 50 |
| Power attack chance | 25 | 33 |
| Each directional power weight | 20 | 20 |
| Idle / hold interval (seconds) | 0.5–1.5 | 0.5–1.5 |
| Melee / ranged switch distance | 250 / 1000 | 250 / 1000 |
| Ranged standoff | 500 | 750 |
| Buff / group standoff | 325 / 325 | 500 / 325 |
| Rush chance / distance multiplier | 25 / 1 | 25 / 1 |
| Standard flags | 0 | 64: prefer ranged |

Both policies use native default advanced inputs. The missile record does not
set the Advanced flag. Neither disables target acquisition. Distances are
native world units; style ranges do not replace the equipped weapon's reach
or projectile collision. The complete float values and provenance are retained
in the audit and [default catalog](M15-COMBAT-STYLE-DEFAULTS.json).

## E1: first tutorial ambush

Bases `014a27`, `014a28`, `017617`, `017619` all use the native default style,
aggression 50, confidence 100 and responsibility 0. Their references and
scripted entry/death conditions remain those in the case inventory.
Their starting hood `08d755` and robe `024de2` are clothing, not the armor and
weapon that explain their intended melee combat.

All four have ability `03e91b` (AbBoundArmorMaceMD). Its ordered self effects
are MYTH, MYHL and BWMA, each with authored duration 1000. The effect records
associate them with armor `033525`, helmet `051b47` and mace `026274`.
The mace is one-handed Blunt, base damage 22, speed 0.9, reach 1 and weight 0.
Its weapon flag is retained by the equipment audit. Do not replace these
actors with ordinary unarmed opponents because the bound-item effects are
not yet executed. Bound creation/equipment/removal and its save/death behavior
are narrow campaign prerequisites; general magic remains M16.

The first actor carries a weak healing potion (`009310`, Restore Health 20);
the fourth carries weak sorcery (`00931a`, Restore Magicka 50). All four are
Imperials and inherit the racial powers Star of the West and Voice of the
Emperor. Inventory presence and racial eligibility do not prove consumption or
casting. Record the actual selection in the original and native campaign runs.

Use native weapon damage, fatigue, condition, blocking and contact rules once
the equipped instance is resolved. Their scripted activation must reach the
same combat authority as ordinary AI. Renault's authored death and essential
escort recovery remain distinct from killing these four mortal opponents.

## E2: Vilverin entrance circuit

The five fixed references select bases `06c355`, `06c357` (twice), `06c35a`
and `06c35d`. All use the native default style; aggression is 100,
responsibility 0, and confidence 75 except boss base `06c35a`'s 100.
These actors have no directly authored SPLO entries. Their inventory lists
resolve weapons and light armor; list chance-none values and duplicate entries
must be preserved rather than granting every candidate item.

The melee spawn origin selects `03407b`. All ten candidate bases use the
native default style, aggression 100, confidence 95 and responsibility 0:
`03db31`, `03db33`, `03db34`, `03db35`, `03db27`, `03db2b`, `03db2d`,
`03db2f`, `069ad5`, `069ad1`.

Both missile origins select `03407c`. All ten candidate bases use
`0ca0d7`, aggression 100, confidence 95 and responsibility 0:
`03dc42`, `03dc43`, `03dc44`, `03dc45`, `03dc46`, `03dc47`, `03dc48`,
`03dc23`, `069ad2`, `069ad3`.

All twenty entries have level 1 and count 1; both actor lists have chance-none
0 and flags 3. None of these bases has a direct SPLO entry. They still have
racial dependencies: melee candidates are Nord or Redguard; missile candidates
are Wood Elf or Dark Elf. An empty actor SPLO list is not proof of no abilities.

The ranged policy prefers the bow but still specifies melee switching. The
runtime must resolve actual bow/ammo/backup instances, consume ammunition once
at release, and use the same collision and cover geometry as the player.
Melee opponents use their selected weapon's type, reach, enchantment dependency and condition, including
two-handed block behavior if such a weapon wins its list. Selection, spawn
identity and inventory must persist across the door/restart cases. No fixed
roll is substituted to simplify the campaign.

## E3: first Arena match

Base `02a2b1`, reference `18ae5b`, uses native default style, aggression 0,
confidence 100 and responsibility 20. Low aggression does not suppress the
match's authored StartCombat. Owyn (`0222b6`) is essential and uses default
style, aggression 5, confidence 80 and responsibility 40.

At level 1 the opponent's leveled entries resolve Arena Iron Longsword
`0977d1`, Fur Helmet `0733e2` and Fur Shield `0977be`, alongside yellow light
raiment `0236f0` and healing potion `098496` (Restore Health 35). The sword
has base damage 10, speed/reach 1, weight 20 and base condition 140.
Arena equipment scripts and corpse-loot prohibition must remain effective.

Ability `0c425a` contains self effects Resist Magic 20, Fortify Attribute 70
for native actor value 4 (**Speed**), Fortify Skill 70 for value 13
(**Athletics**), and Resist Normal Weapons 20. The zero-duration ability
entries must not be discarded as expired spells. In particular, normal-weapon
resistance is an explicit damage integration dependency, not an armor rating.
The actor is a Wood Elf with racial disease resistance and Beast Tongue.

Ability application/removal, actor-value modification and the applicable
resistance branch require narrow campaign support or remain explicit open
acceptance dependencies. General spellcasting is still M16. The physical
controller must preserve lawful Arena combat, shield/weapon blocking and real
death, then let authored match state produce the reward exactly once.

## E4 and jail guards

Rindir (`015e9c`, reference `01d15e`) uses default style, aggression 5,
confidence/responsibility 50, no direct SPLO, and Wood Elf racial inputs.
His selected shop staff is a theft target; its presence does not demonstrate
staff casting. Witness/disposition tests and authorized ownership controls
must decide crime independently of whether a combat target was acquired.

Selected jailor bases `18d1f2`, `18d1f4`, `028c7f`, `028c80` all use default
style, aggression 5, confidence/responsibility 100, and no direct SPLO.
All are Imperial. Signed inventory counts and leveled potion lists are retained
in the audit. The Imperial City night jailor is non-respawning; the other
three selected jailors respawn. None is essential. Guards require their native
arrest/report/legal authority, not an aggression-only attack shortcut.

## Remaining acceptance work

The selected style inputs are resolved and explained here. This does not close
S2's remaining rule families, prove AI sampling/timing, execute any magic, or
close S3–S14. Original combat-policy behavior, native campaign playthroughs,
normal interaction, audiovisual behavior and restart persistence remain gates.
Never classify an unresolved ability as executed merely because its record
or equipment link decoded successfully.
