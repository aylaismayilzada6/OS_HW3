# Memory Allocation Simulator – Report

## 1. Design

The goal of this program is to simulate how memory allocation works in an operating system using a simple and clear approach.

The memory is treated as one continuous block at the beginning. As the program runs, this memory is divided into smaller parts depending on allocation and deallocation requests. Each part of memory is either:
- a process (allocated), or
- a hole (free space)

The program processes a predefined list of operations (allocate/free) and prints the memory state after each step. This makes it easy to see how memory changes over time.
---

## 2. Linked List Implementation

To represent memory, I used a **singly linked list**.

Each node in the list represents a block of memory and contains:
- starting position (`head`)
- size of the block (`size`)
- whether it is free or allocated (`free`)
- process name (`name`)
- pointer to the next block (`next`)

### Why linked list?

A linked list makes sense here because:
- memory blocks change size dynamically
- blocks can be split or merged
- we don’t need fixed-size partitions

### Initialization

At the start, memory is just one big hole:
Hole(0,256)
---

## 3. Allocation Logic

When allocating memory:

1. The program searches for a suitable hole using the selected algorithm
2. If the hole size is exactly equal:
   - it becomes a process block
3. If the hole is bigger:
   - it is split into:
     - the allocated process
     - a smaller remaining hole

This is done by creating a new node and updating the links in the list.

---

## 4. Deallocation Logic

When freeing a process:

1. The program finds the block by its name
2. Marks it as free
3. Then merges adjacent holes:
   - first with the next block (if free)
   - then with the previous block (if free)

This merging is very important because it reduces fragmentation.

Without merging, memory would quickly break into many small useless holes.

---

## 5. Allocation Algorithms

The program supports four algorithms:

### First Fit
- picks the first hole that is large enough
- fast and simple

### Next Fit
- continues searching from the last allocated position
- avoids always using the beginning of memory

### Best Fit
- chooses the smallest hole that fits
- tries to minimize wasted space

### Worst Fit
- chooses the largest hole
- leaves bigger remaining holes

In my current test, I used:
"first", so all operations were done using First Fit.


### Observations from test run
Using the workload:
A(40), B(25), C(30), free B, D(20), free A, E(15), free D, free C

I noticed:

- At the beginning, memory is one big block → no fragmentation
- After several allocations, memory becomes divided into many blocks
- When processes are freed, holes appear in different places
- Fragmentation becomes visible in the middle of execution
- After multiple frees, merging restores larger holes

### Important point

The **merge step** is what prevents fragmentation from getting worse.  
Without merging, the memory would stay split into many small holes.

---

## 7. Comparison of Algorithms

From testing and expected behavior:

### First Fit
- works well and is fast
- but tends to leave small holes at the beginning of memory

### Next Fit
- spreads allocations more evenly
- but might skip better holes earlier in memory

### Best Fit
- tries to be efficient
- but often creates very small unusable holes → **more fragmentation**

### Worst Fit
- avoids very small holes
- but breaks large blocks too quickly

---

## 8. Conclusion

- linked list is a good structure for dynamic memory
- splitting and merging are essential operations
- fragmentation is unavoidable but can be reduced
- different algorithms behave differently depending on the workload

In practice, First Fit and Next Fit tend to be the most practical — they're fast and their fragmentation behavior is predictable. 
Best Fit sounds ideal but the tiny leftovers it creates can actually make things worse over time. 
Worst Fit is useful in specific workloads but risky with large processes.
