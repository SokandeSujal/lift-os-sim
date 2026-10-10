#!/bin/bash
# Assignment 1: Linux build/run menu, arithmetic and AWK comparison workflow.
set -euo pipefail
cleanup(){ jobs -pr | xargs -r kill 2>/dev/null || true; }
trap cleanup EXIT INT TERM

arithmetic_demo(){
  echo "Assignment 1 arithmetic"
  read -rp "First integer: " a; read -rp "Second integer: " b
  [[ $a =~ ^-?[0-9]+$ && $b =~ ^-?[0-9]+$ ]] || { echo "Integers required"; return; }
  echo "sum=$((a+b)) difference=$((a-b)) product=$((a*b))"
  (( b != 0 )) && echo "quotient=$((a/b)) remainder=$((a%b))" || echo "quotient/remainder undefined"
}
compare(){
  mkdir -p out
  ./lift_sim --scenario scenarios/rush_morning.csv --policy fcfs --no-ui --fast > out/compare_fcfs.txt
  ./lift_sim --scenario scenarios/rush_morning.csv --policy nearest --no-ui --fast > out/compare_nearest.txt
  echo "POLICY   AVG WAIT  AVG RIDE  AVG LOAD  EMPTY MOVES  MAKESPAN"
  awk '/Average wait:/{w=$3}/Average ride:/{r=$3}/Average load:/{l=$3}/Empty moves:/{e=$3}/Makespan:/{m=$2}END{printf "FCFS     %-8s  %-8s  %-8s  %-11s  %s\n",w,r,l,e,m}' out/compare_fcfs.txt
  awk '/Average wait:/{w=$3}/Average ride:/{r=$3}/Average load:/{l=$3}/Empty moves:/{e=$3}/Makespan:/{m=$2}END{printf "NEAREST  %-8s  %-8s  %-8s  %-11s  %s\n",w,r,l,e,m}' out/compare_nearest.txt
  fw=$(awk '/Average wait:/{print int($3*100)}' out/compare_fcfs.txt); nw=$(awk '/Average wait:/{print int($3*100)}' out/compare_nearest.txt)
  saved=$((fw-nw)); awk -v saved="$saved" -v base="$fw" 'BEGIN{printf "Nearest average-wait improvement: %.1f%%\n", base ? 100*saved/base : 0}'
}
if [[ ${1:-} == compare ]]; then make; compare; exit 0; fi
while true; do
cat <<'MENU'
1. Build
2. Run rush UI — FCFS
3. Run rush UI — nearest-stop-first
4. Compare both policies
5. Open web visualization
6. Readers-writers sync vs no-sync
7. Zombie demo
8. Orphan demo
9. Run unit tests
10. Show process tree
11. Generate a scenario
12. Clean
A. Assignment 1 arithmetic
0. Exit
MENU
read -rp "Choice: " choice
case "${choice,,}" in
  1) make ;;
  2) ./lift_sim --scenario scenarios/rush_morning.csv --policy fcfs ;;
  3) ./lift_sim --scenario scenarios/rush_morning.csv --policy nearest ;;
  4) compare ;;
  5) command -v xdg-open >/dev/null && xdg-open viz/index.html >/dev/null 2>&1 & echo "Open: $(pwd)/viz/index.html" ;;
  6) echo "Synchronized:"; ./lift_sim --scenario scenarios/rush_morning.csv --policy nearest --no-ui --fast | grep 'Board corruption:'; echo "No synchronization:"; ./lift_sim --scenario scenarios/rush_morning.csv --policy nearest --no-ui --fast --no-sync | grep 'Board corruption:' ;;
  7) ./lift_sim --scenario scenarios/rush_morning.csv --policy nearest --no-ui --fast --lazy-reap 20 >/tmp/lift-zombie.log & zp=$!; sleep 2; ps -el | grep -E 'defunct| Z ' || true; wait "$zp" ;;
  8) ./lift_sim --scenario scenarios/rush_morning.csv --policy nearest --no-ui >/tmp/lift-orphan.log & op=$!; sleep 1; echo "Controller=$op passengers=$(pgrep -P "$op" | tr '\n' ' ')"; kill -9 "$op"; wait "$op" 2>/dev/null || true; sleep 1; pgrep -x lift_sim >/dev/null && echo "FAIL: passengers remain" || echo "PASS: orphan passengers exited" ;;
  9) make test; make smoke ;;
  10) pid=$(pgrep -o lift_sim || true); [[ -n $pid ]] && pstree -p "$pid" || echo "Start a simulation first." ;;
  11) read -rp "Seed: " seed; read -rp "Passenger count: " count; tools/gen_scenario.sh "$seed" "$count" 10 > scenarios/generated.csv; echo "Wrote scenarios/generated.csv" ;;
  12) make clean ;;
  a) arithmetic_demo ;;
  0) exit 0 ;;
  *) echo "Invalid choice" ;;
esac
done
