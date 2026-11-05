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

## Staff implementation — integration checks passed

The original shaman now equips staff066c45, plays `staffattackright`, releases
at its original Hit key (0.7s), spends charge and creates a swept shock projectile.
Schema88 owns projectile flight and serials. The current bounded charge policy
uses authored ENCH cost13; exact skill-adjusted staff cost is not established.
Target magic is limited to instantaneous shock/fire damage; Player Flare is not
wired to casting yet. Absorption/reflection remain unsupported.

Evidence under `build/oblivion-compat/sewer-escape/`:

- `shaman-staff-integration-02`: 50 selected engine tests pass and game built.
  184/186 selected component tests passed; two old wire-suffix assertions failed.
  `shaman-staff-wire-retry-01` passes both repaired assertions, and
  `shaman-staff-state-regression-01` passes all186 selected state/projectile cases.
  Production code is unchanged by those test-only corrections. Fresh sanitizer integration now passes all2186 engine tests with leak checking and1081 ESM4 component tests without leak checking; see the verification artifact. `shaman-staff-normal-integration-01`
  subsequently passes all2186 engine and3259 component tests, including the
  castable-list classification fix below.
- `shaman-staff-windup-runtime-01` actually saved **after** release, at0.908s:
  staff150→137, one projectile impact for5 damage, health65.990028→60.990028.
  `shaman-staff-released-reload-01` preserves that released ownership, then
  performs a new cast: projectile2, charge124, health55.990028. No projectile1
  replay. Whole loaded state matches except existing animated-body world velocity
  projection (tolerance1e-5); comparison receipt is in that runtime directory.
- `shaman-staff-pending-01` genuinely saves at0.332s, released=false, serial1,
  no projectile. `shaman-staff-pending-reload-01` preserves the pending state in a fresh
  process, releases once (charge137, projectile1) and starts the next cast normally.
  Whole-state comparison permits only the existing world-velocity projection
  tolerance1e-5 (observed max1.49e-8). `shaman-staff-flight-03` captures projectile3 at distance284, charge111;
  `shaman-staff-flight-reload-01` restores its exact flight state, hits once for5
  without charging again, then emits new projectile4 (charge98). The new F5 save
  contains projectile4 in flight. Whole-state difference remains only existing
  world-velocity projection (max2.98e-8).
- `shaman-staff-approach-01` passes all six normal-input waypoints from the
  pristine healed save, with eleven real staff shots (ten Player hits, one wall).
  Player survives with15.990028 health; staff charge7, normally unequipped after
  exhaustion. Its genuine Quicksave is the current route continuation input.
- `shaman-fight-01/02` fail victory: two normal C casts heal Player to30.070030,
  but twelve axe swings and shaman unarmed attacks register no melee hits.
  No script/frame errors. `02` retains a pre-strike F5 and inspected screenshot;
  Player yaw was not yet aligned then. Investigate final aim/contact geometry
  before changing combat code. This is **not** encounter acceptance.

The contact failure is now resolved and committed as `f43e8045dc`; see
[SEWER-SHAMAN-CONTACT-VERIFICATION.json](SEWER-SHAMAN-CONTACT-VERIFICATION.json).
The shaman's LVSP090617 contains ordinary Flare/Protect spells, not contact
spells. Castable-only lists no longer block melee admission. Actual contact traces
proved valid reach/cone/LOS, and `shaman-fight-03` subsequently commits axe and
natural damage. The Player dies: that course remains failed. `shaman-power-fight-01`
misses because its driver stops turning too early; `02` checks facing and lands
power attacks but also loses through damage/fatigue. Do not tune combat values to
force those attempts to win. The original general spell selection/casting fallback
is still unimplemented; Player Flare is a useful next gameplay dependency.

Staff acceptance and provenance:
[SEWER-STAFF-PROJECTILE-VERIFICATION.json](SEWER-STAFF-PROJECTILE-VERIFICATION.json).
The full shaman encounter remains unaccepted. Six editable staff restart recipes
live under `scripts/data/oblivion_compat/oblivion_sewer_shaman_staff_*.json`.

## Current chunk — second ambush and reunion

Latest: Player traversed both ledges, reached Baurus/Emperor, opened north door17753
and walked through it normally. `subterrane-north-door-02` is the genuine open-door
input. `subterrane-open-door-crossing-01` proves Player collision passage. Both
assassins are alive; stage34 persists. Navigation now uses authored pathgrid links,
partial routes and NPC door areas. Full2187 engine tests pass; focused49 pass.
The actors move but stop short of melee. Stage36 courses01/02/03 remain failed.
See [SEWER-COMBAT-NAVIGATION-VERIFICATION.json](SEWER-COMBAT-NAVIGATION-VERIFICATION.json).
Next: inspect actual route points/cursor and obstruction at assassin05 upper landing
(1179,5208,-671) and assassin06 north of door (1533,5710,-991). Do not count this as
ambush resolution. Temporary diagnostic source is removed.


The cavern exit now works through original OnActivate script04f700 and ordinary
Space input. StopCombatAlarmOnActor/SCAOnActor performs target-specific active
combat removal and Low combat-package reset; unrelated active fights survive.
Native alarm packages have no current producer; this does not establish general
crime/alarm support. Both focused command tests pass; failed01 was a snapshot
fixture missing three initialized references. Full engine regression remains due.

Normal entry, eleven pathgrid waypoints and original trigger reach stage32.
Fresh continuation then reaches Glenroy's original marker callback and stage34,
starting the two original assassins. Both entry-state comparisons are exact.
Captures inspected; Player alive with30.07003 health. Evidence and save hashes:
[SEWER-SUBTERRANE-ENTRY-VERIFICATION.json](SEWER-SUBTERRANE-ENTRY-VERIFICATION.json).
Stage34 input: `build/oblivion-compat/sewer-escape/subterrane-second-ambush-start-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Editable entry/approach/start recipes are under `scripts/data/oblivion_compat/oblivion_sewer_subterrane_*.json`.

Next: ordinary movement down the authored ledges into the ambush room, verify
combat deaths and stage36, then original reunion/birthsign progression. The
passive stage36 course and room-navigation scout are running; neither is accepted.
The entry corridor graph is disconnected from lower room nodes at the ledge.
Use normal movement to traverse the drop; do not inject position or progression.
Shaman victory, general casting fallback and continuous prison-to-outdoors proof
remain open. All full-escape requirements above remain in force.

The staff projectile fresh sanitizer reload now passes in
`shaman-staff-flight-sanitized-reload-03`: exact loaded state except declared body
velocity projection, restored projectile3 hits once without new charge, new cast4
and normal F5 resave. Capture inspected. Rendered leak checking disabled; separate
engine2186 leak gate passed. Attempts01/02 remain failed (startup/resources and
SDL input deadline). See its `flight-restart-verification.json`.

## Previous chunk — native healing and restart

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

The older `cavern-shaman-approach-01` is a genuine navigation-only save from
before staff implementation. It remains useful for isolated staff release tests,
but use `shaman-staff-approach-01` for the current route (staff shots experienced).
Shaman03b52e/base03b52d owns staff066c45/enchantment066c42 (SHDG5, instant target,
authored cost13). Do not accept an inert substitute for its encounter.

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
