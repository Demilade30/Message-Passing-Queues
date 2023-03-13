#include<stdio.h>
#include<stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include"constant.h"

char *program;

static struct timespec * shareClock = NULL;
static int shmid = -1;

static struct timespec endTime = {.tv_sec = 0, .tv_nsec = 0};
static struct timespec lastCheck = {.tv_sec = 0, .tv_nsec = 0};
static struct timespec timePassed = {.tv_sec = 0, .tv_nsec = 0};

static void addTime(struct timespec *a, const struct timespec *b){
        static const unsigned int max_ns = 1000000000;

        a->tv_sec += b->tv_sec;
        a->tv_nsec += b->tv_nsec;
        if (a->tv_nsec > max_ns)
        {
                a->tv_sec++;
                a->tv_nsec -= max_ns;
        }
}

static void subTime(struct timespec *a, struct timespec *b, struct timespec *c){ 
	if (b->tv_nsec < a->tv_nsec){
		if(b->tv_sec < a->tv_sec){
			c->tv_sec = 0;
			c->tv_nsec = 0;
			return;
		}
                c->tv_sec = b->tv_sec - a->tv_sec - 1;
                c->tv_nsec = a->tv_nsec - b->tv_nsec;
        }else{
                c->tv_sec = b->tv_sec - a->tv_sec;
                c->tv_nsec = b->tv_nsec - a->tv_nsec;
        }
}

static void detachSHM(){
	if(shareClock != NULL){
                if(shmdt(shareClock) == -1){
                        fprintf(stderr,"%s: failed to detach shared memory. ",program);
                        perror("Error");
                }
        }
}

static int attachSHM(){
	shmid = shmget(keySHM, sizeof(struct timespec), 0);
	if(shmid < 0){
		fprintf(stderr,"%s: failed to get id for shared memory. ",program);
                perror("Error");
                return -1;
	}

	shareClock = (struct timespec *)shmat(shmid,NULL,0);
        if(shareClock == NULL){
                fprintf(stderr,"%s: failed to get pointer for shared memory. ",program);
                perror("Error");
                return -1;
        }
		
	return 1;
}

static int isAlreadyPassed(struct timespec *a, struct timespec *b){
	if(a->tv_sec > b -> tv_sec || (a->tv_sec == b->tv_sec && a->tv_nsec >= b->tv_nsec))
		return 1;
	return 0;
}

int main(int argc, char** argv){
	int sec, nsec;
	program = argv[0];

	atexit(detachSHM);
	
	if(attachSHM() != 1)
		return EXIT_FAILURE;

	sec = atoi(argv[1]);
	nsec = atoi(argv[2]);
	addTime(&endTime, shareClock);
	endTime.tv_sec = endTime.tv_sec + sec;
	endTime.tv_nsec = endTime.tv_nsec + nsec;	
	
	fflush(stdout);

	printf("WORKER PID: %d PPID: %d SysClockS: %lu SysclockNano: %lu TermTimeS: %lu TermTimeNano: %lu\n--Just Starting\n",
		getpid(), getppid(), shareClock->tv_sec, shareClock->tv_nsec, endTime.tv_sec, endTime.tv_nsec);	

	lastCheck.tv_sec = shareClock->tv_sec;
	lastCheck.tv_nsec = shareClock->tv_nsec;

	while(1){
		fflush(stdout);
		struct timespec timeDiff = {.tv_sec = 0, .tv_nsec = 0};
                struct timespec oneSec = {.tv_sec = 1, .tv_nsec = 0};

                subTime(&lastCheck, shareClock, &timeDiff);


                if(isAlreadyPassed(&timeDiff, &oneSec)){
                        addTime(&timePassed, &timeDiff);
                        lastCheck.tv_sec = shareClock->tv_sec;
                        lastCheck.tv_nsec = shareClock->tv_nsec;

                        printf("WORKER PID: %d PPID: %d SysClockS: %lu SysclockNano: %lu TermTimeS: %lu TermTimeNano: %lu\n--%lu seconds have passed since starting\n",
                getpid(), getppid(), shareClock->tv_sec, shareClock->tv_nsec, endTime.tv_sec, endTime.tv_nsec, timePassed.tv_sec);
                }


		if(isAlreadyPassed(shareClock, &endTime)){
			        printf("WORKER PID: %d PPID: %d SysClockS: %lu SysclockNano: %lu TermTimeS: %lu TermTimeNano: %lu\n--Terminating\n",
                getpid(), getppid(), shareClock->tv_sec, shareClock->tv_nsec, endTime.tv_sec, endTime.tv_nsec);
		break;
		}
		
	}	



	return EXIT_SUCCESS;
}
