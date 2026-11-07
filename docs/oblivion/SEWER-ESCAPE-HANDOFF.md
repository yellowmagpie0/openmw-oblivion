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

Continuous15 is terminal failed at5 after passing all3 required native Player
rat kills in the original new-game PID366061. Its actual last F5 is parsed/native
exact: Player16HP, all3 dead rats above the tutorial floor, no loads/manual inputs.
The swept overlap correction passes9 actor-bounds,2222 normal engine,580 Python
and2222 ASan/UBSan engine tests (engine leak checking enabled). Retain original
fingerprints in the [new physics proof](SEWER-SWEPT-RECOVERY-VERIFICATION.json).
This closes the tested live-rat floor recurrence, not whole-course acceptance.

Continuous14's Rat41 z-8185 failure and separately recorded ordinary diagnostic
F5 remain retained. Its checkpoint replay did not reproduce the uninterrupted
fall; no deterministic before/after replay is claimed. The expanded geometry
regression independently catches the old airborne floor crossing. Read-only
observers now fail if a required live rat falls below the tutorial floor.

The fixed-cast recovery blocker is corrected in steps5/16/45, retaining the
original54/70/80HP targets. All selected ordinary recovery casts now wait for
fresh magicka readiness and a native applied-effect receipt; intentional partial
heals retain their cast counts. Three real-save diagnostics pass:16→56HP,
armored47.1→75.26HP, and low-magicka recovery to80HP/80MP. Four actual F5s are
parsed/native exact, all automated input delivered, native ends0/0/0. Entry
comparisons permit only animated-body velocity projection differences<=1e-5.
[Recovery proof](SEWER-THRESHOLD-RECOVERY-VERIFICATION.json). All18 focused
recipe tests pass; full67-section assembly has10399 actions. C++ is unchanged.
Follow-up15 correctly stopped without outdoor reload. No rendered game remains.

After focused recovery checks, regenerate a fresh full67-section new-game course
and its guarded own-outdoor reload. Accept only all67 in one PID, actual outdoor
save/reload and ordinary walking, all input/native/save audits and rendered review.
Keep one rendered game at a time, preserve healthy jobs, never overwrite live
snapshots, and make no source edits/commits while `run_checks.py` is live.
Commit bounded completed chunks; never push without explicit request.

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
