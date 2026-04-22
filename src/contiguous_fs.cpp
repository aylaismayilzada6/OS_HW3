#include "contiguous_fs.h"
#include <cmath>
#include <iostream>
#include <iomanip>

ContiguousFS::ContiguousFS(Disk& disk, Journal& journal)
    : disk_(disk), journal_(journal) {}

bool ContiguousFS::create(const std::string& name, int sizeBytes) {
    if (directory_.count(name)) {
        std::cout << "  [Contiguous] ERROR: '" << name << "' already exists.\n";
        return false;
    }
    int blocks = (sizeBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocks == 0) blocks = 1;

    int jid = journal_.logIntent("CREATE(Contiguous)",
        "file=" + name + " blocks=" + std::to_string(blocks));

    std::vector<int> allocated;
    if (!disk_.allocateContiguous(blocks, allocated)) {
        std::cout << "  [Contiguous] ERROR: not enough contiguous space for '"
                  << name << "' (" << blocks << " blocks).\n";
        journal_.abort(jid);
        return false;
    }

    ContiguousEntry e;
    e.name       = name;
    e.startBlock = allocated.front();
    e.blockCount = blocks;
    e.fileSize   = sizeBytes;
    e.isOpen     = false;
    directory_[name] = e;

    std::cout << "  [Contiguous] Created '" << name << "': blocks ["
              << e.startBlock << ".." << (e.startBlock + e.blockCount - 1) << "]\n";
    journal_.commit(jid);
    return true;
}

bool ContiguousFS::open(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [Contiguous] ERROR: '" << name << "' not found.\n";
        return false;
    }
    it->second.isOpen = true;
    std::cout << "  [Contiguous] Opened '" << name << "'.\n";
    return true;
}

bool ContiguousFS::close(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end() || !it->second.isOpen) {
        std::cout << "  [Contiguous] ERROR: '" << name << "' not open.\n";
        return false;
    }
    it->second.isOpen = false;
    std::cout << "  [Contiguous] Closed '" << name << "'.\n";
    return true;
}

bool ContiguousFS::read(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [Contiguous] ERROR: '" << name << "' not found.\n";
        return false;
    }
    const auto& e = it->second;
    std::cout << "  [Contiguous] Read '" << name << "': single seek to block "
              << e.startBlock << ", read " << e.blockCount << " block(s) sequentially.\n";
    return true;
}

bool ContiguousFS::write(const std::string& name, int newSizeBytes) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [Contiguous] ERROR: '" << name << "' not found.\n";
        return false;
    }
    auto& e = it->second;
    int newBlocks = (newSizeBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (newBlocks == 0) newBlocks = 1;

    if (newBlocks == e.blockCount) {
        e.fileSize = newSizeBytes;
        std::cout << "  [Contiguous] Write '" << name << "': same block count, updated in place.\n";
        return true;
    }

    // Must reallocate — free old, try to find new contiguous region
    int jid = journal_.logIntent("WRITE(Contiguous)/REALLOCATE",
        "file=" + name + " newBlocks=" + std::to_string(newBlocks));

    std::vector<int> oldBlocks;
    for (int i = e.startBlock; i < e.startBlock + e.blockCount; ++i)
        oldBlocks.push_back(i);
    disk_.freeBlocks(oldBlocks);

    std::vector<int> allocated;
    if (!disk_.allocateContiguous(newBlocks, allocated)) {
        // Restore old blocks (simulate rollback)
        disk_.allocateContiguous(e.blockCount, allocated);
        std::cout << "  [Contiguous] ERROR: cannot grow '" << name
                  << "' to " << newBlocks << " blocks (no contiguous space).\n";
        journal_.abort(jid);
        return false;
    }

    e.startBlock = allocated.front();
    e.blockCount = newBlocks;
    e.fileSize   = newSizeBytes;
    std::cout << "  [Contiguous] Reallocated '" << name << "' to blocks ["
              << e.startBlock << ".." << (e.startBlock + e.blockCount - 1) << "]\n";
    journal_.commit(jid);
    return true;
}

bool ContiguousFS::remove(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [Contiguous] ERROR: '" << name << "' not found.\n";
        return false;
    }
    const auto& e = it->second;

    // Journaled 3-step removal
    int jid = journal_.logIntent("DELETE(Contiguous)",
        "step1=remove_dir_entry step2=release_blocks["
        + std::to_string(e.startBlock) + ".."
        + std::to_string(e.startBlock + e.blockCount - 1) + "]");

    // Step 1: remove directory entry
    // Step 2: release blocks
    for (int i = e.startBlock; i < e.startBlock + e.blockCount; ++i)
        disk_.freeBlock(i);

    directory_.erase(it);
    std::cout << "  [Contiguous] Deleted '" << name << "'.\n";
    journal_.commit(jid);
    return true;
}

void ContiguousFS::printDirectory() const {
    std::cout << "\n  ── Contiguous Directory ────────────────────\n";
    std::cout << "  " << std::left
              << std::setw(16) << "Name"
              << std::setw(8)  << "Start"
              << std::setw(8)  << "Blocks"
              << std::setw(10) << "Size(B)"
              << "Open\n";
    std::cout << "  " << std::string(48, '-') << '\n';
    for (const auto& [k, e] : directory_) {
        std::cout << "  " << std::left
                  << std::setw(16) << e.name
                  << std::setw(8)  << e.startBlock
                  << std::setw(8)  << e.blockCount
                  << std::setw(10) << e.fileSize
                  << (e.isOpen ? "Yes" : "No") << '\n';
    }
    std::cout << "  ────────────────────────────────────────────\n";
}

void ContiguousFS::printFragmentation() const {
    // Find all free runs on disk
    bool inRun = false;
    int  runLen = 0, maxRun = 0, totalFree = 0, runCount = 0;
    for (int i = 0; i < DISK_BLOCKS; ++i) {
        if (disk_.isFree(i)) {
            ++totalFree;
            if (!inRun) { inRun = true; runLen = 0; ++runCount; }
            ++runLen;
            if (runLen > maxRun) maxRun = runLen;
        } else {
            inRun = false; runLen = 0;
        }
    }
    double frag = (totalFree > 0 && maxRun < totalFree)
                  ? 1.0 - (double)maxRun / totalFree : 0.0;
    std::cout << "  [Contiguous] Fragmentation: " << totalFree
              << " free blocks in " << runCount << " run(s). "
              << "Largest run=" << maxRun
              << ". External frag ratio=" << std::fixed << std::setprecision(2)
              << frag * 100 << "%\n";
}

double ContiguousFS::fragmentationRatio() const {
    int totalFree = 0, maxRun = 0, runLen = 0;
    for (int i = 0; i < DISK_BLOCKS; ++i) {
        if (disk_.isFree(i)) { ++totalFree; ++runLen; if (runLen > maxRun) maxRun = runLen; }
        else runLen = 0;
    }
    return (totalFree > 0 && maxRun < totalFree)
           ? 1.0 - (double)maxRun / totalFree : 0.0;
}
