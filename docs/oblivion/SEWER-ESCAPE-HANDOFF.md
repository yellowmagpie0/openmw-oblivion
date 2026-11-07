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

Continuous20 is terminal failed at39, Player alive29.34HP. Actual original
PID417401 is gone; native end0errors/0pending/5 reviewed startup deferrals.
All automated SDL inputs delivered. After a stale low-health observation, a
manual inventory inspection paused the driver and attempted two ordinary Tab
presses. Both the second manual screenshot and the subsequent source38 capture
show Inventory still open. No manual heal/equip/drop occurred. Source39's C
therefore produced no native cast receipt. Keep this failed; the recorded
"open/close" scope describes the attempted inputs, not successful closure.
[Failure audit](SEWER-CONTINUOUS-20-FAILURE.json). Outdoor reload/audit correctly
withheld. Do not change gameplay based on this test intervention.

Fresh-process `post-log-recovery-01` proved source39's three ordinary healing
casts from the genuine29.34HP save82 and proceeded through chest/potions/patrol,
but failed42: Player killed03b541 then died to03b52f. The diagnostic was stopped
only after verified native Player death. Both80HP/80MP and normal casting were
available; no missing-spell or menu bug. Each attack iteration spent about3.7s
re-aiming, swinging and redundantly tapping forward inside melee range.

The bounded source42 correction now blocks during re-aim, releases before the
light strike and omits that redundant forward tap. All observer Lua, original
two waypoint radii, phase2 and later death/recovery gates are byte-identical.
Engine combat formulas unchanged. [Defense proof](SEWER-REINFORCEMENT-DEFENSE-VERIFICATION.json).

- `reinforcement-defensive-01`: genuine failed diagnostic F5seq8. Later-arriving
  pursuers are killed in the original follow-on fights; no new attack-branch
  coverage claimed. All6 actual F5s exact; entry5 permitted velocity differences.
- `reinforcement-defensive-02`: genuine continuous19 F5seq89, both already
  engaged. Player kills both, leaves route at20.73HP, then original43–45 gates
  restore80HP/80MP. Actual30 attack/7 heal/9 move branches and4 native blocked
  hits. All6 actual F5s exact; entry2 permitted velocity differences.

Both terminalpassed/actionscomplete/exit0, no manual input, all issued inputs
delivered, native end0errors/0pending/0unsupported; all6 captures reviewed.
Focused branch/assembly tests13 pass. Runner unchanged: reuse587 Python;
C++ unchanged: reuse2223 normal and2223 ASan/UBSan engines with original
provenance in the physics proof. All67 assembly10195 actions and LuaJIT bytecode
compilation pass. Earlier [source42 proof](SEWER-REINFORCEMENT-RESPONSE-VERIFICATION.json)
and all failed variants remain historical, not replacement continuous acceptance.

Defense correction committedc6f8c86047. Fresh original-new-game
`full-escape-continuous-21` is now running from
`full21-assembly-02/full-escape.json`: actual gamePID422333, driver422305,
13f2 runtime identity captured. Startup actual /proc state verified live;
no first segment had printed at the last poll. Assembly01 is superseded and
contains old inputs. Monitor the same actual PID with
`wait-course-progress.py full-escape-continuous-21 40`. Avoid unnecessary manual
menu inspection; prior low-health samples can already be superseded by a native
kill/heal. Recheck current native deaths/phase before drawing conclusions.

Guarded own-outdoor reload422302, read-only final auditor422303 and reload
identity watcher422304 are waiting; all3 /proc identities verified non-Z.
See `full21-orchestrator-pids.json` and `full21-*-status.json`. Recorded PIDs are
not future-liveness proof. Revalidate before cleanup; never restart healthy
jobs due observation timeout. Preserve all67 sections/fights, physical exit,
own outdoor reload and whole audits/rendered review. No full21 completion or
diagnostic state splicing claimed.

Runtime snapshot: `rat-descending-recovery-runtime-snapshot-01`, ELF
`13f2f485e27a767cb5c11e07d025a7a414e206c6ee98a5cf5b40b464c14bd56f`;
pinned libs `reunion-runtime-snapshot-01/lib`. One rendered game/controller driver
at a time. Revalidate /proc before cleanup; preserve healthy jobs/snapshots. No
tracked/nonignored edits or commits during `run_checks.py`. Commit bounded
chunks; never push without request.

Continuous19 remains terminal failed42, alive25.85HP. Its genuine F5seq89 was
74.3HP, with patrol03b543 dead and both reinforcements engaged. The fixed-heading
controller missed its first waypoint under knockback; this prompted the proven
source42 correction. Prefix evidence: [early recovery](SEWER-ESCORT-RECOVERY-VERIFICATION.json),
[Club/keyed exit](SEWER-CLUB-FOCUS-VERIFICATION.json),
[northern/western rats](SEWER-WEST-THREAT-RESPONSE-VERIFICATION.json),
[physical cavern entry](SEWER-CONTINUOUS-CAVERN-PREFIX-VERIFICATION.json),
[log encounter](SEWER-JOINED-LOG-ENCOUNTER-VERIFICATION.json).
Continuous19's two log goblins were environmental kills; continuous20's03b518
was a Player kill and03b51a environmental. Preserve distinct run provenance.
Historical details: [checkpoint history](SEWER-ESCAPE-CHECKPOINTS-2026-11-07.md).

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
