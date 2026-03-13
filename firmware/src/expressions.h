#pragma once

// =============================================================================
// DeskPet — expressions.h
// =============================================================================
// Public interface for Muni's face expressions and animations.
//
// Design overview:
//   - An "expression" is a persistent mood/state (happy, sad, etc.) that
//     Muni holds until told to change. It has idle animations (breathing,
//     blinking) that play continuously within it.
//   - An "animation" is a one-shot event (yawn, bounce) that plays once
//     and then returns to the current expression.
//   - expressionTick() advances the animation state machine. Call it every
//     loop() — it uses millis() internally and only redraws when needed.
// =============================================================================

// ---------------------------------------------------------------------------
// Expression IDs
// ---------------------------------------------------------------------------
// These map to MQTT payload strings in mqtt_client.cpp
enum Expression {
    EXPR_NEUTRAL   = 0,
    EXPR_HAPPY     = 1,
    EXPR_SAD       = 2,
    EXPR_SURPRISED = 3,
    EXPR_SLEEPY    = 4,
    EXPR_EXCITED   = 5,
    EXPR_THINKING  = 6,
};

// ---------------------------------------------------------------------------
// Animation IDs (one-shot events)
// ---------------------------------------------------------------------------
enum Animation {
    ANIM_NONE   = 0,
    ANIM_BOUNCE = 1,  // Whole face bounces up/down (arrived home)
    ANIM_BLINK  = 2,  // Single eye blink
    ANIM_YAWN   = 3,  // Mouth opens wide, eyes squeeze (good morning)
    ANIM_BREATHE = 4, // Subtle scale pulse (idle breathing)
};

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

// Set the current persistent expression.
// If holdMs > 0, the expression will automatically return to EXPR_NEUTRAL
// after that many milliseconds (using the value from config.h by default).
// Pass holdMs=0 to hold indefinitely until the next MQTT message.
void expressionSet(Expression expr, uint32_t holdMs = 0);

// Trigger a one-shot animation. The animation plays to completion and then
// the display returns to the current expression.
void animationTrigger(Animation anim);

// Advance the animation state machine by one tick.
// Call this every loop() — it self-throttles using millis().
void expressionTick();

// Return the currently active expression
Expression expressionGet();
