#include "inode_fs.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>

// Maximum supported i-nodes
constexpr int MAX_INODES = 64;

InodeFS::InodeFS(Disk& disk, Journal& journal)
    : disk_(disk), journal_(journal) {
    inodes_.resize(MAX_INODES);
    for (int i = 0; i < MAX_INODES; ++i) inodes_[i].inodeId = -1; // free slot
}

// ── I-node table management ────────────────────────────────────────────────

int InodeFS::allocateInode() {
    for (int i = 0; i < MAX_INODES; ++i) {
        if (inodes_[i].inodeId == -1) {
            inodes_[i] = Inode();
            inodes_[i].inodeId  = i;
            inodes_[i].refCount = 1;
            return i;
        }
    }
    return -1;  // no free i-node
}

void InodeFS::freeInode(int id) {
    if (id >= 0 && id < MAX_INODES)
        inodes_[id].inodeId = -1;  // mark free
}

Inode& InodeFS::getInode(int id) {
    if (id < 0 || id >= MAX_INODES || inodes_[id].inodeId == -1)
        throw std::runtime_error("Invalid inode id: " + std::to_string(id));
    return inodes_[id];
}
const Inode& InodeFS::getInode(int id) const {
    if (id < 0 || id >= MAX_INODES || inodes_[id].inodeId == -1)
        throw std::runtime_error("Invalid inode id: " + std::to_string(id));
    return inodes_[id];
}

// ── Block allocation helpers ───────────────────────────────────────────────

bool InodeFS::allocateBlocksForInode(Inode& node, int totalBlocks) {
    // Fill direct pointers first, then one indirect block
    int directNeeded   = std::min(totalBlocks, INODE_DIRECT_BLOCKS);
    int indirectNeeded = totalBlocks - directNeeded;

    if (indirectNeeded > (int)PTRS_PER_BLOCK) {
        std::cout << "  [Inode] ERROR: file too large (max "
                  << Inode::maxFileSize() << " bytes).\n";
        return false;
    }

    // Allocate direct blocks
    std::vector<int> directAlloc;
    if (!disk_.allocateBlocks(directNeeded, directAlloc)) return false;
    for (int i = 0; i < directNeeded; ++i) node.direct[i] = directAlloc[i];

    // Allocate indirect blocks if needed
    if (indirectNeeded > 0) {
        int indirectBlock = disk_.allocateOneBlock();
        if (indirectBlock == NULL_BLOCK) return false;
        node.indirect = indirectBlock;

        // Allocate blocks pointed to by the indirect block
        std::vector<int> indirectData;
        if (!disk_.allocateBlocks(indirectNeeded, indirectData)) return false;

        // Write pointers into the indirect block (simulate)
        Block& blk = disk_.getBlock(indirectBlock);
        int* ptrs  = reinterpret_cast<int*>(blk.data);
        for (int i = 0; i < indirectNeeded; ++i) ptrs[i] = indirectData[i];
    }
    return true;
}

void InodeFS::freeBlocksOfInode(Inode& node) {
    // Free direct blocks
    for (int i = 0; i < INODE_DIRECT_BLOCKS; ++i) {
        if (node.direct[i] != NULL_BLOCK) {
            disk_.freeBlock(node.direct[i]);
            node.direct[i] = NULL_BLOCK;
        }
    }
    // Free indirect block + pointed blocks
    if (node.indirect != NULL_BLOCK) {
        Block& blk = disk_.getBlock(node.indirect);
        int* ptrs  = reinterpret_cast<int*>(blk.data);
        for (int i = 0; i < (int)PTRS_PER_BLOCK; ++i) {
            if (ptrs[i] != 0) disk_.freeBlock(ptrs[i]);
        }
        disk_.freeBlock(node.indirect);
        node.indirect = NULL_BLOCK;
    }
}

std::vector<int> InodeFS::blocksOf(const Inode& node) const {
    std::vector<int> result;
    for (int i = 0; i < INODE_DIRECT_BLOCKS; ++i)
        if (node.direct[i] != NULL_BLOCK) result.push_back(node.direct[i]);
    if (node.indirect != NULL_BLOCK) {
        const Block& blk = const_cast<Disk&>(disk_).getBlock(node.indirect);
        const int* ptrs  = reinterpret_cast<const int*>(blk.data);
        for (int i = 0; i < (int)PTRS_PER_BLOCK; ++i)
            if (ptrs[i] != 0) result.push_back(ptrs[i]);
    }
    return result;
}

// ── Public API ─────────────────────────────────────────────────────────────

