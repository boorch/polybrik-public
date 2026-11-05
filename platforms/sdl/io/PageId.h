// Stable id of the firmware page on screen ("pattern", "song", ...; "" if none), for the
// legend strip's per-page hint line. Core patch 21 exposes the module pointer.
#pragma once

const char* currentPageId();

// A firmware page is up and the display has no control refresh pending (--from-ready).
bool firmwarePageSettled();

// The action list is up (sdlActionList, core patch 24), for the legend's hint line.
bool listVisible();
