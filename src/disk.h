#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>

// ─────────────────────────────────────────────
//  Disk constants
// ─────────────────────────────────────────────
constexpr int DISK_BLOCKS      = 256;   // total number of blocks on disk
constexpr int BLOCK_SIZE       = 512;   // bytes per block
constexpr int NULL_BLOCK       = -1;    // sentinel for "no block"

// ─────────────────────────────────────────────
//  A single disk block (raw storage)
// ─────────────────────────────────────────────
struct Block {
    uint8_t data[BLOCK_SIZE] = {};   // zero-initialised
};

// ─────────────────────────────────────────────
//  Disk: linear array of blocks + free-space bitmap
// ─────────────────────────────────────────────
class Disk {
public:
    Disk();

    // Returns true on success and writes the allocated block numbers into 'out'.
    bool  allocateContiguous(int count, std::vector<int>& out);
    bool  allocateBlocks    (int count, std::vector<int>& out);   // scattered
    int   allocateOneBlock  ();
    void  freeBlock         (int blockNo);
    void  freeBlocks        (const std::vector<int>& blocks);

    bool  isFree (int blockNo) const;
    int   freeCount() const;
    int   totalBlocks() const { return DISK_BLOCKS; }

    // Raw read / write (used by higher layers)
    Block& getBlock(int blockNo);

    void printBitmap() const;

private:
    Block              blocks_[DISK_BLOCKS];
    std::vector<bool>  free_;         // true = free
};
