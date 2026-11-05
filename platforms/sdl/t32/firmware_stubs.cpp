// Firmware symbols the open synth code (core/synth/Synth.cpp) references but the helper does
// not build: the project struct, two file-manager calls and the audio lock.
// Kept apart from synthhelper.cpp because cFileManager is declared here without fileManager.h.

#include "config.h"
#include "mtStructs.h"
#include "fileTransfer.h"


strMtProject mtProject;

// The helper is single-threaded: the "audio interrupt" lock only has to nest (AudioCS in the
// archives and DISABLE_AUDIO_INTERRUPTS in Synth.cpp). A bridge or 32-bit app with a render
// thread needs a recursive mutex here.
static int audioLockDepth = 0;
void disableAudioInterrupt() { audioLockDepth++; }
void enableAudioInterrupt() { audioLockDepth--; }

bool cFileTransfer::copyFile(FileSystem*, const char*, const char*) { return false; }

// shortcut: newFileManager is the firmware's cFileManager global; Synth.cpp only calls
// createPatchesFolder() on it (getPatchList, savePatch). This stand-in has the same symbol
// name and method mangling, not the real layout. Replace with the real FileManager in a full
// 32-bit app.
class cFileManager
{
  public:
    void createPatchesFolder();
};
cFileManager newFileManager;
void cFileManager::createPatchesFolder() {}
