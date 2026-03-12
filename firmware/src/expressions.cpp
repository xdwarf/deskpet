// =============================================================================
// DeskPet — expressions.cpp
// =============================================================================
// Draws Neo's animated face onto the sprite, then flushes to the display.
//
// HOW THE FACE WORKS
// ==================
// Neo's face is drawn programmatically using simple geometric primitives:
//   - Eyes: filled circles (open), thin ellipses (squinting), arcs (happy)
//   - Mouth: arc or line, width/curvature changes per expression
//   - Cheeks: small pink circles for happy/excited
//   - Pupils: small filled circles inside the eye whites
//
// The face is always centred on a 240×240 canvas. All coordinates are
// defined relative to the centre point (120, 120).
//
// ANIMATION SYSTEM
// ================
// expressionTick() is called every loop(). It checks if enough time has
// passed since the last frame (IDLE_ANIMATION_INTERVAL_MS from config.h),
// and if so:
//   1. Advances internal animation counters (blink timer, breath phase, etc.)
//   2. Calls drawFace() to render the current frame into the sprite
//   3. Calls displayFlush() to push the sprite to the display
//
// One-shot animations (yawn, bounce) use a separate state machine that
// overrides the idle animation until the sequence completes.
// =============================================================================

#include <Arduino.h>
#include "expressions.h"
#include "display.h"
#include "config.h"
#include <math.h>  // for sin(), cos(), PI

// ---------------------------------------------------------------------------
// Colour palette — 16-bit RGB565
// ---------------------------------------------------------------------------
// TFT_eSPI provides colour constants like TFT_BLACK, TFT_WHITE, etc.
// We define project-specific colours here.
static const uint16_t COL_BACKGROUND  = TFT_BLACK;
static const uint16_t COL_EYE_WHITE   = TFT_WHITE;
static const uint16_t COL_EYE_PUPIL   = 0x1082;   // Very dark grey (near black)
static const uint16_t COL_EYE_SHINE   = TFT_WHITE; // Highlight dot in pupil
static const uint16_t COL_MOUTH       = TFT_WHITE;
static const uint16_t COL_CHEEK       = 0xFBAF;    // Soft pink
static const uint16_t COL_FACE_ACCENT = 0x07FF;    // Cyan — for thinking swirl etc.

// ---------------------------------------------------------------------------
// Face geometry constants (all in pixels, relative to display centre)
// ---------------------------------------------------------------------------
static const int CX = DISPLAY_WIDTH  / 2;  // 120 — horizontal centre
static const int CY = DISPLAY_HEIGHT / 2;  // 120 — vertical centre

static const int EYE_SPACING  = 38;  // Distance from centre to each eye X
static const int EYE_Y_OFFSET = -18; // Eyes sit above centre
static const int EYE_RADIUS   = 22;  // Outer eye circle radius
static const int PUPIL_RADIUS = 11;  // Pupil radius
static const int SHINE_RADIUS =  4;  // Shine dot radius

static const int MOUTH_Y_OFFSET = 28; // Mouth sits below centre
static const int MOUTH_RADIUS   = 26; // Arc radius for smile/frown
static const int MOUTH_THICKNESS = 4; // Arc line thickness

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static Expression s_currentExpr = EXPR_NEUTRAL;
static Animation  s_currentAnim = ANIM_NONE;

static uint32_t s_lastFrameMs   = 0;    // millis() at last frame draw
static uint32_t s_exprHoldUntil = 0;   // millis() when expr auto-resets (0 = never)
static uint32_t s_animFrame     = 0;   // Current frame index within one-shot anim
static uint32_t s_animFrameMs   = 0;   // millis() at start of current anim frame

// Idle animation counters
static float    s_breathPhase   = 0.0f; // 0..2π — drives the breathing sine wave
static uint32_t s_blinkTimer    = 0;    // millis() until next blink
static bool     s_blinking      = false;
static uint32_t s_blinkFrame    = 0;    // Frame within the blink (0=open, 1=half, 2=closed)

// ---------------------------------------------------------------------------
// Helper: draw a thick arc
// LovyanGFX fillArc(x, y, r_outer, r_inner, startAngle, endAngle, colour)
// Angles are in degrees, 0=top, clockwise — same convention as TFT_eSPI.
// Unlike TFT_eSPI's drawArc, fillArc takes only one colour. A bg fill is
// unnecessary here because drawFace() clears the sprite before every frame.
// ---------------------------------------------------------------------------
static void drawArc(int x, int y, int r, int thickness, int startDeg, int endDeg, uint16_t col) {
    sprite.fillArc(x, y, r, r - thickness, startDeg, endDeg, col);
}

