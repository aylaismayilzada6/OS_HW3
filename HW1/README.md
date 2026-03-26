Readers-Writers Problem with Load Balancing - Ayla Ismayilzada

Language:
C using POSIX threads and semaphores

Files included:
- main.c
- README.txt
- report.txt

Program description:
This program implements the readers-writers synchronization problem with writer priority and load balancing across three file replicas.

How to compile:
gcc main.c -o program -lpthread

How to run:
./program

Program behavior:
- Multiple readers can read concurrently.
- Readers are distributed across three replicas.
- When the writer intends to write, new readers must wait.
- The writer updates all replicas exclusively.
- Each read/write operation is logged in log.txt.

Output files:
- log.txt (generated when the program runs)

