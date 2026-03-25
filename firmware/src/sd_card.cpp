// =============================================================================
// DeskPet — sd_card.cpp
// =============================================================================
// Mounts an SD card using the hardware-accelerated SDMMC controller.
// The SDMMC controller uses fixed hardware pins: CLK=14, CMD=15, D0=2, CS=13.
// Pin remapping via setPins() is not supported on ESP32-WROOM.
// See sd_card.h for the full design rationale.
// =============================================================================

#include <Arduino.h>
#include <SD_MMC.h>
#include "sd_card.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static bool s_available = false;

// ---------------------------------------------------------------------------
bool sdInit() {
    Serial.println("[SD] Initialising via SDMMC 1-bit mode...");

    // Use the hardware-accelerated SDMMC controller in 1-bit mode.
    // The default hardware pins are fixed: CLK=14, CMD=15, D0=2, CS=13.
    // The second parameter (true) enables 1-bit mode (only D0 line is used).
    // This is much faster than SPI mode and avoids bus sharing with the display.
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("[SD] Mount failed — no card, wrong wiring, or unsupported format");
        Serial.println("[SD] Expected: SDMMC 1-bit mode, FAT32 formatted, ≤32 GB");
        s_available = false;
        return false;
    }

    uint8_t  cardType = SD_MMC.cardType();
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024); // bytes → MB

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
