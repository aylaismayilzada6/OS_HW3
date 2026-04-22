#include <iostream>
#include <string>
#include <iomanip>

#include "disk.h"
#include "journal.h"
#include "contiguous_fs.h"
#include "fat_fs.h"
#include "inode_fs.h"
#include "directory.h"

using namespace std;

// ─────────────────────────────────────────────
// UI helpers
// ─────────────────────────────────────────────
static void section(const string& title) {
    cout << "\n╔══════════════════════════════════════════╗\n";
    cout << "║ " << left << setw(40) << title << " ║\n";
    cout << "╚══════════════════════════════════════════╝\n";
}

static void sub(const string& msg) {
    cout << "\n  ── " << msg << " ──\n";
}

// ─────────────────────────────────────────────
// CONTIGUOUS DEMO
// ─────────────────────────────────────────────
static void demoContiguous(Disk& disk, Journal& journal) {
    section("1. Contiguous Allocation");
    ContiguousFS fs(disk, journal);

    fs.create("alpha.txt", 1024);
    fs.create("beta.bin", 3072);
    fs.create("gamma.log", 512);

    fs.printDirectory();
    disk.printBitmap();

    sub("Read / Open / Close");
    fs.open("alpha.txt");
    fs.read("alpha.txt");
    fs.close("alpha.txt");

    sub("Delete beta (fragmentation)");
    fs.remove("beta.bin");
    fs.printFragmentation();
    disk.printBitmap();

    sub("Grow file after fragmentation");
    fs.write("gamma.log", 2048);

    fs.printDirectory();
    disk.printBitmap();
}

// ─────────────────────────────────────────────
// FAT DEMO
// ─────────────────────────────────────────────
static void demoFAT(Disk& disk, Journal& journal) {
    section("2. FAT Allocation");
    FATFS fs(disk, journal);

    fs.create("readme.txt", 512);
    fs.create("image.png", 4096);
    fs.create("data.csv", 1536);

    fs.printDirectory();
    fs.printFAT();
    disk.printBitmap();

    sub("Read file");
    fs.open("image.png");
    fs.read("image.png");
    fs.close("image.png");

    cout << "\n  FAT memory: " << fs.fatMemoryBytes()
         << " bytes\n";

    sub("Delete file");
    fs.remove("image.png");

    fs.printFAT();
    disk.printBitmap();

    sub("Resize file");
    fs.write("data.csv", 5120);
    fs.printDirectory();
}

// ─────────────────────────────────────────────
// INODE DEMO
// ─────────────────────────────────────────────
static void demoInode(Disk& disk, Journal& journal) {
    section("3. I-node Allocation");
    InodeFS fs(disk, journal);

    fs.create("kernel.bin", 4096);
    fs.create("notes.txt", 512);

    int bigSize = (INODE_DIRECT_BLOCKS + 4) * BLOCK_SIZE;
    fs.create("bigfile.dat", bigSize);

    fs.printDirectory();
    fs.printInodes();
    disk.printBitmap();

    sub("Open / Read / Close");
    fs.open("kernel.bin");
    fs.read("kernel.bin");
    fs.close("kernel.bin");

    sub("Hard link");
    fs.hardLink("kernel.bin", "kernel.bak");
    fs.remove("kernel.bin");
    fs.remove("kernel.bak");

    sub("Soft link");
    fs.softLink("notes.txt", "notes_link");
    fs.open("notes_link");

    fs.remove("notes.txt");
    fs.open("notes_link"); // broken link

    fs.printDirectory();
    fs.printInodes();
}

// ─────────────────────────────────────────────
// DIRECTORY DEMO
// ─────────────────────────────────────────────
static void demoDirectory() {
    section("4. Directory System");
    DirectorySystem dir;

    dir.mkdir("/home");
    dir.mkdir("/var");

    dir.mkfile("/home/file.txt");
    dir.mkfile("/var/log/syslog");

    dir.print("/");

    sub("Remove file");
    dir.remove("/home/file.txt");
    dir.print("/home");
}

// ─────────────────────────────────────────────
// JOURNAL DEMO
// ─────────────────────────────────────────────
static void demoJournal() {
    section("5. Journal");

    Disk disk;
    Journal journal;
    InodeFS fs(disk, journal);

    fs.create("/home/note.txt", 1024);
    fs.create("/var/log/syslog", 512);

    journal.printLog();
}

// ─────────────────────────────────────────────
// MAIN
// ─────────────────────────────────────────────
int main(int argc, char* argv[]) {

    if (argc > 1 && string(argv[1]) == "--help") {
        cout <<
        "Usage: ./fssim\n"
        "Simulates:\n"
        "  - Contiguous allocation\n"
        "  - FAT file system\n"
        "  - I-node system\n"
        "  - Directory system\n"
        "  - Journaling\n";
        return 0;
    }

    cout << "\n╔══════════════════════════════════════════╗\n";
    cout << "║ File System Allocation Simulator         ║\n";
    cout << "╚══════════════════════════════════════════╝\n";

    cout << "Disk: " << DISK_BLOCKS << " blocks × "
         << BLOCK_SIZE << " bytes\n";

    Disk disk;
    Journal journal;

    demoContiguous(disk, journal);
    demoFAT(disk, journal);
    demoInode(disk, journal);
    demoDirectory();
    demoJournal();

    cout << "\nSimulation complete.\n";
    return 0;
}