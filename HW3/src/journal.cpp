#include "journal.h"
#include <iomanip>
#include <stdexcept>

int Journal::logIntent(const std::string& operation, const std::string& details) {
    int id = nextId_++;
    entries_.emplace_back(id, operation, details);
    std::cout << "  [JOURNAL] #" << id << " PENDING  | " << operation;
    if (!details.empty()) std::cout << " | " << details;
    std::cout << '\n';
    return id;
}

void Journal::commit(int entryId) {
    for (auto& e : entries_) {
        if (e.id == entryId) {
            e.state = JournalEntry::State::COMMITTED;
            std::cout << "  [JOURNAL] #" << entryId << " COMMITTED\n";
            return;
        }
    }
    throw std::runtime_error("Journal: unknown entry id " + std::to_string(entryId));
}

void Journal::abort(int entryId) {
    for (auto& e : entries_) {
        if (e.id == entryId) {
            e.state = JournalEntry::State::ABORTED;
            std::cout << "  [JOURNAL] #" << entryId << " ABORTED\n";
            return;
        }
    }
    throw std::runtime_error("Journal: unknown entry id " + std::to_string(entryId));
}

void Journal::printLog(bool pendingOnly) const {
    std::cout << "\n  ── Journal Log ──────────────────────────────\n";
    for (const auto& e : entries_) {
        if (pendingOnly && e.state != JournalEntry::State::PENDING) continue;
        const char* stateStr =
            (e.state == JournalEntry::State::COMMITTED) ? "COMMITTED" :
            (e.state == JournalEntry::State::ABORTED)   ? "ABORTED  " : "PENDING  ";
        std::cout << "  #" << std::setw(3) << e.id << " [" << stateStr << "] "
                  << e.operation;
        if (!e.details.empty()) std::cout << " | " << e.details;
        std::cout << '\n';
    }
    std::cout << "  ─────────────────────────────────────────────\n";
}
