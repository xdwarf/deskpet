#pragma once

// =============================================================================
// DeskPet — sd_card.h
// =============================================================================
// SD card initialisation using the hardware-accelerated SDMMC controller.
//
// The SDMMC controller uses fixed GPIO pins:
//   CLK: GPIO14
//   CMD: GPIO15
//   D0:  GPIO2
//   CS:  GPIO13 (for 1-bit mode fallback, though not strictly needed)
//
// The display uses a separate VSPI (SPI3) controller on different pins,
// so there is no bus contention or shared bus concerns. The SDMMC controller
// provides hardware acceleration and better performance than SPI mode.
//
// LovyanGFX must be initialised first (tft.init() in displayInit()) before
// sdInit() is called, to ensure the SPI3 pin mux configuration doesn't
// interfere with SDMMC.
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
