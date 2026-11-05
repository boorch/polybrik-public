#include "io/PageId.h"

#include "core/interface.h"
#include "core/sdlActionList.h"
#include "display.h"

// The module globals, forward declared: their headers need the firmware's whole include
// chain. Every module class derives from ModuleBase alone (class cX : public ModuleBase),
// so the ModuleBase pointer the interface holds is the object's own address.
class cPatternEditor; class cSongEditor; class cPerformanceMode; class cInstrumentEditor;
class cSamplePlayback; class cSampleEditor; class cSampleRecorder; class cSampleImporter;
class cProjectEditor; class cConfigEditor; class cMasterParams; class cImageViewer;
extern cPatternEditor patternEditor;
extern cSongEditor songEditor;
extern cPerformanceMode performanceMode;
extern cInstrumentEditor instrumentEditor;
extern cSamplePlayback samplePlayback;
extern cSampleEditor sampleEditor;
extern cSampleRecorder sampleRecorder;
extern cSampleImporter sampleImporter;
extern cProjectEditor projectEditor;
extern cConfigEditor configEditor;
extern cMasterParams masterParams;
extern cImageViewer imageViewer;

const char* currentPageId()
{
    const void* m = mtInterface.sdlOnScreenModule();
    if (!m) return "";
    if (m == &patternEditor) return "pattern";
    if (m == &songEditor) return "song";
    if (m == &performanceMode) return "perform";
    if (m == &instrumentEditor) return "params";
    if (m == &samplePlayback) return "sample_play";
    if (m == &sampleEditor) return "sample_edit";
    if (m == &sampleRecorder) return "sample_rec";
    if (m == &sampleImporter) return "sample_load";
    if (m == &projectEditor) return "file";
    if (m == &configEditor) return "config";
    if (m == &masterParams) return "master";
    if (m == &imageViewer) return "image";
    return "";
}

bool firmwarePageSettled()
{
    return *currentPageId() && display.isControlsRefreshProcessIdle();
}

bool listVisible() { return sdlActionList.isVisible(); }
