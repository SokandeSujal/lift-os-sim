# SPEC: lift-os-sim (Smart Lift Controller, Operating Systems Lab mini project)

Course: Operating Systems Laboratory (AID20090), T.Y. B.Tech CSE (AI & DS), Sem V
Platform: Linux (WSL2 is fine). Core language: C11, gcc. Visualization: terminal (ANSI) plus a self-contained web page (plain HTML/CSS/JS, no libraries, no internet).

This file is the full instruction set for the coding agent. Read all of it before writing code. Follow the build order in section 14 and do not skip the tests.

---

## 0. Clean slate

This project replaces the earlier Hospital ER simulator. If the working directory contains any ER-sim files (`er_sim`, `src/dashboard.c`, `scenarios/er_*.csv`, `banker.c`, an old `SPEC.md`, and so on), delete them first. Keep only this file. The old spec is archived outside the repo as `archive/SPEC-er-sim.md`. Do not reuse its code, flags or file names.

---

## 1. The idea in one sentence

> We simulate a building with several lifts and compare two scheduling policies, FCFS and nearest-stop-first, to see which one gives passengers shorter waiting times and better lift utilization.

Why this exists: lift scheduling is a real operating-systems problem in disguise. Passengers are processes, lifts are CPUs, a call is a request, and the controller is the scheduler. Everyone can understand the demo in 10 seconds: two buildings run the same passengers side by side, and one of them is visibly faster.

Scope is deliberately small: **5 lab topics, one command to run, one screen to watch.** Banker's algorithm and page replacement are intentionally left out because they do not fit this story naturally.

| Lab assignment | Topic | Where it appears |
|---|---|---|
| 1 | Linux commands and shell scripting | `run.sh` (menu, build, run, compare table with arithmetic) and `tools/gen_scenario.sh` (seeded scenario generator with awk) |
| 2 | fork(), wait(), zombie, orphan | Each passenger is a forked process. Controller reaps with `waitpid`. `--lazy-reap` shows zombies. Passengers detect an orphaned state. |
| 3 | CPU scheduling: FCFS and SRTF | Two lift policies. FCFS serves stops in request order. Nearest-stop-first is the SRTF analogue (smallest remaining travel time, re-evaluated every tick, preemptive). |
| 4 | IPC with pipes | A passenger sends a "hall call" to the controller through a pipe |
| 6 | Semaphores, readers-writers | Lift capacity is a counting semaphore. The floor display board is a readers-writers problem (passengers read, controller writes). |

---

## 2. Process model

```
lift_sim  (controller = the "building"; also the parent of every passenger)
  |-- passenger_1
  |-- passenger_2
  ...
  |-- passenger_N      (forked at its arrival tick, exits after it is delivered)
```

There are no other processes. The controller is the single owner of the clock, the lifts and the terminal output. This keeps the code short and the viva simple.

- The controller runs a tick loop. One tick is one unit of simulated time.
- At the start of tick `t` the controller forks one passenger process for every passenger whose arrival tick is `t`.
- Each passenger process is a small state machine (section 6).
- Passengers and the controller share one memory region created with `mmap(MAP_SHARED | MAP_ANONYMOUS)` **before the first fork**. No `shm_open` is needed because every passenger is a descendant of the controller, so there is nothing to clean up in `/dev/shm`.
- Semaphores live inside that region and are initialised with `sem_init(&s, 1, value)`. The `1` means "shared between processes". Mention in the viva: a semaphore in ordinary heap memory is copied by `fork()` and would no longer be shared.

---

## 3. Repository layout

```
lift-os-sim/
  Makefile
  run.sh                  menu-driven launcher (Assignment 1)
  README.md
  SPEC.md
  include/
    common.h              constants, structs, enums, shared-memory layout
    core.h                pure simulation engine API
  src/
    main.c                argument parsing, setup, live controller loop, cleanup
    core.c                PURE engine: tick semantics, policies, metrics (no fork, no semaphores)
    passenger.c           passenger process life cycle
    board.c               floor display board: readers-writers protocol
    scenario.c            scenario loader and validator
    render_term.c         ANSI terminal renderer (pure: state in, text out)
    export.c              writes out/run_<policy>.js and .csv
  viz/
    index.html            web visualization (single file, includes CSS and JS)
  tools/
    gen_scenario.sh       seeded scenario generator (bash + awk)
  scenarios/
    tiny_a.csv            hand-verified scenario A (section 11)
    tiny_b.csv            hand-verified scenario B (preemption)
    tiny_c.csv            hand-verified scenario C (capacity limit)
    rush_morning.csv      40 passengers, 10 floors, 3 lifts, up-peak (the main demo)
    starvation.csv        optional: shows far-floor starvation under nearest-stop-first
  tests/
    test_core.c           golden tests for the pure engine
  out/                    generated: run_fcfs.js, run_nearest.js, csv files (git-ignored)
  docs/
    report.md             lab report in the manual's format
    diagrams/
```

