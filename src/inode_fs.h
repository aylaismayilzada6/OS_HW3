#pragma once
#include "disk.h"
#include "journal.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

// ─────────────────────────────────────────────
//  I-node parameters
// ─────────────────────────────────────────────
constexpr int INODE_DIRECT_BLOCKS  = 8;   // direct block pointers per i-node
constexpr int PTRS_PER_BLOCK       = BLOCK_SIZE / sizeof(int);  // 128 with 512B blocks

// ─────────────────────────────────────────────
//  I-node structure
// ─────────────────────────────────────────────
struct Inode {
    int  inodeId   = -1;
    int  fileSize  = 0;
    int  refCount  = 0;          // hard-link reference count
    bool inMemory  = false;      // loaded only when file is open
    bool isSymlink = false;      // soft link flag
    std::string symlinkTarget;   // if isSymlink, path to target

    // Block pointers
    int direct[INODE_DIRECT_BLOCKS] = {};   // direct block addresses
    int indirect = NULL_BLOCK;              // block holding more block pointers

    Inode() { for (auto& d : direct) d = NULL_BLOCK; }

    // Maximum file size with current structure:
    //   INODE_DIRECT_BLOCKS * BLOCK_SIZE  +  PTRS_PER_BLOCK * BLOCK_SIZE
    static int maxFileSize() {
        return (INODE_DIRECT_BLOCKS + (int)PTRS_PER_BLOCK) * BLOCK_SIZE;
    }
};

// ─────────────────────────────────────────────
//  Directory entry for i-node FS
// ─────────────────────────────────────────────
struct InodeDirEntry {
    std::string name;
    int         inodeId = -1;   // points into inode table
    bool        isOpen  = false;
};

// ─────────────────────────────────────────────
//  I-node File System
// ─────────────────────────────────────────────
class InodeFS {
public:
    explicit InodeFS(Disk& disk, Journal& journal);

    bool create   (const std::string& name, int sizeBytes);
    bool open     (const std::string& name);
    bool close    (const std::string& name);
    bool read     (const std::string& name);
    bool write    (const std::string& name, int newSizeBytes);
    bool remove   (const std::string& name);

    // Links
    bool hardLink (const std::string& existingName, const std::string& newName);
    bool softLink (const std::string& targetPath,   const std::string& linkName);

    void printDirectory () const;
    void printInodes    () const;

private:
    Disk&    disk_;
    Journal& journal_;

    std::vector<Inode>                            inodes_;       // inode table
    std::unordered_map<std::string, InodeDirEntry> directory_;   // name -> inode id

    int  allocateInode ();
    void freeInode     (int id);
    Inode& getInode    (int id);
    const Inode& getInode(int id) const;

    bool allocateBlocksForInode(Inode& node, int totalBlocks);
    void freeBlocksOfInode     (Inode& node);
    std::vector<int> blocksOf  (const Inode& node) const;
};
