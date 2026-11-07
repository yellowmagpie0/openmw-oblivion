# Full sewer escape — current handoff

## Target and remaining acceptance

**Full escape remains incomplete.** Accept one original new-game process through
all67 selected sections, physical sewer exit, then fresh-process load of that
run's own outdoor save and ordinary walking continuation. No actor/health/position/
quest injection, skipped required fights or spliced states. The selected course
uses Warrior class/birthsign and melee plus ordinary healing; other character
choices and full M15/S7/E1 remain separate gates.

Remaining: fresh continuous-course completion, own outdoor reload, whole native/input/save
audit and rendered review. Loaded diagnostics cannot replace continuous proof.
The only reviewed native deferrals are5 distant Peryite tick1 SetUnconscious
startup calls; no story-command waiver. Keep all failed runs failed.

## Current checkpoint and next action

Continuous19 is terminal failed at42, Player alive25.85HP. Original PID410270
ended normally after the90-second first-waypoint wait failed. All6366 SDL inputs
including shutdown delivered; native end0errors/0pending/5 reviewed startup
deferrals. No loads/manual input/injection; outdoor follow-up correctly withheld.
Its genuine last F5seq89 has74.3HP, dead patrol03b543 and both03b541/03b52f alive
and engaged. Hits/knockback moved Player off the initial fixed heading; it missed
the original25-unit XY radius and kept walking. Lower elevation came later.

The bounded source42 correction is verified in `reinforcement-response-06` from
that exact89: conditional ordinary move/light-attack/heal decisions, re-aimed
bounded steps, original two waypoints/radii and phase2 preserved. Both pursuers
are Player kills; source43–45 death/potion/full-resource gates pass. Route ends
at8.92HP, then normal recovery reaches80HP/80MP. All6 actual F5s parsed/native
exact, entry differs only in2 permitted body-velocity projections<=1e-5, every
input delivered, native end0/0/0. Three rendered captures reviewed.
[Reinforcement response proof](SEWER-REINFORCEMENT-RESPONSE-VERIFICATION.json).

The small `branch_on_log` runner action waits for a fresh complete observation,
selects its latest named choice, permits only normal controls/read-only waits,
and records the chosen nested inputs. It lets the guard attack while mana
regenerates instead of waiting idle for an unavailable heal. All25 focused and
fresh587 Python tests pass. C++ unchanged: reuse2223 normal and2223 ASan/UBSan
engines with original provenance in the physics proof. All67 assembly10195
actions and Lua compilation pass. Source42 now16 top-level actions (was83).

Retain all failed diagnostics:01 power-attack fatigue/death;02/03 no accepted
immediate cast (the loaded spell was retained);04 every-attack healing exhausts
mana/death;05 fixed cadence reaches missing fresh-cast gate alive near2HP;07
priority/distance experiment dies. Reverted07 and verified current actions and
observer exactly match passing06. No rendered diagnostic remains; the new full20 run is described below.

Fix committed04eeca330a. Original-new-game `full-escape-continuous-20` is now
running from `full20-assembly-02/full-escape.json`: actual PID417401,
driver417358, runtime identity captured13f2. Startup was verified live; section
entry had not yet printed at the last poll. Revalidate actual /proc identity and
monitor the same PID with `wait-course-progress.py full-escape-continuous-20 40`,
especially source42's limited health margin. Ignored guarded follow-up417360,
read-only final auditor417361 and reload-provenance watcher417362 are waiting;
see `full20-orchestrator-pids.json` and their `full20-*-status.json` files.
These are observed PIDs, not proof of future liveness. Keep all67 required
sections/fights and own outdoor reload acceptance. Do not splice loaded06 or
continuous19 into this acceptance run. No full20 completion has been claimed.

Runtime snapshot: `rat-descending-recovery-runtime-snapshot-01`, ELF
`13f2f485e27a767cb5c11e07d025a7a414e206c6ee98a5cf5b40b464c14bd56f`;
pinned libs `reunion-runtime-snapshot-01/lib`. One rendered game/controller driver
at a time. Revalidate /proc before cleanup; preserve healthy jobs/snapshots. No
tracked/nonignored edits or commits during `run_checks.py`. Commit bounded
chunks; never push without request.

Reusable continuous19 prefix evidence: [early recovery](SEWER-ESCORT-RECOVERY-VERIFICATION.json),
[Club/keyed exit](SEWER-CLUB-FOCUS-VERIFICATION.json),
[northern/western rats](SEWER-WEST-THREAT-RESPONSE-VERIFICATION.json),
[physical cavern entry](SEWER-CONTINUOUS-CAVERN-PREFIX-VERIFICATION.json),
[log encounter](SEWER-JOINED-LOG-ENCOUNTER-VERIFICATION.json).
The logs released6 bodies once and dealt31.5 damage to each03b518/03b51a;
both environmental deaths have killer=null. Do not relabel them as Player kills.
All failed full runs remain failed. Historical details remain in the linked
proofs and [checkpoint history](SEWER-ESCAPE-CHECKPOINTS-2026-11-07.md).

