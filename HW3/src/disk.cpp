#include "disk.h"
#include <stdexcept>
#include <iomanip>

Disk::Disk() : free_(DISK_BLOCKS, true) {}

// Allocate 'count' contiguous blocks. Returns false if not enough room.
bool Disk::allocateContiguous(int count, std::vector<int>& out) {
    if (count <= 0) return false;
    // First-fit scan
    int start = -1, run = 0;
    for (int i = 0; i < DISK_BLOCKS; ++i) {
        if (free_[i]) {
            if (start == -1) start = i;
            if (++run == count) {
                // Found a run — mark allocated
                out.clear();
                for (int j = start; j < start + count; ++j) {
                    free_[j] = false;
                    out.push_back(j);
                }
                return true;
            }
        } else {
            start = -1;
            run   = 0;
        }
    }
    return false;   // fragmented or full
}

// Allocate 'count' non-contiguous blocks (for FAT / i-node use).
bool Disk::allocateBlocks(int count, std::vector<int>& out) {
    if (freeCount() < count) return false;
    out.clear();
    for (int i = 0; i < DISK_BLOCKS && (int)out.size() < count; ++i) {
        if (free_[i]) {
            free_[i] = false;
            out.push_back(i);
        }
    }
    return (int)out.size() == count;
}

int Disk::allocateOneBlock() {
    for (int i = 0; i < DISK_BLOCKS; ++i) {
        if (free_[i]) { free_[i] = false; return i; }
    }
    return NULL_BLOCK;
}

void Disk::freeBlock(int blockNo) {
    if (blockNo >= 0 && blockNo < DISK_BLOCKS)
        free_[blockNo] = true;
}

void Disk::freeBlocks(const std::vector<int>& blocks) {
    for (int b : blocks) freeBlock(b);
}

bool Disk::isFree(int blockNo) const {
    return (blockNo >= 0 && blockNo < DISK_BLOCKS) ? free_[blockNo] : false;
}

int Disk::freeCount() const {
    int n = 0;
    for (bool f : free_) if (f) ++n;
    return n;
}

Block& Disk::getBlock(int blockNo) {
    if (blockNo < 0 || blockNo >= DISK_BLOCKS)
        throw std::out_of_range("Invalid block number");
    return blocks_[blockNo];
}

// Print a 16-wide bitmap: '.' = free, 'X' = used
void Disk::printBitmap() const {
    std::cout << "\n  Disk Bitmap (" << DISK_BLOCKS << " blocks, "
              << freeCount() << " free)\n  ";
    for (int i = 0; i < DISK_BLOCKS; ++i) {
        if (i && i % 16 == 0) std::cout << "\n  ";
        std::cout << (free_[i] ? '.' : 'X');
    }
    std::cout << '\n';
}