Design principle: **keep the logic pure.** `core.c` contains the whole simulation (assignment, target choice, serving, metrics) and knows nothing about processes. The live controller wraps it and supplies two callbacks that perform the real semaphore handshakes with passenger processes. Tests use the same engine with trivial callbacks. This guarantees that what the tests verify is exactly what runs live.

---

## 4. Simulation rules (the heart of the project, implement exactly)

### 4.1 World

- Floors `0 .. F-1` (default F = 10). Lifts `0 .. L-1` (default L = 3). Capacity `C` per lift (default 6). Each lift starts at a configured floor (default spread: 0, 4, 9 for 3 lifts).
- Passenger: `id, arrival tick, from floor, to floor` with `from != to`.
- Ticks start at 0. A lift moves **one floor per tick**.
- Passenger states: `FUTURE, WAITING, RIDING, DONE`.

### 4.2 What happens in one tick `t` (in this order)

1. **Arrivals.** Every passenger with `arrival == t` becomes `WAITING` (live mode: forked, request read from the pipe).
2. **Assignment.** Every WAITING passenger without a lift, in id order, is assigned to one lift (4.4). The pickup stop `(PICKUP, from)` is added to that lift's stops.
3. **Lift actions.** For each lift in id order, choose its target (4.5) and perform exactly **one** action:
   - `SERVE` if the target floor equals the lift's current floor. Doors open: first drop off riders whose `to == floor`, then try to board WAITING passengers assigned to this lift whose `from == floor`, in id order (4.6).
   - `MOVE` one floor toward the target (action `U` or `D`).
   - `IDLE` if the lift has no eligible stop.
4. **Board update.** The controller publishes the new lift positions on the floor display board (writer, section 7).
5. **Render and pace.** Draw the terminal frame, record the frame for the exporter, sleep `tick_ms`, `t = t + 1`.

A passenger boarded during a SERVE at tick `t` is riding from tick `t` and the lift can move at tick `t+1`. Dropping off at tick `t` means the passenger is delivered at tick `t`.

### 4.3 Stops

Each lift keeps stops of two kinds: `(PICKUP, floor)` and `(DROP, floor)`. Duplicate stops with the same kind and floor are not added twice (one stop serves everyone there).

- A PICKUP stop is added at assignment time.
- A DROP stop `(DROP, to)` is added when a passenger boards.
- A **full lift** (riders == C) ignores all PICKUP stops until it has space again (they stay in its list). This prevents a full lift from standing at a floor forever.

### 4.4 Assignment rule

- `fcfs` policy: assign to the lift with the **fewest pending stops**. Ties: lower lift id. Distance is ignored. This is the naive controller.
- `nearest` policy: assign to the lift with the **smallest distance** from its current floor to the passenger's `from` floor. Ties: fewest pending stops, then lower lift id.

### 4.5 Target rule (this is the scheduling policy)

- **FCFS (non-preemptive):** stops form an ordered queue. New stops are appended to the tail. The target is the first eligible stop in the queue (eligible means: not a PICKUP while the lift is full). A lift never skips ahead and never changes its mind. After a SERVE, all stops of the kind and floor that were just handled are removed from the queue. If a PICKUP at that floor still has passengers waiting (because the lift was full), re-append one PICKUP stop at the tail. When the lift passes a floor that it has a stop for further down the queue, it does **not** stop there (this is the "convoy effect" of FCFS and is what makes it slow).
- **Nearest-stop-first (preemptive, SRTF analogue):** stops are an unordered set. **Every tick** the target is the eligible stop with the smallest distance to the lift's current floor (remaining travel time). Ties: keep the current direction of travel if possible, otherwise the lower floor. A closer stop appearing mid-trip takes over immediately.
- **PREEMPT event:** logged when, at tick `t`, a lift that was MOVING toward target `T_old` at tick `t-1` chooses a different target `T_new` while `T_old` is still in its stops. Changing target after a SERVE is not a preemption.

