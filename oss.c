#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include "constant.h"
#include <signal.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/shm.h>

char *logfile = "logfile.log";
char* program;

//user parameter
static int proc = 1;
static int simul = 1;
static int timeLimit = DEFAULT_SEC_INTERVAL;

//shareMemory
static struct timespec * shareClock = NULL;
static int shmid = -1;
static int queue_id = -1;
static int curRun = 0;
static int userForked = 0;
static struct PCB * pcb = NULL;
static FILE *fptr = NULL;

static struct timespec nextFork = {.tv_sec = 0, .tv_nsec = 0};
static struct timespec increTime = {.tv_sec = SEC_INCRE, .tv_nsec = NSEC_INCRE};
static struct timespec lastCheck = {.tv_sec = 0, .tv_nsec = 0};

// print message to the console and write into a file
static void printWrite(FILE *fptr, char *fmt, ...)
{
	char buf[BUFFER_LENGTH];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);


	if(fptr != NULL)
	{
		fprintf(fptr, buf);
		fprintf(stdout, buf);
		fflush(fptr);
		fflush(stdout);
	}
}

static void addTime(struct timespec *a, const struct timespec *b){
	//function to add time to the clock
	static const unsigned int max_ns = 1000000000;

  	a->tv_sec += b->tv_sec;
  	a->tv_nsec += b->tv_nsec;
 	while(a->tv_nsec >= max_ns)
  	{
    		a->tv_sec++;
    		a->tv_nsec -= max_ns;
  	}
}
static void subTime(struct timespec *a, struct timespec *b, struct timespec *c){
	//function to find time difference
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
static void helpMenu(){
	printf("Usage: ./oss [-h] [-n proc] [-s simul] [-t timelimit] [-f logfile]\n");
        printf("\t\t-h describes how the project should run and then, terminates.\n");
        printf("\t\t-n proc: the number of total children worker processes to launch.\n");
        printf("\t\t-s simul: the number of children processes that can be running simultaneously and.\n");
	printf("\t\t-t timeLimit: ceilling limit of the time interval that the oss will fork worker process\n");
	printf("\t\t-f logfile: contains the output of oss.c\n");
	printf("\tIf any of the parameter above are not defined by user, the default value for them is 1\n");
}
static int findIndex(pid_t pid){
	int i;
	if(pid == 0){
		for(i = 0; i < simul; i++){
			if(pcb[i].occupied == 0)
				return i;
		}
	}else{
		for(i = 0; i < simul; i++){
			if(pcb[i].pid == pid)
				return i;
		}
	}

	return -1;
}
static int createSHM(){
	fptr = fopen(logfile, "w");
	if(fptr == NULL)
	{
		fprintf(stderr, "%s: ERROR: unable to write the output file.\n", program);
		return -1;
	}

	shmid = shmget(keySHM, sizeof(struct timespec), IPC_CREAT | IPC_EXCL | S_IRWXU);
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
	shareClock->tv_sec = 0;
	shareClock->tv_nsec = 0;

	pcb = (struct PCB *)malloc(sizeof(struct PCB) * simul);
	if(pcb == NULL){
		fprintf(stderr,"%s: failed to create process control block. ",program);
		perror("Error");
		return -1;
	}
	int i;
	for(i = 0; i < simul; i++){
		pcb[i].occupied = 0;
		pcb[i].pid = 0;
		pcb[i].startClock.tv_sec = 0;
		pcb[i].startClock.tv_nsec = 0;
	}

	//create the message queue
	queue_id = msgget(key_queue, IPC_CREAT | IPC_EXCL | 0666);
    	if (queue_id == -1) {
        	perror("Failed to create message queue");
        	exit(1);
    	}
	return 0;
}
static void deallocateSHM(){
	if(shareClock != NULL){
		if(shmdt(shareClock) == -1){
			fprintf(stderr,"%s: failed to detach shared memory. ",program);
                	perror("Error");
		}
	}

	if(shmid != -1){
		if(shmctl(shmid, IPC_RMID, NULL) == -1){
			fprintf(stderr,"%s: failed to delete shared memory. ",program);
                        perror("Error");
		}		
	}
	
	if(queue_id != -1){
		if(msgctl(queue_id, IPC_RMID, NULL) == -1){
			fprintf(stderr,"%s: failed to delete message queue.",program);
                        perror("Error");
		}	
	}

	if(pcb != NULL)
		free(pcb);

	if(fptr != NULL)
	{
		fclose(fptr);
		fptr = NULL;
	}
}
static int checkTimer(){
	struct timespec t = {.tv_sec = 0, .tv_nsec = 0};
	t.tv_sec = rand() % timeLimit + shareClock->tv_sec - nextFork.tv_sec;
	t.tv_nsec = rand() % DEFAULT_NSEC_INTERVAL;

	if(shareClock->tv_sec > nextFork.tv_sec || (shareClock->tv_sec == nextFork.tv_nsec && shareClock->tv_sec >= nextFork.tv_nsec)){
		addTime(&nextFork, &t);
		return 1; //time to Fork
	}

	//increment the clock
	addTime(shareClock, &increTime);
	return 0;		
}
static void checkIfChildTerm(){
	int status;

	pid_t pid = waitpid(-1, &status, WNOHANG);
	
	if(pid > 0){
		int index = findIndex(pid);
		
		if(index == -1){
			fprintf(stderr,"%s: cannot clear pcb for pid %d.\n",program, pid);
			exit(EXIT_FAILURE);
		}

		//Reset entry in PCB
		pcb[index].occupied = 0;
		pcb[index].pid = 0;
		pcb[index].startClock.tv_sec = 0;
		pcb[index].startClock.tv_nsec = 0;

		curRun--;		
	}

}
static void startNewWorker(){
	if(curRun < simul){
		int index = findIndex(0);

		if(index == -1){
			fprintf(stderr,"%s: failed while getting index to start a new worker.\n", program);
			exit(EXIT_FAILURE);
		}
		
		fflush(stdout);
		pid_t pid = fork();
		if(pid == -1){
			fprintf(stderr,"%s: failed to fork a process.",program);
			exit(EXIT_FAILURE);
		}else if(pid == 0){
			//Child process
			execl("./worker", "./worker", NULL);

			fprintf(stderr,"%s: failed to execl. ",program);
			perror("Error");
			exit(EXIT_FAILURE);
		}else{
			//Parent process
			curRun++;
			userForked++;
			
			int randSec = rand() % 100 + 5;
			int randNsec = rand() % 1000000000 + 1;	
			
			//create a message
			message m;
			m.mtype = pid;
			m.sec = randSec;
			m.nsec = randNsec;

			if (msgsnd(queue_id, (void *)&m, MESSAGE_SIZE, 0) == -1){
				fprintf(stderr,"%s: Couldn't send message. ", program);
				perror("Error");
    				exit(EXIT_FAILURE);	
			}  
		
			pcb[index].occupied = 1;
			pcb[index].pid = pid;
			pcb[index].startClock.tv_sec = shareClock->tv_sec;
			pcb[index].startClock.tv_nsec = shareClock->tv_nsec;

		}

		return;
	}	

	struct timespec t = {.tv_sec = 0, .tv_nsec = 0};
        t.tv_sec = rand() % timeLimit + + shareClock->tv_sec - nextFork.tv_sec;
        t.tv_nsec = rand() % DEFAULT_NSEC_INTERVAL + shareClock->tv_nsec - nextFork.tv_nsec;
	
	addTime(&nextFork, &t);	
	addTime(shareClock, &increTime);	
}

static void printPCB(){
	struct timespec timeDiff = {.tv_sec = 0, .tv_nsec = 0};
	subTime(&lastCheck, shareClock, &timeDiff);
	
	if(timeDiff.tv_sec < 0 || timeDiff.tv_nsec < 500000000)
		return;
	
	lastCheck.tv_sec = shareClock->tv_sec;
	lastCheck.tv_nsec = shareClock->tv_nsec;

	printWrite(fptr, "OSS PID: %d, SysClockS: %lu, SysclockNano: %lu\n",getpid(), shareClock->tv_sec, shareClock->tv_nsec);
	printWrite(fptr, "Process Table:\n");
	printWrite(fptr, "Entry\t\tOccupied\tPID\t\tStartS\t\tStartN\n");
	
	int i;
	for(i = 0; i < simul; i++){
		printWrite(fptr, "%d\t\t%d\t\t%d\t\t%lu\t\t%lu\n", i, pcb[i].occupied, pcb[i].pid, pcb[i].startClock.tv_sec, pcb[i].startClock.tv_nsec);
	}
	
	return;
}
static void signalHandler(int sig){
	printWrite(fptr, "%s: signaled with %d\n",program,sig);
	int i;
	for(i = 0; i < simul; i++){
		if(pcb[i].occupied != 0){
			kill(pcb[i].pid, SIGKILL);
		}
	}

	exit(1);
}

int main(int argc, char** argv){
	program = argv[0];

	int opt = 0;

	//Register exit function to deallocate memory
	atexit(deallocateSHM);

	while((opt = getopt(argc, argv, "hn:s:t:f:")) > 0){
		switch (opt){
    			case 'h':
      				helpMenu();
     		 		return -1;
    			case 'n':
				proc = atoi(optarg);
				break;
			case 's':
				simul = atoi(optarg);
				break;
			case 't':
				timeLimit = atoi(optarg);
                                break;
			case 'f':
				logfile = optarg;
				break;
			case '?':
                                fprintf(stderr, "%s: ERROR: Unrecognized option: -%c\n",program,optopt);
                                return -1;
			default:
      				helpMenu();
      				return -1;
		}


	}

	if(simul > MAX_SIMUL_PROCESSES){
		printWrite(fptr, "The number of simultaneous processes cannot be more than %d!\n",MAX_SIMUL_PROCESSES);
		return EXIT_FAILURE;
	}

	if(createSHM() == -1)
		return EXIT_FAILURE;
	
	signal(SIGINT, signalHandler);
	signal(SIGALRM, signalHandler);
	alarm(EXITTIME);
	
	nextFork.tv_sec = rand() % timeLimit + 1;
	nextFork.tv_nsec = rand() % DEFAULT_NSEC_INTERVAL + 1; 
	

	//start forking
	while(userForked < proc){
		if(checkTimer() != 0)
			startNewWorker();
		
		checkIfChildTerm();	
		printPCB();	
	}		

	while(curRun > 0){
		addTime(shareClock, &increTime);
		checkIfChildTerm();
		printPCB();	
	}
	
	return EXIT_SUCCESS;

}
