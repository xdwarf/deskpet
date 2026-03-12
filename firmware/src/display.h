#pragma once

// =============================================================================
// DeskPet — display.h
// =============================================================================
// Public interface for the display subsystem.
//
// Wraps LovyanGFX so the rest of the codebase doesn't need to know about it
// directly. The global `tft` and `sprite` objects are declared here and
// defined in display.cpp so other files (expressions.cpp etc.) can use them.
//
// RENDERING MODEL — double-buffered sprite:
//   1. Draw everything into `sprite` (a RAM framebuffer)
//   2. Call displayFlush() to push the sprite to the display in one SPI burst
//   Result: flicker-free animation — the display never shows a half-drawn frame.
//
// For a 240×240 display at 16-bit colour depth:
//   240 × 240 × 2 = 115,200 bytes ≈ 113 KB of heap
//   The ESP32-C3 has 400 KB of SRAM so this is comfortable.
// =============================================================================

#include "lgfx_config.h"   // LGFX class: panel + bus wiring for our hardware

// LovyanGFX sprite type — off-screen framebuffer, pushed to display as one
// SPI transaction each frame.
using LGFX_Sprite = lgfx::LGFX_Sprite;

// Global objects — defined in display.cpp
extern LGFX        tft;
extern LGFX_Sprite sprite;

// Initialise display hardware, send GC9A01 init sequence, allocate sprite.
// Call once in setup() before any drawing.
void displayInit();

// Fill the physical display with a solid colour. Used for clearing when not
// using the sprite workflow (e.g., the diagnostic red screen in displayInit).
void displayClear(uint16_t colour = TFT_BLACK);

// Push the current sprite contents to the physical display.
// Call this at the end of every rendered frame.
void displayFlush();