// ---------------------------------------------------------------------------
// Helper: draw a 4-point sparkle star (✦) centred at (x, y)
// Used for the EXCITED expression. Two crossing lines at 0°/90° and 45°/135°,
// with the diagonal arms slightly shorter for a classic star shape.
// ---------------------------------------------------------------------------
static void drawSparkle(int x, int y, int size, uint16_t col) {
    // Cardinal arms (full length)
    sprite.drawLine(x, y - size,     x,          y + size,     col);
    sprite.drawLine(x - size, y,     x + size,   y,            col);
    // Diagonal arms (60 % length for pointed-star look)
    int d = (size * 6) / 10;
    sprite.drawLine(x - d, y - d,   x + d, y + d,   col);
    sprite.drawLine(x - d, y + d,   x + d, y - d,   col);
}

// ---------------------------------------------------------------------------
// Helper: draw one eye
// eyeCX, eyeCY  — centre of this eye
// openAmount    — 1.0 = fully open, 0.0 = fully closed (horizontal line)
// expression    — shapes the pupil and white area
// ---------------------------------------------------------------------------
static void drawEye(int eyeCX, int eyeCY, float openAmount, Expression expr) {
    if (openAmount <= 0.05f) {
        // Fully closed — draw a horizontal line
        sprite.drawLine(eyeCX - EYE_RADIUS, eyeCY,
                        eyeCX + EYE_RADIUS, eyeCY, COL_EYE_WHITE);
        return;
    }

    // Vertical radius shrinks as the eye closes
    int eyeH = (int)(EYE_RADIUS * openAmount);
    if (eyeH < 2) eyeH = 2;

    if (expr == EXPR_HAPPY) {
        // Happy eyes: upward arc crescent (^ shape) — squinting with calm joy
        drawArc(eyeCX, eyeCY + 6, EYE_RADIUS, MOUTH_THICKNESS + 2, 210, 330, COL_EYE_WHITE);
    } else if (expr == EXPR_EXCITED) {
        // Excited eyes: wide-open circles — energetic, fully awake look.
        // Larger than neutral eyes, with two shine dots for extra sparkle.
        sprite.fillCircle(eyeCX, eyeCY, EYE_RADIUS + 3, COL_EYE_WHITE);
        sprite.fillCircle(eyeCX, eyeCY, PUPIL_RADIUS,   COL_EYE_PUPIL);
        sprite.fillCircle(eyeCX + 5, eyeCY - 6, SHINE_RADIUS,     COL_EYE_SHINE);
        sprite.fillCircle(eyeCX - 5, eyeCY - 2, SHINE_RADIUS - 2, COL_EYE_SHINE); // second shine
    } else if (expr == EXPR_SAD) {
        // Sad eyes: tilted — inner corners raised, outer corners dropped
        // We achieve this by drawing a slightly rotated ellipse approximated
        // with a filled ellipse and then clipping the top with the background.
        sprite.fillEllipse(eyeCX, eyeCY, EYE_RADIUS, eyeH, COL_EYE_WHITE);
        // Draw pupil
        sprite.fillCircle(eyeCX, eyeCY, PUPIL_RADIUS, COL_EYE_PUPIL);
        sprite.fillCircle(eyeCX + 5, eyeCY - 5, SHINE_RADIUS, COL_EYE_SHINE);
    } else if (expr == EXPR_SLEEPY) {
        // Sleepy eyes: half-open (top half covered by eyelid)
        sprite.fillEllipse(eyeCX, eyeCY, EYE_RADIUS, eyeH, COL_EYE_WHITE);
        // Draw "eyelid" over the top half — a filled rectangle covering the top
        sprite.fillRect(eyeCX - EYE_RADIUS - 2, eyeCY - eyeH - 2,
                        (EYE_RADIUS + 2) * 2, eyeH + 2, COL_BACKGROUND);
        // Pupil — shifted down (droopy)
        sprite.fillCircle(eyeCX, eyeCY + 4, PUPIL_RADIUS - 2, COL_EYE_PUPIL);
    } else if (expr == EXPR_SURPRISED) {
        // Surprised: extra-wide open, large pupils
        sprite.fillCircle(eyeCX, eyeCY, EYE_RADIUS + 4, COL_EYE_WHITE);
        sprite.fillCircle(eyeCX, eyeCY, PUPIL_RADIUS + 2, COL_EYE_PUPIL);
        sprite.fillCircle(eyeCX + 5, eyeCY - 5, SHINE_RADIUS, COL_EYE_SHINE);
    } else if (expr == EXPR_THINKING) {
        // Thinking: one eye normal, one eye slightly squinted (handled in drawFace)
        sprite.fillEllipse(eyeCX, eyeCY, EYE_RADIUS, eyeH, COL_EYE_WHITE);
        sprite.fillCircle(eyeCX, eyeCY, PUPIL_RADIUS, COL_EYE_PUPIL);
        sprite.fillCircle(eyeCX + 5, eyeCY - 5, SHINE_RADIUS, COL_EYE_SHINE);
    } else {
        // Neutral / default: round eyes with pupil
        sprite.fillEllipse(eyeCX, eyeCY, EYE_RADIUS, eyeH, COL_EYE_WHITE);
        sprite.fillCircle(eyeCX, eyeCY, PUPIL_RADIUS, COL_EYE_PUPIL);
        // Shine dot — slightly up and to the right for a lively look
        sprite.fillCircle(eyeCX + 5, eyeCY - 5, SHINE_RADIUS, COL_EYE_SHINE);
    }
}

