#ifndef CONSTANT_H
#define CONSTANT_H

#define MAX_SIMUL_PROCESSES 20
#define BUFFER_LENGTH 4096


#define DEFAULT_SEC_INTERVAL 1
#define DEFAULT_NSEC_INTERVAL 1000000

#define SEC_INCRE 0
#define NSEC_INCRE 50000

#define EXITTIME 60
const key_t keySHM = 10032023;
const key_t key_queue = 12389;

struct PCB{
	int occupied;
	pid_t pid;
	
	struct timespec startClock;
};

typedef struct {
	long mtype;
	int sec;
	int nsec;
}message;

#define MESSAGE_SIZE (sizeof(message))

#endif
