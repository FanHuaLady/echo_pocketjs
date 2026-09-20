#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BOARD="${RV1106_BOARD:-root@192.168.9.121}"
REMOTE_DIR="${RV1106_REMOTE_DIR:-/root/Flower/04_hello_rust}"

HOST="${PROJECT_DIR}/build/rv1106-release/pocket_host"
GUEST_JS="${PROJECT_DIR}/build/guest/display_demo.js"
GUEST_PAK="${PROJECT_DIR}/build/guest/display_demo.pak"

for file in "${HOST}" "${GUEST_JS}" "${GUEST_PAK}"; do
    if [ ! -f "${file}" ]; then
        echo "missing ${file}" >&2
        exit 1
    fi
done

scp "${HOST}" "${GUEST_JS}" "${GUEST_PAK}" "${BOARD}:${REMOTE_DIR}/"
ssh "${BOARD}" "cd '${REMOTE_DIR}' && chmod +x pocket_host && ./pocket_host display_demo.js display_demo.pak 0"
