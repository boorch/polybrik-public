// POSIX port of tracker-ce/platforms/imx/source/FileSystemImpl.cpp and
// platforms/imx/sdmmc/sd/SD.cpp + SdFile.cpp. FatFs calls are
// replaced with <cstdio>, <dirent.h>, <sys/stat.h>, <unistd.h>.
// FatFs AM_HID maps to a leading '.' in the name; FatFs never lists "." and "..".

#include "FileSystemImpl.h"
#include "util_string.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{

// ---- file handle pool (IMX: s_sdFilePool, FF_FS_LOCK = 24) ------------------
struct SdFile
{
    FILE* f = nullptr;
    bool writable = false;
    bool lastOpWrite = false; // stdio needs a seek between a write and a read
    bool truncateOnClose = false; // kWriteCreateNew: cut the file to `written` in sdFileClose
    off_t written = 0;            // furthest byte written since open
};

constexpr size_t kSdFilePoolSize = 24;
SdFile s_sdFilePool[kSdFilePoolSize];
bool s_sdFilePoolUsed[kSdFilePoolSize]{ false };

SdFile* allocateSdFile()
{
    for (size_t i = 0; i < kSdFilePoolSize; ++i)
    {
        if (!s_sdFilePoolUsed[i])
        {
            s_sdFilePoolUsed[i] = true;
            s_sdFilePool[i] = SdFile{};
            return &s_sdFilePool[i];
        }
    }
    return nullptr;
}

void freeSdFile(SdFile* f)
{
    for (size_t i = 0; i < kSdFilePoolSize; ++i)
    {
        if (&s_sdFilePool[i] == f)
        {
            s_sdFilePoolUsed[i] = false;
            return;
        }
    }
}

bool sdFileClose(SdFile* file)
{
    if (file->f == nullptr)
    {
        return true;
    }
    bool ok = true;
    if (file->writable)
    {
        ok = fflush(file->f) == 0;
        if (file->truncateOnClose)
        {
            ok = ftruncate(fileno(file->f), file->written) == 0 && ok;
        }
        ok = fsync(fileno(file->f)) == 0 && ok;
    }
    ok = (fclose(file->f) == 0) && ok;
    file->f = nullptr;
    file->truncateOnClose = false;
    return ok;
}

uint64_t sdFileSize(SdFile* file)
{
    if (file->lastOpWrite)
    {
        fflush(file->f);
    }
    struct stat st;
    return fstat(fileno(file->f), &st) == 0 ? (uint64_t)st.st_size : 0;
}

// FatFs f_lseek: clamps to the size in read mode, extends the file in write mode.
bool sdFileSeek(SdFile* file, uint64_t pos)
{
    uint64_t size = sdFileSize(file);
    if (pos > size)
    {
        if (file->writable)
        {
            fflush(file->f);
            if (ftruncate(fileno(file->f), (off_t)pos) != 0)
            {
                return false;
            }
        }
        else
        {
            pos = size;
        }
    }
    file->lastOpWrite = false;
    return fseeko(file->f, (off_t)pos, SEEK_SET) == 0;
}

// ---- directory reading (IMX: SdDir over f_readdir) --------------------------
struct DirEntry
{
    char fname[256];
    bool isDir;
    bool isHidden;
    uint64_t fsize;
};

struct DirReader
{
    DIR* dir = nullptr;
    std::string path;

    bool open(const std::string& hostPath)
    {
        close();
        dir = opendir(hostPath.c_str());
        path = hostPath;
        return dir != nullptr;
    }

    void close()
    {
        if (dir != nullptr)
        {
            closedir(dir);
            dir = nullptr;
        }
    }

    // false at end of folder (FatFs: fname[0] == 0)
    bool next(DirEntry& e)
    {
        while (dirent* d = readdir(dir))
        {
            if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
            {
                continue;
            }
            polystrncpy(e.fname, d->d_name, sizeof(e.fname));
            struct stat st;
            std::string full = path + "/" + d->d_name;
            bool ok = stat(full.c_str(), &st) == 0;
            e.isDir = ok && S_ISDIR(st.st_mode);
            e.fsize = ok ? (uint64_t)st.st_size : 0;
            e.isHidden = d->d_name[0] == '.';
            return true;
        }
        return false;
    }

    ~DirReader() { close(); }
};

bool isMacMetaFile(const DirEntry& e)
{
    return e.fname[0] == '.' && e.fname[1] == '_';
}

bool isMacMetaFolder(const DirEntry& e)
{
    return strcmp(e.fname, "__MACOSX") == 0;
}

