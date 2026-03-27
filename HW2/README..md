# Memory Allocation Simulator (Linked List Based)

## Context
A simple **C program** that simulates how an operating system manages memory using dynamic allocation strategies.

Memory is represented as a **linked list of blocks**, where each block can be:
- a **process (allocated memory)**  
- a **hole (free memory)**  

The simulator executes a predefined workload of allocations and deallocations and prints the memory state after each operation.

---

## How it Works

- Memory starts as **one large free block**
- When allocating:
  - A suitable hole is found
  - The hole is either:
    - fully used, or
    - split into a process + smaller hole
- When freeing:
  - The block becomes a hole
  - Adjacent holes are **merged automatically**

---

## Features

- Supports classic allocation algorithms:
  - **First Fit**
  - **Next Fit**
  - **Best Fit**
  - **Worst Fit**
- Dynamic memory splitting during allocation
- Automatic **merging of adjacent holes** during deallocation
- Step-by-step logging of memory state
- Simple and readable linked list implementation

---

## Files
├── main.c ← main source code
├── README.md ← this file
└── report.md 

---

## How to Compile

You need **GCC** (or any C99-compatible compiler).

```bash
gcc -Wall -o HW2 main.c
```

That's it — no external libraries.

---

## How to Run

```bash
./main.c
```
