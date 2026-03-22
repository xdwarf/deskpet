// =============================================================================
// DeskPet — leds.cpp
// =============================================================================
// Slow midnight-blue breathing effect for 3 WS2812B LEDs on GPIO9.
// Uses NeoPixelBus with the ESP32-C3 RMT backend.
// See leds.h for the design overview.
// =============================================================================

#include <Arduino.h>
#include <NeoPixelBus.h>
#include "leds.h"
#include "config.h"

// ---------------------------------------------------------------------------
// NeoPixelBus strip
// ---------------------------------------------------------------------------
// NeoGrbFeature  — GRB colour order, standard for WS2812B
// NeoEsp32Rmt0Ws2812xMethod — ESP32 RMT channel 0, 800 kHz WS2812(B) timing.
//   NeoPixelBus manages the RMT peripheral directly (not via Arduino or
//   ESP-IDF spi_master), so it does not conflict with LovyanGFX's SPI2 use.
static NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> s_strip(LED_COUNT, PIN_LED_DATA);

// ---------------------------------------------------------------------------
// Breathing parameters
// ---------------------------------------------------------------------------
// Peak colour — midnight blue. Brightness is scaled by multiplying each
// channel, so these values represent the colour at maximum brightness.
static const uint8_t BASE_R =  0;
static const uint8_t BASE_G = 20;
static const uint8_t BASE_B = 80;

// Brightness scale range: 10 → 200 (out of 255).
// Floor of 10 keeps LEDs faintly glowing at the dim end rather than
// cutting off completely, which looks more natural as a breathing effect.
static const uint8_t BRIGHT_MIN  =  10;
static const uint8_t BRIGHT_MAX  = 200;

// One full breath cycle = 4 seconds, matching the display's breathing period
// (step 0.0785 rad at 50 ms/frame → 2π ≈ 4 s), so the room light and
// Muni's face pulse in sync.
static const uint32_t BREATH_PERIOD_MS = 4000;

// Tick interval — update LEDs every 20 ms.
static const uint32_t TICK_INTERVAL_MS = 20;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint32_t s_lastTickMs = 0;

// ---------------------------------------------------------------------------
// Helper: scale a base colour by a brightness value (0–255)
// ---------------------------------------------------------------------------
static RgbColor scaleColour(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    return RgbColor(
        (uint8_t)((r * brightness) / 255),
        (uint8_t)((g * brightness) / 255),
        (uint8_t)((b * brightness) / 255)
    );
}

// ---------------------------------------------------------------------------
void ledInit() {
    s_strip.Begin();

    // Start at minimum brightness — avoids a harsh flash on power-on.
    RgbColor colour = scaleColour(BASE_R, BASE_G, BASE_B, BRIGHT_MIN);
    for (uint16_t i = 0; i < LED_COUNT; i++) {
        s_strip.SetPixelColor(i, colour);
    }
    s_strip.Show();

    Serial.printf("[LEDs] Initialised — %d WS2812B on GPIO%d (NeoPixelBus RMT)\n",
                  LED_COUNT, PIN_LED_DATA);
}

// ---------------------------------------------------------------------------
void ledTick() {
    uint32_t now = millis();
    if (now - s_lastTickMs < TICK_INTERVAL_MS) {
        return;
    }
    s_lastTickMs = now;

    // Map position in the breath cycle (0–BREATH_PERIOD_MS) to a sine wave.
    // We compute sin() over 0–2π and remap the -1..1 output to 0..1, then
    // scale into [BRIGHT_MIN, BRIGHT_MAX].
    float pos    = (float)(now % BREATH_PERIOD_MS) / (float)BREATH_PERIOD_MS; // 0.0–1.0
    float sinVal = (sinf(pos * 2.0f * PI) + 1.0f) * 0.5f;                    // 0.0–1.0
    uint8_t brightness = (uint8_t)(BRIGHT_MIN + sinVal * (BRIGHT_MAX - BRIGHT_MIN));

    RgbColor colour = scaleColour(BASE_R, BASE_G, BASE_B, brightness);
    for (uint16_t i = 0; i < LED_COUNT; i++) {
        s_strip.SetPixelColor(i, colour);
    }
    s_strip.Show();
}
