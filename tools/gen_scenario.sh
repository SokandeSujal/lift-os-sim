#!/bin/bash
# Assignment 1: seeded Bash/AWK morning-rush scenario generator.
set -euo pipefail
if (( $# != 3 )); then echo "Usage: $0 SEED COUNT FLOORS" >&2; exit 1; fi
seed=$1 count=$2 floors=$3
[[ $seed =~ ^[0-9]+$ && $count =~ ^[0-9]+$ && $floors =~ ^[0-9]+$ ]] || { echo "Arguments must be non-negative integers" >&2; exit 1; }
(( count >= 1 && count <= 64 && floors >= 2 && floors <= 20 )) || { echo "COUNT 1..64, FLOORS 2..20" >&2; exit 1; }
awk -v seed="$seed" -v count="$count" -v floors="$floors" 'BEGIN {
  srand(seed)
  lifts = floors >= 8 ? 3 : (floors >= 4 ? 2 : 1)
  printf "@floors=%d\n@lifts=%d\n@capacity=6\n@start=", floors, lifts
  for (i=0; i<lifts; i++) printf "%s%d", i ? "," : "", lifts == 1 ? 0 : int(i*(floors-1)/(lifts-1))
  printf "\n# id,arrival,from,to\n"
  for (i=1; i<=count; i++) {
    arr = int((i-1)*60/count + rand()*2)
    bucket = (i-1) % 20
    if (bucket < 14) { from=0; to=1 }
    else if (bucket == 14 && i > 20) { from=floors-1; to=floors-2 }
    else if (bucket < 17) { from=1; to=floors>2 ? 2 : 0 }
    else if (bucket == 17 && i > 20) { from=floors-1; to=0 }
    else { from=1; to=0 }
    printf "%d,%d,%d,%d\n", i, arr, from, to
  }
}'
