#pragma once
#include "disk.h"
#include "journal.h"
#include <string>
#include <unordered_map>
#include <vector>

// ─────────────────────────────────────────────
//  Directory entry for contiguous allocation
// ─────────────────────────────────────────────
struct ContiguousEntry {
    std::string name;
    int         startBlock = NULL_BLOCK;
    int         blockCount = 0;
    int         fileSize   = 0;     // bytes
    bool        isOpen     = false;
};

// ─────────────────────────────────────────────
//  Contiguous Allocation File System
// ─────────────────────────────────────────────
class ContiguousFS {
public:
    explicit ContiguousFS(Disk& disk, Journal& journal);

    bool create (const std::string& name, int sizeBytes);
    bool open   (const std::string& name);
    bool close  (const std::string& name);
    bool read   (const std::string& name);
    bool write  (const std::string& name, int newSizeBytes);
    bool remove (const std::string& name);

    void printDirectory() const;
    void printFragmentation() const;

    // Returns external fragmentation ratio [0,1]
    double fragmentationRatio() const;

private:
    Disk&    disk_;
    Journal& journal_;
    std::unordered_map<std::string, ContiguousEntry> directory_;
};
