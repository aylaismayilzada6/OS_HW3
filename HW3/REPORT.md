# File System Allocation Simulator — Report
Assignment 3 | Operating Systems

---

## 1. Contiguous Allocation

Each file is stored as a single unbroken run of blocks. The directory entry
holds just two numbers: start block and block count. Reading a file requires
one disk seek, then all blocks are read sequentially — the best possible
read performance.

**Problem — external fragmentation.** When files are deleted, they leave
gaps. A new file needing 6 blocks may fail even when 6 free blocks exist,
because no single run of 6 is available. The simulator shows this when
`file_b.bin` is deleted: the free space splits into two runs and the
fragmentation ratio rises above 0%.

Growing a file is also expensive — the OS must find a new, larger
contiguous region, copy the data, and release the old blocks.


---

## 2. FAT (File Allocation Table)

Each block has a slot in an array kept entirely in RAM. A slot holds the
number of the *next* block in the file's chain, or end of chain.
The directory entry stores only the first block number.

Because blocks can be anywhere on disk, there is **no external
fragmentation**. Growing a file simply appends a free block to the chain.

**Problem — memory overhead.** The entire FAT must always be in memory,
even when no files are open. For our 64-block demo this is only 256 bytes,
but on a 1 TB real disk the FAT would occupy gigabytes of RAM constantly.

Random access inside a large file is also slow: to reach byte offset `B`,
the OS must walk `B / BLOCK_SIZE` FAT pointers from the start.

---

## 3. I-node Allocation

Each file gets its own i-node structure containing metadata and block
pointers. The first `DIRECT_PTRS = 4` pointers point directly to data
blocks. For larger files, one **indirect pointer** points to a block whose
contents are more block numbers (up to `BLOCK_SIZE / sizeof(int) = 128`
extra pointers in a real system).

Key property: the i-node is loaded into memory **only when the file is
opened**, and evicted when closed. A system with thousands of files pays
almost no memory cost if few are open at once.

**Hard links** let multiple directory entries point to the same i-node.
A `refCount` field tracks how many names exist; blocks are freed only when
`refCount` reaches zero. This is why deleting `kernel.bin` left the data
alive — `kernel.bak` still held a reference.

**Soft (symbolic) links** are a separate i-node whose content is a path
string. If the target is deleted the symlink becomes *broken* — opening it
fails — which the simulator demonstrates with `notes_link`.

**Best for:** general-purpose file systems (Linux ext4, macOS APFS) where
memory efficiency, link support, and fast metadata access all matter.

---

## 4. Comparison Table

| Feature | Contiguous | FAT | I-node |

---

## 5. Journaling

Before every multi-step operation (create, resize, delete) a `PENDING`
entry is written to the log. After the operation succeeds it is marked
`COMMITTED`. If the system crashed between those two events, the recovery
code can replay or roll back any `PENDING` entries at next boot, keeping
the disk consistent. This is the same technique used by NTFS and ext3/ext4.
