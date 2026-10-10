/* Assignment 1: scenario loader for deterministic lift experiments.
 * It parses @key configuration and four-field passenger CSV records.
 * Validation rejects impossible floors, duplicate ids and malformed starts.
 * Early validation keeps the pure and live engines inside fixed array bounds. */
#define _DEFAULT_SOURCE
#include "common.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *trim(char *s){while(isspace((unsigned char)*s))s++;char*e=s+strlen(s);while(e>s&&isspace((unsigned char)e[-1]))*--e='\0';return s;}
static int integer(const char*s,int*out){char*e;errno=0;long v=strtol(s,&e,10);while(isspace((unsigned char)*e))e++;if(errno||e==s||*e||v<0||v>1000000)return-1;*out=(int)v;return 0;}
static int starts(Scenario*s,char*v){int n=0;char*save=NULL;for(char*t=strtok_r(v,",",&save);t;t=strtok_r(NULL,",",&save)){if(n>=MAX_LIFTS||integer(trim(t),&s->start[n]))return-1;n++;}return n;}
int scenario_load(const char*path,Scenario*s,char*err,unsigned z){memset(s,0,sizeof*s);s->floors=10;s->lifts=3;s->capacity=6;s->tick_ms=300;s->start[0]=0;s->start[1]=4;s->start[2]=9;const char*base=strrchr(path,'/');if(!base)base=strrchr(path,'\\');base=base?base+1:path;snprintf(s->name,sizeof s->name,"%s",base);char*dot=strrchr(s->name,'.');if(dot)*dot='\0';FILE*f=fopen(path,"r");if(!f){snprintf(err,z,"cannot open %s: %s",path,strerror(errno));return-1;}char line[512];int ln=0,start_count=-1;while(fgets(line,sizeof line,f)){ln++;char*p=trim(line);if(!*p||*p=='#')continue;if(*p=='@'){char*eq=strchr(++p,'=');if(!eq){snprintf(err,z,"line %d: expected @key=value",ln);goto bad;}*eq='\0';char*k=trim(p),*v=trim(eq+1);int x;if(!strcmp(k,"start")){start_count=starts(s,v);if(start_count<0){snprintf(err,z,"line %d: invalid start list",ln);goto bad;}continue;}if(integer(v,&x)){snprintf(err,z,"line %d: invalid configuration number",ln);goto bad;}if(!strcmp(k,"floors"))s->floors=x;else if(!strcmp(k,"lifts"))s->lifts=x;else if(!strcmp(k,"capacity"))s->capacity=x;else if(!strcmp(k,"tick_ms"))s->tick_ms=x;else{snprintf(err,z,"line %d: unknown key %s",ln,k);goto bad;}continue;}if(s->passenger_count>=MAX_PASSENGERS){snprintf(err,z,"line %d: too many passengers",ln);goto bad;}int v[4],n=0;char*save=NULL;for(char*t=strtok_r(p,",",&save);t;t=strtok_r(NULL,",",&save)){if(n>=4||integer(trim(t),&v[n])){snprintf(err,z,"line %d: invalid passenger field",ln);goto bad;}n++;}if(n!=4){snprintf(err,z,"line %d: expected 4 fields",ln);goto bad;}PassengerSpec*q=&s->passenger[s->passenger_count++];q->id=v[0];q->arrival=v[1];q->from=v[2];q->to=v[3];}
fclose(f);if(s->floors<2||s->floors>MAX_FLOORS||s->lifts<1||s->lifts>MAX_LIFTS||s->capacity<1||s->tick_ms<1||!s->passenger_count){snprintf(err,z,"configuration out of range or no passengers");return-1;}if(start_count>=0&&start_count!=s->lifts){snprintf(err,z,"@start count must equal @lifts");return-1;}if(start_count<0){for(int i=0;i<s->lifts;i++)s->start[i]=s->lifts==1?0:(i*(s->floors-1))/(s->lifts-1);}for(int i=0;i<s->lifts;i++)if(s->start[i]>=s->floors){snprintf(err,z,"lift start floor out of range");return-1;}for(int i=0;i<s->passenger_count;i++){PassengerSpec*p=&s->passenger[i];if(p->id<=0||p->from>=s->floors||p->to>=s->floors||p->from==p->to){snprintf(err,z,"passenger %d has invalid id or floor",p->id);return-1;}for(int j=i+1;j<s->passenger_count;j++)if(p->id==s->passenger[j].id){snprintf(err,z,"duplicate passenger id %d",p->id);return-1;}}return 0;
bad:fclose(f);return-1;}
const char*policy_name(Policy p){return p==POLICY_FCFS?"fcfs":"nearest";}
int sleep_ms(int ms){struct timespec ts={ms/1000,(long)(ms%1000)*1000000L};while(nanosleep(&ts,&ts)<0)if(errno!=EINTR)return-1;return 0;}
