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

Continuous17 cleared the prior blockers in its original new-game PID398617:
all3 Player wall-rat kills, post-wall recovery42→58HP, loot/gear, both authored
corridor runners physically entering01fbb9, Player zombie kill, and post-zombie
recovery36.4→80HP in7 native applied casts. Actual F5seq23/29/30 are parsed/native
exact. No loads or manual game input. Rendered ambush, equipment and recovery
captures inspected; standing creature-corpse pose remains a known limitation.
[Continuous prefix proof](SEWER-CONTINUOUS-RECOVERY-PREFIX-VERIFICATION.json).

The same run failed at15. Source14 killed original northern rat038173, but its
actual F5seq32 retains64.96HP and038172 actively fighting the Player. The walking
course ignored that pursuer;038171 joined and delivered the killing blow.
The Player-death callback and observer failures are retained. The failed course
was intentionally stopped after death; no outdoor reload or acceptance audit
was launched. Actual game/driver are gone. Source31/32 are genuine pre/post-northern
rat checkpoints for focused testing; never reset actors or splice continuations.

Current chunk: make source15 follow its original five waypoints while responding
to nearby038171/038172 through ordinary aiming, attacks and bounded movement.
Retain the later source16 required-death gates; a distant enemy need not be chased
through walls before its original route. Prove the north-to-west course from the
actual pre-fight save, then launch another original-new-game full67 course.

Current engine snapshot: `rat-descending-recovery-runtime-snapshot-01`, ELF
`13f2f485e27a767cb5c11e07d025a7a414e206c6ee98a5cf5b40b464c14bd56f`.
Downward-only sweep retains grounded support protection and lateral/upward
contact-based separation. New geometry negative/positive regression,10 focused,
2223 normal and2223 ASan/UBSan engine tests pass (engine leak checking enabled).
[Current physics proof](SEWER-DESCENDING-RECOVERY-VERIFICATION.json) retains the
first failed driver/D-Bus shutdown-leak attempt and clean Mesa/X11 rerun, plus
matched real-save baseline/swept/corrected corridor departures. Earlier
[all-direction sweep evidence](SEWER-SWEPT-RECOVERY-VERIFICATION.json) is historical.

Controller recovery fixes retain original54/70/80HP goals and require fresh
magicka readiness and native applied-effect receipts; intentional partial heals
retain their counts. [Threshold/resource proof](SEWER-THRESHOLD-RECOVERY-VERIFICATION.json)
contains3 focused real-save diagnostics and current580 Python tests at81c9629a80,
fingerprint `662012fac3ad6f70bb59cc46149d09566444f17882b20ecbd0d243306491f2cb`.
C++ and controller code were unchanged after their gates before continuous17.

Accept only all67 sections in one original PID, physical outdoor exit, own fresh-PID
outdoor reload and ordinary walking, every-save/input/native audits and rendered
review. Keep one rendered game at a time, preserve healthy jobs and immutable
snapshots, and make no source edits/commits while `run_checks.py` is live.
Commit bounded chunks; never push without explicit request.

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
