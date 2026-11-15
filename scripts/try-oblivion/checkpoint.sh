#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/../.." && pwd)
if (( $# < 1 || $# > 2 )); then
    printf 'Usage: %s floor|combat|story|exit [/path/to/Oblivion/Data]\n' "$0" >&2
    exit 2
fi
SAVE=$(python3 "$REPO_DIR/scripts/sewer_regression.py" checkpoint-path --checkpoint "$1" \
    --checkpoint-dir "${SEWER_CHECKPOINT_DIR:-$REPO_DIR/build/oblivion-compat/sewer-checkpoints}")
DATA_DIR=${2:-${OBLIVION_DATA:-$HOME/.local/share/Steam/steamapps/common/Oblivion/Data}}
exec "$SCRIPT_DIR/launch.sh" checkpoint "$DATA_DIR" "$SAVE"
