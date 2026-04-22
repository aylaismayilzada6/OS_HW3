#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <ctime>

// ─────────────────────────────────────────────
//  Journal entry: one pending file-system operation
// ─────────────────────────────────────────────
struct JournalEntry {
    enum class State { PENDING, COMMITTED, ABORTED };

    int         id;
    std::string operation;      // human-readable description
    std::string details;        // extra context (paths, block lists …)
    State       state = State::PENDING;
    std::time_t timestamp;

    JournalEntry(int id_, const std::string& op, const std::string& det)
        : id(id_), operation(op), details(det), timestamp(std::time(nullptr)) {}
};

// ─────────────────────────────────────────────
//  Journal: write-ahead log
// ─────────────────────────────────────────────
class Journal {
public:
    // Log intent BEFORE executing. Returns the entry id.
    int  logIntent  (const std::string& operation, const std::string& details = "");
    void commit     (int entryId);
    void abort      (int entryId);

    void printLog   (bool pendingOnly = false) const;
    const std::vector<JournalEntry>& entries() const { return entries_; }

private:
    std::vector<JournalEntry> entries_;
    int nextId_ = 1;
};
