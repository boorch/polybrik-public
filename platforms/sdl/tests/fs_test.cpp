// Assert-based self-check of FileSystemImpl on a fresh temp directory.
#undef NDEBUG
#include "FileSystemImpl.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static int g_mountCalls = 0;
static bool g_mountArg = false;
static void onMount(bool inserted)
{
    g_mountCalls++;
    g_mountArg = inserted;
}

static void touch(const std::string& p, const char* data = "x")
{
    FILE* f = fopen(p.c_str(), "wb");
    assert(f);
    fputs(data, f);
    fclose(f);
}

int main()
{
    char tmpl[] = "/tmp/fs_test_XXXXXX";
    assert(mkdtemp(tmpl));
    std::string root = tmpl;

    // missing root: not inserted, callback never fires
    {
        FileSystemImpl missing((root + "/nope").c_str());
        missing.init();
        assert(!missing.isSdCardInserted());
        missing.setMountEventCallback(onMount);
        missing.update();
        assert(g_mountCalls == 0);
    }

    FileSystemImpl fs(root.c_str());
    fs.setMountEventCallback(onMount);
    fs.update(); // before init: nothing
    assert(g_mountCalls == 0);
    fs.init();
    assert(fs.isSdCardInserted());
    fs.update();
    fs.update();
    assert(g_mountCalls == 1 && g_mountArg == true);
    printf("mount callback: ok (fired %d time)\n", g_mountCalls);

    // mkdir, exists, mkdir on existing
    assert(fs.mkdir(0, "/Projects"));
    assert(fs.exists("/Projects"));
    assert(fs.mkdir(1, "/Projects"));
    assert(!fs.mkdir(0, "/no/such/parent"));
    printf("mkdir: ok\n");

    // create, write, close, reopen, size, read back
    auto [ok, h] = fs.openFile("/Projects/a.bin", FileSystem::kWriteCreateNew);
    assert(ok && h);
    assert(fs.writeFile("hello world", 11, h) == 11);
    fs.closeFile(h);
    assert(fs.getFileSize("/Projects/a.bin") == 11);
    assert(fs.getFileSize("/Projects/missing.bin") == 0);
    assert(fs.getFileSize("/Projects") == 0);

    auto [ok2, r] = fs.openFile("/Projects/a.bin");
    assert(ok2);
    assert(fs.getFileSize(r) == 11);
    char buf[32] = {};
    assert(fs.readFile(buf, sizeof(buf), r) == 11);
    assert(memcmp(buf, "hello world", 11) == 0);
    assert(fs.readFile(buf, sizeof(buf), r) == 0);
    assert(fs.writeFile("z", 1, r) == -1); // read-only handle
    assert(fs.seekFile(100, r));           // read mode clamps
    assert(fs.getFileAvailable(r) == 0);
    fs.closeFile(r);
    printf("write/read/size: ok\n");

    // read failure on missing file, no handle supplied
    auto [ok3, m] = fs.openFile("/Projects/missing.bin");
    assert(!ok3 && m == nullptr);

    // caller-supplied handle stays allocated on failure, closeFile(h,false) keeps it
    FileSystem::FileHandle keep = nullptr;
    auto [ok4, k] = fs.openFile("/Projects/missing.bin", FileSystem::kRead, &keep);
    assert(!ok4 && keep != nullptr && k == keep);
    assert(!fs.isFileOpen(keep));
    auto [ok5, k2] = fs.openFile("/Projects/a.bin", FileSystem::kRead, &keep);
    assert(ok5 && k2 == keep && fs.isFileOpen(keep));
    fs.closeFile(keep, false);
    assert(!fs.isFileOpen(keep));
    printf("caller-supplied handle: ok\n");

    // seek past EOF in write mode extends; endFile truncates at position
    auto [ok6, w] = fs.openFile("/Projects/a.bin", FileSystem::kWriteOpen);
    assert(ok6);
    assert(fs.seekFile(20, w));
    assert(fs.getFileSize(w) == 20);
    assert(fs.writeFile("!", 1, w) == 1);
    assert(fs.getFileSize(w) == 21);
    assert(fs.seekFile(5, w));
    fs.endFile(w);
    assert(fs.getFileSize(w) == 5);
    assert(fs.seekFileCur(-2, w));
    assert(fs.readFile(buf, 2, w) == 2 && memcmp(buf, "lo", 2) == 0);
    assert(fs.writeFile("XY", 2, w) == 2); // read then write on the same handle
    fs.closeFile(w);
    assert(fs.getFileSize("/Projects/a.bin") == 7);
    auto [ok7, a] = fs.openFile("/Projects/a.bin", FileSystem::kWriteAppend);
    assert(ok7 && fs.writeFile("Z", 1, a) == 1);
    fs.closeFile(a);
    auto [ok8, rr] = fs.openFile("/Projects/a.bin");
    memset(buf, 0, sizeof(buf));
    assert(ok8 && fs.readFile(buf, sizeof(buf), rr) == 8 && strcmp(buf, "helloXYZ") == 0);
    fs.closeFile(rr);
    printf("seek extend/endFile/append: ok\n");

    // kWriteCreateNew: the exporter reopens master.wav per packet, seeks to the old end and
    // writes on (mtAudioExport.cpp:332-340); FatFs keeps the old bytes there, so must we.
    // A shorter rewrite must still end up truncated like FA_CREATE_ALWAYS.
    auto [ok9, c1] = fs.openFile("/Projects/c.bin", FileSystem::kWriteCreateNew);
    assert(ok9 && fs.writeFile("0123456789", 10, c1) == 10);
    fs.closeFile(c1);
    auto [ok10, c2] = fs.openFile("/Projects/c.bin", FileSystem::kWriteCreateNew);
    assert(ok10 && fs.seekFile(10, c2) && fs.writeFile("abcde", 5, c2) == 5);
    assert(fs.seekFile(0, c2) && fs.writeFile("H", 1, c2) == 1); // header rewrite at the end
    fs.closeFile(c2);
    assert(fs.getFileSize("/Projects/c.bin") == 15);
    auto [ok11, c3] = fs.openFile("/Projects/c.bin");
    memset(buf, 0, sizeof(buf));
    assert(ok11 && fs.readFile(buf, sizeof(buf), c3) == 15 && strcmp(buf, "H123456789abcde") == 0);
    fs.closeFile(c3);
    auto [ok12, c4] = fs.openFile("/Projects/c.bin", FileSystem::kWriteCreateNew);
    assert(ok12 && fs.writeFile("xy", 2, c4) == 2);
    fs.closeFile(c4);
    assert(fs.getFileSize("/Projects/c.bin") == 2);
    auto [ok13, c5] = fs.openFile("/Projects/c.bin", FileSystem::kWriteCreateNew);
    assert(ok13);
    fs.closeFile(c5); // nothing written: empty, like FA_CREATE_ALWAYS
    assert(fs.getFileSize("/Projects/c.bin") == 0);
    printf("kWriteCreateNew reopen keeps bytes, shorter rewrite truncates: ok\n");

    // pool: the caller-kept handle holds one slot, so 23 more opens succeed, then fail
    FileSystem::FileHandle hs[24];
    int opened = 0;
    for (int i = 0; i < 24; i++)
    {
        auto [o, hh] = fs.openFile("/Projects/a.bin");
        hs[i] = hh;
        if (o)
            opened++;
    }
    assert(opened == 23 && hs[23] == nullptr);
    fs.closeFile(keep); // release it: one more fits, the 25th open overall fails
    auto [o24, h24] = fs.openFile("/Projects/a.bin");
    assert(o24);
    auto [o25, h25] = fs.openFile("/Projects/a.bin");
    assert(!o25 && h25 == nullptr);
    fs.closeFile(h24);
    for (int i = 0; i < 23; i++)
        fs.closeFile(hs[i]);
    auto [oAfter, hAfter] = fs.openFile("/Projects/a.bin");
    assert(oAfter);
    fs.closeFile(hAfter);
    printf("handle pool (24, 25th fails): ok\n");

    // case-insensitive resolve
    mkdir((root + "/Samples").c_str(), 0777);
    touch(root + "/Samples/x.wav", "abc");
    assert(fs.exists("/samples/X.WAV"));
    auto [oc, hc] = fs.openFile("/samples/X.WAV");
    assert(oc && fs.getFileSize(hc) == 3);
    fs.closeFile(hc);
    assert(fs.getFileSize("/SAMPLES/x.WAV") == 3);
    printf("case-insensitive open: ok\n");

    // listing fixtures
    touch(root + "/Samples/10 b.wav");
    touch(root + "/Samples/2 a.WAV");
    touch(root + "/Samples/notes.txt");
    touch(root + "/Samples/.hidden.wav");
    touch(root + "/Samples/._x.wav");
    mkdir((root + "/Samples/Kicks").c_str(), 0777);
    touch(root + "/Samples/Kicks/k1.wav");
    touch(root + "/Samples/Kicks/k2.wav");
    mkdir((root + "/Samples/__MACOSX").c_str(), 0777);
    mkdir((root + "/Samples/.git").c_str(), 0777);

    // createFilesList, folders with a parent slot at 0 (as fileManagerBrowse does)
    char* list[16] = {};
    int next = fs.createFilesList("/Samples", 1, list, 16, 5000, FileSystem::enListFileFilter_folder);
    assert(next == 2);
    assert(list[0] == nullptr && strcmp(list[1], "/Kicks") == 0);

    // max_files_in_subdir splits a big folder into "(1-1) (2-2)" entries
    next = fs.createFilesList("/Samples", 0, list, 16, 5000, FileSystem::enListFileFilter_folder,
                              0, 1, FileSystem::enListFileFilter_wave);
    assert(next == 2 && strcmp(list[0], "/Kicks (1-1)") == 0 && strcmp(list[1], "/Kicks (2-2)") == 0);

    // waves: hidden, ._ and non-wav skipped, readdir order, return value = next index
    next = fs.createFilesList("/Samples", 0, list, 16, 5000, FileSystem::enListFileFilter_wave);
    assert(next == 3);
    bool seen[3] = {};
    for (int i = 0; i < next; i++)
    {
        if (!strcmp(list[i], "x.wav")) seen[0] = true;
        if (!strcmp(list[i], "10 b.wav")) seen[1] = true;
        if (!strcmp(list[i], "2 a.WAV")) seen[2] = true;
    }
    assert(seen[0] && seen[1] && seen[2]);
    next = fs.createFilesList("/Samples", 0, list, 16, 5000, FileSystem::enListFileFilter_wave, 2);
    assert(next == 1);
    assert(fs.createFilesList("/nope", 0, list, 16, 5000) == FileSystem::kListError);
    for (auto& p : list) { delete[] p; p = nullptr; }
    printf("createFilesList: ok\n");

    // listDirContents: sorted with compareStrings, folder prefix
    char names[512];
    char* fl[16];
    int count = 16;
    fs.listDirContents("/Samples", FileSystem::kFiles, nullptr, names, sizeof(names), 64, count, fl);
    assert(count == 4);
    assert(!strcmp(fl[0], "2 a.WAV") && !strcmp(fl[1], "10 b.wav"));
    count = 16;
    fs.listDirContents("/Samples", FileSystem::kFiles,
                       [](const char* n) { return strstr(n, ".txt") == nullptr; },
                       names, sizeof(names), 64, count, fl);
    assert(count == 3);
    count = 16;
    fs.listDirContents("/Samples", FileSystem::kFolders, nullptr, names, sizeof(names), 64, count, fl);
    assert(count == 1 && !strcmp(fl[0], "/Kicks"));
    printf("listDirContents: ok\n");

    // countContainedFiles: 3 waves + 1 visible folder (__MACOSX counts as in IMX)
    assert(fs.countContainedFiles("/Samples") == 5);

    // instruments: "NN name.pti" slots
    mkdir((root + "/Projects/instruments").c_str(), 0777);
    touch(root + "/Projects/instruments/01 Kick.pti", "1234");
    touch(root + "/Projects/instruments/12 Snare.PTI", "12");
    touch(root + "/Projects/instruments/49 Bad.pti");
    touch(root + "/Projects/instruments/NoNum.pti");
    char* inst[48] = {};
    uint32_t total = 0;
    assert(fs.createInstrumentsList("/Projects/instruments", inst, 48, &total) == 2);
    assert(!strcmp(inst[0], "Kick") && !strcmp(inst[11], "Snare") && total == 6);
    for (auto& p : inst) delete[] p;
    printf("countContainedFiles/createInstrumentsList: ok\n");

    // rename, remove, recursive delete
    assert(fs.rename("/Projects/a.bin", "/Projects/b.bin"));
    assert(!fs.rename("/Projects/b.bin", "/Samples/x.wav")); // FR_EXIST
    assert(fs.remove("/Projects/b.bin") && !fs.remove("/Projects/b.bin"));
    assert(fs.removeOnlyDirFiles("/Samples") && fs.exists("/Samples") && !fs.exists("/Samples/Kicks"));
    assert(fs.removeDirWithFiles("/Projects") && !fs.exists("/Projects"));
    printf("rename/remove: ok\n");

    assert(!fs.enableFastSeek(nullptr, nullptr, 0));
    assert(fs.pollAsyncRead() && !fs.isAsyncReadBusy() && !fs.waitAsyncRead());

    rmdir((root + "/Samples").c_str());
    rmdir(root.c_str());
    printf("fs_test: ALL PASSED\n");
    return 0;
}
