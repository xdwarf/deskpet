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
// The fix: a static chunk buffer (64 rows × 240 px × 2 B = 30.72 KB) lives in
// BSS at link time — zero heap involvement. spritePlayerTick() reads and
// pushes CHUNK_ROWS at a time. Because each tft.pushImage() call ends with
// SPI.endTransaction() (bus_shared=true in lgfx_config.h), the SD library
// can take the SPI2 bus for the next s_file.read() without conflict.
//
// DUAL-CORE OPTIMIZATION (ESP32-WROOM)
// =====================================
// With the dual-core refactor, Core 1 is now dedicated to display tasks,
// so we can use larger chunks (64 rows instead of 16) without blocking
// the network stack on Core 0. This reduces SPI transactions per frame
// from 15 (240÷16) to just 4 (240÷64), significantly improving throughput.
//
// LOOP vs PLAY-ONCE
// =================
// spritePlayerLoad(path, loop=true)  — neutral idle; loops forever.
// spritePlayerLoad(path, loop=false) — expression; plays through once, stops,
//   and sets s_finished = true so expressionTick() can call expressionSet(NEUTRAL).
//   spritePlayerStop() (interrupt) deliberately does NOT set s_finished — only
//   natural play-through completion triggers the return to neutral.
// =============================================================================

#include <Arduino.h>
#include <SD_MMC.h>
#include "sprite_player.h"
#include "sd_card.h"
#include "display.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
// CHUNK_ROWS = 64 is now safe with dual-core ESP32-WROOM (Core 1 dedicated to display).
// This reduces SPI transactions from 15 tpu frame to 4, improving throughput significantly.
// Buffer size: 64 × 240 × 2 = 30.72 KB (still well below available SRAM).
static const int32_t  CHUNK_ROWS         = 64;  // Optimized for dual-core display priority
static const uint32_t CHUNK_BYTES        = (uint32_t)CHUNK_ROWS * DISPLAY_WIDTH * 2; // 30,720
static const uint32_t FRAME_BYTES        = (uint32_t)DISPLAY_HEIGHT * DISPLAY_WIDTH * 2; // 115,200
static const uint32_t FRAME_INTERVAL_MS  = 33; // 30fps target (16 for 60fps testing)

// Double-buffer for chunked streaming: read next chunk while current chunk is DMA-pushed.
static uint8_t s_chunkBuf[2][CHUNK_BYTES];
static int s_activeBuf = 0; // 0 or 1

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static File     s_file;
static bool     s_active      = false;
static bool     s_loop        = true;  // true = loop forever, false = play once
static bool     s_finished    = false; // set for one tick after play-once completes
static uint32_t s_lastFrameMs = 0;
static uint32_t s_frameIndex  = 0;    // 0-based
static uint32_t s_totalFrames = 0;
static uint32_t s_loopCount   = 0;

// ---------------------------------------------------------------------------
bool spritePlayerLoad(const char* path, bool loop) {
    spritePlayerStop(); // close any currently open file first

    if (!sdAvailable()) {
        Serial.println("[SpritePlayer] SD not available");
        return false;
    }
    if (!SD_MMC.exists(path)) {
        Serial.printf("[SpritePlayer] Not found: %s\n", path);
        return false;
    }

    s_file = SD_MMC.open(path, FILE_READ);
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
    s_loop        = loop;
    s_finished    = false;
    s_lastFrameMs = 0; // render first frame immediately on next tick
    s_active      = true;

    Serial.printf("[SpritePlayer] Loaded %s (%lu frame(s), %s)\n",
                  path, s_totalFrames, loop ? "looping" : "play once");
    return true;
}

// ---------------------------------------------------------------------------
void spritePlayerStop() {
    if (s_active) {
        s_file.close();
        s_active = false;
        // s_finished is NOT set here — only natural play-through completion
        // signals the return to neutral. An interrupted sprite is just dropped.
    }
}

// ---------------------------------------------------------------------------
bool spritePlayerActive() {
    return s_active;
}

// ---------------------------------------------------------------------------
bool spritePlayerFinished() {
    // Returns true exactly once after a play-once sprite completes naturally.
    // Cleared on read so the caller only sees it for a single tick.
    bool f = s_finished;
    s_finished = false;
    return f;
}

// ---------------------------------------------------------------------------
void spritePlayerTick() {
    if (!s_active) return;

    uint32_t now = millis();
    if (now - s_lastFrameMs < FRAME_INTERVAL_MS) return;

    uint32_t frameStart = now;

    uint32_t totalReadTime = 0;
    uint32_t totalPushTime = 0;

    // End of file reached
    if (s_frameIndex >= s_totalFrames) {
        if (s_loop) {
            // Loop: seek back and keep playing
            s_file.seek(0);
            s_frameIndex = 0;
            s_loopCount++;
        } else {
            // Play-once: stop and signal completion
            Serial.printf("[SpritePlayer] Finished after %lu loop(s)\n", s_loopCount);
            s_file.close();
            s_active   = false;
            s_finished = true;
            return;
        }
    }

    // Stream the frame in CHUNK_ROWS slices with double buffering and DMA.
    // Read the next chunk while the previous chunk is being pushed.
    int32_t rowY = 0;
    int bufIndex = 0;
    bool pendingDMA = false;
    uint32_t lastPushStart = 0;

    while (rowY < (int32_t)DISPLAY_HEIGHT) {
        uint32_t remainingRows = DISPLAY_HEIGHT - rowY;
        uint32_t rowsToRead = min((uint32_t)CHUNK_ROWS, remainingRows);
        uint32_t bytesToRead = rowsToRead * DISPLAY_WIDTH * 2;

        uint32_t readStart = millis();
        size_t got = s_file.read(s_chunkBuf[bufIndex], bytesToRead);
        totalReadTime += millis() - readStart;

        if (got < bytesToRead) {
            Serial.printf("[SpritePlayer] Short read at row %d (%u/%lu B) — stopping\n",
                          rowY, got, bytesToRead);
            spritePlayerStop();
            return;
        }

        // After read completes, wait for the previous DMA push to finish so we can
        // safely launch the next one (and avoid rewriting the buffer in flight).
        if (pendingDMA) {
            tft.waitDMA();
            totalPushTime += millis() - lastPushStart;
            pendingDMA = false;
        }

        lastPushStart = millis();
        tft.pushImageDMA(0, rowY, DISPLAY_WIDTH, rowsToRead,
                         (lgfx::rgb565_t*)s_chunkBuf[bufIndex]);
        pendingDMA = true;

        rowY += rowsToRead;
        bufIndex ^= 1; // switch between 0 and 1 (double buffer)
    }

    // Wait for the final DMA push to complete before marking frame done.
    if (pendingDMA) {
        tft.waitDMA();
        totalPushTime += millis() - lastPushStart;
    }

    s_frameIndex++;
    uint32_t elapsed = millis() - frameStart;
    s_lastFrameMs = millis();

    Serial.printf("[SpritePlayer] frame %lu/%lu loop %lu %lums (Read: %lums, Push: %lums)\n",
                  s_frameIndex, s_totalFrames, s_loopCount, elapsed, totalReadTime, totalPushTime);
}