Known limitation to document in the report: nearest-stop-first can starve a passenger at a far floor while closer requests keep arriving. This is the same starvation property the manual lists for SRTF. `scenarios/starvation.csv` demonstrates it.

### 4.6 Boarding and the capacity semaphore

Each lift has a counting semaphore `cap[k]` initialised to `C`. Boarding a passenger means a successful non-blocking `sem_trywait(&cap[k])`. If it fails (value is 0), a `FULL` event is logged and the passenger keeps waiting. Dropping off a passenger does `sem_post(&cap[k])`.

In live mode **the passenger process itself performs these calls** (section 6), triggered by the controller. In pure mode the engine uses an integer counter with the same behaviour.

### 4.7 Stuck detector

If for 50 consecutive ticks no lift moved or served while at least one passenger is WAITING or RIDING, log `ERROR stuck`, clean up and exit with code 2. With correct logic this never happens. It exists to make bugs obvious.

### 4.8 Metrics (computed by the pure engine, identical in live mode)

Per passenger: `wait = board_tick - arrival`, `ride = deliver_tick - board_tick`, `total = deliver_tick - arrival` (these are the waiting-time and turnaround-time analogues from the scheduling assignment).

Overall:
- `avg wait`, `avg ride`, `avg total`.
- `makespan`: the tick at which the last passenger is delivered.
- `empty moves %`: share of MOVE actions made with zero riders onboard (wasted travel).
- `avg load %`: average over all lift-ticks of `riders / C`. This is the lift utilization number.
- `preempts`: number of PREEMPT events. `full events`: number of FULL events.
- `delivered / total` passengers.

---

## 5. Shared memory layout (`common.h`)

One `struct Shared`, allocated with `mmap` before the first fork:
- Config copy: floors, lifts, capacity, policy, tick_ms.
- Clock: `volatile int tick`, `volatile int done`.
- Per lift: `sem_t cap`, `int floor`, `int riders`.
- Per passenger slot: `id, pid, state, from, to, arrival, board_tick, deliver_tick, lift`, a command field (`CMD_NONE, CMD_BOARD, CMD_EXIT`), a command argument (lift id), a result field (`RES_BOARDED, RES_FULL, RES_EXITED`), and two semaphores: `sem_t go` (controller to passenger) and `sem_t ack` (passenger to controller).
- Floor display board: `volatile unsigned seq_a, seq_b`, `int pos[MAX_LIFTS]`, `int dir[MAX_LIFTS]`, plus `sem_t rw_mutex, rw_wrt`, `int read_count`, counters `reads`, `writes`, `corrupt`.

Limits: `MAX_PASSENGERS = 64`, `MAX_LIFTS = 6`, `MAX_FLOORS = 20`.

---

## 6. Passenger process (Assignments 2, 4, 6)

Forked by the controller at the start of the arrival tick. Life cycle:

```
close read end of the pipe
write ONE fixed-size Request {id, from, to, arrival} to the pipe      // hall call, single write()
loop:
    if getppid() changed (controller died)  -> exit(1)                 // orphan handling
    r = sem_timedwait(&me.go, 50 ms)
    if timeout:
        board_read()                                                   // reader protocol, section 7
        continue
    switch me.cmd:
      CMD_BOARD:  if sem_trywait(&cap[me.cmd_lift]) == 0 -> res = BOARDED else res = FULL
                  sem_post(&me.ack)
      CMD_EXIT:   sem_post(&cap[me.cmd_lift]); res = EXITED
                  sem_post(&me.ack); exit(0)
```

Rules:
- The Request struct is a few ints, far below `PIPE_BUF` (4096 on Linux), so one `write()` is atomic and requests from different passengers never interleave. Mention in the viva.
- The controller never trusts timing. After forking the passengers of tick `t` it reads **exactly** `n_new * sizeof(Request)` bytes from the pipe (blocking), then sorts the requests by id. This makes the simulation deterministic even though the processes run concurrently.
- When the controller wants a passenger to board or exit it sets `cmd`, `cmd_lift`, posts `go`, then waits on `ack`. It handles passengers one at a time in id order, so results do not depend on process scheduling.
- While waiting, passengers keep polling the floor display board. That is what makes them real concurrent readers.

---

