#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <iostream>
#include <iomanip>

// ─────────────────────────────────────────────
//  Node in the directory tree (dir or file)
// ─────────────────────────────────────────────
struct DirNode {
    std::string name;
    bool        isDir = false;

    // Children (only meaningful for directories)
    std::unordered_map<std::string, std::shared_ptr<DirNode>> children;

    explicit DirNode(const std::string& n, bool dir = false)
        : name(n), isDir(dir) {}
};

// ─────────────────────────────────────────────
//  Hierarchical Directory System
//  (independent of the per-FS allocation logic;
//   sits on top as a path → name mapping layer)
// ─────────────────────────────────────────────
class DirectorySystem {
public:
    DirectorySystem();

    bool mkdir  (const std::string& path);
    bool mkfile (const std::string& path);
    bool remove (const std::string& path);
    bool exists (const std::string& path) const;

    void print  (const std::string& path = "/") const;

private:
    std::shared_ptr<DirNode> root_;

    // Returns nullptr if not found
    std::shared_ptr<DirNode> resolve(const std::string& path) const;
    // Returns the parent node and the leaf name
    std::shared_ptr<DirNode> resolveParent(const std::string& path,
                                           std::string& leafName) const;

    static std::vector<std::string> splitPath(const std::string& path);
    void printTree(const std::shared_ptr<DirNode>& node,
                   const std::string& prefix, bool last) const;
};