bool hasExtCaseless(const char* name, size_t len, const char ext[4])
{
    return len >= 5 && name[len - 4] == '.' && tolower((unsigned char)name[len - 3]) == ext[0] &&
           tolower((unsigned char)name[len - 2]) == ext[1] &&
           tolower((unsigned char)name[len - 1]) == ext[2];
}

// SdDir::filterFileEntry. sdDirPath is the SD path of the listed folder (IMX dir_path).
bool filterFileEntry(const DirEntry& fno, FileSystem::enListFileFilter filter, const char* sdDirPath)
{
    if (fno.isHidden)
    {
        return false;
    }

    if (fno.isDir && filter != FileSystem::enListFileFilter_folder &&
        filter != FileSystem::enListFileFilter_all)
    {
        return false;
    }

    uint8_t filenameLen = strlen(fno.fname);

    switch (filter)
    {
        case FileSystem::enListFileFilter_all:
            return true;

        case FileSystem::enListFileFilter_folder:
            return fno.isDir && !isMacMetaFolder(fno);

        case FileSystem::enListFileFilter_wave_inst:
        case FileSystem::enListFileFilter_wave:
        {
            if (filenameLen < 5)
            {
                return false;
            }
            if (!hasExtCaseless(fno.fname, filenameLen, "wav"))
            {
                if (filter != FileSystem::enListFileFilter_wave_inst ||
                    !hasExtCaseless(fno.fname, filenameLen, "pti"))
                {
                    return false;
                }
            }
            if ((strlen(sdDirPath) + filenameLen) > 253)
            {
                return false;
            }
            if (isMacMetaFile(fno))
            {
                return false;
            }
            return true;
        }
        case FileSystem::enListFileFilter_firmware:
        {
#ifdef TRACKER_MINI
            constexpr char kFirmwareFileNameStart[] = "TrackerMini";
#else
            constexpr char kFirmwareFileNameStart[] = "TrackerPlus";
#endif
            if (filenameLen < strlen(kFirmwareFileNameStart) + 4)
            {
                return false;
            }
            if (strncmp(&fno.fname[filenameLen - 4], ".ptf", 4) != 0)
            {
                return false;
            }
            if (strncmp(kFirmwareFileNameStart, fno.fname, strlen(kFirmwareFileNameStart)) != 0)
            {
                return false;
            }
            if (isMacMetaFile(fno))
            {
                return false;
            }
            return true;
        }
        case FileSystem::enListFileFilter_mod:
        {
            char lowCase[256];
            strcpy(lowCase, fno.fname);
            for (char* c = lowCase; *c; c++)
            {
                *c = tolower((unsigned char)*c);
            }
            return strstr(lowCase, ".mod") != NULL || strstr(lowCase, ".it") != NULL;
        }
        case FileSystem::enListFileFilter_game:
        {
            if (isMacMetaFile(fno))
            {
                return false;
            }
            char lowCase[256];
            strcpy(lowCase, fno.fname);
            for (char* c = lowCase; *c; c++)
            {
                *c = tolower((unsigned char)*c);
            }
            return strstr(lowCase, ".nes") != NULL;
        }
    }
    return false;
}

// SdCard::delete_node
bool deleteNode(const std::string& path, bool removeDir)
{
    DirReader dir;
    if (!dir.open(path))
    {
        return false;
    }
    DirEntry e;
    bool ok = true;
    while (ok && dir.next(e))
    {
        std::string child = path + "/" + e.fname;
        ok = e.isDir ? deleteNode(child, true) : ::remove(child.c_str()) == 0;
    }
    dir.close();
    if (ok && removeDir)
    {
        ok = ::rmdir(path.c_str()) == 0;
    }
    return ok;
}

bool pathExists(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

} // namespace

// ---- lifecycle --------------------------------------------------------------

FileSystemImpl::FileSystemImpl(const char* sdRoot) : root(sdRoot ? sdRoot : "")
{
    while (root.size() > 1 && root.back() == '/')
    {
        root.pop_back();
    }
}

FileSystemImpl::~FileSystemImpl()
{
    nesDir_close();
}