bool InodeFS::create(const std::string& name, int sizeBytes) {
    if (directory_.count(name)) {
        std::cout << "  [Inode] ERROR: '" << name << "' already exists.\n";
        return false;
    }
    int blocks = (sizeBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocks == 0) blocks = 1;

    int jid = journal_.logIntent("CREATE(Inode)",
        "file=" + name + " blocks=" + std::to_string(blocks));

    int inodeId = allocateInode();
    if (inodeId < 0) {
        std::cout << "  [Inode] ERROR: no free i-nodes.\n";
        journal_.abort(jid);
        return false;
    }

    Inode& node = getInode(inodeId);
    node.fileSize = sizeBytes;
    node.inMemory = false;   // not open yet

    if (!allocateBlocksForInode(node, blocks)) {
        std::cout << "  [Inode] ERROR: disk full for '" << name << "'.\n";
        freeInode(inodeId);
        journal_.abort(jid);
        return false;
    }

    InodeDirEntry entry;
    entry.name    = name;
    entry.inodeId = inodeId;
    directory_[name] = entry;

    auto blks = blocksOf(node);
    std::cout << "  [Inode] Created '" << name << "': inode=" << inodeId
              << " blocks=[";
    for (int i = 0; i < (int)blks.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << blks[i];
    }
    std::cout << "] (inode NOT in memory until opened)\n";
    journal_.commit(jid);
    return true;
}

bool InodeFS::open(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [Inode] ERROR: '" << name << "' not found.\n";
        return false;
    }
    Inode& node = getInode(it->second.inodeId);
    // Handle broken symlink
    if (node.isSymlink) {
        if (!directory_.count(node.symlinkTarget)) {
            std::cout << "  [Inode] ERROR: symlink '" << name
                      << "' -> '" << node.symlinkTarget << "' is broken.\n";
            return false;
        }
        std::cout << "  [Inode] Symlink '" << name << "' -> '"
                  << node.symlinkTarget << "', following...\n";
        return open(node.symlinkTarget);
    }
    node.inMemory = true;
    it->second.isOpen = true;
    std::cout << "  [Inode] Opened '" << name << "': inode " << node.inodeId
              << " loaded into memory.\n";
    return true;
}

bool InodeFS::close(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end() || !it->second.isOpen) {
        std::cout << "  [Inode] ERROR: '" << name << "' not open.\n";
        return false;
    }
    Inode& node = getInode(it->second.inodeId);
    node.inMemory = false;
    it->second.isOpen = false;
    std::cout << "  [Inode] Closed '" << name << "': inode " << node.inodeId
              << " evicted from memory.\n";
    return true;
}

bool InodeFS::read(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [Inode] ERROR: '" << name << "' not found.\n";
        return false;
    }
    const Inode& node = getInode(it->second.inodeId);
    if (node.isSymlink) {
        std::cout << "  [Inode] Read symlink '" << name << "' -> '"
                  << node.symlinkTarget << "'\n";
        return read(node.symlinkTarget);
    }
    auto blks = blocksOf(node);
    int directCount   = 0, indirectCount = 0;
    for (int b : blks) {
        if (directCount < INODE_DIRECT_BLOCKS && node.direct[directCount] == b)
            ++directCount;
        else
            ++indirectCount;
    }
    std::cout << "  [Inode] Read '" << name << "': " << directCount
              << " direct block(s), " << indirectCount << " indirect block(s).\n";
    return true;
}

bool InodeFS::write(const std::string& name, int newSizeBytes) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [Inode] ERROR: '" << name << "' not found.\n";
        return false;
    }
    Inode& node   = getInode(it->second.inodeId);
    int newBlocks = (newSizeBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (newBlocks == 0) newBlocks = 1;

    int jid = journal_.logIntent("WRITE(Inode)",
        "file=" + name + " newSize=" + std::to_string(newSizeBytes));

    freeBlocksOfInode(node);
    if (!allocateBlocksForInode(node, newBlocks)) {
        std::cout << "  [Inode] ERROR: cannot resize '" << name << "'.\n";
        journal_.abort(jid);
        return false;
    }
    node.fileSize = newSizeBytes;
    std::cout << "  [Inode] Wrote '" << name << "': now " << newBlocks << " block(s).\n";
    journal_.commit(jid);
    return true;
}

bool InodeFS::remove(const std::string& name) {
    auto it = directory_.find(name);
    if (it == directory_.end()) {
        std::cout << "  [Inode] ERROR: '" << name << "' not found.\n";
        return false;
    }
    int inodeId = it->second.inodeId;
    Inode& node = getInode(inodeId);

    int jid = journal_.logIntent("DELETE(Inode)",
        "step1=remove_dir_entry step2=decrement_refcount step3=free_inode+blocks"
        " | file=" + name + " inode=" + std::to_string(inodeId)
        + " refCount=" + std::to_string(node.refCount));

    // Step 1: remove directory entry
    directory_.erase(it);

    // Step 2: decrement reference count
    --node.refCount;
    std::cout << "  [Inode] Removed dir entry '" << name << "' (inode "
              << inodeId << ", refCount now " << node.refCount << ").\n";

    // Step 3: free i-node + blocks only when refCount reaches zero
    if (node.refCount <= 0) {
        freeBlocksOfInode(node);
        freeInode(inodeId);
        std::cout << "  [Inode] refCount=0: freed inode " << inodeId
                  << " and its disk blocks.\n";
    } else {
        std::cout << "  [Inode] File still accessible via " << node.refCount
                  << " remaining hard link(s).\n";
    }

    journal_.commit(jid);
    return true;
}

