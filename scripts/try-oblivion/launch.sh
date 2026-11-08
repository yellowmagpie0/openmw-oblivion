#!/usr/bin/env bash
# Shared launcher. Wrappers supply only a fixed scenario name, never engine flags.
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/../.." && pwd)
SCENARIO=${1:-}
if (( $# < 1 || $# > 3 )) || [[ "$SCENARIO" != checkpoint && $# -gt 2 ]] || [[ "$SCENARIO" == checkpoint && $# -ne 3 ]]; then
    printf 'Usage: %s SCENARIO [/path/to/Oblivion/Data]\n' "$0" >&2
    exit 2
fi
NEW_GAME=0
case "$SCENARIO" in
    tutorial)
        START=ImperialDungeon01
        NEW_GAME=1
        HINT='Start the authored prison opening. Follow the Emperor and Blades, then explore the tunnels. The complete escape is still under validation.' ;;
    checkpoint)
        START=ImperialDungeon01
        HINT='Resume the retained checkpoint with normal controls. Saves stay in this private tryout directory.' ;;
    prison)
        START=ImperialDungeon01::ref=0x1fc41::side=south
        HINT='Explore the prison directly: movement, jumping, activation, doors, containers and save/load. This skips the new-game introduction.' ;;
    inventory)
        START=ICMarketDistrictAFightingChance::ref=0x1d15a::side=south
        HINT='Explore A Fighting Chance: inspect merchandise, activate containers, take items and open inventory to equip them. Ownership applies; general merchant dialogue is unfinished.' ;;
    city)
        START=ICMarketDistrict01::ref=0x038490::side=east
        HINT='Walk the Market District, watch NPC schedules and door travel, inspect actors and enter nearby shops.' ;;
    swimming)
        START=OldBridgeExterior::ref=0x60423::side=south
        HINT='Explore the waterfront, walk into the water (the recorded course backs up from this start), swim, and inspect outdoor lighting, water and sound.' ;;
    hackdirt)
        START=ChorrolNorthernGoodsAndTrade
        HINT='Chorrol before A Shadow over Hackdirt: start at the shop entrance. Answer Dar-Ma and close dialogue, then sidestep left and climb the stairs to Seed-Neeus. The timed pickup, overland arrival and original Jiv meeting/key are verified. Cavern combat, rescue and reward remain under validation.' ;;
    horse)
        START=4,16::ref=0x68451::side=east
        HINT='Look down toward the nearby horse. Aim at it and press Space to mount; press Space again to dismount. Try moving and saving while mounted.' ;;
    creatures)
        START=GoblinJimsCave::ref=0x67067::side=south
        HINT='Inspect goblin models and animations in their original cave. Combat, death and dungeon progression are unfinished; this is an exploration start.' ;;
    oblivion)
        START=OblivionMqKvatchSmallTower03
        HINT='Explore an Oblivion tower: architecture, lighting and quasi-exterior environment rendering. This does not set up or complete the Kvatch quest.' ;;
    *)
        printf 'Unknown scenario: %s\nChoose: tutorial prison inventory city hackdirt swimming horse creatures oblivion (use checkpoint.sh for saved checkpoints)\n' "$SCENARIO" >&2
        exit 2 ;;
esac

DATA_DIR=${2:-${OBLIVION_DATA:-$HOME/.local/share/Steam/steamapps/common/Oblivion/Data}}
SOURCE_BIN=${OPENMW_BIN:-$REPO_DIR/build/openmw}
SOURCE_RESOURCES=${OPENMW_RESOURCES:-$REPO_DIR/build/resources}
SOURCE_CONFIG=$(dirname -- "$SOURCE_BIN")/openmw.cfg
SOURCE_DEFAULTS=$(dirname -- "$SOURCE_BIN")/defaults.bin
for required in "$SOURCE_BIN" "$SOURCE_CONFIG" "$SOURCE_DEFAULTS" "$DATA_DIR/Oblivion.esm"; do
    if [[ ! -f "$required" ]]; then
        printf 'Required file is missing: %s\nUse an existing completed build; this launcher never builds anything.\n' "$required" >&2
        exit 1
    fi
done
if [[ ! -d "$SOURCE_RESOURCES" ]]; then
    printf 'Resources directory is missing: %s\n' "$SOURCE_RESOURCES" >&2
    exit 1
fi
if [[ ! -x "$SOURCE_BIN" ]]; then
    printf 'Executable is not executable: %s\n' "$SOURCE_BIN" >&2
    exit 1
fi
DATA_DIR=$(cd -- "$DATA_DIR" && pwd -P)
# Config values use quoted strings. Reject delimiters instead of emitting a
# malformed config or allowing an environment-supplied path to inject options.
case "$DATA_DIR" in
    *\"*|*$'\n'*|*$'\r'*) printf 'Unsupported quote or newline in Data path.\n' >&2; exit 2 ;;
esac

# A new private directory for EVERY invocation. Never use shared build outputs,
# player profiles, previous tryouts, or an environment-supplied output path.
# Retain the directory even on failure so logs and saves remain inspectable.
# Debug executables can exceed 1 GiB. Retaining copies in a tmpfs /tmp fills
# its quota and competes with the game for RAM/swap. Keep them on user storage.
TRYOUT_ROOT="$HOME/.local/state/openmw-oblivion/tryouts"
mkdir -p -- "$TRYOUT_ROOT"
RUN_DIR=$(mktemp -d "$TRYOUT_ROOT/${SCENARIO}.XXXXXXXX")
printf 'Private tryout directory: %s\n' "$RUN_DIR"
mkdir -p "$RUN_DIR/runtime" "$RUN_DIR/config" "$RUN_DIR/userdata" "$RUN_DIR/local-data" \
    "$RUN_DIR/xdg-config" "$RUN_DIR/xdg-data" "$RUN_DIR/cache" "$RUN_DIR/work"

