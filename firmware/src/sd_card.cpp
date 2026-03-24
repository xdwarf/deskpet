// =============================================================================
// DeskPet — sd_card.cpp
// =============================================================================
// Mounts an SD card on the shared SPI2 bus alongside the GC9A01 display.
// See sd_card.h for the full design rationale.
// =============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "sd_card.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static bool s_available = false;

// A dedicated SPIClass instance for the SD card.
// Using FSPI maps to SPI2_HOST on ESP32-C3 — the same peripheral LovyanGFX
// already initialised. The ESP32 Arduino core handles this gracefully: when
// begin() is called on an already-registered bus it reuses the existing
// registration rather than re-initialising it.
static SPIClass s_sdSPI(FSPI);

// ---------------------------------------------------------------------------
bool sdInit() {
    Serial.println("[SD] Initialising...");

    // Start the SPI bus for the SD card with our pin mapping.
    // LovyanGFX has already called spi_bus_initialize() for SPI2_HOST, so
    // this call configures the pin mux and sets the clock divider without
    // reinitialising the bus hardware.
    s_sdSPI.begin(
        PIN_TFT_SCLK,  // SCK  — GPIO4, shared with display
        PIN_SD_MISO,   // MISO — GPIO3
        PIN_TFT_MOSI,  // MOSI — GPIO6, shared with display
        PIN_SD_CS      // SS   — GPIO10, SD chip select
    );

    // Attempt to mount the card.
    // 25 MHz is the standard safe SD speed and reduces risk of SPI sharing
    // timing issues with the display/wifi subsystems.
    if (!SD.begin(PIN_SD_CS, s_sdSPI, 25000000)) {
        Serial.println("[SD] Mount failed — no card, wrong wiring, or unsupported format");
        Serial.println("[SD] Expected: FAT32 formatted, ≤32 GB");
        s_available = false;
        return false;
    }

    uint8_t  cardType = SD.cardType();
    uint64_t cardSize = SD.cardSize() / (1024 * 1024); // bytes → MB

    const char* typeStr = "UNKNOWN";
    switch (cardType) {
        case CARD_MMC:  typeStr = "MMC";    break;
        case CARD_SD:   typeStr = "SD";     break;
        case CARD_SDHC: typeStr = "SDHC";   break;
        default:        typeStr = "UNKNOWN"; break;
    }

    Serial.printf("[SD] Mounted — type: %s, size: %llu MB\n", typeStr, cardSize);
    s_available = true;
    return true;
}

// ---------------------------------------------------------------------------
bool sdAvailable() {
    return s_available;
}
