#!/bin/bash
# Documentation helper: capture a real ANSI frame and render it as a PNG.
set -euo pipefail
mkdir -p docs/screenshots
tmp=$(mktemp -d)
cleanup(){ rm -rf "$tmp"; }
trap cleanup EXIT
script -q -c "timeout --signal=INT 2 ./lift_sim --scenario scenarios/rush_morning.csv --policy nearest --tick-ms 100" "$tmp/raw" >/dev/null || true
perl -pe 's/\e\[[0-9;?]*[A-Za-z]//g; s/\r//g' "$tmp/raw" | awk '/ lift-os-sim/{buf=""}{buf=buf $0 ORS}END{printf "%s",buf}' > "$tmp/frame"
text=$(cat "$tmp/frame")
convert -background '#121413' -fill '#ebe5d5' -font DejaVu-Sans-Mono -pointsize 15 -bordercolor '#121413' -border 20x20 "label:$text" docs/screenshots/terminal.png
echo "Wrote docs/screenshots/terminal.png"