# Ordinary copies (no hard links) decouple a running game from later relinks.
# Dereference resource symlinks so they cannot point back to mutable build files.
cp --reflink=auto -- "$SOURCE_BIN" "$RUN_DIR/runtime/openmw"
cp -- "$SOURCE_CONFIG" "$RUN_DIR/runtime/openmw.cfg"
cp -- "$SOURCE_DEFAULTS" "$RUN_DIR/runtime/defaults.bin"
cp -RL -- "$SOURCE_RESOURCES" "$RUN_DIR/runtime/resources"
if ! cmp -s -- "$SOURCE_BIN" "$RUN_DIR/runtime/openmw"; then
    printf 'The executable changed during copying. No game launched. Retry after linking finishes.\n' >&2
    exit 1
fi
sha256sum -- "$RUN_DIR/runtime/openmw" > "$RUN_DIR/executable.sha256"
if [[ "$SCENARIO" == checkpoint ]]; then
    CHECKPOINT_SAVE=$(realpath -- "$3")
    [[ -f "$CHECKPOINT_SAVE" ]] || { printf 'Checkpoint is missing: %s\n' "$CHECKPOINT_SAVE" >&2; exit 1; }
    cp --reflink=auto -- "$CHECKPOINT_SAVE" "$RUN_DIR/checkpoint.omwsave"
    cmp -s -- "$CHECKPOINT_SAVE" "$RUN_DIR/checkpoint.omwsave" || { printf 'Checkpoint changed during copying.\n' >&2; exit 1; }
    sha256sum -- "$RUN_DIR/checkpoint.omwsave" > "$RUN_DIR/checkpoint.sha256"
    # Retained regression saves name this read-only descriptor. Preserve its
    # identity without running the automated observer or changing saved bytes.
    mkdir -p "$RUN_DIR/local-data/scripts"
    printf 'PLAYER: scripts/tutorial_passage_observer.lua\n' > "$RUN_DIR/local-data/tutorial-passage-observer.omwscripts"
    printf 'return {engineHandlers={}}\n' > "$RUN_DIR/local-data/scripts/tutorial_passage_observer.lua"
fi
printf 'scenario=%s\nsource executable=%s\nsource resources=%s\ndata=%s\nstart=%s\n' \
    "$SCENARIO" "$SOURCE_BIN" "$SOURCE_RESOURCES" "$DATA_DIR" "$START" > "$RUN_DIR/session.txt"
if [[ "$SCENARIO" == checkpoint ]]; then printf 'source checkpoint=%s\n' "$CHECKPOINT_SAVE" >> "$RUN_DIR/session.txt"; fi

cat > "$RUN_DIR/config/openmw.cfg" <<EOF
replace=config
replace=data
replace=content
resources="$RUN_DIR/runtime/resources"
data="$DATA_DIR"
data-local="$RUN_DIR/local-data"
content=Oblivion.esm
user-data="$RUN_DIR/userdata"
start=$START
skip-menu=1
new-game=$NEW_GAME
no-grab=1
EOF
if [[ "$SCENARIO" == checkpoint ]]; then
    printf 'content=tutorial-passage-observer.omwscripts\n' >> "$RUN_DIR/config/openmw.cfg"
fi
cat > "$RUN_DIR/config/settings.cfg" <<'EOF'
[Video]
fullscreen = false
resolution x = 1280
resolution y = 720
framerate limit = 60
[Models]
load unsupported nif files = true
[Shaders]
force shaders = true
[General]
screenshot format = png
EOF

# Do not inherit scenario event injection, report destinations, replay sockets,
# fixed seeds or renderer output paths from another development session.
while IFS= read -r variable; do
    case "$variable" in
        OPENMW_*|OSG_*|SDL_*|ALSOFT_CONF|ALSOFT_LOGFILE|MESA_SHADER_CACHE_DIR|MESA_SHADER_CACHE_DIR_32BIT|MESA_SHADER_CACHE_DIR_64BIT|EGL_PLATFORM|LIBGL_ALWAYS_SOFTWARE|LD_PRELOAD)
            unset "$variable" ;;
    esac
done < <(compgen -e)
export XDG_CONFIG_HOME="$RUN_DIR/xdg-config"
export XDG_DATA_HOME="$RUN_DIR/xdg-data"
export XDG_CACHE_HOME="$RUN_DIR/cache"
export MESA_SHADER_CACHE_DIR="$RUN_DIR/cache/mesa"
export TMPDIR="$RUN_DIR/work"

printf '%s\n' "$HINT"
printf 'Mouse: look | WASD: move | Space: activate | E: jump | F: draw/sheath | left mouse: attack | right mouse: block | C: cast | Tab: inventory | F5/F9: quicksave/load | Esc: menu\n'
printf 'Saves: %s/userdata/saves\nLogs and settings: %s/config\n' "$RUN_DIR" "$RUN_DIR"
printf 'Every launch starts fresh. This directory is retained after quitting; delete it manually when finished.\n'
cd -- "$RUN_DIR/work"
GAME_ARGS=(--replace=config --config "$RUN_DIR/config" --game-profile=oblivion)
if [[ "$SCENARIO" == checkpoint ]]; then GAME_ARGS+=(--load-savegame "$RUN_DIR/checkpoint.omwsave"); fi
exec "$RUN_DIR/runtime/openmw" "${GAME_ARGS[@]}"
