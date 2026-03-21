#pragma once

// =============================================================================
// DeskPet — leds.h
// =============================================================================
// Non-blocking WS2812B LED control via FastLED.
//
// Three LEDs on GPIO9 run a continuous slow breathing effect in midnight blue
// (approximately R:0, G:20, B:80). The brightness pulses up and down on a
// sine wave with a ~4-second period — the same rhythm as the display's idle
// breathing animation, so the LEDs feel in sync with Muni's face.
//
// All timing is millis()-based. ledTick() is safe to call every loop()
// iteration — it self-throttles and never calls delay().
//
// FUTURE: ledSetColour() / ledSetExpression() hooks will let the LEDs react
// to Muni's current expression (e.g. red pulse for excited, blue for sad).
// =============================================================================

// Initialise FastLED with the LED strip configuration.
// Call once in setup(), before the main loop starts.
void ledInit();

// Advance the breathing animation by one tick.
// Call every loop() — returns immediately if not enough time has elapsed.
void ledTick();
