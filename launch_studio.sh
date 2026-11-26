#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WEBKIT_DISABLE_DMABUF_RENDERER=1 "$ROOT_DIR/studio/src-tauri/target/release/f1_secu_pitwall_studio"
