#pragma once

// =============================================================================
// DeskPet — sprite_player.h
// =============================================================================
// Streams pre-converted RGB565 sprite frames from SD card to the GC9A01
// display at up to 20fps.
//
// HOW IT WORKS
// ============
// Each .sprite file is a flat binary of concatenated frames:
//   frame 0: 240×240×2 = 115,200 bytes (raw little-endian RGB565)
//   frame 1: 115,200 bytes
//   ...
// Frames are looped indefinitely until spritePlayerStop() or a new
// spritePlayerLoad() is called.
//
// On each tick the player:
//   1. Checks if 50ms have elapsed since the last frame.
//   2. Reads exactly FRAME_BYTES from the open SD file into s_frameBuf.
//   3. Seeks back to the file start when EOF is reached (loop).
//   4. Calls tft.pushImage() to push pixels directly to the physical display
//      — bypassing the LGFX_Sprite double-buffer, which is left untouched so
//      the programmatic face path can resume cleanly on fallback.
//
// MEMORY
// ======
//   spritePlayerInit() allocates a single 115,200-byte heap buffer once.
//   Call it BEFORE wifiConnect() to ensure the allocation succeeds while
//   the heap is still uncontested. The buffer is never freed.
//
//   If malloc fails (logged to Serial), all spritePlayerLoad() calls return
//   false and the device falls back to programmatic face drawing.
//
// INTEGRATION
// ===========
//   expressionSet() calls spritePlayerLoad() for every expression change.
//   expressionTick() delegates to spritePlayerTick() when a sprite is loaded,
//   bypassing the programmatic blink/breathe logic.
// =============================================================================

// Allocate the frame buffer. Call once in setup(), after displayInit() and
// sdInit(), before wifiConnect().
// Returns false (and logs) if malloc fails — sprite playback will be disabled
// but the rest of the firmware continues normally.
bool spritePlayerInit();

// Open a sprite file and begin looping it.
// path: absolute SD path, e.g. "/sprites/muni/happy.sprite"
// Returns false if:
//   - spritePlayerInit() was not called or failed
//   - SD card is not available
//   - file does not exist or is too small to contain one frame
// On failure the caller should fall back to programmatic drawing.
bool spritePlayerLoad(const char* path);

// Stop playback and close the current file.
// Safe to call when no sprite is loaded.
void spritePlayerStop();

// Returns true if a sprite file is currently open and looping.
bool spritePlayerActive();

// Advance playback by one frame if 50ms have elapsed.
// Safe to call every loop() — self-throttles via millis().
// Logs achieved frame time to Serial for tuning.
void spritePlayerTick();
