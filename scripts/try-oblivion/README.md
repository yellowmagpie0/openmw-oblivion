# Isolated player tryouts

Run one script from any directory, using an **existing completed build**:

```sh
./scripts/try-oblivion/tutorial.sh
./scripts/try-oblivion/city.sh "/path/to/Oblivion/Data"
```

The optional argument is your Oblivion Data directory. Otherwise `OBLIVION_DATA`
is used, then `~/.local/share/Steam/steamapps/common/Oblivion/Data`.
Only `Oblivion.esm` game content is loaded; DLC and user mods are excluded. Saved checkpoint starts also load a no-op compatibility descriptor for the saved read-only observer.

| Script | Starting experience | Existing scenario used for the location |
| --- | --- | --- |
| `tutorial.sh` | Authored new-game prison opening, dialogue and Emperor/Blades escort | `oblivion_m14_tutorial_escort.json` |
| `checkpoint.sh floor\|combat\|story\|exit` | Genuine retained sewer checkpoint with normal controls; private save copy | [Sewer regression catalog](../data/oblivion_compat/sewer_regression_checkpoints.json) |
| `prison.sh` | Direct prison exploration, activation, doors, jumping and save/load | `oblivion_m12_jump_course.json` |
| `inventory.sh` | A Fighting Chance: merchandise, containers, inventory and equipment | M13 market start in `docs/oblivion/M13-ITEMS-INVENTORY-ECONOMY.md` |
| `city.sh` | Imperial City Market District, NPC schedules, shops and door travel | `oblivion_m14_city_schedule.json` |
| `swimming.sh` | Waterfront movement, swimming, water, outdoor lighting and audio | `oblivion_m12_swim_course.json` |
| `horse.sh` | Start beside a horse for mounting, riding and dismounting | `oblivion_m14_horse.json` |
| `creatures.sh` | Goblin Jim's Cave, creature appearance and animation | `oblivion_m11_creature_roster.json` |
| `oblivion.sh` | Kvatch Oblivion tower architecture and environment | `oblivion_m10_quasi_exterior_environment.json` |

Except for `tutorial.sh` and `checkpoint.sh`, these are direct inspection starts with the default
player. They do not replay prerequisites, grant equipment or gold, change quest
stages, make enemies passive, or import existing saves. Take available items
normally; shop ownership still applies. These starts reuse locations from
existing evidence, not the automation, generated plugins or scripted setup in
those evidence courses. Merchant services, combat/death, crime and complete
tutorial/dungeon/quest progression are not promised by these tryouts. See the
[implementation reports](../../docs/oblivion/README.md) for accepted scope;
M15 is still in progress. The current executable may lag the source code.

Mouse looks, WASD moves, Space activates, E jumps, and Tab opens inventory.
F draws/sheathes a weapon, left mouse attacks, right mouse blocks, and C casts
the selected spell. F5 quicksaves, F9 quickloads, and Esc opens the menu. These
are fresh-profile Oblivion bindings. The window is 1280×720 with sound enabled and mouse grabbing
disabled. For the horse, look down toward it before activating. At the
waterfront, the recorded swim course backs up into the water from this start.

## Isolation while development continues

Every invocation creates a private `/tmp/openmw-oblivion-SCENARIO.XXXXXXXX`
directory and prints its path. It contains copies of the executable, local
build configuration, compiled settings defaults and resources, plus private config, data-local, saves,
screenshots, logs, input bindings, Lua storage, temporary files and caches.
Copies use no hard links and resource symlinks are dereferenced. A later build
relink does not replace the executable being played. An executable change
detected during copying aborts before launch; retry once linking finishes.
Resources should also come from a completed build; copying a directory while
it is being regenerated cannot provide an atomic snapshot.

The launcher reads the build and installed game assets. It never builds,
changes tracked implementation files, writes into the build/game-data/player
profile directories, reuses another tryout's state, or controls other processes.
Inherited `OPENMW_*`, `OSG_*` and `SDL_*` settings and OpenAL config/log
overrides are removed from the child environment, along with `LD_PRELOAD`,
`EGL_PLATFORM` and `LIBGL_ALWAYS_SOFTWARE`; XDG and Mesa caches are
redirected. The desktop display/audio environment is retained. Running the game
still consumes CPU/GPU/RAM and disk I/O alongside the other session.

The directory stays after exit or failure, including logs and any saves you
made. Every subsequent invocation starts fresh. To revisit a save, retain that
directory and use its copied executable/config with a separately isolated
environment; these simple launchers deliberately offer no shared profile or
resume option. Delete only the printed directory when finished. `/tmp` may be
cleaned by the system, so copy out any saves/screenshots you want to keep.

To select another already-built executable/resources pair:

```sh
OPENMW_BIN=/absolute/path/to/build/openmw \
OPENMW_RESOURCES=/absolute/path/to/build/resources \
  ./scripts/try-oblivion/city.sh "/path/to/Oblivion/Data"
```

The executable's directory must contain its stock `openmw.cfg` and `defaults.bin`.
Paths to the Data directory containing quotes
or newlines are rejected. Linux, Bash and GNU coreutils are required. Dynamic
system libraries and original game assets remain shared read-only inputs.

## Validation status

The new checkpoint launcher was runtime-smoked with a genuine final-sewer save:
private-copy hash preserved, rendered scene reviewed, ordinary F5 and F9 passed.
Diagnostics are retained under
`build/oblivion-compat/sewer-regression/checkpoint-launcher-smoke-02` and recorded
in [closure evidence](../../docs/oblivion/SEWER-REGRESSION-CLOSURE-VERIFICATION.json).
The first smoke targeted Xvfb while the host Wayland session received the game;
that harness failure is retained separately. The successful smoke selected X11
in its test environment; the launcher preserves the human desktop environment.
This verifies the checkpoint path, not every direct-location launcher or full
sewer acceptance. Shell syntax checks passed; no build was run for these changes.

Checkpoint provenance, regeneration, the focused suite and full integration command are documented in [SEWER-REGRESSION.md](../../docs/oblivion/SEWER-REGRESSION.md). Full continuous sewer acceptance is still separate.
