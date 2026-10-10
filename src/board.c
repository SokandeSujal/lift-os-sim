/* Assignment 6: first readers-writers solution for the floor display board.
 * Passengers may read positions concurrently while the controller writes alone.
 * Matching sequence numbers prove that a reader saw one complete board version.
 * The no-sync path exposes the deliberately slow multi-field writer update. */
#define _DEFAULT_SOURCE
#include "common.h"
#include <errno.h>
#include <unistd.h>

void board_read(Shared*s){if(!s->no_sync){if(sem_wait(&s->rw_mutex)<0)return;s->read_count++;if(s->read_count==1&&sem_wait(&s->rw_wrt)<0){s->read_count--;sem_post(&s->rw_mutex);return;}sem_post(&s->rw_mutex);}unsigned a=s->seq_a;volatile int checksum=0;for(int i=0;i<s->lifts;i++)checksum+=s->pos[i]+s->dir[i];(void)checksum;unsigned b=s->seq_b;if(a!=b)__sync_fetch_and_add(&s->corrupt,1);__sync_fetch_and_add(&s->reads,1);if(!s->no_sync){if(sem_wait(&s->rw_mutex)<0)return;s->read_count--;if(!s->read_count)sem_post(&s->rw_wrt);sem_post(&s->rw_mutex);}}
int board_write(Shared*s,const int pos[],const int dir[]){if(!s->no_sync&&sem_wait(&s->rw_wrt)<0)return-1;s->seq_a++;for(int i=0;i<s->lifts;i++){s->pos[i]=pos[i];s->dir[i]=dir[i];usleep(2000);}s->seq_b=s->seq_a;__sync_fetch_and_add(&s->writes,1);if(!s->no_sync&&sem_post(&s->rw_wrt)<0)return-1;return 0;}
