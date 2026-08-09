#!/usr/bin/env bash
set -euo pipefail

# Launch the current OpenMW build directly into the Oblivion prison cell.
# Pass the Oblivion Data directory as the first argument, or set OBLIVION_DATA.

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)

DATA_DIR=${1:-${OBLIVION_DATA:-$HOME/.local/share/Steam/steamapps/common/Oblivion/Data}}
OPENMW_BIN=${OPENMW_BIN:-$REPO_DIR/build/openmw}
RESOURCES_DIR=${OPENMW_RESOURCES:-$REPO_DIR/build/resources}
USER_DATA_DIR=${OPENMW_OBLIVION_USER_DATA:-$REPO_DIR/build/oblivion-userdata}

if [[ ! -x "$OPENMW_BIN" ]]; then
    printf 'OpenMW executable not found or not executable: %s\n' "$OPENMW_BIN" >&2
    printf 'Build it first with: cmake --build %q --target openmw -j2\n' "$REPO_DIR/build" >&2
    exit 1
fi
if [[ ! -d "$RESOURCES_DIR" ]]; then
    printf 'OpenMW resources directory not found: %s\n' "$RESOURCES_DIR" >&2
    exit 1
fi
if [[ ! -f "$DATA_DIR/Oblivion.esm" ]]; then
    printf 'Oblivion.esm not found in Data directory: %s\n' "$DATA_DIR" >&2
    printf 'Usage: %s [/path/to/Oblivion/Data]\n' "$0" >&2
    exit 1
fi

mkdir -p "$USER_DATA_DIR"
CONFIG_DIR=$(mktemp -d "${TMPDIR:-/tmp}/openmw-oblivion-config.XXXXXX")
cleanup() {
    rm -rf -- "$CONFIG_DIR"
}
trap cleanup EXIT

cat >"$CONFIG_DIR/openmw.cfg" <<EOF
replace=config
resources="$RESOURCES_DIR"
data="$DATA_DIR"
content=Oblivion.esm
start=ImperialDungeon01
skip-menu=1
new-game=0
user-data="$USER_DATA_DIR"
EOF

cat >"$CONFIG_DIR/settings.cfg" <<'EOF'
[Video]
fullscreen = false
[Models]
load unsupported nif files = true
[General]
screenshot format = png
EOF

printf 'Launching Oblivion from %s\n' "$DATA_DIR"
printf 'Saves and screenshots will be kept in %s\n' "$USER_DATA_DIR"
printf 'Controls: mouse to look, W/A/S/D to move, Space to activate, F5 to quicksave, Esc to quit.\n'
exec "$OPENMW_BIN" \
    --replace=config \
    --config "$CONFIG_DIR" \
    --game-profile=oblivion
