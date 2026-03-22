// =============================================================================
// DeskPet — sprite_player.cpp
// =============================================================================
// Streams .sprite frames from SD card directly to the GC9A01 display.
// See sprite_player.h for the design overview.
// =============================================================================

#include <Arduino.h>
#include <SD.h>
#include "sprite_player.h"
#include "sd_card.h"
#include "display.h"   // for tft, DISPLAY_WIDTH, DISPLAY_HEIGHT
#include "config.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static const uint32_t FRAME_BYTES       = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2; // 115,200
static const uint32_t FRAME_INTERVAL_MS = 50; // 20fps target

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint8_t*  s_frameBuf    = nullptr; // heap-allocated frame buffer
static File      s_file;                  // currently open sprite file
static bool      s_active      = false;
static uint32_t  s_lastFrameMs = 0;
static uint32_t  s_frameIndex  = 0;      // 0-based frame index in current loop
static uint32_t  s_totalFrames = 0;
static uint32_t  s_loopCount   = 0;

// ---------------------------------------------------------------------------
bool spritePlayerInit() {
    s_frameBuf = (uint8_t*)malloc(FRAME_BYTES);
    if (!s_frameBuf) {
        Serial.printf("[SpritePlayer] ERROR: malloc(%lu) failed — sprite playback disabled\n",
                      FRAME_BYTES);
        return false;
    }
    Serial.printf("[SpritePlayer] Frame buffer allocated (%lu bytes free heap remaining)\n",
                  (unsigned long)ESP.getFreeHeap());
    return true;
}

// ---------------------------------------------------------------------------
bool spritePlayerLoad(const char* path) {
    // Always stop any current playback first
    spritePlayerStop();

    if (!s_frameBuf) {
        // malloc failed at init — nothing we can do
        return false;
    }
    if (!sdAvailable()) {
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

    size_t fileSize = s_file.size();
    if (fileSize < FRAME_BYTES) {
        Serial.printf("[SpritePlayer] %s too small (%u bytes, need %lu)\n",
                      path, fileSize, FRAME_BYTES);
        s_file.close();
        return false;
    }

    s_totalFrames = fileSize / FRAME_BYTES;
    s_frameIndex  = 0;
    s_loopCount   = 0;
    s_lastFrameMs = 0; // force first frame immediately on next tick
    s_active      = true;

    Serial.printf("[SpritePlayer] Loaded %s (%lu frame(s))\n", path, s_totalFrames);
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
    if (!s_active || !s_frameBuf) return;

    uint32_t now = millis();
    if (now - s_lastFrameMs < FRAME_INTERVAL_MS) return;

    uint32_t frameStart = now;

    // If we've exhausted all frames, loop back
    if (s_frameIndex >= s_totalFrames) {
        s_file.seek(0);
        s_frameIndex = 0;
        s_loopCount++;
    }

    // Read one frame from SD into the frame buffer
    size_t bytesRead = s_file.read(s_frameBuf, FRAME_BYTES);
    if (bytesRead < FRAME_BYTES) {
        // Unexpected short read — stop rather than display garbage
        Serial.printf("[SpritePlayer] Short read at frame %lu: got %u/%lu bytes — stopping\n",
                      s_frameIndex, bytesRead, FRAME_BYTES);
        spritePlayerStop();
        return;
    }

    // Push raw RGB565 pixels directly to the physical display.
    // Casting to lgfx::rgb565_t lets LovyanGFX apply the panel-specific
    // colour order and inversion transforms so colours are correct on the
    // GC9A01 (same transforms used for programmatic drawing).
    tft.pushImage(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                  (lgfx::rgb565_t*)s_frameBuf);

    uint32_t elapsed = millis() - frameStart;
    s_lastFrameMs = millis();

    Serial.printf("[SpritePlayer] frame %lu/%lu loop %lu %lums\n",
                  s_frameIndex + 1, s_totalFrames, s_loopCount, elapsed);

    s_frameIndex++;
}
