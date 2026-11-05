#pragma once

class Canvas;

// Writes the canvas as an 8-bit RGB PNG (alpha dropped). Returns false on failure.
bool savePng(const Canvas& canvas, const char* path);
