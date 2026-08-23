# M12 player mechanics and character creation

Status: accepted through 2026-08-23. Implementation revision
`94383690e9478bdc866592912e9d3a724f7b00d4`.

M12 completes the native Oblivion player backend. A player is now constructed
from the selected race, sex, class, and birthsign; exposes Oblivion's eight
attributes, 21 skills, and derived values; and uses those values for movement,
jumping, fatigue, encumbrance, swimming, activation, and save/load. The
Oblivion keyboard profile and physical gamepad path drive the same live player
state, and the race, class, and birthsign menus operate on released TES4
records rather than compatibility placeholders.

## Delivered implementation

- Native `CLAS` DATA and `BSGN` records are parsed and retained. The complete
  base-game census loads 111 classes and 13 birthsigns without parser failures;
  classes expose favored attributes, specialization, seven major skills,
  services, flags, and training data, while birthsigns expose their names,
  descriptions, icons, and spells.
- The native race, class, and birthsign projections build all eight Oblivion
  attributes and exactly 21 distinct skills. Starting skills combine the base,
  racial, specialization, and major-skill adjustments. Health, magicka,
  fatigue, carry capacity, breath time, and birthsign effects—including
  stunted magicka—are derived from that selected identity.
- The live player actor values are authoritative for ObScript `GetAV`,
  `GetBaseAV`, `ModAV`, `SetAV`, and `ForceAV`. Attribute and skill mutations
  feed movement and are captured into the native runtime state instead of
  remaining report-only values.
- Player walk, run, sneak, over-encumbrance, jump height/velocity, run and jump
  fatigue costs, fatigue recovery, carry weight, swimming, and underwater
  breath use the Oblivion profile path. The constants agree with released GMST
  data, including walk 90--130, run multiplier 3, sneak multiplier 0.6,
  encumbrance effect 0.4, strength carry multiplier 5, jump fatigue 30, run
  fatigue 8/second, and fatigue recovery 10/second.
- Jump input is consumed at the physics boundary so a keyboard or gamepad edge
  cannot be cleared by the shared Lua-control update. The physics impulse uses
  the native positive gravity convention and the selected Acrobatics and
  encumbrance values.
- The Oblivion default keyboard profile provides `W/A/S/D`, `Shift` run,
  `Ctrl` sneak, `E` jump, `Space` activate, `C` cast, `F` ready item, `R`
  point of view, `Tab` menu, `T` rest, and the existing quicksave/load keys.
  Gamepad input remains configurable and its normal OpenMW bindings are fully
  live; acceptance exercises A for activation, left trigger for jump, and
  right-stick click for point of view through a virtual uinput controller.
- First/third-person switching uses the native M11 player rigs and the
  Oblivion camera field of view. Activation uses Oblivion's 150-unit reach.
  Profile-owned controls stay isolated from Morrowind defaults.
- The real race, class, and birthsign menus enumerate TES4 records, update the
  player model and live stats, and record each completed character-generation
  stage. Their review screen displays the selected identity and derived
  values without the former generated class or portrait placeholder.
- Native runtime state is version 3. It persists player name, stable race,
  class and birthsign FormKeys, sex, character-generation flags, all base and
  modified actor values, current health/magicka/fatigue and breath, and exact
  position. Version 2 states migrate with deterministic defaults; invalid
  identity references and generation flags are rejected.
- Deterministic scenarios can target an exact exterior reference and offset.
  This permits genuine underwater movement courses without ground adjustment
  silently moving the player out of the water.

## Formula and data verification

Property tests cover sex-dependent race attributes, representative direct-stat
and stunted-magicka birthsigns, the complete one-to-one 21-skill mapping,
derived-value formulas, movement boundaries, monotonic encumbrance penalties,
over-encumbrance, positive jump velocity, and fatigue rates. Runtime-state
tests cover schema 2 migration, schema 3 round trips, invalid flags,
non-finite values, and stable identity validation. The same formulas are
exercised against the actual Imperial, CharactergenClass, Acrobat, Apprentice,
and Player records in the runtime courses.

The configured-menu course selected Imperial, Acrobat, and Apprentice and
reported 90 health, 180 magicka, 150 fatigue, 200 carry capacity, and 17.5
seconds of breath, with generation flags `14`. The general player course
changed Speed from 40 to 47 through native ObScript, then preserved that value,
the identity, and the current fatigue through a quicksave/reload. Recovery
after loading subsequently restored fatigue toward its 145 base at the native
10-per-second rate.