Adjacent UI observation: source2 retains Magic-menu state, so source5's relative
navigation visits Stats; MinorHealing remains selected and actual casting works.
Do not claim a new spell selection there. Loot subsequently opens Inventory
normally and Club equipment passes. No gate was waived for this observation.

## Current recovery correction

Continuous13 failed at12 despite reaching full health: from47.1HP, five ordinary
Minor Healing casts reached80; its sixth applied at80, but the controller waited
for a sixth *increase*. It had already passed the landing, three Player rat kills,
gear selection and a native Player zombie kill. Its actual pre-heal F5seq29 is
retained. Native end0errors/0pending/5 reviewed deferrals; no unreviewed errors.
Its follow-up correctly stopped without reload.

Selected steps12/20/27/39 now follow actual health. Full recovery stops at80HP;
39 retains three real gains or full health. Every attempt still uses ordinary C
and a real native applied-effect receipt. Observers remain read-only, with no
invented gain messages. [Proof](SEWER-ADAPTIVE-RECOVERY-VERIFICATION.json):

- `adaptive-healing-01`: genuine47.1→80,5 casts.
- `adaptive-healing-partial-01`: genuine47.1→68.22,3 casts.
- `adaptive-healing-full-cap-01`: own genuine80→80,1 applied cast,0 repeats.

All3 actual F5s are parsed/native exact; entries0diff, input delivered, native
ends0/0/0. Final capture inspected. All18 focused recipe tests pass. Fresh
`adaptive-healing-broad-python-01` passes all580 Python tests atb11b8aeb5c,
fingerprint `f86efdbb8b215f398295105864c28bfd37a669f43a1428873700be45837650e1`.
C++ is unchanged by these controller corrections.

## Reusable implementation evidence

- Grounded overlap rejection no longer lowers actors through supporting floors.
  The geometry regression fails on the old solver and passes after repair;
  airborne recovery remains checked. All9 actor-bounds,2222 normal engine and
  2222 ASan/UBSan engine tests pass. Sanitizer engine leak checking enabled,
  source44685d182f, fingerprint
  `a24bf96b69b2818e67eb85c53934fc928c3647b1f2ed6f3b73adededacef4e41`.
  [Repair](SEWER-RAT-FLOOR-REPAIR-VERIFICATION.json),
  [diagnosis](SEWER-RAT-FLOOR-DIAGNOSIS.json). No rendered leak claim.
- Ordinary landing sidestep and combat-aware wall approach pass in continuous12
  and13 without manual input; all3 native Player rat kills retained.
  [Landing history](SEWER-LANDING-INPUT-VERIFICATION.json).
- Section10 inventory navigation handles retained focus. Continuous13 F5seq28
  is parsed/native exact: both Rusty Iron armor pieces and war axe equipped,
  healing potion consumed,80HP, one original-new-game PID with no loads/manual
  inputs. [Gear proof](SEWER-GEAR-FOCUS-VERIFICATION.json). Full12's original
  gear-controller failure remains retained.
- Original ownership58, bounty reset and faction80 commands have normal-input
  and own-reload proofs:
  [ownership](SEWER-OWNERSHIP-VERIFICATION.json),
  [crime gold](SEWER-SETCRIMEGOLD-VERIFICATION.json),
  [faction rank](SEWER-SETFACTIONRANK-VERIFICATION.json).
  Schema90 preserves ordered shared-base factions without mutating authored
  content. Original executable setter/loader evidence is linked in those proofs.
  Earlier3265 component and other gate evidence retain original provenance.
- Loaded original65–67 and own outdoor reload pass, without skipped western rat:
  [western route](SEWER-WESTERN-RAT-EXIT-VERIFICATION.json),
  [manhole](SEWER-MANHOLE-TARGET-VERIFICATION.json),
  [follower yielding](SEWER-FOLLOWER-YIELD-VERIFICATION.json),
  [reunion detection](SEWER-REUNION-DETECTION-VERIFICATION.json).

Known limits: held torch omitted from perception light; upright creature corpses;
T-pose inventory preview; armor appearance can lag; blank birthsign descriptions;
take-all-only loot; silent/synchronous dialogue; general NPC spell fallback and
Player Flare missing. The post-class indoor breath bar cleared on own reload.
Exterior terrain/water/lighting remain rough; no complete wilderness claim.
Historical checkpoints: [2026-11-07](SEWER-ESCAPE-CHECKPOINTS-2026-11-07.md),
[earlier history](SEWER-ESCAPE-HISTORY.md), plus the linked scoped proofs.
