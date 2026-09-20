#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
POCKETJS_DIR="${PROJECT_DIR}/third_party/pocketjs"
ENTRY="${SCRIPT_DIR}/../app/display_demo/main.tsx"
OUTPUT_DIR="${SCRIPT_DIR}/../build/guest"

if command -v bun >/dev/null 2>&1; then
    BUN="$(command -v bun)"
else
    BUN="${BUN:-${BUN_INSTALL:-${HOME}/.bun}/bin/bun}"
fi

if [ ! -x "${BUN}" ]; then
    echo "bun is required to build the official PocketJS guest" >&2
    echo "set BUN=/path/to/bun or add Bun to PATH" >&2
    exit 1
fi

if [ ! -d "${POCKETJS_DIR}/node_modules" ]; then
    echo "missing ${POCKETJS_DIR}/node_modules" >&2
    echo "run: cd ${POCKETJS_DIR} && bun install" >&2
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"
"${BUN}" "${SCRIPT_DIR}/prepare_assets_rv1106.ts"
rm -f \
    "${OUTPUT_DIR}/main.js" \
    "${OUTPUT_DIR}/main.pak" \
    "${OUTPUT_DIR}/display_demo.js" \
    "${OUTPUT_DIR}/display_demo.pak"

(
    cd "${POCKETJS_DIR}"
    "${BUN}" tools/build.ts "${ENTRY}" \
        --framework=solid \
        --density=1 \
        --hz=60 \
        --outdir="${OUTPUT_DIR}"
)

mv "${OUTPUT_DIR}/main.js" "${OUTPUT_DIR}/display_demo.js"
mv "${OUTPUT_DIR}/main.pak" "${OUTPUT_DIR}/display_demo.pak"
printf 'guest js:  %s\n' "${OUTPUT_DIR}/display_demo.js"
printf 'guest pak: %s\n' "${OUTPUT_DIR}/display_demo.pak"
