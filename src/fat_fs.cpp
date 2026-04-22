#include "fat_fs.h"
#include <iostream>
#include <iomanip>

FATFS::FATFS(Disk& disk, Journal& journal)
    : disk_(disk), journal_(journal), fat_(DISK_BLOCKS, NULL_BLOCK) {}

// ── Helpers ────────────────────────────────────────────────────────────────

std::vector<int> FATFS::chainOf(int first) const {
    std::vector<int> chain;
    int cur = first;
    while (cur != NULL_BLOCK && cur != FAT_EOC) {
        chain.push_back(cur);
        cur = fat_[cur];
    }
    return chain;
}

void FATFS::freeChain(int first) {
    int cur = first;
    while (cur != NULL_BLOCK && cur != FAT_EOC) {
        int next = fat_[cur];
        fat_[cur] = NULL_BLOCK;
        disk_.freeBlock(cur);
        cur = next;
    }
}

bool FATFS::buildChain(int blocks, int& firstBlockOut) {
    std::vector<int> allocated;
    if (!disk_.allocateBlocks(blocks, allocated)) return false;

    // Link them in the FAT
    for (int i = 0; i < (int)allocated.size() - 1; ++i)
        fat_[allocated[i]] = allocated[i + 1];
    fat_[allocated.back()] = FAT_EOC;

    firstBlockOut = allocated.front();
    return true;
}

// ── Public API ─────────────────────────────────────────────────────────────

bool FATFS::create(const std::string& name, int sizeBytes) {
    if (directory_.count(name)) {
        std::cout << "  [FAT] ERROR: '" << name << "' already exists.\n";
        return false;
    }
    int blocks = (sizeBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocks == 0) blocks = 1;

    int jid = journal_.logIntent("CREATE(FAT)",
        "file=" + name + " blocks=" + std::to_string(blocks));

    int first = NULL_BLOCK;
    if (!buildChain(blocks, first)) {
        std::cout << "  [FAT] ERROR: not enough blocks for '" << name << "'.\n";
        journal_.abort(jid);
        return false;
    }

    FATEntry e;
    e.name       = name;
    e.firstBlock = first;
    e.fileSize   = sizeBytes;
    directory_[name] = e;

    auto chain = chainOf(first);
    std::cout << "  [FAT] Created '" << name << "': chain [";
    for (int i = 0; i < (int)chain.size(); ++i) {
        if (i) std::cout << "->";
        std::cout << chain[i];
    }
    std::cout << "->EOC]\n";
    journal_.commit(jid);
    return true;
}

bool FATFS::open(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [FAT] ERROR: '" << name << "' not found.\n";
        return false;
    }
    it->second.isOpen = true;
    std::cout << "  [FAT] Opened '" << name << "'. (FAT already in memory — "
              << fatMemoryBytes() << " bytes resident)\n";
    return true;
}

bool FATFS::close(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end() || !it->second.isOpen) {
        std::cout << "  [FAT] ERROR: '" << name << "' not open.\n";
        return false;
    }
    it->second.isOpen = false;
    std::cout << "  [FAT] Closed '" << name << "'. (FAT remains in memory)\n";
    return true;
}

bool FATFS::read(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [FAT] ERROR: '" << name << "' not found.\n";
        return false;
    }
    auto chain = chainOf(it->second.firstBlock);
    std::cout << "  [FAT] Read '" << name << "': following " << chain.size()
              << " FAT pointer(s) (random seeks per block, no seek if buffer cache hit).\n";
    return true;
}

bool FATFS::write(const std::string& name, int newSizeBytes) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [FAT] ERROR: '" << name << "' not found.\n";
        return false;
    }
    auto& e = it->second;
    int newBlocks = (newSizeBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (newBlocks == 0) newBlocks = 1;
    int oldBlocks = (int)chainOf(e.firstBlock).size();

    int jid = journal_.logIntent("WRITE(FAT)",
        "file=" + name + " oldBlocks=" + std::to_string(oldBlocks)
        + " newBlocks=" + std::to_string(newBlocks));

    if (newBlocks == oldBlocks) {
        e.fileSize = newSizeBytes;
        std::cout << "  [FAT] Write '" << name << "': same block count, in-place update.\n";
        journal_.commit(jid);
        return true;
    }

    // Rebuild chain
    freeChain(e.firstBlock);
    int first = NULL_BLOCK;
    if (!buildChain(newBlocks, first)) {
        std::cout << "  [FAT] ERROR: cannot resize '" << name << "'.\n";
        journal_.abort(jid);
        return false;
    }
    e.firstBlock = first;
    e.fileSize   = newSizeBytes;
    std::cout << "  [FAT] Resized '" << name << "' to " << newBlocks << " block(s).\n";
    journal_.commit(jid);
    return true;
}

bool FATFS::remove(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [FAT] ERROR: '" << name << "' not found.\n";
        return false;
    }

    int jid = journal_.logIntent("DELETE(FAT)",
        "step1=remove_dir_entry step2=free_FAT_chain step3=free_disk_blocks | file=" + name);

    freeChain(it->second.firstBlock);
    directory_.erase(it);
    std::cout << "  [FAT] Deleted '" << name << "' and freed FAT chain.\n";
    journal_.commit(jid);
    return true;
}

void FATFS::printDirectory() const {
    std::cout << "\n  ── FAT Directory ───────────────────────────\n";
    std::cout << "  " << std::left
              << std::setw(16) << "Name"
              << std::setw(10) << "1stBlock"
              << std::setw(10) << "Size(B)"
              << "Chain\n";
    std::cout << "  " << std::string(56, '-') << '\n';
    for (const auto& [k, e] : directory_) {
        auto chain = chainOf(e.firstBlock);
        std::cout << "  " << std::left
                  << std::setw(16) << e.name
                  << std::setw(10) << e.firstBlock
                  << std::setw(10) << e.fileSize;
        for (int i = 0; i < (int)chain.size(); ++i) {
            if (i) std::cout << "->";
            std::cout << chain[i];
        }
        std::cout << "->EOC\n";
    }
    std::cout << "  FAT memory overhead: " << fatMemoryBytes() << " bytes\n";
    std::cout << "  ────────────────────────────────────────────\n";
}

void FATFS::printFAT() const {
    std::cout << "\n  ── FAT Table (non-free entries) ────────────\n";
    for (int i = 0; i < DISK_BLOCKS; ++i) {
        if (fat_[i] == NULL_BLOCK) continue;
        std::cout << "  fat[" << std::setw(3) << i << "] -> ";
        if (fat_[i] == FAT_EOC) std::cout << "EOC";
        else                    std::cout << fat_[i];
        std::cout << '\n';
    }
    std::cout << "  ────────────────────────────────────────────\n";
}
