#pragma once

// =============================================================================
// DeskPet — sd_card.h
// =============================================================================
// SD card initialisation on the shared SPI2 bus.
//
// The SD card shares SCK (GPIO4) and MOSI (GPIO6) with the GC9A01 display.
// It gets its own MISO (GPIO3) and CS (GPIO10) lines.
//
// LovyanGFX must be initialised first (tft.init() in displayInit()) before
// sdInit() is called. LovyanGFX calls spi_bus_initialize() for SPI2_HOST;
// once the bus is registered, the Arduino SPIClass can add the SD card as a
// second device on the same host. bus_shared=true in lgfx_config.h tells
// LovyanGFX to release the bus between transactions so the SD library can
// take turns.
//
// FUTURE USE: RGB565 sprite animation frames will be read from the SD card
// and streamed directly to the display using tft.pushImage().
// =============================================================================

// Call once in setup(), after displayInit().
// Mounts the SD card and logs success or failure to Serial.
// Returns true if the card mounted successfully.
bool sdInit();

// Returns true if the SD card is currently mounted and usable.
bool sdAvailable();
