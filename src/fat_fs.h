#pragma once
#include "disk.h"
#include "journal.h"
#include <string>
#include <unordered_map>
#include <vector>

constexpr int FAT_EOC = -2;   // End Of Chain marker in FAT table

// ─────────────────────────────────────────────
//  FAT Directory Entry
// ─────────────────────────────────────────────
struct FATEntry {
    std::string name;
    int         firstBlock = NULL_BLOCK;
    int         fileSize   = 0;
    bool        isOpen     = false;
};

// ─────────────────────────────────────────────
//  FAT File System
//  The entire FAT table lives in memory — this is the
//  primary trade-off vs i-nodes.
// ─────────────────────────────────────────────
class FATFS {
public:
    explicit FATFS(Disk& disk, Journal& journal);

    bool create (const std::string& name, int sizeBytes);
    bool open   (const std::string& name);
    bool close  (const std::string& name);
    bool read   (const std::string& name);
    bool write  (const std::string& name, int newSizeBytes);
    bool remove (const std::string& name);

    void printDirectory () const;
    void printFAT       () const;

    // Returns memory used by FAT table in bytes
    int  fatMemoryBytes () const { return DISK_BLOCKS * (int)sizeof(int); }

private:
    Disk&    disk_;
    Journal& journal_;

    // FAT table: fat_[i] holds the next block index for block i,
    //            FAT_EOC  = end of chain,
    //            NULL_BLOCK = free (not in FAT chain)
    std::vector<int> fat_;

    std::unordered_map<std::string, FATEntry> directory_;

    // Helpers
    std::vector<int> chainOf   (int firstBlock) const;
    void             freeChain (int firstBlock);
    bool             buildChain(int blocks, int& firstBlockOut);
};
