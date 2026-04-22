#include "directory.h"
#include <sstream>
#include <algorithm>

DirectorySystem::DirectorySystem() {
    root_ = std::make_shared<DirNode>("/", true);
}

std::vector<std::string> DirectorySystem::splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::istringstream ss(path);
    std::string token;
    while (std::getline(ss, token, '/'))
        if (!token.empty()) parts.push_back(token);
    return parts;
}

std::shared_ptr<DirNode> DirectorySystem::resolve(const std::string& path) const {
    if (path == "/") return root_;
    auto parts = splitPath(path);
    auto cur   = root_;
    for (const auto& p : parts) {
        auto it = cur->children.find(p);
        if (it == cur->children.end()) return nullptr;
        cur = it->second;
    }
    return cur;
}

std::shared_ptr<DirNode> DirectorySystem::resolveParent(
        const std::string& path, std::string& leafName) const {
    auto parts = splitPath(path);
    if (parts.empty()) return nullptr;
    leafName = parts.back();
    parts.pop_back();

    auto cur = root_;
    for (const auto& p : parts) {
        auto it = cur->children.find(p);
        if (it == cur->children.end() || !it->second->isDir) return nullptr;
        cur = it->second;
    }
    return cur;
}

bool DirectorySystem::mkdir(const std::string& path) {
    std::string leaf;
    auto parent = resolveParent(path, leaf);
    if (!parent) {
        std::cout << "  [Dir] ERROR: parent path not found for '" << path << "'.\n";
        return false;
    }
    if (parent->children.count(leaf)) {
        std::cout << "  [Dir] ERROR: '" << path << "' already exists.\n";
        return false;
    }
    parent->children[leaf] = std::make_shared<DirNode>(leaf, true);
    std::cout << "  [Dir] Created directory: " << path << '\n';
    return true;
}

bool DirectorySystem::mkfile(const std::string& path) {
    std::string leaf;
    auto parent = resolveParent(path, leaf);
    if (!parent) {
        std::cout << "  [Dir] ERROR: parent directory not found for '" << path << "'.\n";
        return false;
    }
    if (parent->children.count(leaf)) {
        std::cout << "  [Dir] ERROR: '" << path << "' already exists.\n";
        return false;
    }
    parent->children[leaf] = std::make_shared<DirNode>(leaf, false);
    std::cout << "  [Dir] Created file entry: " << path << '\n';
    return true;
}

bool DirectorySystem::remove(const std::string& path) {
    std::string leaf;
    auto parent = resolveParent(path, leaf);
    if (!parent || !parent->children.count(leaf)) {
        std::cout << "  [Dir] ERROR: '" << path << "' not found.\n";
        return false;
    }
    parent->children.erase(leaf);
    std::cout << "  [Dir] Removed: " << path << '\n';
    return true;
}

bool DirectorySystem::exists(const std::string& path) const {
    return resolve(path) != nullptr;
}

void DirectorySystem::printTree(const std::shared_ptr<DirNode>& node,
                                const std::string& prefix, bool last) const {
    std::cout << prefix << (last ? "└── " : "├── ")
              << node->name << (node->isDir ? "/" : "") << '\n';
    if (!node->isDir) return;
    std::string childPrefix = prefix + (last ? "    " : "│   ");
    auto it = node->children.begin();
    while (it != node->children.end()) {
        auto next = std::next(it);
        printTree(it->second, childPrefix, next == node->children.end());
        it = next;
    }
}

void DirectorySystem::print(const std::string& path) const {
    auto node = resolve(path);
    if (!node) { std::cout << "  [Dir] '" << path << "' not found.\n"; return; }
    std::cout << "\n  ── Directory Tree: " << path << " ─────────────\n";
    std::cout << "  " << node->name << (node->isDir ? "/" : "") << '\n';
    std::string childPrefix = "  ";
    auto it = node->children.begin();
    while (it != node->children.end()) {
        auto next = std::next(it);
        printTree(it->second, childPrefix, next == node->children.end());
        it = next;
    }
    std::cout << "  ────────────────────────────────────────────\n";
}
