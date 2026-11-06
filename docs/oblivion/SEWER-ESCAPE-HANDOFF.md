# Full sewer escape — current handoff

## Target and remaining acceptance

Play the original opening continuously from prison through the physical sewer
exit and fresh-process outdoor save/load. Include ordinary loot/equipment,
combat/traversal and shaman victory, escort reunion, Emperor death/Amulet,
birthsign/class choices, Baurus CharacterGen88 and MQ01 exit88. No injected
health, quest, position or completion, skipped required encounters, or spliced
save state qualifies. **Full escape remains incomplete.**

Remaining: join the original gameplay continuously through shaman victory and
the physical exterior, then fresh outdoor save/load. All2202 normal engine
cases and all2202 ASan/UBSan engine cases pass at their recorded fingerprints.
The accumulated sanitizer gate enables leak checking and builds the instrumented
runtime; fresh rendered sanitizer gameplay remains separate. Component3262 and
Python560 coverage retain their original provenance, with no fresh pass inferred.

## Current chunk — idle camera preference save/load fixed and accepted

A long idle escort wait selected temporary Vanity mode. Old `getModeForSave()`
returned it as-is, causing World to serialize CAM_/FIRS0 and reload third person
after a first-person choice. The getter now preserves the existing selected view
for Vanity/Preview while retaining native recovery priority and existing static
handling. No format change; old explicit saved FIRS values remain authoritative.

The new test fails on old code, then all4 camera/recovery cases and all2203 normal
engine cases pass after fix. Actual idle save observes FirstPerson→Vanity, writes
FIRS1 and fresh-loads FirstPerson with zero native T4ST entry differences at both
boundaries. Captures inspected; positive runs have no unreviewed errors. The old
runtime negative observation reproduces FIRS0, but its raw scenario remains failed
on a resource-revision mismatch (old binary5b8 versus regenerated resources5d738).
[SEWER-IDLE-VIEW-SAVE-VERIFICATION.json](SEWER-IDLE-VIEW-SAVE-VERIFICATION.json).

The prior2202 ASan/UBSan engine pass remains at a057b0af86 with leak checking;
it does not cover this new camera change. Batch fresh sanitizer/rendered gates
at the next required integration checkpoint. No component/Python pass is inferred.

## Gameplay checkpoint and next action

Latest genuine course save:
`build/oblivion-compat/sewer-escape/joined-post-wall-healing-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Ordinary menu selection and three actual Heal Minor Wounds releases restore
38→46→54→62 health; magicka50.249794 after natural regeneration. Inventory/death
history unchanged, no combat/actions/pending deaths. Entry is identical except10
recorded animated-body world-velocity projection differences≤1e-5. Capture
inspected; no health write or saved-state substitution.
[SEWER-JOINED-HEALING-VERIFICATION.json](SEWER-JOINED-HEALING-VERIFICATION.json).

`joined-room-rat-01` is RUNNING from that genuine healed save (launcher80994).
It selects first person by ordinary R, then uses read-only camera/world aim and
normal movement/power punch to approach and fight037e40/base02509b. That rat is
alive at(1774.61,2717.20,-821.36). Course source is ignored
`joined-room-rat.json`; Lua syntax passes, gameplay outcome remains pending.
Poll the current process before another UI course. No build helper is live.

The source post-wall save remains available in `joined-wall-rats-06`, health38,
position(585.390,3175.597,-855.278). Both authored wall rats are Player-killed,
base count2. Its old third-person CAM value stays authoritative; the new course
changes view through real R input. After room victory, continue key-corpse loot,
Club equipment and keyed door. Restore a safe east/northeast lane around y3240
before reusing the old corpse approach: a direct line near y3188 hits a wall.

[Wall-rat proof](SEWER-JOINED-WALL-RATS-VERIFICATION.json) is explicitly composite:
raw03 fails later on crossing-facing input, but its actual first-kill F5 (health68)
restores exactly in06, which passes second kill/crossing. Failed01/02/04/05 remain
failed. World-axis camera observations/fresh input feedback converge; actual
contacts/deaths remain required. Both native T4ST entry comparisons are exact.

The preceding [original19→30 course](SEWER-CONTINUOUS-ESCORT-VERIFICATION.json)
proves passage, ambush/five deaths, conversations26–29, all three escort cell
transitions and callback30. Its raw scenario fails solely an inherited MQ01
stage24 exclusion. Independent master audit/trace proves original rat wall
activation legitimately sets24; separate reviewed acceptance passes, preserving
the raw failure. Corrected recipes forbid premature MQ01 exit88. The input follows
[new game→19](SEWER-CONTINUOUS-OPENING-VERIFICATION.json) without splicing.

Continue by the next observed gameplay blocker through solo dungeon/shaman,
reunion, Emperor/Amulet, choices and physical exterior. Then verify fresh
full-course outdoor load and current rendered sanitizer gameplay. Assemble the
complete new-game course once joins work. Do not splice separate shaman/late
saves, inject health/quest/position, or broaden into unrelated services/exact math.
Preserve failures/provenance and keep UI courses single. F8 selects only local
read-only observer phases; phase-scoped wait/aim patterns prevent stale matches.

## Reusable gameplay evidence

- [Shaman victory/reload](SEWER-SHAMAN-VICTORY-VERIFICATION.json):18 normal axe
  contacts,11 actual shock releases, charge150→7, unarmed retaliation, Player
  killer and death count1. Player3.780029 HP; two fresh-reload healing casts reach
  17.860031. Legacy87→89 defaults and13 independently audited ONAM poses are
  explicitly reviewed; only tiny animated-body velocity projection differences
  remain. Native death below1 HP explains remaining0.840938 on the dead shaman.
  Post-victory input: `build/oblivion-compat/sewer-escape/shaman-victory-reload-heal-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
  CharacterGen30, Natural Caverns; no active engagements/actions/projectiles or
  pending death events. This victory is separate from the later outdoor chain.
