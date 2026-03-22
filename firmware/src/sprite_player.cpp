// =============================================================================
// DeskPet — sprite_player.cpp
// =============================================================================
// Streams .sprite frames from SD card to the GC9A01 display in chunks.
// See sprite_player.h for the design overview.
//
// WHY CHUNKED READS (no malloc)
// ==============================
// An earlier version allocated a 115,200-byte heap buffer at boot to hold a
// full frame. This malloc reliably failed on ESP32-C3: even before WiFi,
// heap fragmentation from LGFX_Sprite's own 115 KB allocation left no
// contiguous 115 KB block available. When malloc returned nullptr the entire
// player was permanently disabled — causing both the "sprite playback
// disabled" and the silent "no sprite found" failures.
//
// The fix: a static 3,840-byte chunk buffer (8 rows × 240 px × 2 B) lives in
// BSS at link time — zero heap involvement. spritePlayerTick() reads and
// pushes CHUNK_ROWS at a time. Because each tft.pushImage() call ends with
// SPI.endTransaction() (bus_shared=true in lgfx_config.h), the SD library
// can take the SPI2 bus for the next s_file.read() without conflict.
// =============================================================================

#include <Arduino.h>
#include <SD.h>
#include "sprite_player.h"
#include "sd_card.h"
#include "display.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static const int32_t  CHUNK_ROWS         = 8;
static const uint32_t CHUNK_BYTES        = (uint32_t)CHUNK_ROWS * DISPLAY_WIDTH * 2; // 3,840
static const uint32_t FRAME_BYTES        = (uint32_t)DISPLAY_HEIGHT * DISPLAY_WIDTH * 2; // 115,200
static const uint32_t FRAME_INTERVAL_MS  = 50; // 20fps target

// Static chunk buffer — 3,840 bytes in BSS; no malloc, no fragmentation.
// CHUNK_ROWS=8 divides DISPLAY_HEIGHT=240 evenly (30 chunks/frame).
static uint8_t s_chunkBuf[CHUNK_BYTES];

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static File     s_file;
static bool     s_active      = false;
static uint32_t s_lastFrameMs = 0;
static uint32_t s_frameIndex  = 0; // 1-based, for logging only
static uint32_t s_totalFrames = 0;
static uint32_t s_loopCount   = 0;

// ---------------------------------------------------------------------------
bool spritePlayerLoad(const char* path) {
    spritePlayerStop(); // close any currently open file first

    if (!sdAvailable()) {
        Serial.println("[SpritePlayer] SD not available");
        return false;
    }
    if (!SD.exists(path)) {
        Serial.printf("[SpritePlayer] Not found: %s\n", path);
        return false;
    }

    s_file = SD.open(path, FILE_READ);
    if (!s_file) {
        Serial.printf("[SpritePlayer] Failed to open: %s\n", path);
        return false;
    }

    uint32_t fileSize = s_file.size();
    if (fileSize < FRAME_BYTES) {
        Serial.printf("[SpritePlayer] %s too small (%u B, need %lu B)\n",
                      path, fileSize, FRAME_BYTES);
        s_file.close();
        return false;
    }

    s_totalFrames = fileSize / FRAME_BYTES;
    s_frameIndex  = 0;
    s_loopCount   = 0;
    s_lastFrameMs = 0; // render first frame immediately on next tick
    s_active      = true;

    Serial.printf("[SpritePlayer] Loaded %s (%lu frame(s), %lu B)\n",
                  path, s_totalFrames, fileSize);
    return true;
}

// ---------------------------------------------------------------------------
void spritePlayerStop() {
    if (s_active) {
        s_file.close();
        s_active = false;
    }
}

// ---------------------------------------------------------------------------
bool spritePlayerActive() {
    return s_active;
}

// ---------------------------------------------------------------------------
void spritePlayerTick() {
    if (!s_active) return;

    uint32_t now = millis();
    if (now - s_lastFrameMs < FRAME_INTERVAL_MS) return;

    uint32_t frameStart = now;

    // Seek back to start when all frames have played
    if (s_frameIndex >= s_totalFrames) {
        s_file.seek(0);
        s_frameIndex = 0;
        s_loopCount++;
    }

    // Stream the frame CHUNK_ROWS at a time.
    //
    // Each iteration:
    //   1. s_file.read() — SD CS asserted, SPI2 used at 25 MHz, SD CS released.
    //   2. tft.pushImage() — display CS asserted, SPI2 used at 40 MHz,
    //      display CS released (bus_shared=true ends the transaction).
    //
    // The two operations never overlap, so SPI2 is never double-claimed.
    int32_t rowY = 0;
    while (rowY < (int32_t)DISPLAY_HEIGHT) {
        size_t got = s_file.read(s_chunkBuf, CHUNK_BYTES);
        if (got < CHUNK_BYTES) {
            Serial.printf("[SpritePlayer] Short read at row %d (%u/%lu B) — stopping\n",
                          rowY, got, CHUNK_BYTES);
            spritePlayerStop();
            return;
        }

        // pushImage for this horizontal slice.
        // With bus_shared=true LovyanGFX calls SPI.endTransaction() after
        // each call, releasing the bus for the next SD read above.
        tft.pushImage(0, rowY, DISPLAY_WIDTH, CHUNK_ROWS,
                      (lgfx::rgb565_t*)s_chunkBuf);

        rowY += CHUNK_ROWS;
    }

    s_frameIndex++;
    uint32_t elapsed = millis() - frameStart;
    s_lastFrameMs = millis();

    Serial.printf("[SpritePlayer] frame %lu/%lu loop %lu %lums\n",
                  s_frameIndex, s_totalFrames, s_loopCount, elapsed);
}
