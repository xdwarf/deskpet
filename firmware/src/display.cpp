// =============================================================================
// DeskPet — display.cpp
// =============================================================================
// Implements the display subsystem using LovyanGFX.
//
// KEY DIFFERENCE FROM TFT_eSPI:
//   LovyanGFX calls spi_bus_initialize() from the ESP-IDF spi_master driver
//   directly inside tft.init(). It never goes through Arduino's SPIClass
//   wrapper, which was causing a TG1WDT_SYS_RST crash on ESP32-C3 SuperMini.
//   No SPI.begin() or watchdog workarounds are needed here.
//
// RENDERING MODEL — double-buffering with LGFX_Sprite:
//   LGFX_Sprite allocates (width × height × 2) bytes of heap RAM (16-bit colour).
//   We draw into this buffer, then push it to the physical display in one
//   SPI burst. This eliminates visible tearing/flicker.
//
//   For 240×240 @ 16-bit: 240 × 240 × 2 = 115,200 bytes ≈ 113 KB.
//   The ESP32-C3 has 400 KB SRAM, so this is fine.
// =============================================================================

#include "display.h"
#include "config.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Global objects — declared extern in display.h
// ---------------------------------------------------------------------------

// The main display driver. Config (pins, driver, SPI host) lives in lgfx_config.h.
LGFX tft;

// Off-screen framebuffer. We draw into this, then push to tft each frame.
LGFX_Sprite sprite(&tft);

// ---------------------------------------------------------------------------
void displayInit() {
    Serial.println("[Display] Calling tft.init()...");
    Serial.flush();

    // LovyanGFX initialises SPI3 via spi_bus_initialize() internally.
    // No SPI.begin() needed — the library owns the bus entirely.
    tft.init();

    Serial.println("[Display] tft.init() done, setting rotation...");
    Serial.flush();
    tft.setRotation(0);

    // --- DISPLAY DIAGNOSTIC ---
    // Fill red to confirm SPI comms and pixel output are working.
    // If you see red on boot, the driver and wiring are good. Remove once confirmed.
    Serial.println("[Display] Filling RED (diagnostic)...");
    Serial.flush();
    tft.fillScreen(TFT_RED);
    delay(1000);
    // --------------------------

    tft.fillScreen(TFT_BLACK);

    // Allocate the sprite framebuffer.
    // setColorDepth(16) = RGB565 — same as the display's native format,
    // so no conversion is needed when pushing pixels.
    Serial.println("[Display] Creating sprite...");
    Serial.flush();

    // On boards with PSRAM, prefer PSRAM for the framebuffer to reduce DMA SRAM
    // pressure. If PSRAM is not available, fall back to internal SRAM.
#if defined(ESP32) || defined(ESP8266)
    if (psramFound()) {
        sprite.setPsram(true);
        Serial.println("[Display] PSRAM detected — using PSRAM for sprite buffer");
    } else {
        sprite.setPsram(false);
        Serial.println("[Display] No PSRAM detected — using internal SRAM for sprite buffer");
    }
#endif

    sprite.setColorDepth(16);
    sprite.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT);

    Serial.println("[Display] Init complete.");
    Serial.flush();
}

// ---------------------------------------------------------------------------
void displayClear(uint16_t colour) {
    tft.fillScreen(colour);
}

// ---------------------------------------------------------------------------
void displayFlush() {
    // Push the entire sprite buffer to the display starting at pixel (0, 0).
    // LovyanGFX handles the SPI DMA transaction internally.
    sprite.pushSprite(0, 0);
}
