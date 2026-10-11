#!/bin/bash
# Build the C engine and start its small local visualization bridge.
set -euo pipefail
cd "$(dirname "$0")"
command -v python3 >/dev/null || { echo "python3 is required" >&2; exit 1; }
make
exec python3 tools/web_server.py "$@"