bool InodeFS::hardLink(const std::string& existingName, const std::string& newName) {
    auto it = directory_.find(existingName);
    if (it == directory_.end()) {
        std::cout << "  [Inode] ERROR: hard link source '" << existingName << "' not found.\n";
        return false;
    }
    if (directory_.count(newName)) {
        std::cout << "  [Inode] ERROR: '" << newName << "' already exists.\n";
        return false;
    }
    int jid = journal_.logIntent("HARD_LINK",
        "source=" + existingName + " link=" + newName);

    int inodeId = it->second.inodeId;
    getInode(inodeId).refCount++;

    InodeDirEntry entry;
    entry.name    = newName;
    entry.inodeId = inodeId;
    directory_[newName] = entry;

    std::cout << "  [Inode] Hard link '" << newName << "' -> inode " << inodeId
              << " (refCount=" << getInode(inodeId).refCount << ").\n";
    journal_.commit(jid);
    return true;
}

bool InodeFS::softLink(const std::string& targetPath, const std::string& linkName) {
    if (directory_.count(linkName)) {
        std::cout << "  [Inode] ERROR: '" << linkName << "' already exists.\n";
        return false;
    }
    int jid = journal_.logIntent("SOFT_LINK",
        "target=" + targetPath + " link=" + linkName);

    int inodeId = allocateInode();
    if (inodeId < 0) {
        std::cout << "  [Inode] ERROR: no free i-nodes for symlink.\n";
        journal_.abort(jid);
        return false;
    }
    Inode& node       = getInode(inodeId);
    node.isSymlink    = true;
    node.symlinkTarget = targetPath;
    node.fileSize     = (int)targetPath.size();

    InodeDirEntry entry;
    entry.name    = linkName;
    entry.inodeId = inodeId;
    directory_[linkName] = entry;

    std::cout << "  [Inode] Soft link '" << linkName << "' -> '" << targetPath
              << "' (own inode=" << inodeId << ").\n";
    journal_.commit(jid);
    return true;
}

void InodeFS::printDirectory() const {
    std::cout << "\n  ── Inode Directory ─────────────────────────\n";
    std::cout << "  " << std::left
              << std::setw(16) << "Name"
              << std::setw(8)  << "Inode"
              << std::setw(8)  << "RefCnt"
              << std::setw(10) << "Size(B)"
              << std::setw(8)  << "InMem"
              << "Type\n";
    std::cout << "  " << std::string(60, '-') << '\n';
    for (const auto& [k, e] : directory_) {
        const Inode& node = getInode(e.inodeId);
        std::cout << "  " << std::left
                  << std::setw(16) << e.name
                  << std::setw(8)  << node.inodeId
                  << std::setw(8)  << node.refCount
                  << std::setw(10) << node.fileSize
                  << std::setw(8)  << (node.inMemory ? "Yes" : "No");
        if (node.isSymlink) std::cout << "symlink -> " << node.symlinkTarget;
        else                std::cout << "regular";
        std::cout << '\n';
    }
    std::cout << "  ────────────────────────────────────────────\n";
}

void InodeFS::printInodes() const {
    std::cout << "\n  ── Active I-nodes ──────────────────────────\n";
    for (const auto& node : inodes_) {
        if (node.inodeId == -1) continue;
        std::cout << "  Inode " << std::setw(3) << node.inodeId
                  << " | size=" << std::setw(6) << node.fileSize
                  << " | ref=" << node.refCount
                  << " | inMem=" << (node.inMemory ? "Y" : "N");
        if (node.isSymlink) {
            std::cout << " | SYMLINK->" << node.symlinkTarget;
        } else {
            std::cout << " | direct=[";
            bool first = true;
            for (int i = 0; i < INODE_DIRECT_BLOCKS; ++i) {
                if (node.direct[i] == NULL_BLOCK) break;
                if (!first) std::cout << ',';
                std::cout << node.direct[i];
                first = false;
            }
            std::cout << ']';
            if (node.indirect != NULL_BLOCK) std::cout << " indirect=" << node.indirect;
        }
        std::cout << '\n';
    }
    std::cout << "  ────────────────────────────────────────────\n";
}
