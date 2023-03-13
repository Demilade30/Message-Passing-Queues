# Process-Tables-and-Shared-Memory
Process tables and shared memory

## Project: Maintaining a ”simulated system clock” in shared memory
This project consists of two executables, `oss` and `worker`. The `oss` executable launches workers and maintains a ”simulated system clock” in shared memory. The clock consists of two separate integers (one storing seconds, the other nanoseconds) in shared memory, both of which are initialized to zero.

## Author
* Name: Divine Akinjiyan (completed on: 02/23/2023)
* Version Control: GitHub

## Getting Started
These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

## Makefile
A makefile has been created to compile and create the executables. Type `make` to tun the project. The makefile uses the `all` command to compile multiple executables from the same makefile.

## Executables
### OSS
`oss` maintains a process table (consisting of Process Control Blocks, one for each process). The first thing it keeps track of is the PID of the children processes, as well as the time right before `oss` does a fork to launch that child process (based on the simulated clock). It also contains an entry for if the entry in the process table is empty (ie: not being used).

### Worker
`worker` takes in two command line arguments, corresponding to how many seconds and nanoseconds it should stay in the system. `worker` will then attach to shared memory and examine the simulated system clock. It will figure out what time it should terminate by adding up the system clock time and the time passed to it. It will go into a loop, constantly checking the system clock to see if this time has passed. If it ever looks at the system clock and sees values over the ones when it should terminate, it outputs information and then terminates.

## Usage
To compile the source files into executables, simply run the makefile with the command `make`.

## How to run the program
	`make`

Example:
```bash
./oss [-h] [-n] [-s] [-t]
```

*	-h	describes how the project should run and then, terminates
*	-n	the number of worker processes
*	-s	the number of processes that can be running simultaneously and 
*	-t	the bound of time that a child process will be launched for

## Note
Make sure to use the correct syntax when running the executables and makefile.

## License
The project is licensed under the MIT License: <https://opensource.org/licenses/MIT>.

## Acknowledgements
* Mark Hauschild, Associate Teaching Professor Ph.D. in Applied Mathematics, University of Missouri-St. Louis, 2013, hauschildm@umsl.edu
