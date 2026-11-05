# Full sewer escape — current handoff

## Target and remaining acceptance

Play the original opening continuously from prison through the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
MQ01 stage88 exit and fresh-process save/load outdoors. No injected health,
quest/completion state or skipped required encounters qualifies.
**Full escape remains incomplete.** Remaining: functional shaman staff encounter,
rest of caverns, reunion, Emperor/Amulet, character choices, final sewer exit,
outdoor reload and continuous prison-to-outdoors acceptance. Upright goblin
corpses remain a visible presentation gap.

## Current chunk — native healing and restart

Normal C input casts the selected owned instantaneous self-restoration spell,
plays the native cast sequence while retaining the weapon, and applies its cost
and restoration once at release. Schema87 persists pending casts. Cancellation,
resource admission and action exclusion belong to the combat service. The
successful release records Restoration tutorial history using SKIL usage slot1.
The genuine axe-equipped Player heals7.04 for19 magicka (armor affects spell power).

Ordinary C/F5 captured wind-up at0.332s, before release0.366667s. A fresh process
resumes it, heals once and records0.6 Restoration credit. Another fresh process
loads the released0.664s checkpoint, finishes recovery without another heal,
debit or history increment. Whole restored state matches except existing animated
body velocity projections (max error2.98e-8; tolerance1e-5); packed velocities and
all other ownership/state remain exact. Six further spaced casts heal six times;
three faster low-magicka attempts admit one, without resource underflow.

Validation: final focused47 engine cases pass normally and under ASan/UBSan
with engine leak checking. Full2182 engine/3257 component gates passed before
the final one-line skill-credit correction; unchanged Python556 and four codec
cases retain their original provenance. Runtime courses cover the correction.

Evidence, build/test provenance, limits and retained attempts:
[SEWER-HEAL-CAST-VERIFICATION.json](SEWER-HEAL-CAST-VERIFICATION.json).
The captured courses prove healing with the axe drawn and sheathed, not general
magic, NPC casting, spell projectiles or all weapon animation variants. Native
spell hotkeys, learned spellbook ownership and noninstant effects remain outside
this chunk. General loot currently takes all container contents with a notification;
selective container browsing remains a limitation.

## Next action — shaman staff, then reunion

The healed pre-approach save is
`build/oblivion-compat/sewer-escape/tutorial-heal-low-magicka-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
SHA256:`9db20fa7762acc1ff9da633e28c36ec9208b0aac789d9a2602870d37afd29006`.
Player health65.990028, magicka20.439541, Restoration history4.8, no pending cast;
position(5508.464,3257.950,-448.217), axe equipped, seven cavern goblins dead.

`cavern-shaman-approach-01` reaches all six original-pathgrid waypoints under
ordinary movement, to(5685.125,3945.735,-456.408). Capture inspected. The shaman
moves near the Player but staff066c45 remains unequipped with charge150. This
proves approach, **not a functional shaman fight**. Implement actual staff selection,
cast animation, target projectile/contact damage and charge/save ownership; return
to the pristine healed checkpoint to verify the encounter. Existing melee AI only
selects ordinary physical weapons. Do not accept an inert or unarmed substitute.
Shaman03b52e/base03b52d owns enchantment066c42 (SHDG5, instant target, cost13).
Player Flare shares the next required target-magic path; avoid unrelated magic scope.

Remaining rats047b65/66/67 and original triggers must be checked as encountered.
Door03816c at(4384,6144,64) connects to026c36 in Imperial Subterrane022288 near
(3200,3203,-761). Reunion is record-inspected, not traversal-accepted.

## Reusable earlier evidence

- [Completed melee tutorial slice](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md): prison,
  first assassin ambush, Player participation, escort departure, fresh combat reload.
- [Trap hazards](SEWER-TRAP-HAZARD-VERIFICATION.json) and
  [rolling logs](SEWER-ROLLING-LOG-VERIFICATION.json): original activation,
  environmental damage, persistent contact/body state and restart without replay.
- [Cavern healing/patrol](SEWER-CAVERN-HEALING-VERIFICATION.json) and
  [eastern goblin](SEWER-EASTERN-CAVERN-VERIFICATION.json): ordinary loot,
  potions, equipment, navigation and melee. Preserve failed/death attempts.
- [Spell cost](SEWER-SPELL-COST-VERIFICATION.json) and
  [spell menu](SEWER-SPELL-MENU-VERIFICATION.json): independent cost evidence,
  normal selection/HUD and fresh selected-spell reload.

Earlier checks retain their original provenance; do not relabel them as current
source runs. Historical detail: [SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md).