// ---------------------------------------------------------------------------
// Helper: draw the mouth for a given expression
// The mouth is drawn as an arc (smile/frown) or a line (neutral/sleeping).
// ---------------------------------------------------------------------------
static void drawMouth(Expression expr, float openAmount) {
    int mx = CX;
    int my = CY + MOUTH_Y_OFFSET;

    switch (expr) {
        case EXPR_HAPPY:
            // Warm smile — moderate arc
            drawArc(mx, my - MOUTH_RADIUS + 8, MOUTH_RADIUS, MOUTH_THICKNESS, 140, 400, COL_MOUTH);
            break;

        case EXPR_EXCITED:
            // Huge grin — wider arc (130→410 vs 140→400) and thicker stroke
            drawArc(mx, my - MOUTH_RADIUS + 4, MOUTH_RADIUS + 6, MOUTH_THICKNESS + 3, 130, 410, COL_MOUTH);
            break;

        case EXPR_SAD:
            // Frown — arc from top-left to top-right (concave downward = frown)
            drawArc(mx, my + MOUTH_RADIUS - 8, MOUTH_RADIUS, MOUTH_THICKNESS, 320, 220, COL_MOUTH);
            break;

        case EXPR_SURPRISED: {
            // Open O-shaped mouth
            int r = (int)(14 * openAmount);
            if (r < 4) r = 4;
            sprite.fillEllipse(mx, my, r + 4, r, COL_EYE_WHITE);
            sprite.fillEllipse(mx, my, r, r - 4, COL_BACKGROUND);
            break;
        }

        case EXPR_SLEEPY:
            // Slightly open mouth — a small oval
            sprite.fillEllipse(mx, my, 12, 6, COL_EYE_WHITE);
            sprite.fillEllipse(mx, my, 8, 3, COL_BACKGROUND);
            break;

        case EXPR_THINKING:
            // Small asymmetric line — mouth pulled to one side
            sprite.drawLine(mx - 4, my + 2, mx + 14, my - 4, COL_MOUTH);
            sprite.drawLine(mx - 4, my + 3, mx + 14, my - 3, COL_MOUTH);
            break;

        case EXPR_NEUTRAL:
        default:
            // Flat line — neither happy nor sad
            sprite.drawLine(mx - 18, my, mx + 18, my, COL_MOUTH);
            sprite.drawLine(mx - 18, my + 1, mx + 18, my + 1, COL_MOUTH);
            break;
    }
}

