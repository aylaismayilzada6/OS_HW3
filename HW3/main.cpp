/*
 * ============================================================
 *  File System Allocation Simulator
 *  Assignment 3 — Operating Systems
 * ============================================================
 *
 *  Implements in ONE file:
 *    1. Contiguous Allocation
 *    2. FAT
 *    3. I-node Allocation  (with direct + 1 indirect pointer)
 *    4. Hard links & Soft links
 *    5. Simple hierarchical directory  
 *    6. Logging Journal
 *
 *  Compile:  g++ -std=c++17 -o fssim main.cpp
 *  Run:      ./fssim
 * ============================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include <sstream>
using namespace std;



// ============================================================
//  DISK SETTINGS
// ============================================================
const int TOTAL_BLOCKS = 64;   // number of blocks on our simulated disk
const int BLOCK_SIZE   = 512;  // bytes per block 
const int NONE         = -1;   // "null" block pointer

// ============================================================
//  FREE-SPACE BITMAP
//  true  = block is free
//  false = block is in use
// ============================================================
bool freeMap[TOTAL_BLOCKS];

void initDisk() {
    for (int i = 0; i < TOTAL_BLOCKS; i++)
        freeMap[i] = true;
}

// Allocate the first free block found; returns NONE if disk is full
int allocOneBlock() {
    for (int i = 0; i < TOTAL_BLOCKS; i++)
        if (freeMap[i]) { freeMap[i] = false; return i; }
    return NONE;
}

// Allocate 'n' scattered free blocks; returns false if not enough space
bool allocBlocks(int n, vector<int>& out) {
    out.clear();
    for (int i = 0; i < TOTAL_BLOCKS && (int)out.size() < n; i++)
        if (freeMap[i]) { freeMap[i] = false; out.push_back(i); }
    return (int)out.size() == n;
}

// Allocate 'n' CONTIGUOUS blocks; returns false if no run is big enough
bool allocContiguous(int n, vector<int>& out) {
    out.clear();
    for (int start = 0; start <= TOTAL_BLOCKS - n; start++) {
        // Check if all n blocks starting at 'start' are free
        bool allFree = true;
        for (int j = 0; j < n; j++) {
            if (!freeMap[start + j]) { allFree = false; break; }
        }
        if (allFree) {
            for (int j = 0; j < n; j++) {
                freeMap[start + j] = false;
                out.push_back(start + j);
            }
            return true;
        }
    }
    return false;
}

void freeBlock(int b)               { if (b != NONE) freeMap[b] = true; }
void freeBlocks(vector<int>& list)  { for (int b : list) freeBlock(b); list.clear(); }

int countFree() {
    int n = 0; for (bool f : freeMap) if (f) n++; return n;
}

// Print a visual bitmap  '.' = free  'X' = used
void printDiskBitmap() {
    cout << "\n  Disk bitmap (" << countFree() << "/" << TOTAL_BLOCKS << " free)\n  ";
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (i && i % 16 == 0) cout << "\n  ";
        cout << (freeMap[i] ? '.' : 'X');
    }
    cout << "\n";
}


// ============================================================
//  JOURNAL  (write-ahead log)
// ============================================================
struct JEntry {
    int    id;
    string description;
    bool   committed;
};

vector<JEntry> journal;
int nextJID = 1;

int  jBegin(const string& desc) {
    journal.push_back({nextJID, desc, false});
    cout << "  [JOURNAL] #" << nextJID << " PENDING  — " << desc << "\n";
    return nextJID++;
}
void jCommit(int id) {
    for (auto& e : journal) if (e.id == id) { e.committed = true; break; }
    cout << "  [JOURNAL] #" << id << " COMMITTED\n";
}
void printJournal() {
    cout << "\n  === Journal Log ===\n";
    for (auto& e : journal)
        cout << "  #" << e.id << " [" << (e.committed ? "COMMITTED" : "PENDING  ")
             << "] " << e.description << "\n";
    cout << "  ===================\n";
}


// ============================================================
//  1. CONTIGUOUS ALLOCATION
// ============================================================
struct ContigFile {
    int  start  = NONE;
    int  blocks = 0;
    int  size   = 0;       // bytes
    bool open   = false;
};

unordered_map<string, ContigFile> contigDir;

void contig_create(const string& name, int sizeBytes) {
    int n = max(1, (sizeBytes + BLOCK_SIZE - 1) / BLOCK_SIZE);
    int jid = jBegin("CREATE(Contiguous) file=" + name + " blocks=" + to_string(n));
    vector<int> blks;
    if (contigDir.count(name)) { cout << "  Already exists.\n"; return; }
    if (!allocContiguous(n, blks)) { cout << "  No contiguous space!\n"; return; }
    contigDir[name] = {blks[0], n, sizeBytes, false};
    cout << "  [Contig] Created '" << name << "' blocks ["
         << blks[0] << ".." << blks[0]+n-1 << "]\n";
    jCommit(jid);
}

void contig_delete(const string& name) {
    if (!contigDir.count(name)) { cout << "  Not found.\n"; return; }
    auto& f = contigDir[name];
    int jid = jBegin("DELETE(Contiguous) file=" + name
                     + " free blocks [" + to_string(f.start)
                     + ".." + to_string(f.start+f.blocks-1) + "]");
    for (int i = f.start; i < f.start + f.blocks; i++) freeBlock(i);
    contigDir.erase(name);
    cout << "  [Contig] Deleted '" << name << "'\n";
    jCommit(jid);
}

void contig_open (const string& name) {
    if (!contigDir.count(name)) { cout << "  Not found.\n"; return; }
    contigDir[name].open = true;
    cout << "  [Contig] Opened '" << name << "' (single seek to block "
         << contigDir[name].start << ")\n";
}
void contig_close(const string& name) {
    if (!contigDir.count(name)) { cout << "  Not found.\n"; return; }
    contigDir[name].open = false;
    cout << "  [Contig] Closed '" << name << "'\n";
}
void contig_read (const string& name) {
    if (!contigDir.count(name)) { cout << "  Not found.\n"; return; }
    auto& f = contigDir[name];
    cout << "  [Contig] Read '" << name << "': seek to block " << f.start
         << ", read " << f.blocks << " block(s) sequentially — excellent performance.\n";
}
void contig_write(const string& name, int newSize) {
    if (!contigDir.count(name)) { cout << "  Not found.\n"; return; }
    auto& f = contigDir[name];
    int n = max(1, (newSize + BLOCK_SIZE - 1) / BLOCK_SIZE);
    int jid = jBegin("WRITE(Contiguous) file=" + name + " newBlocks=" + to_string(n));
    if (n == f.blocks) { f.size = newSize; cout << "  [Contig] Updated in place.\n"; jCommit(jid); return; }
    // Must reallocate contiguously
    for (int i = f.start; i < f.start + f.blocks; i++) freeBlock(i);
    vector<int> blks;
    if (!allocContiguous(n, blks)) {
        cout << "  [Contig] Cannot grow — no contiguous space!\n"; return;
    }
    f.start = blks[0]; f.blocks = n; f.size = newSize;
    cout << "  [Contig] Reallocated '" << name << "' to blocks ["
         << f.start << ".." << f.start+n-1 << "]\n";
    jCommit(jid);
}

void contig_showDir() {
    cout << "\n  --- Contiguous Directory ---\n";
    cout << "  " << left << setw(14)<<"Name" << setw(7)<<"Start" << setw(8)<<"Blocks" << "Size\n";
    for (auto& [k,f] : contigDir)
        cout << "  " << left << setw(14)<<k << setw(7)<<f.start << setw(8)<<f.blocks << f.size << "B\n";
}

void contig_showFrag() {
    int totalFree = 0, maxRun = 0, currentRun = 0;
    
    // Count free blocks and find longest contiguous run
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (freeMap[i]) {
            totalFree++;
            currentRun++;
            maxRun = max(maxRun, currentRun);
        } else {
            currentRun = 0;
        }
    }
    
    double frag = (totalFree > 0 && maxRun < totalFree)
                  ? (1.0 - (double)maxRun / totalFree) * 100.0 : 0.0;
    
    cout << "  [Contig] " << totalFree << " free blocks, largest run=" << maxRun
         << ". Fragmentation=" << fixed << setprecision(1) << frag << "%\n";
}


// ============================================================
//  2. FAT  (File Allocation Table)
//     fat[i] = next block in chain, or NONE (end of chain)
// ============================================================
const int FAT_EOC = -2;   // End Of Chain

int fat[TOTAL_BLOCKS];    // the in-memory FAT table

void initFAT() { for (int i = 0; i < TOTAL_BLOCKS; i++) fat[i] = NONE; }

struct FATFile {
    int  firstBlock = NONE;
    int  size       = 0;
    bool open       = false;
};

unordered_map<string, FATFile> fatDir;

// Build a linked chain of n blocks in the FAT; returns first block
int fatBuildChain(int n) {
    vector<int> blks;
    if (!allocBlocks(n, blks)) return NONE;
    for (int i = 0; i < n-1; i++) fat[blks[i]] = blks[i+1];
    fat[blks[n-1]] = FAT_EOC;
    return blks[0];
}

// Follow chain starting at 'first', collect all blocks
vector<int> fatChain(int first) {
    vector<int> chain;
    for (int cur = first; cur != NONE && cur != FAT_EOC; cur = fat[cur]) {
        chain.push_back(cur);
    }
    return chain;
}

// Free a chain
void fatFreeChain(int first) {
    for (int cur = first; cur != NONE && cur != FAT_EOC; ) {
        int next = fat[cur];
        fat[cur] = NONE;
        freeBlock(cur);
        cur = next;
    }
}

void fat_create(const string& name, int sizeBytes) {
    int n = max(1, (sizeBytes + BLOCK_SIZE - 1) / BLOCK_SIZE);
    int jid = jBegin("CREATE(FAT) file=" + name + " blocks=" + to_string(n));
    if (fatDir.count(name)) { cout << "  Already exists.\n"; return; }
    int first = fatBuildChain(n);
    if (first == NONE) { cout << "  Disk full!\n"; return; }
    fatDir[name] = {first, sizeBytes, false};
    auto chain = fatChain(first);
    cout << "  [FAT] Created '" << name << "' chain: ";
    for (int i = 0; i < (int)chain.size(); i++) { if(i) cout<<"->"; cout<<chain[i]; }
    cout << "->EOC\n";
    jCommit(jid);
}

void fat_delete(const string& name) {
    if (!fatDir.count(name)) { cout << "  Not found.\n"; return; }
    int jid = jBegin("DELETE(FAT) step1=remove_dir step2=free_FAT_chain file=" + name);
    fatFreeChain(fatDir[name].firstBlock);
    fatDir.erase(name);
    cout << "  [FAT] Deleted '" << name << "'\n";
    jCommit(jid);
}

void fat_open (const string& name) {
    if (!fatDir.count(name)) { cout << "  Not found.\n"; return; }
    fatDir[name].open = true;
    cout << "  [FAT] Opened '" << name << "' (FAT already in memory — "
         << TOTAL_BLOCKS * (int)sizeof(int) << " bytes overhead always)\n";
}
void fat_close(const string& name) {
    if (!fatDir.count(name)) { cout << "  Not found.\n"; return; }
    fatDir[name].open = false;
    cout << "  [FAT] Closed '" << name << "' (FAT stays in memory)\n";
}
void fat_read (const string& name) {
    if (!fatDir.count(name)) { cout << "  Not found.\n"; return; }
    auto chain = fatChain(fatDir[name].firstBlock);
    cout << "  [FAT] Read '" << name << "': followed " << chain.size()
         << " FAT pointer(s) — one seek per block in worst case.\n";
}
void fat_write (const string& name, int newSize) {
    if (!fatDir.count(name)) { cout << "  Not found.\n"; return; }
    int n = max(1, (newSize + BLOCK_SIZE - 1) / BLOCK_SIZE);
    int jid = jBegin("WRITE(FAT) file=" + name + " newBlocks=" + to_string(n));
    fatFreeChain(fatDir[name].firstBlock);
    int first = fatBuildChain(n);
    if (first == NONE) { cout << "  Disk full!\n"; return; }
    fatDir[name].firstBlock = first;
    fatDir[name].size = newSize;
    cout << "  [FAT] Resized '" << name << "' to " << n << " block(s).\n";
    jCommit(jid);
}

void fat_showDir() {
    cout << "\n  --- FAT Directory ---\n";
    cout << "  " << left << setw(14)<<"Name" << setw(11)<<"1stBlock" << setw(8)<<"Size" << "Chain\n";
    for (auto& [k,f] : fatDir) {
        auto chain = fatChain(f.firstBlock);
        cout << "  " << left << setw(14)<<k << setw(11)<<f.firstBlock << setw(8)<<to_string(f.size)+"B";
        for (int i=0;i<(int)chain.size();i++){if(i) cout<<"->";cout<<chain[i];} cout<<"->EOC\n";
    }
    cout << "  FAT table memory: " << TOTAL_BLOCKS * (int)sizeof(int) << " bytes (always loaded)\n";
}

void fat_showTable() {
    cout << "\n  --- FAT Table (used entries only) ---\n";
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (fat[i] == NONE) continue;
        cout << "  fat[" << setw(2) << i << "] -> " << (fat[i]==FAT_EOC?"EOC":to_string(fat[i])) << "\n";
    }
}


// ============================================================
//  3. I-NODE ALLOCATION
// ============================================================
const int DIRECT_PTRS = 4;   // direct block pointers per inode

struct Inode {
    int  id        = NONE;
    int  size      = 0;
    int  refCount  = 0;
    bool inMemory  = false;   // only true while file is open
    bool isSymlink = false;
    string symlinkTarget;
    int  direct[DIRECT_PTRS] = {NONE,NONE,NONE,NONE};
    int  indirect  = NONE;    // points to a block containing more block numbers
};

const int MAX_INODES = 16;
Inode inodeTable[MAX_INODES];

void initInodes() { for (auto& n : inodeTable) n.id = NONE; }

int allocInode() {
    for (int i = 0; i < MAX_INODES; i++)
        if (inodeTable[i].id == NONE) { inodeTable[i] = Inode(); inodeTable[i].id = i; inodeTable[i].refCount=1; return i; }
    return NONE;
}
void freeInode(int id) { if (id>=0&&id<MAX_INODES) inodeTable[id].id = NONE; }

struct InodeFile { int inodeId = NONE; bool open = false; };
unordered_map<string,InodeFile> inodeDir;

// Collect all data blocks from an inode (direct + indirect)
vector<int> inodeBlocks(const Inode& nd) {
    vector<int> blks;
    for (int i = 0; i < DIRECT_PTRS; i++)
        if (nd.direct[i] != NONE) blks.push_back(nd.direct[i]);
    if (nd.indirect != NONE) {
        // simulate reading block numbers stored in that block
        // we store them as a sub-array in a small side-table,reuse freeMap index as proxy storage
        // In a real OS this block's bytes would hold int[] pointers.
        // For the simulator we track them separately in inodeIndirectData.
    }
    return blks;
}

// Side-table: inodeIndirectData[inodeId] = list of indirect block numbers
vector<int> inodeIndirectData[MAX_INODES];

void inodeFreeBlocks(Inode& nd) {
    for (int i = 0; i < DIRECT_PTRS; i++) { freeBlock(nd.direct[i]); nd.direct[i]=NONE; }
    if (nd.indirect != NONE) {
        for (int b : inodeIndirectData[nd.id]) freeBlock(b);
        inodeIndirectData[nd.id].clear();
        freeBlock(nd.indirect);
        nd.indirect = NONE;
    }
}

bool inodeAllocBlocks(Inode& nd, int total) {
    int directNeed   = min(total, DIRECT_PTRS);
    int indirectNeed = total - directNeed;
    const int PTRS_PER_BLK = BLOCK_SIZE / (int)sizeof(int); // 128
    if (indirectNeed > PTRS_PER_BLK) { cout << "  File too large!\n"; return false; }

    vector<int> dblks;
    if (!allocBlocks(directNeed, dblks)) return false;
    for (int i = 0; i < directNeed; i++) nd.direct[i] = dblks[i];

    if (indirectNeed > 0) {
        int indBlk = allocOneBlock();
        if (indBlk == NONE) return false;
        nd.indirect = indBlk;
        vector<int> iblks;
        if (!allocBlocks(indirectNeed, iblks)) return false;
        inodeIndirectData[nd.id] = iblks;
    }
    return true;
}

void inode_create(const string& name, int sizeBytes) {
    int n = max(1, (sizeBytes + BLOCK_SIZE - 1) / BLOCK_SIZE);
    int jid = jBegin("CREATE(Inode) file=" + name + " blocks=" + to_string(n));
    if (inodeDir.count(name)) { cout << "  Already exists.\n"; return; }
    int id = allocInode();
    if (id == NONE) { cout << "  No free inodes!\n"; return; }
    Inode& nd = inodeTable[id];
    nd.size = sizeBytes;
    if (!inodeAllocBlocks(nd, n)) { freeInode(id); cout << "  Disk full!\n"; return; }
    inodeDir[name] = {id, false};
    
    // Print direct blocks
    cout << "  [Inode] Created '" << name << "' inode=" << id << " direct=[";
    bool first = true;
    for (int i = 0; i < DIRECT_PTRS; i++) {
        if (nd.direct[i] != NONE) {
            if (!first) cout << ",";
            cout << nd.direct[i];
            first = false;
        }
    }
    cout << "]";
    
    // Print indirect blocks
    if (nd.indirect != NONE) {
        cout << " indirect=" << nd.indirect << "->[";
        first = true;
        for (int b : inodeIndirectData[id]) {
            if (!first) cout << ",";
            cout << b;
            first = false;
        }
        cout << "]";
    }
    cout << "  (inode not in memory yet)\n";
    jCommit(jid);
}

void inode_open(const string& name) {
    if (!inodeDir.count(name)) { cout << "  Not found.\n"; return; }
    Inode& nd = inodeTable[inodeDir[name].inodeId];
    if (nd.isSymlink) {
        cout << "  [Inode] Symlink '" << name << "' -> '" << nd.symlinkTarget << "'\n";
        if (!inodeDir.count(nd.symlinkTarget)) { cout << "  Broken symlink!\n"; return; }
        inode_open(nd.symlinkTarget); return;
    }
    nd.inMemory = true; inodeDir[name].open = true;
    cout << "  [Inode] Opened '" << name << "': inode " << nd.id << " loaded into memory.\n";
}

void inode_close(const string& name) {
    if (!inodeDir.count(name)||!inodeDir[name].open) { cout << "  Not open.\n"; return; }
    Inode& nd = inodeTable[inodeDir[name].inodeId];
    nd.inMemory = false; inodeDir[name].open = false;
    cout << "  [Inode] Closed '" << name << "': inode " << nd.id << " evicted from memory.\n";
}

void inode_read(const string& name) {
    if (!inodeDir.count(name)) { cout << "  Not found.\n"; return; }
    Inode& nd = inodeTable[inodeDir[name].inodeId];
    int directCnt=0; for(int d:nd.direct) if(d!=NONE) directCnt++;
    int indirectCnt = (int)inodeIndirectData[nd.id].size();
    cout << "  [Inode] Read '" << name << "': " << directCnt
         << " direct block(s)" << (indirectCnt?" + "+to_string(indirectCnt)+" indirect":"") << ".\n";
}

void inode_write(const string& name, int newSize) {
    if (!inodeDir.count(name)) { cout << "  Not found.\n"; return; }
    Inode& nd = inodeTable[inodeDir[name].inodeId];
    int n = max(1,(newSize+BLOCK_SIZE-1)/BLOCK_SIZE);
    int jid = jBegin("WRITE(Inode) file="+name+" newBlocks="+to_string(n));
    inodeFreeBlocks(nd);
    if (!inodeAllocBlocks(nd,n)) { cout << "  Disk full!\n"; return; }
    nd.size = newSize;
    cout << "  [Inode] Wrote '" << name << "': now " << n << " block(s).\n";
    jCommit(jid);
}

void inode_delete(const string& name) {
    if (!inodeDir.count(name)) { cout << "  Not found.\n"; return; }
    int id = inodeDir[name].inodeId;
    Inode& nd = inodeTable[id];
    int jid = jBegin("DELETE(Inode) step1=remove_dir step2=refCount-- step3=free_if_zero | file="+name);
    inodeDir.erase(name);
    nd.refCount--;
    cout << "  [Inode] Removed dir entry '" << name << "' refCount now " << nd.refCount << ".\n";
    if (nd.refCount <= 0) {
        inodeFreeBlocks(nd); freeInode(id);
        cout << "  [Inode] refCount=0 -> freed inode " << id << " and its blocks.\n";
    } else {
        cout << "  [Inode] Data still alive (" << nd.refCount << " hard link(s) remain).\n";
    }
    jCommit(jid);
}

void inode_hardLink(const string& existing, const string& linkName) {
    if (!inodeDir.count(existing)) { cout << "  Source not found.\n"; return; }
    if (inodeDir.count(linkName))  { cout << "  Link name already exists.\n"; return; }
    int id = inodeDir[existing].inodeId;
    inodeTable[id].refCount++;
    inodeDir[linkName] = {id, false};
    cout << "  [Inode] Hard link '" << linkName << "' -> inode " << id
         << " (refCount=" << inodeTable[id].refCount << ")\n";
}

void inode_softLink(const string& target, const string& linkName) {
    if (inodeDir.count(linkName)) { cout << "  Link name already exists.\n"; return; }
    int id = allocInode();
    if (id == NONE) { cout << "  No free inodes!\n"; return; }
    inodeTable[id].isSymlink = true;
    inodeTable[id].symlinkTarget = target;
    inodeTable[id].size = (int)target.size();
    inodeDir[linkName] = {id, false};
    cout << "  [Inode] Soft link '" << linkName << "' -> '" << target << "' (own inode=" << id << ")\n";
}

void inode_showDir() {
    cout << "\n  --- Inode Directory ---\n";
    cout << "  " << left << setw(14)<<"Name" << setw(7)<<"Inode" << setw(7)<<"Refs" << setw(8)<<"InMem" << "Info\n";
    for (auto& [k,e] : inodeDir) {
        Inode& nd = inodeTable[e.inodeId];
        cout << "  " << left << setw(14)<<k << setw(7)<<nd.id << setw(7)<<nd.refCount
             << setw(8)<<(nd.inMemory?"Yes":"No");
        if (nd.isSymlink) cout << "symlink -> " << nd.symlinkTarget;
        else              cout << nd.size << "B";
        cout << "\n";
    }
}


// ============================================================
//  4. SIMPLE HIERARCHICAL DIRECTORY
//     (just a name map — shows mkdir / path structure)
// ============================================================
unordered_map<string,bool> dirTree;   // path -> isDir

void dir_mkdir (const string& path) { dirTree[path]=true;  cout << "  [Dir] mkdir  " << path << "\n"; }
void dir_mkfile(const string& path) { dirTree[path]=false; cout << "  [Dir] mkfile " << path << "\n"; }
void dir_remove(const string& path) {
    if (!dirTree.count(path)) { cout << "  Not found.\n"; return; }
    dirTree.erase(path); cout << "  [Dir] removed " << path << "\n";
}
void dir_list() {
    cout << "\n  --- Directory Tree ---\n";
    for (auto& [p,isDir] : dirTree)
        cout << "  " << p << (isDir ? "/" : "") << "\n";
}


// ============================================================
//  MAIN — demo workload
// ============================================================
static void banner(const string& title) {
    cout << "\n======================================\n"
         << "  " << title << "\n"
         << "======================================\n";
}

int main() {
    cout << "File System Allocation Simulator\n";
    cout << "Disk: " << TOTAL_BLOCKS << " blocks x " << BLOCK_SIZE << "B = "
         << TOTAL_BLOCKS*BLOCK_SIZE/1024 << " KB\n";

    // ── 1. CONTIGUOUS ─────────────────────────────────────
    banner("1. Contiguous Allocation");
    initDisk();

    contig_create("file_a.txt", 1024);   // 2 blocks
    contig_create("file_b.bin", 3072);   // 6 blocks
    contig_create("file_c.log",  512);   // 1 block
    contig_showDir();
    printDiskBitmap();

    cout << "\n-- Open / Read / Close --\n";
    contig_open ("file_a.txt");
    contig_read ("file_a.txt");
    contig_close("file_a.txt");

    cout << "\n-- Delete file_b → creates a hole (external fragmentation) --\n";
    contig_delete("file_b.bin");
    contig_showFrag();
    printDiskBitmap();

    cout << "\n-- Grow file_c from 1 to 4 blocks (must reallocate) --\n";
    contig_write("file_c.log", 2048);
    contig_showDir();
    printDiskBitmap();
    contig_showFrag();

    // ── 2. FAT ────────────────────────────────────────────
    banner("2. FAT (File Allocation Table)");
    initDisk(); initFAT();

    fat_create("readme.txt",  512);
    fat_create("image.png",  2048);
    fat_create("data.csv",   1536);
    fat_showDir();
    fat_showTable();

    cout << "\n-- Open / Read --\n";
    fat_open ("image.png");
    fat_read ("image.png");
    fat_close("image.png");

    cout << "\n-- Delete image.png → free its FAT chain --\n";
    fat_delete("image.png");
    fat_showTable();
    printDiskBitmap();

    cout << "\n-- Grow data.csv --\n";
    fat_write("data.csv", 3072);
    fat_showDir();

    // ── 3. I-NODES ────────────────────────────────────────
    banner("3. I-node Allocation");
    initDisk(); initInodes();
    for (auto& v : inodeIndirectData) v.clear();

    inode_create("kernel.bin", 2048);    // 4 direct blocks
    inode_create("notes.txt",   512);    // 1 block
    // Large file that needs indirect pointer (4 direct + 2 via indirect)
    inode_create("bigfile.dat", 3072);
    inode_showDir();
    printDiskBitmap();

    cout << "\n-- Open (inode loaded), Read, Close (inode evicted) --\n";
    inode_open ("kernel.bin");
    inode_read ("kernel.bin");
    inode_close("kernel.bin");

    cout << "\n-- Hard link: kernel.bak -> same inode as kernel.bin --\n";
    inode_hardLink("kernel.bin", "kernel.bak");
    inode_showDir();

    cout << "\n-- Delete kernel.bin: refCount drops but data survives --\n";
    inode_delete("kernel.bin");
    inode_showDir();

    cout << "\n-- Delete kernel.bak: refCount=0, blocks freed --\n";
    inode_delete("kernel.bak");
    inode_showDir();
    printDiskBitmap();

    cout << "\n-- Soft link: notes_link -> notes.txt --\n";
    inode_softLink("notes.txt", "notes_link");
    inode_open("notes_link");    // follows symlink
    inode_close("notes.txt");

    cout << "\n-- Delete notes.txt: symlink becomes broken --\n";
    inode_delete("notes.txt");
    inode_open("notes_link");    // should fail

    // ── 4. DIRECTORY ──────────────────────────────────────
    banner("4. Hierarchical Directory");
    dir_mkdir ("/home");
    dir_mkdir ("/home/ayla");
    dir_mkdir ("/home/user2");
    dir_mkfile("/home/ayla/resume.pdf");
    dir_mkfile("/home/user2/photo.jpg");
    dir_mkfile("/var/log/syslog");
    dir_list();
    dir_remove("/home/user2/photo.jpg");
    cout << "  After removing /home/user2/photo.jpg:\n";
    dir_list();

    // ── 5. JOURNAL SUMMARY ────────────────────────────────
    banner("5. Journal Summary");
    printJournal();

    cout << "\nDone.\n";
    return 0;
}