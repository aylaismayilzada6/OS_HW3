#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
Here, each node represnts a block of memeory
2 possibles: either a process or a hole (free space)

*/
struct Node {
    int start;
    int size;
    int free; // 1 => hole, 0 => process
    char name[10];
    struct Node* next;
    
};

// ptr used for Next Fit (remembers last position)
struct Node* lastNextFit = NULL;

// head of the linked list
struct Node* head = NULL; // declare the head as NULL

//create a new memory block
struct Node* create(int start, int size, int free, char name[]) {
    struct Node* n = (struct Node*)malloc(sizeof(struct Node)); // memory allocation
    n->start = start;
    n->size = size;
    n->free = free;
    // checks if it is a process or hole, if yes, copy the name 
    if (name != NULL) strcpy(n->name, name);
    else strcpy(n->name, "");
    n->next = NULL;
    return n;
    
}

// init the memory
void init(int size){
    head = create(0, size, 1, NULL);
    lastNextFit = head;
}

// print the current memory state 

void printMem(){
    struct Node* t = head;
    while (t!= NULL){
        if (t->free) printf("Hole(%d,%d) ", t->start, t->size);
        else
            printf("%s(%d,%d) ", t->name, t->start, t->size);
        t = t->next;
    }
    printf("\n");
    
}

//free the whole list

void clearMem(){
    struct Node* t = head;
    
    while (t != NULL) {
        struct Node* next = t->next;
        free(t);
        t = next;
    }
    head = NULL;
    lastNextFit = NULL;
}

//Firt Fit -> return first hole that is large enough

struct  Node* firstFit (int size){
    struct Node* t = head;
    while(t != NULL){
        if (t->free && t->size >= size)
        return t;
        t = t->next;
        
    }
    
    return NULL;
}

// next fit -> cont searching from last used position

struct Node* nextFit(int size) {
    struct Node* t;

    if (lastNextFit == NULL) lastNextFit = head;
    t = lastNextFit;

    while (t != NULL) {
        if (t->free && t->size >= size)
            return t;
        t = t->next;
    }
    
    t = head;
    while (t != lastNextFit) {
        if (t->free && t->size >= size)
            return t;
        t = t->next;
    }

    return NULL;
}

//Best Fit -> find the smalles hole that fits the process

struct Node* bestFit(int size) {
    struct Node* t = head;
    struct Node* best = NULL;

    while (t != NULL) {
        if (t->free && t->size >= size) {
            if (best == NULL || t->size < best->size) best = t;
        }
        t = t->next;
    }
    return best;
}
// Wosrt Fit finds the largest available hole
struct Node* worstFit(int size) {
    struct Node* t = head;
    struct Node* worst = NULL;
    while (t != NULL) {
        if (t->free && t->size >= size) {
            if (worst == NULL || t->size > worst->size) worst = t;
        }
        t = t->next;
    }

    return worst;
}

// choose allocation based on algorithm

struct Node* findHole(int size, char algorithm[]) {
    if (strcmp(algorithm, "first") == 0)
        return firstFit(size);

    if (strcmp(algorithm, "next") == 0)
        return nextFit(size);

    if (strcmp(algorithm, "best") == 0)
        return bestFit(size);

    if (strcmp(algorithm, "worst") == 0)
        return worstFit(size);

    return NULL;
}

//Allocate memory for a process

void allocate(char name[], int size, char algorithm[]) {

    struct Node* hole = findHole(size, algorithm);
    
    if (hole == NULL) {
        printf("allocation has failed  %s %d\n", name, size);
        printMem();
        return;
        
    }
     if (hole->size == size) {
        hole->free = 0;
        strcpy(hole->name, name);
        lastNextFit = hole ;
     } 
     else {
          struct Node* newHole = create(hole->start + size, hole->size - size, 1, NULL);
 
        // now we need to assign attributes again
        newHole ->next = hole ->next;
        hole -> size = size;
        hole -> free = 0;
        strcpy(hole->name, name);
        hole -> next = newHole;
        lastNextFit = hole;
        
        
     }
     printf("Allocated %s %d\n", name, size);
     printMem();
}

// Free a process by name and merge adjecent holes 
void freeMem(char name[]){
    struct Node* t = head;
    struct Node* prev = NULL;
    while(t != NULL){
        if (t->free==0){
            if(strcmp(t->name, name) == 0){
                t->free = 1;
                t->name[0]='\0';
                
                 if (t->next != NULL && t->next->free) {
                    struct Node* temp = t->next;
                    t->size = t->size + t->next->size;
                    t->next = t->next->next;
                    free(temp);
                }
                
                if (prev && prev->free) {
                    prev->size = prev->size + t->size;
                    prev->next = t->next;
                    if (lastNextFit == t) lastNextFit = prev;
                    free(t);
                    t = prev;
                    
                if (t->next != NULL && t->next->free) {
                    struct Node* temp = t->next;
                    t->size += temp->size;
                    t->next = temp->next;
                    free(temp);
                }
                
            }
                
                printf("free %s\n", name);
                printMem();
                return; 

            }
        }
        prev = t;
        t=t->next;
    }
    printf("Failed to free %s\n", name);
    printMem();
    


}


struct Op {
    int type;      // 1 = allocate, 2 = free
    char name[10];
    int size;      // only used for allocate
};



int main() {
    int totalMemory = 256;

    struct Op workload[] = {
        {1, "A", 40},
        {1, "B", 25},
        {1, "C", 30},
        {2, "B", 0},
        {1, "D", 20},
        {2, "A", 0},
        {1, "E", 15},
        {2, "D", 0},
        {2, "C", 0}
        
    };

    int n = sizeof(workload) / sizeof(workload[0]);
    int i;

    init(totalMemory);
    printMem();

    for (int i = 0; i < n; i++) {

        if (workload[i].type == 1) {
            allocate(workload[i].name, workload[i].size, "first");
        }
        else if (workload[i].type == 2) {
            freeMem(workload[i].name);
        }
    }
    clearMem();
    return 0;
}