// ---------------------------------------------------------------------------
// Core draw function — renders one complete frame into the sprite
// breathScale: 0.0..1.0 sine value, used to subtly shift eye Y for breathing
// blinkOpenL/R: 0.0 (closed) to 1.0 (open) for each eye independently
// ---------------------------------------------------------------------------
static void drawFace(Expression expr,
                     float breathScale,
                     float blinkOpenL,
                     float blinkOpenR) {

    // Clear the sprite to background
    sprite.fillSprite(COL_BACKGROUND);

    // Apply breathing: eyes shift up/down (±4 px) — visibly noticeable
    float breathOffset = breathScale * 4.0f;

    int leftEyeX  = CX - EYE_SPACING;
    int leftEyeY  = CY + EYE_Y_OFFSET + (int)breathOffset;
    int rightEyeX = CX + EYE_SPACING;
    int rightEyeY = CY + EYE_Y_OFFSET + (int)breathOffset;

    // For THINKING: squint the right eye slightly
    float rightOpenOverride = blinkOpenR;
    if (expr == EXPR_THINKING) {
        rightOpenOverride = blinkOpenR * 0.55f; // half-closed
    }

    // Draw eyes
    drawEye(leftEyeX,  leftEyeY,  blinkOpenL, expr);
    drawEye(rightEyeX, rightEyeY, rightOpenOverride, expr);

    // Draw cheeks — larger and brighter for EXCITED
    if (expr == EXPR_HAPPY) {
        sprite.fillEllipse(leftEyeX  - 10, leftEyeY  + 26, 16, 8, COL_CHEEK);
        sprite.fillEllipse(rightEyeX + 10, rightEyeY + 26, 16, 8, COL_CHEEK);
    } else if (expr == EXPR_EXCITED) {
        // Bigger, more saturated cheeks
        sprite.fillEllipse(leftEyeX  - 14, leftEyeY  + 30, 22, 11, COL_CHEEK);
        sprite.fillEllipse(rightEyeX + 14, rightEyeY + 30, 22, 11, COL_CHEEK);
        // Four sparkle stars scattered around the face in cyan accent colour.
        // Positions are chosen to sit in the empty space around the eyes without
        // overlapping the eye whites or the mouth arc.
        drawSparkle(leftEyeX  - 30, leftEyeY  - 24, 6, COL_FACE_ACCENT); // outer-left
        drawSparkle(rightEyeX + 30, rightEyeY - 24, 6, COL_FACE_ACCENT); // outer-right
        drawSparkle(leftEyeX  + 24, leftEyeY  - 30, 4, COL_FACE_ACCENT); // inner-left-top
        drawSparkle(rightEyeX - 24, rightEyeY - 30, 4, COL_FACE_ACCENT); // inner-right-top
    }

    // Draw mouth
    drawMouth(expr, 1.0f);

    // Thinking dots (three dots to the upper right)
    if (expr == EXPR_THINKING) {
        int dotX = CX + 50;
        int dotY = CY - 50;
        for (int i = 0; i < 3; i++) {
            sprite.fillCircle(dotX + i * 12, dotY, 4, COL_FACE_ACCENT);
        }
    }
}

// ---------------------------------------------------------------------------
// expressionSet — change the current persistent expression
// ---------------------------------------------------------------------------
void expressionSet(Expression expr, uint32_t holdMs) {
    s_currentExpr = expr;

    if (holdMs > 0) {
        s_exprHoldUntil = millis() + holdMs;
    } else if (holdMs == 0 && s_exprHoldUntil == 0) {
        // Use the config default if the caller passed 0 and there's no existing timer
        // EXPRESSION_HOLD_MS = 0 means "hold forever" per config.example.h definition
        s_exprHoldUntil = 0;
    }

    // Reset blink timer so we don't immediately blink on expression change
    s_blinkTimer = millis() + 3000;
    s_blinking   = false;
    s_blinkFrame = 0;

    Serial.printf("[Expressions] Set to %d\n", (int)expr);
}

// ---------------------------------------------------------------------------
void animationTrigger(Animation anim) {
    s_currentAnim = anim;
    s_animFrame   = 0;
    s_animFrameMs = millis();
    Serial.printf("[Expressions] Animation triggered: %d\n", (int)anim);
}

// ---------------------------------------------------------------------------
Expression expressionGet() {
    return s_currentExpr;
}