## Automated and end-to-end verification

The completed revision passed:

| Suite | Result |
| --- | ---: |
| Component tests | 1,529 / 1,529 |
| Engine tests | 511 / 511 |
| Python compatibility-harness tests | 28 / 28 |
| Base-master CLAS records parsed | 111 / 111 |
| Base-master BSGN records parsed | 13 / 13 |
| Final M12 runtime manifests | 5 / 5 |
| Wide official ObScript runtime regression | 1 / 1 |
| Morrowind runtime regression | 1 / 1 |

The durable unit logs are under
`build/oblivion-compat/m12-final-tests/`. Runtime evidence uses only the
OpenMW runtime and real released records/assets:

- `build/oblivion-compat/m12-player-final12/` combines keyboard and gamepad
  point-of-view switching, both jump paths, walk/run animation, movement
  through the tutorial area, gamepad activation, control disabling/enabling,
  ObScript actor-value mutation, save, reload, and a complete native state
  report. All 9,992 loaded script units compile with zero runtime diagnostics.
- `build/oblivion-compat/m12-jump-final5/` isolates keyboard `E` and physical
  gamepad left-trigger input. Each produces an upward physics impulse and an
  exact 30-point fatigue cost; the gamepad capture comes from the virtual
  uinput device, not direct action injection.
- `build/oblivion-compat/m12-menus-final9/` opens and captures the native race,
  class, and birthsign lists, selects real records, completes the review stage,
  and validates the resulting live and serialized character state.
- `build/oblivion-compat/m12-activation-final4/` uses keyboard `Space` and
  gamepad A against the same real locked chest reference. Both focus the chest
  inside the native reach, show its level-7 lock tooltip, and execute the
  expected locked result.
- `build/oblivion-compat/m12-swim-final5/` starts relative to the real
  OldBridgeExterior reference `0x60423`, remains below the water surface,
  plays the official first-person `swimbackward.kf` with 66 bound controllers,
  and persists native `T4ST` state.
- `build/oblivion-compat/m12-m7-wide-regression/` runs the broad M7 event
  sequence with all eleven official files: 11,051 script units compile with no
  failures while base-game and DLC interactions, quests, save state, and
  captures pass.
- `build/oblivion-compat/m12-morrowind-regression/` boots, saves, and reloads a
  normal Morrowind game. Its save contains none of the TES4 profile or runtime
  records, proving that M12's player and input behavior is profile-gated.

The race/class/birthsign screens, four first/third-person keyboard/gamepad
captures, locked-chest tooltip, underwater capture, and Morrowind post-load
capture were inspected directly. The camera captures show the player only in
third person; animation and state logs independently confirm walk, run, jump,
and swim transitions. No original Oblivion executable, Wine, or Proton was
used. The swim log also contains an existing common-asset preload warning for
the TES3 `xargonian_swimkna` name; the live player path independently loads and
plays the correct TES4 first-person swim KF, so this is not a missing M12
player animation.

## Reproduce and inspect

Build and run the combined player course with a local Oblivion installation:

```sh
cmake --build build --target openmw components-tests openmw-tests esmtool -j2
python3 scripts/oblivion_compat.py scenario \
  scripts/data/oblivion_compat/oblivion_m12_player_course.json \
  --output build/oblivion-compat/m12-player \
  --variable "source=$PWD" \
  --variable "openmw=$PWD/build/openmw" \
  --variable "resources=$PWD/build/resources" \
  --variable "oblivion_data=/path/to/Oblivion/Data"
```

After any M12 scenario has generated its isolated configuration, inspect it
interactively with:

```sh
build/openmw --replace=config \
  --config build/oblivion-compat/m12-player/config
```

Use `W/A/S/D`, `Shift`, `Ctrl`, `E`, `Space`, and `R` to check movement,
running, sneaking, jumping, activation, and the camera. `F5`/`F9` checks the
same native save/load path. The character-menu manifest is
`scripts/data/oblivion_compat/oblivion_m12_character_menus.json`; substitute
its generated config to inspect all three selection screens.

## Scope boundary

M12 owns the player identity, statistics, character-generation backend,
controls, locomotion, camera, activation reach, and persistence of those
systems. M13 owns mutable TES4 inventory stores, item stacking, equipment,
hotkeys, condition, locks/traps, ownership, and economy; the M12 encumbrance
calculation reads the real native starting inventory only and does not pretend
to implement those operations. M14 owns navigation and AI, and M15 owns
combat. None of those later systems is used as an M12 acceptance substitute.