void FileSystemImpl::init()
{
    struct stat st;
    cardInserted = !root.empty() && stat(root.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void FileSystemImpl::update()
{
    if (mountEventPending && cardInserted && cardStateChangeCallback != nullptr)
    {
        mountEventPending = false;
        cardStateChangeCallback(true);
    }
}

bool FileSystemImpl::isSdCardInserted()
{
    return cardInserted;
}

void FileSystemImpl::setMountEventCallback(CardStateChangeCallback funct)
{
    cardStateChangeCallback = funct;
    mountEventPending = true;
}

std::string FileSystemImpl::resolve(const char* path)
{
    std::string p = path ? path : "";
    std::string exact = root + ((p.empty() || p[0] != '/') ? "/" : "") + p;
    if (pathExists(exact))
    {
        return exact;
    }

    // FAT is case-insensitive: match each missing component against its folder.
    std::string out = root;
    size_t i = 0;
    while (i < p.size())
    {
        size_t j = p.find('/', i);
        if (j == std::string::npos)
        {
            j = p.size();
        }
        if (j > i)
        {
            std::string comp = p.substr(i, j - i);
            std::string cand = out + "/" + comp;
            if (!pathExists(cand))
            {
                if (DIR* d = opendir(out.c_str()))
                {
                    while (dirent* e = readdir(d))
                    {
                        if (strcasecmp(e->d_name, comp.c_str()) == 0)
                        {
                            cand = out + "/" + e->d_name;
                            break;
                        }
                    }
                    closedir(d);
                }
            }
            out = cand;
        }
        i = j + 1;
    }
    return out;
}

// ---- SdCard -----------------------------------------------------------------

bool FileSystemImpl::exists(const char* path)
{
    return pathExists(resolve(path));
}

bool FileSystemImpl::rename(const char* oldPath, const char* newPath)
{
    std::string from = resolve(oldPath);
    std::string to = resolve(newPath);
    // f_rename fails with FR_EXIST instead of overwriting.
    // shortcut: a case-only rename resolves to the same path and is refused, fix if a caller needs it.
    if (pathExists(to))
    {
        return false;
    }
    return ::rename(from.c_str(), to.c_str()) == 0;
}

bool FileSystemImpl::mkdir(uint8_t hidden, const char* path, bool pFlag)
{
    // IMX ignores pFlag (f_mkdir is not recursive); hidden has no POSIX equivalent.
    return ::mkdir(resolve(path).c_str(), 0777) == 0 || errno == EEXIST;
}

bool FileSystemImpl::remove(const char* path)
{
    return ::remove(resolve(path).c_str()) == 0;
}

bool FileSystemImpl::removeDirWithFiles(const char* path)
{
    return deleteNode(resolve(path), true);
}

bool FileSystemImpl::removeOnlyDirFiles(const char* path)
{
    return deleteNode(resolve(path), false);
}

// ---- files ------------------------------------------------------------------

size_t FileSystemImpl::getFileSize(const char* path)
{
    struct stat st;
    if (stat(resolve(path).c_str(), &st) != 0 || !S_ISREG(st.st_mode))
    {
        return 0;
    }
    return st.st_size;
}

size_t FileSystemImpl::getFileSize(FileHandle fileHandle)
{
    SdFile* file = (SdFile*)fileHandle;
    if (file->f == nullptr)
    {
        return 0;
    }
    return sdFileSize(file);
}

size_t FileSystemImpl::getFileAvailable(FileHandle fileHandle)
{
    assert(fileHandle);
    SdFile* file = (SdFile*)fileHandle;
    if (file->f == nullptr)
    {
        return 0;
    }
    uint64_t size = sdFileSize(file);
    off_t pos = ftello(file->f);
    return pos < 0 || (uint64_t)pos > size ? 0 : size - pos;
}

std::tuple<bool, FileSystem::FileHandle> FileSystemImpl::openFile(const char* filepath,
                                                                   FileOpenMode openMode,
                                                                   FileHandle* fileHandle)
{
    SdFile* file;
    if (fileHandle == nullptr)
    {
        file = allocateSdFile();
    }
    else if (*fileHandle == nullptr)
    {
        file = allocateSdFile();
        *fileHandle = (FileHandle)file;
    }
    else
    {
        file = (SdFile*)*fileHandle;
    }

    if (file == nullptr)
    {
        return std::make_tuple(false, nullptr);
    }

    sdFileClose(file); // SdFile::open closes first

    std::string host = resolve(filepath);
    struct stat st;
    bool present = stat(host.c_str(), &st) == 0;
    bool isOpen = false;

    if (!present || S_ISREG(st.st_mode))
    {
        const char* mode = nullptr;
        switch (openMode)
        {
            case kWriteOpen:      // FA_OPEN_ALWAYS
            case kWriteAppend:    // FA_OPEN_APPEND: open always, pointer at the end
                mode = present ? "r+b" : "w+b";
                break;
            case kWriteCreateNew: // FA_CREATE_ALWAYS, truncated at close instead of at open: FatFs frees
                // the old chain and re-allocates the same clusters on a seek past EOF (ff.c "Reuse the
                // cluster hole"), which the exporter relies on when it reopens master.wav per packet
                // (mtAudioExport.cpp:332-340); a POSIX truncate-at-open turns that into a zero hole.
                mode = present ? "r+b" : "w+b";
                break;
            default:
                mode = present ? "rb" : nullptr;
                break;
        }
        if (mode != nullptr)
        {
            file->f = fopen(host.c_str(), mode);
            file->writable = openMode != kRead;
            file->lastOpWrite = false;
            file->truncateOnClose = openMode == kWriteCreateNew;
            file->written = 0;
            isOpen = file->f != nullptr;
            if (isOpen && openMode == kWriteAppend)
            {
                fseeko(file->f, 0, SEEK_END);
            }
        }
    }

    if (!isOpen && fileHandle == nullptr)
    {
        freeSdFile(file);
        file = nullptr;
    }

    return std::make_tuple(isOpen, (FileHandle)file);
}

int32_t FileSystemImpl::readFile(void* destBuf, size_t bufLen, FileHandle fileHandle)
{
    assert(fileHandle != nullptr);
    SdFile* file = (SdFile*)fileHandle;
    assert(file->f != nullptr);

    if (file->lastOpWrite)
    {
        fseeko(file->f, 0, SEEK_CUR);
        file->lastOpWrite = false;
    }
    size_t n = fread(destBuf, 1, bufLen, file->f);
    if (ferror(file->f))
    {
        clearerr(file->f);
        return -1;
    }
    return (int32_t)n;
}

int32_t FileSystemImpl::writeFile(const void* data, size_t dataSize, FileHandle fileHandle)
{
    assert(fileHandle != nullptr);
    SdFile* file = (SdFile*)fileHandle;
    assert(file->f != nullptr);

    if (!file->writable)
    {
        return -1; // FR_DENIED
    }
    if (!file->lastOpWrite)
    {
        fseeko(file->f, 0, SEEK_CUR);
        file->lastOpWrite = true;
    }
    size_t n = fwrite(data, 1, dataSize, file->f);
    if (ferror(file->f))
    {
        clearerr(file->f);
        return -1;
    }
    off_t end = ftello(file->f);
    if (end > file->written)
    {
        file->written = end;
    }
    return (int32_t)n;
}

bool FileSystemImpl::seekFileCur(int offset, FileHandle fileHandle)
{
    assert(fileHandle != nullptr);
    SdFile* file = (SdFile*)fileHandle;
    assert(file->f != nullptr);

    if (offset == 0)
    {
        return true;
    }
    off_t pos = ftello(file->f) + offset;
    if (pos < 0)
    {
        return false;
    }
    return sdFileSeek(file, (uint64_t)pos);
}

bool FileSystemImpl::seekFile(uint32_t pos, FileHandle fileHandle)
{
    assert(fileHandle != nullptr);
    SdFile* file = (SdFile*)fileHandle;
    assert(file->f != nullptr);

    return sdFileSeek(file, pos);
}

void FileSystemImpl::closeFile(FileHandle fileHandle, bool deleteOnClose)
{
    if (fileHandle == nullptr)
    {
        return;
    }
    SdFile* file = (SdFile*)fileHandle;
    sdFileClose(file);
    if (deleteOnClose)
    {
        freeSdFile(file);
    }
}

void FileSystemImpl::endFile(FileHandle fileHandle)
{
    assert(fileHandle != nullptr);
    SdFile* file = (SdFile*)fileHandle;
    if (file->f == nullptr || !file->writable)
    {
        return; // f_truncate: FR_DENIED
    }
    fflush(file->f);
    off_t pos = ftello(file->f);
    if (pos >= 0)
    {
        ftruncate(fileno(file->f), pos);
    }
}

bool FileSystemImpl::isFileOpen(FileHandle fileHandle)
{
    return fileHandle != nullptr && ((SdFile*)fileHandle)->f != nullptr;
}

// No FAT cluster map on POSIX: false forces fillJobSynchronously.
bool FileSystemImpl::enableFastSeek(FileHandle, uint32_t*, uint32_t, uint32_t*, uint32_t*)
{
    return false;
}

bool FileSystemImpl::readSectorsAsync(uint8_t*, uint32_t, uint32_t)
{
    return false;
}

bool FileSystemImpl::pollAsyncRead(bool* outSuccess)
{
    if (outSuccess != nullptr)
    {
        *outSuccess = false;
    }
    return true;
}

bool FileSystemImpl::waitAsyncRead()
{
    return false;
}

bool FileSystemImpl::isAsyncReadBusy()
{
    return false;
}

// ---- lists (SdDir) ----------------------------------------------------------

int FileSystemImpl::createInstrumentsList(const char* path,
                                          char** list,
                                          uint8_t list_length,
                                          uint32_t* sizeOut)
{
    DirReader dir;
    if (!dir.open(resolve(path)))
    {
        return kListError;
    }

    uint8_t n = 0;
    DirEntry fno;
    while (n < list_length && dir.next(fno))
    {
        if (fno.isHidden || fno.isDir)
        {
            continue;
        }
        int nameLen = strlen(fno.fname);
        if (!hasExtCaseless(fno.fname, nameLen, "pti"))
        {
            continue;
        }

        // "NN name.pti": one or two digits, a space, then the name
        char* p = fno.fname;
        int idx1 = -1, idx2 = -1;
        if (*p >= '0' && *p <= '9')
        {
            idx1 = *p - '0';
        }
        else
        {
            continue;
        }
        p++;
        if (*p >= '0' && *p <= '9')
        {
            idx2 = *p - '0';
            p++;
        }
        if (*p != ' ')
        {
            continue;
        }
        p++;
        if (idx2 >= 0)
        {
            idx1 = idx1 * 10 + idx2;
        }
        if (idx1 < 1 || idx1 > 48)
        {
            continue;
        }
        idx1--;
        if (list[idx1] != nullptr)
        {
            continue;
        }

        int len = nameLen - (p - fno.fname) - 4;
        list[idx1] = new char[len + 1];
        strncpy(list[idx1], p, len);
        list[idx1][len] = 0;

        *sizeOut += fno.fsize;
        n++;
    }
    return n;
}

int FileSystemImpl::countContainedFiles(const char* path)
{
    DirReader dir;
    if (!dir.open(resolve(path)))
    {
        return 0;
    }

    uint8_t n = 0; // uint8_t as in IMX SdDir::countContainedFiles
    DirEntry fno;
    while (dir.next(fno))
    {
        if (fno.isHidden)
        {
            continue;
        }
        if (!fno.isDir)
        {
            uint8_t filenameLen = strlen(fno.fname);
            if (!hasExtCaseless(fno.fname, filenameLen, "wav"))
            {
                continue;
            }
            if ((strlen(path) + filenameLen) > 253)
            {
                continue;
            }
        }
        n++;
    }
    return n;
}

static uint16_t countFilesInSubDirectory(const std::string& hostDir,
                                         const char* sdDirPath,
                                         const char* dirName,
                                         FileSystem::enListFileFilter filter)
{
    DirReader dir;
    if (!dir.open(hostDir + "/" + dirName))
    {
        return 0;
    }
    uint16_t count = 0;
    DirEntry e;
    while (dir.next(e))
    {
        if (filterFileEntry(e, filter, sdDirPath))
        {
            count++;
        }
    }
    return count;
}

int FileSystemImpl::createFilesList(const char* path,
                                    uint8_t start_line,
                                    char** list,
                                    uint8_t list_length,
                                    uint16_t max_used_memory,
                                    enListFileFilter filter,
                                    uint16_t skip_count,
                                    uint16_t max_files_in_subdir,
                                    enListFileFilter folderSizeFilter)
{
    std::string hostDir = resolve(path);
    DirReader dir;
    if (!dir.open(hostDir))
    {
        return kListError;
    }

    for (uint8_t i = 0; i < list_length; i++)
    {
        if (list[i] != nullptr)
        {
            delete[] list[i];
            list[i] = nullptr;
        }
    }

    uint8_t n = start_line;

    if (max_used_memory < 255)
    {
        return n;
    }

    uint16_t memory_used = 0;
    DirEntry fno;

    while (1)
    {
        if (n >= list_length)
            break;
        if (memory_used > max_used_memory - 255)
            break;

        if (!dir.next(fno))
        {
            break;
        }
        if (!filterFileEntry(fno, filter, path))
        {
            continue;
        }
        if (skip_count > 0)
        {
            skip_count--;
            continue;
        }

        uint16_t len = strlen(fno.fname);
        len += fno.isDir ? 2 : 1;

        list[n] = new char[len];
        memory_used += len;

        if (fno.isDir)
        {
            strcpy(list[n], "/");
            strcat(list[n], fno.fname);
            int16_t files_count = countFilesInSubDirectory(hostDir, path, fno.fname, folderSizeFilter);

            if (max_files_in_subdir != 0 && files_count > max_files_in_subdir)
            {
                uint16_t offset = 0;
                len += 4 + 2 * 5;

                while (files_count > 0 && n < list_length - 1)
                {
                    if (list[n] != nullptr)
                    {
                        delete[] list[n];
                        memory_used -= len;
                    }
                    list[n] = new char[len];
                    memory_used += len;
                    snprintf(list[n],
                             len,
                             "/%s (%d-%d)",
                             fno.fname,
                             offset + 1,
                             offset + max_files_in_subdir);
                    files_count -= max_files_in_subdir;

                    n++;
                    offset += max_files_in_subdir;
                }
                n--;
            }
        }
        else
        {
            strcpy(list[n], fno.fname);
        }

        n++;
    }

    return n;
}

void FileSystemImpl::listDirContents(const char* dirPath,
                                     DirContentType contentType,
                                     std::function<bool(const char* name)> nameFilter,
                                     char* namesBuffer,
                                     int namesBufferSize,
                                     int maxNameLen,
                                     int& count,
                                     char** fileList)
{
    assert(count != 0);
    assert(fileList != nullptr);

    DirReader sdDir;
    if (!sdDir.open(resolve(dirPath)))
    {
        count = 0;
        return;
    }

    DirEntry fileInfo;
    int listPos = 0;
    while (listPos < count)
    {
        if (!sdDir.next(fileInfo))
        {
            break;
        }
        if (fileInfo.isHidden)
        {
            continue;
        }
        bool isFolder = fileInfo.isDir;
        int nameLen;

        if (contentType == kFolders)
        {
            if (!isFolder || isMacMetaFolder(fileInfo))
            {
                continue;
            }
            if (nameFilter != nullptr && !nameFilter(fileInfo.fname))
            {
                continue;
            }
            nameLen = strlen(fileInfo.fname) + 2;
        }
        else if (contentType == kFiles)
        {
            if (isFolder || isMacMetaFile(fileInfo))
            {
                continue;
            }
            if (nameFilter != nullptr && !nameFilter(fileInfo.fname))
            {
                continue;
            }
            nameLen = strlen(fileInfo.fname) + 1;
        }
        else
        {
            continue;
        }

        if (nameLen > namesBufferSize)
        {
            break;
        }
        if (nameLen > maxNameLen)
        {
            continue;
        }

        fileList[listPos] = namesBuffer;

        if (isFolder)
        {
            *namesBuffer = '/';
            namesBuffer++;
        }

        polystrncpy(namesBuffer, fileInfo.fname, maxNameLen + 1);

        namesBuffer += nameLen;
        namesBufferSize -= nameLen;
        listPos++;
    }
    count = listPos;

    sdDir.close();

    std::sort(fileList,
              fileList + count,
              [](const char* a, const char* b) { return compareStrings(a, b); });
}

// ---- NES leftovers (UI slot is nullptr) -------------------------------------

int FileSystemImpl::readNbFiles(const char* romPath)
{
    DirReader dir;
    if (!dir.open(resolve(romPath)))
    {
        return 0;
    }
    int totalFiles = 0;
    DirEntry e;
    while (dir.next(e))
    {
        totalFiles++;
    }
    return totalFiles;
}

bool FileSystemImpl::nesDir_open(const char* path)
{
    nesDir_close();
    nesDirPath = resolve(path);
    nesDir = opendir(nesDirPath.c_str());
    return nesDir != nullptr;
}

bool FileSystemImpl::nesDir_readItem(char* path, uint8_t* isDir, uint8_t* isHid, uint32_t* size)
{
    if (nesDir == nullptr)
    {
        return false;
    }
    while (dirent* d = readdir(nesDir))
    {
        if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
        {
            continue;
        }
        struct stat st;
        bool ok = stat((nesDirPath + "/" + d->d_name).c_str(), &st) == 0;
        *isDir = ok && S_ISDIR(st.st_mode);
        *isHid = d->d_name[0] == '.';
        if (size != nullptr)
        {
            *size = ok ? (uint32_t)st.st_size : 0;
        }
        strcpy(path, d->d_name);
        return true;
    }
    return false;
}

void FileSystemImpl::nesDir_close()
{
    if (nesDir != nullptr)
    {
        closedir(nesDir);
        nesDir = nullptr;
    }
}