## 7. Floor display board: readers-writers (Assignment 6)

The board is the "which floor is each lift on" screen that people look at. Many passengers read it at the same time. The controller is the only writer and updates it at the end of every tick.

First readers-writers solution, exactly as in the manual: `rw_mutex = 1` protects `read_count`, `rw_wrt = 1` gives exclusive access to writers, `read_count` counts active readers.

- Reader (`board_read`, in every waiting passenger): reader protocol, copy `seq_a`, `pos[]`, `seq_b`, release. If `seq_a != seq_b`, increment `corrupt` (the reader saw a half-written board).
- Writer (`board_write`, in the controller): `sem_wait(&rw_wrt)`, increment `seq_a`, write all positions, `usleep(2000)` (a deliberately slow update so a race is possible), set `seq_b = seq_a`, `sem_post(&rw_wrt)`.
- Flag `--no-sync` skips the semaphores. With it, the `corrupt` counter must go up in the rush scenario; without it, the counter must be exactly 0. This is the best live proof of why synchronization is needed. Tune the sleeps (writer 2 ms, readers poll every 10 to 50 ms) so the unsynchronized run shows a nonzero count reliably.
- Known limitation to document: this version favours readers and can starve the writer if readers never stop. With a handful of passengers and short reads it does not happen here.

---

## 8. Reaping, zombies and orphans (Assignment 2)

- Every tick the controller calls `waitpid(-1, &status, WNOHANG)` in a loop and logs a `REAPED` event for each finished passenger. At the end it waits for all remaining children.
- `--lazy-reap N`: the controller skips reaping for `N` ticks after a passenger is delivered. During that time `ps -el | grep defunct` shows the zombie passengers. The terminal footer prints a hint line telling the user to run that command in a second terminal. This is the zombie demo for the viva.
- Orphan handling: every passenger checks `getppid()` in its poll loop and exits if the parent changed. Demo: start a run, `kill -9` the controller from another terminal, then show that no passenger process remains.
- A normal run must leave no zombies and no stray processes.

---

## 9. Command line

```
./lift_sim --scenario scenarios/rush_morning.csv --policy fcfs|nearest [options]

--tick-ms N        real milliseconds per tick (default 300)
--fast             same as --tick-ms 40
--no-ui            no terminal drawing; print only events and the final table
--no-sync          disable the readers-writers semaphores (corruption demo)
--lazy-reap N      delay reaping by N ticks (zombie demo)
--out DIR          where to write run_<policy>.js and .csv (default out/)
--seed N           seed for anything random (default 42)
--help
```

Exit codes: 0 success, 1 bad input, 2 internal error (system call failure or stuck detector).

Handle SIGINT and SIGTERM in the controller: kill all passengers, wait for them, restore the terminal, exit. Always restore the terminal cursor on exit.

---

## 10. Scenario files and the generator

CSV with `#` comments and `@key=value` lines:

```
@floors=10
@lifts=3
@capacity=6
@start=0,4,9
# id,arrival,from,to
1,0,0,7
2,0,0,5
3,1,6,0
```

Validation: `from != to`, floors in range, ids unique, arrival >= 0, passenger count <= `MAX_PASSENGERS`, number of `@start` values equals `@lifts`. On error print a clear message with the line number and exit 1.

`tools/gen_scenario.sh <seed> <count> <floors>` (bash plus awk) generates a morning-rush scenario: arrivals spread over about 60 ticks, 70% start at floor 0 going up, 15% go between upper floors, 15% go down to floor 0. It uses a seeded awk `srand(seed)` so the same seed gives the same file. `scenarios/rush_morning.csv` is the output of `gen_scenario.sh 42 40 10` and is committed.

`scenarios/starvation.csv` (optional): two lifts busy near the ground floor with a steady stream of short trips, and one passenger at floor 9. Under `nearest` that passenger waits visibly longer than under `fcfs`.

---

## 11. Hand-verified golden scenarios (these are the primary tests)

All three use 1 lift starting at floor 0, 10 floors. Ticks are numbered from 0. These numbers were worked out by hand from the rules in section 4, so any mismatch means the engine is wrong.

**Scenario A** (capacity 6): `P1 arr 0, 8 to 9`, `P2 arr 0, 2 to 3`, `P3 arr 0, 5 to 6`.

