// =============================================================================
// DeskPet — threading.cpp
// =============================================================================
// FreeRTOS queue initialization and inter-core message helpers.
//
// Provides safe-to-call queueing functions for Core 0 (MQTT callbacks) to
// communicate with Core 1 (display task) without blocking or contention.
// =============================================================================

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "threading.h"

// ---------------------------------------------------------------------------
// Global queue instances
// ---------------------------------------------------------------------------
QueueHandle_t g_exprQueue = NULL;    // Expression message queue
QueueHandle_t g_animQueue = NULL;    // Animation message queue

// Queue statistics (for debugging)
static struct {
    uint32_t exprQueued    = 0;
    uint32_t exprProcessed = 0;
    uint32_t animQueued    = 0;
    uint32_t animProcessed = 0;
    uint32_t queueOverflow = 0;
} s_stats;

// ---------------------------------------------------------------------------
// Forward declarations of task functions (defined in main.cpp)
// ---------------------------------------------------------------------------
extern void displayTask(void *arg);
extern void networkTask(void *arg);

// ---------------------------------------------------------------------------
void threadingInit() {
    // Create the expression message queue
    // Size: 10 messages of ExprMessage size
    // This is small because expressions arrive infrequently via MQTT
    g_exprQueue = xQueueCreate(10, sizeof(TaskMessage));
    if (g_exprQueue == NULL) {
        Serial.println("[Err] Failed to create expression queue!");
        return;
    }
    Serial.println("[Threading] Expression queue created");

    // Create the animation message queue
    // Size: 5 messages (one-shot animations, even fewer than expressions)
    g_animQueue = xQueueCreate(5, sizeof(TaskMessage));
    if (g_animQueue == NULL) {
        Serial.println("[Err] Failed to create animation queue!");
        return;
    }
    Serial.println("[Threading] Animation queue created");

    // Create display task on Core 1 (high priority)
    // Stack size 6144 bytes (6 KB) — sprite buffer is allocated on heap, not stack
    BaseType_t result1 = xTaskCreatePinnedToCore(
        displayTask,                    // Task function
        "DisplayTask",                  // Name (for debugging)
        6144,                           // Stack size in bytes
        NULL,                           // Parameter passed to task
        3,                              // Priority (higher = more important)
        NULL,                           // Task handle (optional)
        1                               // Core affinity: Core 1
    );

    if (result1 == pdPASS) {
        Serial.println("[Threading] Display task created on Core 1");
    } else {
        Serial.println("[Err] Failed to create display task!");
        return;
    }

    // Create network task on Core 0 (medium priority)
    // Stack size 4096 bytes (4 KB) — handles WiFi, MQTT, LED
    BaseType_t result2 = xTaskCreatePinnedToCore(
        networkTask,                    // Task function
        "NetworkTask",                  // Name (for debugging)
        4096,                           // Stack size in bytes
        NULL,                           // Parameter passed to task
        2,                              // Priority (lower than display, higher than loop)
        NULL,                           // Task handle (optional)
        0                               // Core affinity: Core 0
    );

    if (result2 == pdPASS) {
        Serial.println("[Threading] Network task created on Core 0");
    } else {
        Serial.println("[Err] Failed to create network task!");
        return;
    }

    Serial.println("[Threading] Dual-core task setup complete");
}

// ---------------------------------------------------------------------------
void threadingShutdown() {
    // Clean up queues on shutdown (optional, before reboot)
    if (g_exprQueue != NULL) {
        vQueueDelete(g_exprQueue);
        g_exprQueue = NULL;
    }
    if (g_animQueue != NULL) {
        vQueueDelete(g_animQueue);
        g_animQueue = NULL;
    }
    Serial.println("[Threading] Queues deleted");
}

// ---------------------------------------------------------------------------
// Queue helpers for Core 0
// ---------------------------------------------------------------------------

BaseType_t threadingQueueExpressionISR(int exprId, uint32_t holdMs) {
    // ISR-safe variant — can be called from interrupt context
    // (Though MQTT callbacks don't actually run in ISR, this is here for future use)
    
    if (g_exprQueue == NULL) {
        return pdFAIL;
    }

    TaskMessage msg = {
        .type = TaskMessage::MSG_EXPRESSION,
        .data = {
            .expr = {
                .expressionId = exprId,
                .holdMs = holdMs
            }
        }
    };

    BaseType_t result = xQueueSendFromISR(g_exprQueue, &msg, NULL);
    
    if (result == pdPASS) {
        s_stats.exprQueued++;
    } else {
        s_stats.queueOverflow++;
        Serial.printf("[Warn] Expression queue overflow (exprId=%d)\n", exprId);
    }

    return result;
}

BaseType_t threadingQueueExpression(int exprId, uint32_t holdMs) {
    // Normal (non-ISR) variant — called from MQTT callback on Core 0
    
    if (g_exprQueue == NULL) {
        return pdFAIL;
    }

    TaskMessage msg = {
        .type = TaskMessage::MSG_EXPRESSION,
        .data = {
            .expr = {
                .expressionId = exprId,
                .holdMs = holdMs
            }
        }
    };

    // xQueueSend with 0 timeout = non-blocking
    // Returns pdPASS if enqueued, pdFAIL if queue full
    BaseType_t result = xQueueSend(g_exprQueue, &msg, 0);

    if (result == pdPASS) {
        s_stats.exprQueued++;
    } else {
        s_stats.queueOverflow++;
        Serial.printf("[Warn] Expression queue overflow (exprId=%d)\n", exprId);
    }

    return result;
}

BaseType_t threadingQueueAnimation(int animId) {
    // Queue a one-shot animation — called from MQTT callback on Core 0
    
    if (g_animQueue == NULL) {
        return pdFAIL;
    }

    TaskMessage msg = {
        .type = TaskMessage::MSG_ANIMATION,
        .data = {
            .anim = {
                .animationId = animId
            }
        }
    };

    BaseType_t result = xQueueSend(g_animQueue, &msg, 0);

    if (result == pdPASS) {
        s_stats.animQueued++;
    } else {
        s_stats.queueOverflow++;
        Serial.printf("[Warn] Animation queue overflow (animId=%d)\n", animId);
    }

    return result;
}

// ---------------------------------------------------------------------------
void threadingPrintStats() {
    // Print queue statistics for performance monitoring (called periodically)
    
    UBaseType_t exprWaiting = (g_exprQueue != NULL) ? uxQueueMessagesWaiting(g_exprQueue) : 0;
    UBaseType_t animWaiting = (g_animQueue != NULL) ? uxQueueMessagesWaiting(g_animQueue) : 0;

    Serial.printf("[Stats] Expr queued: %lu, Anim queued: %lu, Queue overflow: %lu\n",
                  s_stats.exprQueued, s_stats.animQueued, s_stats.queueOverflow);
    Serial.printf("[Stats] Expr pending: %u, Anim pending: %u\n",
                  exprWaiting, animWaiting);
}
