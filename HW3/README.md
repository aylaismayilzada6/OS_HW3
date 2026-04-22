# File System Allocation Simulator

## Files
- `main.cpp` — entire simulator (one file)
- `README.md` — this file
- `REPORT.md` — written analysis

## Compile & Run
```bash
g++ -std=c++17 -o fssim main.cpp
./fssim
```


## What it simulates
1. **Contiguous Allocation** — first-fit, external fragmentation tracking
2. **FAT** — linked-list chain in memory, EOC markers, memory overhead
3. **I-nodes** — direct + indirect pointers, hard links (refCount), soft links
4. **Hierarchical Directory** — mkdir / mkfile / remove with paths
5. **Journal** — write-ahead log printed at the end
