#ifndef LIFT_CORE_H
#define LIFT_CORE_H
#include "common.h"

typedef struct { int kind, floor; } Stop;
typedef struct {
    int floor, riders, direction;
    int stops_count;
    Stop stops[MAX_STOPS];
    int last_target, last_action;
} CoreLift;
typedef struct {
    int id, arrival, from, to;
    int state, lift, board_tick, deliver_tick;
} CorePassenger;
typedef struct { int f, action, target, rider_count, riders[MAX_PASSENGERS]; } LiftFrame;
typedef struct { int t; LiftFrame lift[MAX_LIFTS]; } Frame;
typedef struct { int t, passenger, lift; char name[12]; } CoreEvent;
typedef struct {
    double avg_wait, avg_ride, avg_total, empty_move_pct, avg_load_pct;
    int makespan, preempts, full_events, delivered;
} Metrics;
typedef struct Core Core;
typedef int (*CoreBoardFn)(void *ctx, int passenger_slot, int lift);
typedef int (*CoreExitFn)(void *ctx, int passenger_slot, int lift);
typedef struct { CoreBoardFn board; CoreExitFn exit; void *ctx; } CoreCallbacks;

struct Core {
    Scenario scenario;
    Policy policy;
    int tick, delivered, stuck_ticks;
    long move_actions, empty_moves, load_sum, lift_ticks;
    int preempts, full_events;
    CoreLift lift[MAX_LIFTS];
    CorePassenger passenger[MAX_PASSENGERS];
    Frame frames[MAX_TICKS]; int frame_count;
    CoreEvent events[MAX_EVENTS]; int event_count;
    CoreCallbacks callbacks;
};

int core_init(Core *core, const Scenario *scenario, Policy policy, CoreCallbacks callbacks);
int core_step(Core *core);
int core_run(Core *core, int max_ticks);
void core_metrics(const Core *core, Metrics *out);
void core_event(Core *core, const char *name, int passenger, int lift);
int core_validate(const Core *core, char *err, unsigned err_size);
void render_terminal(const Core *core, const Shared *shared, int lazy_reap);
int export_run(const Core *core, const char *out_dir, unsigned seed);

#endif