| Policy | P1 wait / deliver | P2 wait / deliver | P3 wait / deliver | avg wait | avg total | makespan | preempts |
|---|---|---|---|---|---|---|---|
| fcfs | 8 / 24 | 15 / 31 | 19 / 35 | 14.0 | 30.0 | 35 | 0 |
| nearest | 12 / 14 | 2 / 4 | 7 / 9 | 7.0 | 9.0 | 14 | 0 |

(Under FCFS the lift serves 8, then 2, then 5, then drops at 9, 3 and 6, crossing the same floors again and again. Under nearest it serves 2, 3, 5, 6, 8, 9 in a straight sweep.)

**Scenario B** (capacity 6): `P1 arr 0, 8 to 9`, `P2 arr 2, 4 to 5`.

| Policy | P1 wait / deliver | P2 wait / deliver | avg wait | avg total | preempts |
|---|---|---|---|---|---|
| fcfs | 8 / 19 | 11 / 24 | 9.5 | 20.5 | 0 |
| nearest | 10 / 12 | 2 / 6 | 6.0 | 8.0 | 1 |

(Under nearest, at tick 2 the lift is on floor 2 heading to floor 8, sees the new call at floor 4 and switches target. That is the one PREEMPT.)

**Scenario C** (capacity 2): `P1 arr 0, 1 to 5`, `P2 arr 0, 1 to 5`, `P3 arr 0, 1 to 5`. Both policies give the same result:

| P1 wait / deliver | P2 wait / deliver | P3 wait / deliver | makespan | FULL events |
|---|---|---|---|---|
| 1 / 6 | 1 / 6 | 11 / 16 | 16 | 1 |

(At tick 1 the lift reaches floor 1, boards P1 and P2, and the capacity semaphore refuses P3. The lift delivers both riders at tick 6, returns to floor 1 and boards P3 at tick 11, delivering at tick 16.)

---

## 12. Visualization

### 12.1 Terminal (default)

The controller redraws one frame per tick using plain ANSI escape codes (cursor home `\033[H`, clear below, hide cursor, restore on exit). No ncurses. Minimum terminal size 80 x 28. If the terminal is smaller, print a short "enlarge the window" message instead.

Frame layout (floors drawn top to bottom, highest floor first):

```
 lift-os-sim | policy: NEAREST-STOP-FIRST | tick 14 | delivered 9/40
 -----------------------------------------------------------------
  F9 |            |            |            |   waiting: 7 12
  F8 |            |            |            |
  F7 | [L0 3/6 ^] |            |            |   waiting: 4
  F6 |            |            |            |
  F5 |            | [L1 6/6 v] |            |
  ...
  F0 |            |            | [L2 0/6 -] |   waiting: 1 5 8 9 11 14
 -----------------------------------------------------------------
 avg wait 6.8 | avg load 41% | empty moves 18% | preempts 3 | full 2
 events: t14 L1 BOARD p7 | t14 L0 DROP p3 | ...
```

Each lift cell shows `[Lk riders/C direction]` with `^`, `v` or `-`. Colour: green when riders are under 70% of capacity, amber at 70% or more, red when full. Waiting passengers are listed per floor and coloured by waiting time (green under 5 ticks, amber under 15, red above). Footer shows the live metrics from section 4.8 and the last 3 events. In `--lazy-reap` mode add the zombie hint line.

### 12.2 Web page `viz/index.html`

One self-contained file, opened by double-clicking (it must work over `file://`, so it must not use `fetch` on local files). It loads data with ordinary script tags: `../out/run_fcfs.js` and `../out/run_nearest.js`. If the data files are missing, show a clear message saying to run `./run.sh compare` first.

**Main feature: side-by-side race.** The page shows two buildings, **FCFS on the left and Nearest-stop-first on the right**, running the same passengers on the same clock, with synchronized playback. Seeing the right side finish earlier is the headline of the demo.

Each building shows:
- A tall shaft per lift with floor lines and floor numbers. Lift cars are rounded rectangles that glide smoothly between floors (interpolate between consecutive frames over the tick duration). The door opens (animated gap) during a SERVE tick. The car displays one small person dot per rider, with empty slots shown as outlines, so fullness is obvious.
- A direction arrow and a floor indicator on every car, like a real display.
- On each floor, to the left of the shafts, the **queue of waiting passengers** drawn as small figures or dots. Colour encodes waiting time (green, amber, red) and ticks upward live, so long waits are visible as a red crowd. Show the passenger id on hover.
- Passengers appear at their floor at their arrival tick, walk into the car at boarding, and disappear at delivery with a small tick/fade.
- A live stat strip under each building: delivered / total, average wait so far, average load %, empty-move %, preempts.
- A small line chart under both buildings: number of waiting passengers over time, one line per policy. This is the second proof that one policy is better.

