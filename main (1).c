#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_REPLICAS 3

/* 
Overview: 
    semt_t is a data type used in semaphores to create mutex
    sem_post increases the semaphore and wakes waiting threads
    sem_wait logic : is the value is 0, the thread must wait 
    sem_init initializes a semaphore before it can be used
    pthread_create creates a new thread, in the program, its used to create reader and writer
    pthread_join makes the main thread wait until the created threads finish execution
    logStatus() writes information about every read and write operation to log.txt
    


for documentation, i am reffering to: 
https://pubs.opengroup.org/onlinepubs/7908799/xsh/semaphore.h.html

*/

int readCount[NUM_REPLICAS] = {0, 0, 0}; // number of readers currently reading replica i
int totalReaders = 0;
int writerWaiting = 0; // when writer wabts to write, new readers must wait
int writerActive = 0; // writerActive  = 1 if writer is currently writing



// to fill replicas with some content 
char replicas[NUM_REPLICAS][100] = {
    "test content 1",
    "test content 2",
    "test content 3"
};

/*  
    mutex provides var such as counters and status flags,
    writeLock gives access to all replicas
    
*/
sem_t mutex;
sem_t writeLock;

void logStatus(const char *action, int id, int replica); // just a function declaration



/* 
instead of writing num of replicas manually, 
I created global var:  #define NUM_REPLICAS 3
*/

int chooseReplica() {
    int minIndex = 0;

    for (int i = 1; i < NUM_REPLICAS; i++) {
        if (readCount[i] < readCount[minIndex]) {
            minIndex = i;
        }
    }

    return minIndex;
}

/*
Reader should:
    - wait if a writer is waiting
    - choose the least busy replica 
    - read once and then terminate 
*/
void* reader(void* arg) {
    int id = *(int*)arg;
    int replica;
    

    while (1) {
        sem_wait(&mutex);// try to enter cr, but wait is smn using it 


        // if a writer is not waiting, reader can enter      
        if (writerWaiting == 0) {
            
            replica = chooseReplica();

            readCount[replica]++;
            totalReaders++;
            
            
            if (totalReaders == 1) {
                sem_wait(&writeLock);
            }
            printf("reader %d START reading from replica %d\n", id, replica + 1);
            
            sem_post(&mutex);
            break;
        }

        sem_post(&mutex);// tells the prog to leave cr and allows others to enter
        usleep(100000);
    }


    sleep(1);
    
    printf("reader %d FINISH reading from replica %d\n", id, replica + 1);
    
    logStatus("READ", id, replica + 1);
    
    sem_wait(&mutex);
    
    readCount[replica]--;
    totalReaders--;
    
    // last reader releases the writer
    if (totalReaders == 0) {
        sem_post(&writeLock);
    }
    sem_post(&mutex);

    return NULL;
}


/*

Writer should:
 - periodically try to write 
 - if one inteds to rite, new readers must wait 
 - update all replicas together 
 
*/
void* writer(void* arg) {
    
    int version = 1;
    sleep(rand() % 3 + 1);
    
    sem_wait(&mutex);
    writerWaiting = 1;
    sem_post(&mutex);

    sem_wait(&writeLock); // waits untill all reders finish 
    sem_wait(&mutex);
    writerActive = 1;
    sem_post(&mutex);
    
    char newContent[100];
    sprintf(newContent, "Updated version %d", version++);

    /*
    updates all replicas 
    */
    for (int i = 0; i < NUM_REPLICAS; i++) {
        strcpy(replicas[i], newContent);
    }

    
    printf("writer START writing to all replicas\n");
    sleep(2);
    printf("writer FINISH writing to all replicas\n");
    
    logStatus("WRITE", 0, 0);
    
    sem_wait(&mutex);
    writerActive = 0;
    writerWaiting = 0;
    sem_post(&mutex);

    sem_post(&writeLock);

    return NULL;
}



/* 
    append one entry after each read or write,
    each log should inlude:
    action type (read or write),
    thread is, 
    replica id,
    readers count on each replica (in a form of tuple),
    if writer is active,
    and contents of each replicas

*/
void logStatus(const char *action, int id, int replica) {
    FILE *logFile = fopen("log.txt", "a");
    if (logFile == NULL) return;
    
    sem_wait(&mutex);
    
  fprintf(logFile,
        "%s | Thread %d | Replica %d | Readers: [%d, %d, %d] | Writer active: %d | Contents: [%s] [%s] [%s]\n |" ,
        action,
        id,
        replica,
        readCount[0], readCount[1], readCount[2],
        writerActive,
        replicas[0], replicas[1], replicas[2]);

    sem_post(&mutex);
    fclose(logFile);
}



int main() {
    pthread_t r1, r2, r3, w;
    int id1 = 1, id2 = 2, id3 = 3;
    
    srand(time(NULL));
    
    FILE *logFile = fopen("log.txt", "w");
        if (logFile != NULL) {
            fclose(logFile);
        }
    

    sem_init(&mutex, 0, 1);
    sem_init(&writeLock, 0, 1);
    
    // writers and reders at random intervals 

    pthread_create(&r1, NULL, reader, &id1);
    usleep((rand() % 1000 + 500) * 1000);
    
    pthread_create(&r2, NULL, reader, &id2);
    usleep((rand() % 1000 + 500) * 1000);
    
    pthread_create(&w, NULL, writer, NULL);
    usleep((rand() % 1000 + 500) * 1000);
    
    pthread_create(&r3, NULL, reader, &id3);

    pthread_join(r1, NULL);
    pthread_join(r2, NULL);
    pthread_join(r3, NULL);
    pthread_join(w, NULL);

    sem_destroy(&mutex);
    sem_destroy(&writeLock);

    return 0;
}