# Full sewer escape — current handoff

## Target and remaining acceptance

Play the original opening continuously from prison through the physical sewer
exit and fresh-process outdoor save/load. Include ordinary loot/equipment,
combat/traversal and shaman victory, escort reunion, Emperor death/Amulet,
birthsign/class choices, Baurus CharacterGen88 and MQ01 exit88. No injected
health, quest, position or completion, skipped required encounters, or spliced
save state qualifies. **Full escape remains incomplete.**

Remaining: CharacterGen88 AddTopic effects/native topic persistence, the
continuous prison-to-outdoors course including the shaman, and accumulated
sanitizer integration. All2201 normal engine cases pass at the fingerprint in
the autosave artifact. Component3262 coverage retains its original475c provenance.
No fresh component/Python/sanitizer result is claimed. The cancelled
`default-open-sanitizer-integration-01` ran no tests; reuse objects, not its result.

## Current chunk — script autosave and exact fresh reload accepted

Original CharacterGen88 requests Autosave while Baurus dialogue is open. The
request now waits for OpenMW's saveable-UI gate, then writes a real automatic
file without F5. It belongs to the selected game across retention/rollback,
coalesces repeated blocked requests, clears on cleanup and respects the autosave
setting. Manual quicksave behaviour is unchanged. Deferred timing adapts to
OpenMW's unsaveable open-dialogue UI; native queue timing is not claimed.

`baurus-class-autosave-01` writes only `Autosave.omwsave`; its recipe has no F5.
`baurus-autosave-reload-01` reloads every field exactly in a fresh process, with
Warrior class, CharacterGen88/MQ02=20, health84.297676, one key/Amulet, unchanged
deaths and no autosave/dialogue replay. Both courses pass with no unreviewed
errors; captures inspected. All2201 engine cases pass;32 independent original
save-dispatch paths pass with notification/disk operations captured boundaries.
[SEWER-SCRIPT-AUTOSAVE-VERIFICATION.json](SEWER-SCRIPT-AUTOSAVE-VERIFICATION.json).

Automatic checkpoint:
`build/oblivion-compat/sewer-escape/baurus-class-autosave-01/userdata/saves/Bendu_Olo/Autosave.omwsave`.
Recipes: `oblivion_sewer_baurus_class_autosave.json` and
`oblivion_sewer_baurus_autosave_reload.json` in `scripts/data/oblivion_compat/`.
This class save and the outdoor checkpoint below predate the separate shaman
victory. The continuous course must join the gameplay through ordinary controls.

## Next action

Implement/review native AddTopic using the real DIAL identity and persistent
knowledge owner. It remains in ObScript's deferred set. Native dialogue currently
handles explicit linked choices only; DIAS read/prepared restore filters known
topics through TES3 DIAL records, dropping native ones. Keep linked tutorial
choices intact while verifying native topic knowledge/save/load and any required
Baurus interaction. Then assemble the continuous opening and batch the sanitizer
gates. Do not broaden into unrelated dialogue services or exact combat arithmetic.

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
