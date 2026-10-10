/* Assignments 2, 4 and 6: lifecycle of one forked passenger process.
 * A fixed request is written atomically to the controller's common pipe.
 * Timed command waits allow concurrent display-board reads and orphan checks.
 * Boarding and exit operate the chosen lift's process-shared capacity semaphore. */
#define _DEFAULT_SOURCE
#include "common.h"
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int timed_wait(sem_t*s){struct timespec ts;if(clock_gettime(CLOCK_REALTIME,&ts)<0)return-1;ts.tv_nsec+=20000000L;if(ts.tv_nsec>=1000000000L){ts.tv_sec++;ts.tv_nsec-=1000000000L;}for(;;){if(sem_timedwait(s,&ts)==0)return 0;if(errno==EINTR)continue;if(errno==ETIMEDOUT)return 1;return-1;}}
void passenger_process(Shared*s,int slot,int fd,pid_t controller){signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);SharedPassenger*p=&s->passenger[slot];p->pid=getpid();Request req={p->id,p->from,p->to,p->arrival};ssize_t n;do n=write(fd,&req,sizeof req);while(n<0&&errno==EINTR);if(n!=(ssize_t)sizeof req)_exit(2);close(fd);for(;;){if(getppid()!=controller)_exit(1);int w=timed_wait(&p->go);if(w<0)_exit(2);if(w>0){board_read(s);continue;}int lift=p->cmd_lift;if(p->cmd==CMD_BOARD){if(sem_trywait(&s->lift[lift].cap)==0)p->result=RES_BOARDED;else if(errno==EAGAIN)p->result=RES_FULL;else _exit(2);if(sem_post(&p->ack)<0)_exit(2);}else if(p->cmd==CMD_EXIT){if(sem_post(&s->lift[lift].cap)<0)_exit(2);p->result=RES_EXITED;if(sem_post(&p->ack)<0)_exit(2);_exit(0);}}}