- [Exterior exit/reload](SEWER-EXTERIOR-EXIT-VERIFICATION.json): original review
  Finished, MQ01 exit88, cleared chargen history, weather/crime reset, native
  regenerating-rat resurrection, physical Tamriel entry and exact fresh reload.
  Native reset preserves position/death history; Player, attached preserve-state
  and owned-paralysis resets remain guarded. General leveled inventory and broad
  active-actor visual resurrection are unverified. Outdoor input:
  `build/oblivion-compat/sewer-escape/sewers03-outdoor-reload-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
  Health85.971672, Warrior class/birthsign, one Amulet, no key. Three failed
  encounter probes contributed genuine pre-failure F5 prefixes only; all failed
  statuses and eight exact composite boundaries remain explicit in the artifact.
- [Exit review ownership](SEWER-EXIT-MESSAGEBOX-VERIFICATION.json),
  [weekday/PC achievement](SEWER-EXIT-CALENDAR-VERIFICATION.json),
  [class/Baurus88](SEWER-CLASS-CHOICE-VERIFICATION.json),
  [key door/final sewer entry](SEWER-FINAL-SEWERS-ENTRY-VERIFICATION.json),
  [western rat](SEWER-FINAL-WESTERN-RAT-VERIFICATION.json).
- [Emperor/Amulet/final assassin](SEWER-EMPEROR-DEATH-VERIFICATION.json),
  [friendly greeting](SEWER-FRIENDLY-GREETING-VERIFICATION.json),
  [default-open Sanctum gate](SEWER-SANCTUM-GATE-VERIFICATION.json),
  [Sanctum/bound dagger](SEWER-SANCTUM-ENTRY-VERIFICATION.json),
  [daily packages](SEWER-DAILY-PACKAGE-VERIFICATION.json),
  [reunion](SEWER-REUNION-SEX-CONDITION-VERIFICATION.json),
  [birthsign](SEWER-BIRTHSIGN-VERIFICATION.json).
- [Opening melee slice](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md),
  [traps](SEWER-TRAP-HAZARD-VERIFICATION.json),
  [logs](SEWER-ROLLING-LOG-VERIFICATION.json),
  [loot/healing](SEWER-CAVERN-HEALING-VERIFICATION.json),
  [eastern goblin](SEWER-EASTERN-CAVERN-VERIFICATION.json),
  [healing casts](SEWER-HEAL-CAST-VERIFICATION.json),
  [staff projectiles](SEWER-STAFF-PROJECTILE-VERIFICATION.json),
  [Subterrane entry](SEWER-SUBTERRANE-ENTRY-VERIFICATION.json),
  [second ambush](SEWER-SECOND-AMBUSH-VERIFICATION.json).

## Known limits and operation

General NPC spell fallback and Player Flare remain unsupported; the shaman proof
covers staff/unarmed combat. Upright goblin corpses, blank birthsign descriptions
and take-all-only loot remain gaps. Other birthsign choices lack this runtime
coverage. No full M15/S7/E1 or general magic/dialogue acceptance is implied.

Run UI courses singly. Use dpad_y hat axes: virtual-pad EV_KEY D-pad buttons do
not reach SDL here. Linked choices begin unselected; Down then A chooses first.
F5 needs a closed conversation. Keep failed/death attempts and original evidence
provenance. Historical detail: [SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md)
and the linked verification artifacts.
