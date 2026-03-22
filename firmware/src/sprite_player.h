#pragma once

// =============================================================================
// DeskPet — sprite_player.h
// =============================================================================
// Streams pre-converted RGB565 sprite frames from SD card to the GC9A01
// display in 8-row chunks at ~20fps.
//
// HOW IT WORKS
// ============
// Each .sprite file is a flat binary of concatenated 240×240 frames:
//   frame 0: 240×240×2 = 115,200 bytes (raw little-endian RGB565)
//   frame 1: 115,200 bytes  ...  (frames concatenated, no header)
//
// spritePlayerTick() streams one frame per 50ms by reading CHUNK_ROWS (8)
// rows of pixels from SD, then calling tft.pushImage() for that slice,
// alternating SD-read / display-write until 240 rows are sent. Each
// tft.pushImage() ends with SPI.endTransaction() (bus_shared=true in
// lgfx_config.h), so the SD library can reclaim SPI2 for the next read.
//
// NO HEAP ALLOCATION
// ==================
// The 3,840-byte chunk buffer (8×240×2) is a static array in BSS — it exists
// at link time and never touches malloc. This avoids the fragmentation issues
// that prevent a 115 KB malloc from succeeding on ESP32-C3 after LGFX_Sprite
// has already claimed its 115 KB block.
//
// No initialisation function is required. Simply call spritePlayerLoad() when
// you want to start playing a file.
//
// INTEGRATION
// ===========
//   expressionSet() calls spritePlayerLoad() on every expression change.
//   spritePlayerLoad() returns false if the file is missing, which signals
//   the caller to fall back to programmatic face drawing.
//   expressionTick() delegates to spritePlayerTick() when a sprite is active,
//   bypassing the programmatic blink/breathe path.
// =============================================================================

// Open a sprite file and begin looping it.
// path: absolute SD path, e.g. "/sprites/muni/happy.sprite"
// Returns false if SD is unavailable, file not found, or file too small.
// On false the caller should fall back to programmatic drawing.
bool spritePlayerLoad(const char* path);

// Stop playback and close the current file.
// Safe to call when nothing is playing.
void spritePlayerStop();

// Returns true if a sprite file is currently open and looping.
bool spritePlayerActive();

// Advance playback by one frame if 50ms have elapsed.
// Safe to call every loop() — self-throttles via millis().
// Logs achieved frame time to Serial for tuning.
void spritePlayerTick();
