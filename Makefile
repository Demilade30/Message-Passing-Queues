CC=gcc
CFLAGS=-Wall -Werror -O2 -g
OBJ= oss.o worker.o
DEPS= oss.c worker.c

all: oss worker

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<
oss: $(OBJ)
	$(CC) $(CFLAGS) -lm -o $@ $@.o

worker: $(OBJ)
	$(CC) $(CFLAGS) -lm -o $@ $@.o
clean:
	rm -rf oss worker *.log *.o a.out 
