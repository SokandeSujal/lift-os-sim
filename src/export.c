/* Assignment 3: deterministic run export for visualization and analysis.
 * CSV rows expose per-passenger scheduling metrics for shell comparison.
 * JSON powers the local web application while JavaScript keeps file:// mode.
 * Every format is written from the same C engine state used by live mode. */
#define _DEFAULT_SOURCE
#include "core.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int write_object(FILE *f, const Core *c, unsigned seed) {
    Metrics m;
    core_metrics(c, &m);
    fprintf(f, "{\"meta\":{\"policy\":\"%s\",\"scenario\":\"%s\",\"floors\":%d,\"lifts\":%d,\"capacity\":%d,\"ticks\":%d,\"seed\":%u},\"passengers\":[",
            policy_name(c->policy), c->scenario.name, c->scenario.floors,
            c->scenario.lifts, c->scenario.capacity, c->frame_count, seed);
    for (int i = 0; i < c->scenario.passenger_count; i++) {
        const CorePassenger *p = &c->passenger[i];
        fprintf(f, "%s{\"id\":%d,\"arr\":%d,\"from\":%d,\"to\":%d,\"board\":%d,\"deliver\":%d,\"lift\":%d}",
                i ? "," : "", p->id, p->arrival, p->from, p->to,
                p->board_tick, p->deliver_tick, p->lift);
    }
    fputs("],\"frames\":[", f);
    for (int t = 0; t < c->frame_count; t++) {
        const Frame *fr = &c->frames[t];
        fprintf(f, "%s{\"t\":%d,\"lifts\":[", t ? "," : "", fr->t);
        for (int k = 0; k < c->scenario.lifts; k++) {
            const LiftFrame *l = &fr->lift[k];
            fprintf(f, "%s{\"f\":%d,\"a\":\"%c\",\"r\":[", k ? "," : "", l->f, l->action);
            for (int r = 0; r < l->rider_count; r++)
                fprintf(f, "%s%d", r ? "," : "", l->riders[r]);
            fprintf(f, "],\"tgt\":%d}", l->target);
        }
        fputs("]}", f);
    }
    fputs("],\"events\":[", f);
    for (int i = 0; i < c->event_count; i++) {
        const CoreEvent *e = &c->events[i];
        fprintf(f, "%s{\"t\":%d,\"ev\":\"%s\",\"p\":%d,\"l\":%d}",
                i ? "," : "", e->t, e->name, e->passenger, e->lift);
    }
    fprintf(f, "],\"stats\":{\"avgWait\":%.4f,\"avgRide\":%.4f,\"avgTotal\":%.4f,\"makespan\":%d,\"emptyMovePct\":%.4f,\"avgLoadPct\":%.4f,\"preempts\":%d,\"fullEvents\":%d,\"delivered\":%d}}",
            m.avg_wait, m.avg_ride, m.avg_total, m.makespan,
            m.empty_move_pct, m.avg_load_pct, m.preempts,
            m.full_events, m.delivered);
    return ferror(f) ? -1 : 0;
}

int export_run(const Core *c, const char *dir, unsigned seed) {
    if (mkdir(dir, 0755) < 0 && errno != EEXIST) return -1;
    char csv[512], js[512], json[512];
    snprintf(csv, sizeof csv, "%s/run_%s.csv", dir, policy_name(c->policy));
    snprintf(js, sizeof js, "%s/run_%s.js", dir, policy_name(c->policy));
    snprintf(json, sizeof json, "%s/run_%s.json", dir, policy_name(c->policy));

    FILE *f = fopen(csv, "w");
    if (!f) return -1;
    fputs("id,arrival,from,to,board,deliver,lift,wait,ride,total\n", f);
    for (int i = 0; i < c->scenario.passenger_count; i++) {
        const CorePassenger *p = &c->passenger[i];
        fprintf(f, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", p->id,
                p->arrival, p->from, p->to, p->board_tick,
                p->deliver_tick, p->lift, p->board_tick - p->arrival,
                p->deliver_tick - p->board_tick, p->deliver_tick - p->arrival);
    }
    if (fclose(f) < 0) return -1;

    f = fopen(json, "w");
    if (!f) return -1;
    if (write_object(f, c, seed) < 0 || fclose(f) < 0) return -1;

    f = fopen(js, "w");
    if (!f) return -1;
    fprintf(f, "window.LIFT_RUNS=window.LIFT_RUNS||{};window.LIFT_RUNS[\"%s\"]=", policy_name(c->policy));
    if (write_object(f, c, seed) < 0) { fclose(f); return -1; }
    fputs(";\n", f);
    return fclose(f) < 0 ? -1 : 0;
}