Controls: play / pause, step back, step forward, speed (0.5x, 1x, 2x, 4x), a scrub slider with tick number, restart. Keyboard: space, left, right.

At the end show a result banner comparing the two runs: average wait, average load, makespan, and the percentage improvement.

Design guidance (the `frontend-design` skill applies): make it look like a purposeful product, not a default template. A dark graphite background with a warm brass accent works well for an "elevator lobby" feel. Use CSS custom properties for colours, support `prefers-color-scheme`, use a clean system font stack, no external fonts, no libraries. It must work at 1280 px wide and degrade to stacked buildings on a narrow screen. Do not use localStorage.

### 12.3 Data format written by the C program (`export.c`)

For each run write `out/run_<policy>.js` and `out/run_<policy>.csv` (one row per passenger: id, arrival, from, to, board, deliver, lift, wait, ride, total). The JS file looks like this:

```js
window.LIFT_RUNS = window.LIFT_RUNS || {};
window.LIFT_RUNS["fcfs"] = {
  meta: { policy:"fcfs", scenario:"rush_morning", floors:10, lifts:3, capacity:6, ticks:97, seed:42 },
  passengers: [ { id:1, arr:0, from:0, to:7, board:3, deliver:12, lift:1 } ],   // -1 if never
  frames: [ { t:0, lifts:[ { f:0, a:"I", r:[], tgt:-1 } ] } ],                  // state AFTER tick t
  events: [ { t:3, ev:"BOARD", p:1, l:1 } ],
  stats: { avgWait:0, avgRide:0, avgTotal:0, makespan:0, emptyMovePct:0, avgLoadPct:0, preempts:0, fullEvents:0, delivered:0 }
};
```

Frame fields: `f` floor after the action, `a` action (`U`, `D`, `S`, `I`), `r` ids of riders after the tick, `tgt` target floor or -1. The web page derives who is waiting at tick `t` as the passengers with `arr <= t` and `board > t` (or board = -1). Event names: `ARRIVE, ASSIGN, BOARD, FULL, DROP, PREEMPT, REAPED, ERROR`.

Both policies must be run on the same scenario to compare, so `run.sh compare` runs them back to back with `--fast --no-ui` and then both files exist.

---

## 13. run.sh, tests, quality bar, documentation

### 13.1 run.sh (Assignment 1)

`#!/bin/bash`, `set -euo pipefail`, a `trap` that kills children on exit. A numbered menu using `read -p` and `case`:

1. Build (`make`)
2. Run rush scenario with terminal UI, FCFS
3. Run rush scenario with terminal UI, nearest-stop-first
4. Compare both policies: run headless, print a table (avg wait, avg ride, avg load, empty moves, makespan) and the percentage improvement computed with `$(( ))` and `awk`
5. Open the web visualization (`xdg-open viz/index.html`, or print the path if it is not available)
6. Readers-writers demo: run with sync, then with `--no-sync`, print both corruption counts
7. Zombie demo: run with `--lazy-reap 20` and show `ps -el | grep defunct`
8. Orphan demo: start a run, kill the controller with `kill -9`, show with `ps` that no passengers remain
9. Run unit tests (`make test`)
10. Show the process tree (`pstree -p $(pgrep -o lift_sim)`)
11. Generate a new scenario with `tools/gen_scenario.sh` (ask for seed and count)
12. Clean
0. Exit

