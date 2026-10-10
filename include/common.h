#ifndef LIFT_COMMON_H
#define LIFT_COMMON_H
#define _POSIX_C_SOURCE 200809L
#include <semaphore.h>
#include <sys/types.h>

#define MAX_PASSENGERS 64
#define MAX_LIFTS 6
#define MAX_FLOORS 20
#define MAX_STOPS (MAX_PASSENGERS * 2)
#define MAX_TICKS 2048
#define MAX_EVENTS 8192
#define EVENT_TEXT 96

typedef enum { POLICY_FCFS, POLICY_NEAREST } Policy;
typedef enum { FUTURE, WAITING, RIDING, DONE } PassengerState;
typedef enum { CMD_NONE, CMD_BOARD, CMD_EXIT } Command;
typedef enum { RES_NONE, RES_BOARDED, RES_FULL, RES_EXITED } CommandResult;
typedef enum { STOP_PICKUP, STOP_DROP } StopKind;

typedef struct { int id, arrival, from, to; } PassengerSpec;
typedef struct {
    int floors, lifts, capacity, tick_ms;
    int start[MAX_LIFTS];
    int passenger_count;
    PassengerSpec passenger[MAX_PASSENGERS];
    char name[64];
} Scenario;
typedef struct { int id, from, to, arrival; } Request;

typedef struct {
    int id;
    pid_t pid;
    volatile int state;
    int from, to, arrival, board_tick, deliver_tick, lift;
    volatile int cmd, cmd_lift, result;
    sem_t go, ack;
} SharedPassenger;
typedef struct { sem_t cap; volatile int floor, riders; } SharedLift;
typedef struct {
    int floors, lifts, capacity, policy, tick_ms, passenger_count, no_sync;
    volatile int tick, done;
    SharedLift lift[MAX_LIFTS];
    SharedPassenger passenger[MAX_PASSENGERS];
    volatile unsigned seq_a, seq_b;
    int pos[MAX_LIFTS], dir[MAX_LIFTS];
    sem_t rw_mutex, rw_wrt;
    volatile int read_count;
    volatile unsigned long reads, writes, corrupt;
} Shared;

int scenario_load(const char *path, Scenario *out, char *err, unsigned err_size);
const char *policy_name(Policy policy);
int sleep_ms(int ms);

void board_read(Shared *shared);
int board_write(Shared *shared, const int pos[], const int dir[]);
void passenger_process(Shared *shared, int slot, int pipe_write, pid_t controller);

#endif
