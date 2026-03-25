#pragma once

// =============================================================================
// DeskPet — threading.h
// =============================================================================
// FreeRTOS task management and inter-core message queues.
//
// ARCHITECTURE
// ============
// Core 0: Networking & status (WiFi, MQTT, LEDs)
// Core 1: Display & rendering (sprite player, animation state)
//
// Messages flow from Core 0 callbacks → queue → Core 1 processing.
// This avoids direct callback interference with display timing.
// =============================================================================

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ---------------------------------------------------------------------------
// Message Queue Structures
// ---------------------------------------------------------------------------

// Expression message sent from MQTT callback (Core 0) to display task (Core 1)
struct ExprMessage {
    int expressionId;      // Expression enum value
    uint32_t holdMs;       // How long to hold (0 = hold until next message)
};

// Animation message sent from MQTT callback (Core 0) to display task (Core 1)
struct AnimMessage {
    int animationId;       // Animation enum value
};

// Generic message union for a single queue
struct TaskMessage {
    enum Type {
        MSG_EXPRESSION = 0,
        MSG_ANIMATION = 1,
    } type;
    
    union {
        ExprMessage expr;
        AnimMessage anim;
    } data;
};

// ---------------------------------------------------------------------------
// Global queue handles
// ---------------------------------------------------------------------------
extern QueueHandle_t g_exprQueue;      // Expression messages (Core 0 → Core 1)
extern QueueHandle_t g_animQueue;      // Animation messages (Core 0 → Core 1)

// ---------------------------------------------------------------------------
// Task creation and cleanup
// ---------------------------------------------------------------------------
void threadingInit();                  // Called from setup()
void threadingShutdown();              // Called before reboot (optional)

// ---------------------------------------------------------------------------
// Task function prototypes
// ---------------------------------------------------------------------------
void displayTask(void *pvParameters);
void networkTask(void *pvParameters);

// ---------------------------------------------------------------------------
// Queue helpers for Core 0 (MQTT callbacks, etc.)
// =========================================================================
// These are safe to call from MQTT callback context (Core 0).

// Queue an expression change from an interrupt context (ISR-safe)
// Returns pdTRUE if queued, pdFALSE if queue was full
BaseType_t threadingQueueExpressionISR(int exprId, uint32_t holdMs);

// Queue an expression change from normal (non-ISR) context
// Returns pdTRUE if queued, pdFALSE if queue was full
BaseType_t threadingQueueExpression(int exprId, uint32_t holdMs);

// Queue an animation from normal context
BaseType_t threadingQueueAnimation(int animId);

// ---------------------------------------------------------------------------
// Debug / monitoring
// ---------------------------------------------------------------------------
// Print queue fill levels to serial (for performance analysis)
void threadingPrintStats();