It must also contain a small labelled block that reads two numbers and prints their sum, difference, product, quotient and remainder (a nod to the manual's Assignment 1). Make it executable (`chmod +x`).

### 13.2 Build and tests

`Makefile` targets: `make` (builds `lift_sim`, flags `-std=c11 -Wall -Wextra -Werror -O2 -pthread`, link `-lrt`), `make test`, `make asan` (`-fsanitize=address,undefined -g`), `make clean`. Zero warnings.

`tests/test_core.c` tests the pure engine only (no fork, no semaphores):
- Scenarios A, B and C from section 11, for both policies, asserting every wait, deliver, average, makespan, PREEMPT count and FULL count in the tables.
- Invariants on `rush_morning.csv` for both policies: every passenger is delivered, no lift ever carries more than C riders, `deliver > board >= arrival` for everyone, no lift moves more than one floor per tick.
- Determinism: two runs of the same scenario and policy give identical frames.
- Print PASS or FAIL per case, exit non-zero on any failure.

Add an integration check (Makefile target `make smoke`): run `scenarios/tiny_a.csv` live with `--no-ui --fast` for both policies and confirm the final table matches the golden numbers. This proves that the live process version agrees with the pure engine.

### 13.3 Quality bar

- No zombies and no stray passenger processes after a normal run, after Ctrl+C, and after `kill -9` of the controller.
- Two live runs of the same scenario and policy produce identical metrics.
- Check every return value of `fork`, `pipe`, `mmap`, `sem_*`, `read`, `write`. Print `perror` and exit 2 on failure.
- Only the controller prints to the terminal.
- No busy loops without a sleep or a timed wait.
- Every `.c` file starts with a comment naming the lab assignment it covers and explaining the algorithm in 3 to 6 lines. Comments explain why, not just what.

### 13.4 Documentation

`README.md`: what it is (the one-sentence pitch), how to build and run, the flags, the scenario format, a screenshot or GIF of the web race, the process diagram.

`docs/report.md` using the lab manual's headings: Title, Aim, Objectives, Theory, Algorithm, Input, Output, Conclusion, FAQs. Include:
- Mapping table from section 1.
- Process diagram (controller, passengers, pipe, shared memory).
- A flowchart each for: the tick loop, the passenger life cycle, the readers-writers protocol.
- Results table FCFS vs nearest on `rush_morning.csv` and screenshots from both visualizations.
- Design decisions: why processes (the manual teaches them), why semaphores live in shared memory, why the controller reads an exact number of pipe bytes (determinism), why one atomic pipe write per request, why the engine is pure and tested with golden numbers.
- Limitations: starvation under nearest-stop-first, reader preference in readers-writers, one floor per tick.
- At least 10 FAQs, for example: Why is a lift a good model of a CPU scheduler? What is the convoy effect and where do you see it here? Why does the capacity use a counting semaphore and not a binary one? What would happen without the board semaphores (show the corruption counter)? How can a zombie be produced and removed? Why can pipe writes of at most PIPE_BUF bytes not interleave? What is an orphan process and how does a passenger notice it? How does nearest-stop-first relate to SRTF and what is its drawback?

---

## 14. Build order (do each step, run `make test`, then continue)

1. Clean slate (section 0). `common.h`, `scenario.c` with validation, Makefile.
2. `core.c` pure engine with the exact rules of section 4, plus `test_core.c` with scenarios A, B, C. **All golden numbers must pass before anything else is built.**
3. `render_term.c` (state to text), checked by running the pure engine with `--no-fork` style output.
4. Live controller in `main.c`: shared memory, pipe, passenger processes, semaphore handshake, reaping. Add `make smoke` and confirm it matches the pure engine.
5. `board.c` readers-writers, then `--no-sync` and the corruption counter.
6. `--lazy-reap`, orphan detection, signal handling.
7. `export.c` and the data files.
8. `viz/index.html` (side-by-side race first, then controls, then charts and banner).
9. `tools/gen_scenario.sh`, `scenarios/rush_morning.csv`, `run.sh`.
10. README and report.

After each step give the human a two-line summary: what works, what is next. Ask only when a decision changes the design. Do not add features outside this spec (no Banker's algorithm, no paging, no threads, no extra processes, no external libraries).

---

## 15. Definition of done

- `make` has zero warnings and `make test` and `make smoke` pass.
- Golden scenarios A, B and C match the tables in section 11 for both policies, in the pure engine and live.
- `./run.sh compare` prints the comparison table, and nearest-stop-first beats FCFS on `rush_morning.csv` in average wait and in average load (if it does not, investigate before tuning anything; do not fake numbers).
- The terminal UI shows lifts moving, riders per lift, waiting passengers per floor and live metrics.
- `viz/index.html` opens by double-click and shows the synchronized side-by-side race with controls, queues, car occupancy, charts and the final banner.
- `--no-sync` shows a nonzero corruption count, the normal run shows exactly 0.
- `--lazy-reap` shows zombies in `ps`, and `kill -9` of the controller leaves no passenger processes.
- README and report are complete.