// ---------------------------------------------------------------------------
// expressionTick — advance animation state and redraw if needed
// ---------------------------------------------------------------------------
void expressionTick() {
    uint32_t now = millis();

    // Check if the expression hold timer has expired → return to neutral
    if (s_exprHoldUntil > 0 && now >= s_exprHoldUntil) {
        s_exprHoldUntil = 0;
        expressionSet(EXPR_NEUTRAL);
    }

    // Throttle frame rate to IDLE_ANIMATION_INTERVAL_MS
    if (now - s_lastFrameMs < IDLE_ANIMATION_INTERVAL_MS) {
        return;
    }
    s_lastFrameMs = now;

    // -------------------------------------------------------------------
    // Advance idle animation counters
    // -------------------------------------------------------------------

    // Breathing: advance phase by a small step each frame
    // Full cycle = 2π radians. At 50ms/frame → 20 frames/s.
    // Period of ~4 seconds: step = 2π / (4000ms / 50ms) ≈ 0.0785
    s_breathPhase += 0.0785f;
    if (s_breathPhase > 2.0f * PI) s_breathPhase -= 2.0f * PI;
    float breathScale = (sin(s_breathPhase) + 1.0f) * 0.5f; // 0.0 to 1.0

    // Blinking
    float blinkOpenL = 1.0f;
    float blinkOpenR = 1.0f;

    if (!s_blinking) {
        // Not currently blinking — check if it's time to start
        if (now >= s_blinkTimer) {
            s_blinking   = true;
            s_blinkFrame = 0;
            s_animFrameMs = now; // reuse animFrameMs for blink timing
        }
    } else {
        // Advance blink frames: open → half → closed → half → open
        // Each blink frame lasts ~60ms
        if (now - s_animFrameMs > 60) {
            s_blinkFrame++;
            s_animFrameMs = now;
        }
        switch (s_blinkFrame) {
            case 0: blinkOpenL = blinkOpenR = 1.0f;  break;
            case 1: blinkOpenL = blinkOpenR = 0.5f;  break;
            case 2: blinkOpenL = blinkOpenR = 0.05f; break;
            case 3: blinkOpenL = blinkOpenR = 0.5f;  break;
            default:
                // Blink complete — reset
                s_blinking   = false;
                s_blinkFrame = 0;
                // Schedule next blink: random-ish interval between 3 and 7 seconds
                // Using a simple pseudo-random based on millis
                s_blinkTimer = now + 3000 + (now % 4000);
                break;
        }
    }

    // -------------------------------------------------------------------
    // Handle one-shot animations (override idle state)
    // -------------------------------------------------------------------
    if (s_currentAnim != ANIM_NONE) {
        switch (s_currentAnim) {

            case ANIM_BOUNCE: {
                // Bounce: shift the whole face up/down over 8 frames (400ms)
                int offsets[] = {0, -8, -14, -8, 0, 8, 4, 0};
                int numFrames = 8;
                int yOff = offsets[s_animFrame % numFrames];

                sprite.fillSprite(COL_BACKGROUND);
                // Redraw with Y offset — just shift the eye coordinates
                drawEye(CX - EYE_SPACING, CY + EYE_Y_OFFSET + yOff, 1.0f, EXPR_HAPPY);
                drawEye(CX + EYE_SPACING, CY + EYE_Y_OFFSET + yOff, 1.0f, EXPR_HAPPY);
                sprite.fillEllipse(CX - EYE_SPACING - 10, CY + EYE_Y_OFFSET + yOff + 26, 16, 8, COL_CHEEK);
                sprite.fillEllipse(CX + EYE_SPACING + 10, CY + EYE_Y_OFFSET + yOff + 26, 16, 8, COL_CHEEK);
                drawMouth(EXPR_HAPPY, 1.0f);
                displayFlush();

                s_animFrame++;
                if (s_animFrame >= (uint32_t)numFrames) {
                    s_currentAnim = ANIM_NONE;
                    s_animFrame   = 0;
                }
                return; // skip normal draw below
            }

            case ANIM_YAWN: {
                // Yawn: eyes close, mouth opens wide over 12 frames (~600ms)
                // Then hold open for 6 frames, then close back
                int totalFrames = 20;
                float t = (float)s_animFrame / (float)totalFrames;
                float eyeOpen  = (t < 0.4f) ? (1.0f - t * 2.5f) : (t > 0.7f ? (t - 0.7f) * 3.3f : 0.0f);
                float mouthOpen = (t < 0.5f) ? t * 2.0f : 1.0f - (t - 0.5f) * 2.0f;
                if (eyeOpen < 0.0f) eyeOpen = 0.0f;
                if (mouthOpen < 0.0f) mouthOpen = 0.0f;

                sprite.fillSprite(COL_BACKGROUND);
                drawEye(CX - EYE_SPACING, CY + EYE_Y_OFFSET, eyeOpen, EXPR_SLEEPY);
                drawEye(CX + EYE_SPACING, CY + EYE_Y_OFFSET, eyeOpen, EXPR_SLEEPY);
                // Wide open mouth for yawn
                sprite.fillEllipse(CX, CY + MOUTH_Y_OFFSET, (int)(18 * mouthOpen) + 4, (int)(20 * mouthOpen) + 2, COL_EYE_WHITE);
                if (mouthOpen > 0.3f) {
                    sprite.fillEllipse(CX, CY + MOUTH_Y_OFFSET, (int)(14 * mouthOpen), (int)(14 * mouthOpen), COL_BACKGROUND);
                }
                displayFlush();

                s_animFrame++;
                if (s_animFrame >= (uint32_t)totalFrames) {
                    s_currentAnim = ANIM_NONE;
                    s_animFrame   = 0;
                    // After yawning, go sleepy
                    expressionSet(EXPR_SLEEPY);
                }
                return;
            }

            default:
                // Unknown or ANIM_NONE — fall through to normal draw
                s_currentAnim = ANIM_NONE;
                break;
        }
    }

    // -------------------------------------------------------------------
    // Normal draw: render current expression with idle animations
    // -------------------------------------------------------------------
    drawFace(s_currentExpr, breathScale, blinkOpenL, blinkOpenR);
    displayFlush();
}
