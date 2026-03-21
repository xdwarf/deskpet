// =============================================================================
// DeskPet — leds.cpp
// =============================================================================
// Slow midnight-blue breathing effect for 3 WS2812B LEDs on GPIO9.
// See leds.h for the design overview.
// =============================================================================

#include <Arduino.h>
#include <FastLED.h>
#include "leds.h"
#include "config.h"

// ---------------------------------------------------------------------------
// FastLED strip
// ---------------------------------------------------------------------------
static CRGB s_leds[LED_COUNT];

// ---------------------------------------------------------------------------
// Breathing parameters
// ---------------------------------------------------------------------------
// Base colour — midnight blue. These values are the peak (full brightness)
// colour. FastLED's setBrightness() scales all channels proportionally.
static const uint8_t BASE_R =  0;
static const uint8_t BASE_G = 20;
static const uint8_t BASE_B = 80;

// Brightness range: 10 (dim, not fully off) → 200 (vivid but not blinding).
// Keeping the floor at 10 means the LEDs are always faintly glowing rather
// than cutting off completely, which looks more natural.
static const uint8_t BRIGHT_MIN  =  10;
static const uint8_t BRIGHT_MAX  = 200;

// One full breath cycle = 4 seconds (4000 ms), matching the display's
// breathing sine period (step 0.0785 rad at 50 ms/frame → 2π ≈ 4 s).
static const uint32_t BREATH_PERIOD_MS = 4000;

// Tick interval — update LEDs every 20 ms (50 fps cap, plenty smooth).
static const uint32_t TICK_INTERVAL_MS = 20;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint32_t s_lastTickMs = 0;

// ---------------------------------------------------------------------------
void ledInit() {
    // WS2812B on GPIO9, GRB colour order (standard for WS2812B).
    // DATA_RATE_MHZ(3) is a safe conservative speed — works reliably even
    // with long wire runs.
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(s_leds, LED_COUNT)
           .setCorrection(TypicalLEDStrip);

    // Start at minimum brightness to avoid a bright flash on power-on
    FastLED.setBrightness(BRIGHT_MIN);

    // Fill all LEDs with the base colour — brightness drives the animation
    fill_solid(s_leds, LED_COUNT, CRGB(BASE_R, BASE_G, BASE_B));
    FastLED.show();

    Serial.printf("[LEDs] Initialised — %d WS2812B on GPIO%d\n", LED_COUNT, PIN_LED_DATA);
}

// ---------------------------------------------------------------------------
void ledTick() {
    uint32_t now = millis();
    if (now - s_lastTickMs < TICK_INTERVAL_MS) {
        return;
    }
    s_lastTickMs = now;

    // Map current time into a 0.0–1.0 sine wave over BREATH_PERIOD_MS.
    // sin8() is FastLED's fast 8-bit sine: input 0–255 maps to 0–2π,
    // output 0–255 maps to sin range 0–1.
    uint8_t phase     = (uint8_t)((now % BREATH_PERIOD_MS) * 255 / BREATH_PERIOD_MS);
    uint8_t sinVal    = sin8(phase);   // 0–255

    // Scale sinVal into [BRIGHT_MIN, BRIGHT_MAX]
    uint8_t brightness = map(sinVal, 0, 255, BRIGHT_MIN, BRIGHT_MAX);

    FastLED.setBrightness(brightness);
    FastLED.show();
}
