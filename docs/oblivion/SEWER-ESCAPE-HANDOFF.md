# Full sewer escape — current handoff

## Target and remaining acceptance

Play the original opening continuously from prison through the physical sewer
exit and fresh-process outdoor save/load. Include ordinary loot/equipment,
combat/traversal and shaman victory, escort reunion, Emperor death/Amulet,
birthsign/class choices, Baurus CharacterGen88 and MQ01 exit88. No injected
health, quest, position or completion, skipped required encounters, or spliced
save state qualifies. **Full escape remains incomplete.**

Remaining: join the genuine escort from50 through Emperor/Amulet, class choice
and physical exterior, then prove a single-process new-game escape and fresh
outdoor reload. Solo dungeon/shaman, second ambush, reunion and Warrior birthsign
are connected. Historical separate late/outdoor saves do not prove the remaining join.

## Current chunk — original reunion/birthsign50 joined, escort departs

Latest fully inspected main checkpoint:
`build/oblivion-compat/sewer-escape/joined-escort-door-clearance-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Subterrane022288, Player(1444.228,5496.875,-991.036), health100/magicka80,
Warrior birthsign01fd9d, class still placeholder0230e6. CharacterGen50.
Both second-ambush assassins dead:0176ff/Glenroy032a17,017708/Baurus032a16;
only bases017618/017620 count0→1. All3 escorts alive. No active combat or pending
work. Original stages32/34/36/40/42/43/44/45/50, birthsign bonusesStr50/End50/maxHP100
and actual UI replies are proven. Ordinary A+Shift_L0.4s clears Player from doorway;
Emperor then crosses to(1568.116,5711.307,-991.041). Capture inspected.
[Connected ambush/reunion/birthsign](SEWER-JOINED-REUNION-BIRTHSIGN-VERIFICATION.json).

Raw first ambush course intentionally stops during36wait; genuine first4 phases/
891-action F5 prefix accepted at34/80HP/first assassin dead. Backstep wait fails,
not used. Moving to doorway resolves second assassin before actual Space opens
017753(groupopen/progress1); no door-causes-kill claim. Clean door-resolution,
reunion42, native Warrior-menu50 and sidestep courses each fresh-restore exactly.
Automatic Emperor greeting interrupts planned approach; direct original menu
response passes from unchanged42 save. Left-offset/centre follow attempts fail
before any checkpoint; sidestep then frees Emperor. No state splicing.

The centre-follow attempt02 crossed the doorway but missed waypoint3 after a
sideways deflection, then overshot; terminal failed, no new checkpoint. A bounded
normal-input feedback loop now corrects aim between half-second W bursts. Nine
focused loop tests and the full569 Python suite pass; see
[adaptive input verification](SEWER-ADAPTIVE-INPUT-VERIFICATION.json).
Current detached UI `joined-adaptive-escort-01`, supervisor112576/child112578,
status `joined-adaptive-escort-01-launcher-status.json`. From actual100HP clearance
save:21 original escort corridor points and original52/54 gates. Inspect actual
process/status/scenario before another UI run. No gameplay pass claimed yet.
Next: escort through Sanctum, Emperor/Amulet, class/Baurus88, physical exterior,
uninterrupted new-game escape and fresh outdoor load. Full escape remains incomplete.

[Connected Subterrane entry](SEWER-JOINED-SUBTERRANE-ENTRY-VERIFICATION.json):
clean978-action post-shaman10heal/26originalPGRD/Space03816c→022288 course,
80HP/80MP, CharacterGen30, inventory/death counts unchanged.

[Connected shaman victory](SEWER-JOINED-SHAMAN-VICTORY-VERIFICATION.json): original
469-action staff/approach F5 prefix (raw later fails melee distance),11 real shock
impacts5 each, staff150→7, Player80→25; clean fresh four-heal/sprint/17axe-contact
victory saves15.33HP with only base03b52d count0→1 and all queues clear. Slow
sprint02 Player death/no checkpoint; lowercase sprint01 preflight/no gameplay.
Shorter control pauses in successful264-action continuation. Upright corpse limit;
no state splicing, full uninterrupted course still unproven.

[Pre-shaman recovery](SEWER-JOINED-PRE-SHAMAN-RECOVERY-VERIFICATION.json) proves
clean80HP/80MP,11 normalC releases, actual Sorcery/Respite use, unchanged life/counts
and FIRS1. This precedes the successful connected shaman encounter.

[Eastern defenders proof](SEWER-JOINED-EASTERN-DEFENDERS-VERIFICATION.json).

Early reinforcing handoff completes but defender52f joins, leaving8HP/active
combat. Four realC casts recover to21.7; immediate settled-aim axe fight kills52f.
Original missed-waypoint followup is stopped/no checkpoint; incorrectly launched
8HP eastern navigation dies/no checkpoint. Both failures retained. No state splice.
Northern patrol real finalF5/fresh native victory accepted separately; launcher143
prevents a whole-course pass claim:
[patrol evidence](SEWER-JOINED-NORTHERN-PATROL-VERIFICATION.json).

[Recovery/chest proof](SEWER-JOINED-CAVERN-RECOVERY-VERIFICATION.json) records
three normal casts5.02→26.14, six original downhill points, original chest051d72
Space take-all and three actual inventory healing-potion uses→80.
[Log encounter proof](SEWER-JOINED-LOG-ENCOUNTER-VERIFICATION.json): original
Space releases6 log bodies; neither environmental kill claimed; Player kills
03b518/03b51a via genuine first-kill F5 prefix and clean final continuation.
Failed/death attempts remain failed; upright corpse limit. General combat casting
not accepted; dedicated ordinary recovery casts proven.

Earlier cavern joins:
[first goblin](SEWER-JOINED-FIRST-GOBLIN-VERIFICATION.json),
[western passage/second goblin](SEWER-JOINED-SECOND-GOBLIN-VERIFICATION.json),
[lower route/Caverns entry](SEWER-JOINED-CAVERN-ENTRY-VERIFICATION.json).
Their raw failures and reviewed prefix/expectation acceptances remain separate.

Earlier accepted joins:
[root entrance/combat](SEWER-JOINED-ROOT-ENTRANCE-VERIFICATION.json),
[17-point root descent](SEWER-JOINED-ROOT-DESCENT-VERIFICATION.json),
[western encounters](SEWER-JOINED-WEST-VERIFICATION.json),
[actual root attacker](SEWER-JOINED-ROOT-ATTACKER-VERIFICATION.json),
[north passage](SEWER-JOINED-NORTH-VERIFICATION.json),
[zombie](SEWER-JOINED-ZOMBIE-VERIFICATION.json),
[recovery](SEWER-JOINED-ZOMBIE-HEALING-VERIFICATION.json),
[key loot/door](SEWER-JOINED-KEY-LOOT-VERIFICATION.json),
[first chest/gear](SEWER-JOINED-SUBSTRUCTURE-START-VERIFICATION.json).
Failed-prefix acceptances, exact fresh boundaries and all raw failures remain explicit.

## Verification and reusable opening evidence

Camera preference save/load is fixed and accepted: temporary Vanity/Preview retain
the selected view; old explicit saves remain authoritative. Red regression,
all4 camera/recovery cases and2203 normal engine cases pass. Real idle FP→Vanity
save writesFIRS1 and fresh-loads FirstPerson with exact native state at both
boundaries. [Camera proof](SEWER-IDLE-VIEW-SAVE-VERIFICATION.json).

Current camera-fix ASan/UBSan engine integration PASSES all2203 cases, exact
inventory, zero failures/skips, leak checking enabled and runtime built:
`idle-view-sanitizer-integration-02/verification.json`, source d697f176f8 clean.
First01 also has independently verified2203 passing XML cases, but its supervisor
lost its session before final report; it is not the accepted integration gate.
02 repeats only to resolve that missing terminal evidence. Current rendered
sanitizer fresh reload/save also PASSES: FP observed, exact native entry and actual
FIRS1 save, clean exit0, capture inspected. The source idle FP→Vanity F5 prefix
really writesFIRS1 but its raw scenario fails a10-second SDL receipt timeout
because the wrong key left the default active. Raw failure retained; independent
prefix observation and clean fresh continuation are explicit composite evidence.
Correct key `sdl_input_timeout_seconds=90` is verified in fresh receipt provenance.
Rendered leak checking is disabled; engine leak coverage stays separate.
[Current sanitizer proof](SEWER-IDLE-VIEW-SANITIZER-VERIFICATION.json).
Component3262/Python560 retain their original provenance; no fresh pass inferred.

The genuine opening chain remains linked without state splicing:
[new game→19](SEWER-CONTINUOUS-OPENING-VERIFICATION.json),
[original19→30 escort](SEWER-CONTINUOUS-ESCORT-VERIFICATION.json),
[wall-rat victories/passage](SEWER-JOINED-WALL-RATS-VERIFICATION.json),
[normal healing38→62](SEWER-JOINED-HEALING-VERIFICATION.json),
[first-person room rat](SEWER-JOINED-ROOM-RAT-VERIFICATION.json).
Escort raw failure is solely the inherited MQ01wall24 exclusion: independent
original-record/trace audit accepts the authored wall activation separately.
Wall proof accepts genuine failed03 first-kill F5 followed by exact06 continuation;
all raw failures remain failed. No full single-process acceptance is inferred.

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
covers staff/unarmed combat. Upright creature corpses, blank birthsign descriptions
and take-all-only loot remain gaps. Other birthsign choices lack this runtime
coverage. No full M15/S7/E1 or general magic/dialogue acceptance is implied.

Run UI courses singly. Use dpad_y hat axes: virtual-pad EV_KEY D-pad buttons do
not reach SDL here. Linked choices begin unselected; Down then A chooses first.
F5 needs a closed conversation. Keep failed/death attempts and original evidence
provenance. Historical detail: [SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md)
and the linked verification artifacts.
