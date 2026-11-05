#ifndef PLATFORMS_SDL_FILESYSTEMIMPL_H_
#define PLATFORMS_SDL_FILESYSTEMIMPL_H_

// POSIX port of tracker-ce/platforms/imx/source/FileSystemImpl.
// Paths passed in are absolute from the SD root; sdRoot is prefixed.

#include "Platform.h"

#include <dirent.h>
#include <functional>
#include <stdint.h>
#include <string>

class FileSystemImpl final : public FileSystem
{
  public:
    explicit FileSystemImpl(const char* sdRoot);
    ~FileSystemImpl();

    bool isSdCardInserted() override;
    void setMountEventCallback(CardStateChangeCallback funct) override;

    bool exists(const char* path) override;
    bool rename(const char* oldPath, const char* newPath) override;
    bool mkdir(uint8_t hidden, const char* path, bool pFlag = true) override;
    bool remove(const char* path) override;
    bool removeDirWithFiles(const char* path) override;
    bool removeOnlyDirFiles(const char* path) override;

    size_t getFileSize(const char* path) override;
    size_t getFileSize(FileHandle fileHandle) override;
    size_t getFileAvailable(FileHandle fileHandle) override;
    std::tuple<bool, FileHandle> openFile(const char* filepath,
                                          FileOpenMode openMode = kRead,
                                          FileHandle* fileHandle = nullptr) override;

    int32_t readFile(void* destBuf, size_t bufLen, FileHandle file) override;
    int32_t writeFile(const void* data, size_t dataSize, FileHandle fileHandle) override;
    bool seekFileCur(int offset, FileHandle fileHandle) override;
    bool seekFile(uint32_t pos, FileHandle fileHandle) override;
    void closeFile(FileHandle fileHandle, bool deleteOnClose = true) override;
    void endFile(FileHandle file) override;
    bool isFileOpen(FileHandle file) override;
    bool enableFastSeek(FileHandle fileHandle,
                        uint32_t* clmtBuffer,
                        uint32_t clmtElementCount,
                        uint32_t* outDatabaseSector = nullptr,
                        uint32_t* outSectorsPerCluster = nullptr) override;
    bool readSectorsAsync(uint8_t* buffer, uint32_t startSector, uint32_t sectorCount) override;
    bool pollAsyncRead(bool* outSuccess = nullptr) override;
    bool waitAsyncRead() override;
    bool isAsyncReadBusy() override;

    int createInstrumentsList(const char* path,
                              char** list,
                              uint8_t list_length,
                              uint32_t* sizeOut) override;
    int countContainedFiles(const char* path) override;

    int createFilesList(const char* path,
                        uint8_t start_line,
                        char** list,
                        uint8_t list_length,
                        uint16_t max_used_memory,
                        enListFileFilter filter = enListFileFilter_all,
                        uint16_t skip_count = 0,
                        uint16_t max_files_in_subdir = 0,
                        enListFileFilter folderSizeFilter = enListFileFilter_all) override;

    void listDirContents(const char* dirPath,
                         DirContentType contentType,
                         std::function<bool(const char* name)> nameFilter,
                         char* namesBuffer,
                         int namesBufferSize,
                         int maxNameLen,
                         int& outNamesCount,
                         char** fileList) override;

    int readNbFiles(const char* romPath) override;

    bool nesDir_open(const char* path) override;
    bool nesDir_readItem(char* path, uint8_t* isDir, uint8_t* isHid, uint32_t* size) override;
    void nesDir_close() override;

    // Same roles as the IMX init()/update(): init validates the root folder,
    // update fires the mount callback once.
    void init();
    void update();

  private:
    std::string root;
    bool cardInserted = false;
    CardStateChangeCallback cardStateChangeCallback = nullptr;
    bool mountEventPending = false;
    DIR* nesDir = nullptr;
    std::string nesDirPath;

    // Host path for an SD path; falls back to a case-insensitive match per component.
    std::string resolve(const char* path);
};

#endif /* PLATFORMS_SDL_FILESYSTEMIMPL_H_ */
