// =============================================================================
// DeskPet — main.cpp (Dual-Core FreeRTOS Version)
// =============================================================================
// Entry point for the ESP32-WROOM firmware with FreeRTOS dual-core support.
//
// CORE ARCHITECTURE
// =================
// Core 1 (Display Task):
//   - expressionTick() — animates Muni's face/sprites at fixed ~50ms interval
//   - Poll message queues for MQTT-generated expression/animation changes
//   - Isolated from any blocking I/O — guaranteed consistent frame timing
//
// Core 0 (Network Task):
//   - wifiMaintain() — handles reconnection (may block briefly)
//   - mqttMaintain() — maintains broker connection, processes incoming messages
//   - ledTick() — non-blocking LED breathing animation
//   - Can tolerate blocking I/O without affecting display framerate
//
// MQTT CALLBACK FLOW
// ==================
// onMqttMessage() (runs on Core 0 context)
//   → calls threadingQueueExpression() / threadingQueueAnimation()
//   → message queued safely
//   → returns immediately (non-blocking)
//
// Display task (Core 1, high priority)
//   ← receives queued messages
//   → calls expressionSet() / animationTrigger()
//   → no contention with MQTT
//
// KEY CHANGE FROM ORIGINAL
// =========================
// - loop() is now empty; the FreeRTOS scheduler runs the tasks instead
// - No vTaskDelete(NULL) — Arduino's loopTask still runs on Core 0,
//   just at lower priority than our networkTask
// =============================================================================

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include "display.h"
#include "expressions.h"
#include "wifi_manager.h"
#include "mqtt_client.h"
#include "sprite_manager.h"
#include "sd_card.h"
#include "leds.h"
#include "threading.h"

// ---------------------------------------------------------------------------
// Constants for Core 1 (Display Task)
// ---------------------------------------------------------------------------
static const uint32_t DISPLAY_TASK_INTERVAL_MS = 5;   // Poll every 5ms for messages/ticks
static const uint32_t DISPLAY_TASK_STACK_SIZE  = 6144; // 6 KB (sprite buffer is heap, not stack)
static const int      DISPLAY_TASK_PRIORITY    = 3;    // High priority — must not miss frames

// ---------------------------------------------------------------------------
// Constants for Core 0 (Network Task)
// ---------------------------------------------------------------------------
static const uint32_t NETWORK_TASK_INTERVAL_MS = 10;   // Poll every 10ms for WiFi/MQTT
static const uint32_t NETWORK_TASK_STACK_SIZE  = 4096; // 4 KB
static const int      NETWORK_TASK_PRIORITY    = 2;    // Medium priority

// ---------------------------------------------------------------------------
// Task handles (for monitoring/debugging, optional)
// ---------------------------------------------------------------------------
static TaskHandle_t s_displayTaskHandle = NULL;
static TaskHandle_t s_networkTaskHandle = NULL;

// ---------------------------------------------------------------------------
// Core 1 Task: Display & Animation
// ---------------------------------------------------------------------------
// Handles all time-critical display and sprite updates.
// Completely isolated from blocking I/O.
// ---------------------------------------------------------------------------
void displayTask(void *arg) {
    (void)arg; // unused parameter

    Serial.println("[Task] Display task started on Core 1");

    // Small delay to ensure Core 0 finishes its setup
    vTaskDelay(100 / portTICK_PERIOD_MS);

    while (1) {
        // Poll the expression queue for any queued expression changes
        // This is safe because we're the only consumer on Core 1
        TaskMessage msg;

        if (xQueueReceive(g_exprQueue, &msg, 0)) {
            // Non-blocking receive — pdTRUE if a message was available
            if (msg.type == TaskMessage::MSG_EXPRESSION) {
                Serial.printf("[Task] Display task received expression #%d\n", msg.data.expr.expressionId);
                expressionSet((Expression)msg.data.expr.expressionId, msg.data.expr.holdMs);
            }
        }

        // Poll animation queue
        if (xQueueReceive(g_animQueue, &msg, 0)) {
            if (msg.type == TaskMessage::MSG_ANIMATION) {
                Serial.printf("[Task] Display task received animation #%d\n", msg.data.anim.animationId);
                animationTrigger((Animation)msg.data.anim.animationId);
            }
        }

        // Step the animation — renders to sprite, calls displayFlush()
        // expressionTick() self-throttles using millis(), so it's safe to call every iteration
        expressionTick();

        // Brief yield to let other tasks on Core 1 run (if any)
        vTaskDelay(DISPLAY_TASK_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

// ---------------------------------------------------------------------------
// Core 0 Task: Networking & Status
// ---------------------------------------------------------------------------
// Handles WiFi, MQTT, and LED updates.
// Can block briefly without affecting display framerate (running on different core).
// ---------------------------------------------------------------------------
void networkTask(void *arg) {
    (void)arg; // unused parameter

    Serial.println("[Task] Network task started on Core 0");
    Serial.println("[Task] Waiting for display task to initialise...");

    // Wait a bit for the display task to finish its initial setup
    vTaskDelay(500 / portTICK_PERIOD_MS);

    while (1) {
        // Keep WiFi alive — reconnects automatically if dropped
        // May block for up to WIFI_RETRY_DELAY_MS if a reconnect is needed
        wifiMaintain();

        // Keep MQTT alive — reconnects if needed, processes incoming messages
        // onMqttMessage() callback will queue expressions/animations for Core 1
        // This is safe because the queue is FreeRTOS-protected
        mqttMaintain();

        // Step the LED breathing animation — non-blocking
        ledTick();

        // Print diagnostics periodically (every ~10 seconds)
        static uint32_t lastDiagMs = 0;
        if (millis() - lastDiagMs > 10000) {
            lastDiagMs = millis();
            threadingPrintStats();
        }

        // Sleep briefly to avoid hammering the CPU
        vTaskDelay(NETWORK_TASK_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

// ---------------------------------------------------------------------------
// setup() — runs once at boot, on the default Arduino loopTask (Core 1)
// ---------------------------------------------------------------------------
void setup() {
    // Start serial for debug output
    Serial.begin(115200);
    delay(500); // Give USB serial time to enumerate
    Serial.println("\n=== DeskPet booting (Dual-Core) ===");

    // Initialise WS2812B LEDs (NeoPixelBus, RMT channel 0)
    ledInit();

    // Initialise the GC9A01 display and allocate sprite buffer
    displayInit();
    Serial.println("[Display] Initialised");

    // Mount the SD card on the shared SPI2 bus
    // Must come after displayInit() so the SPI bus is already registered
    sdInit();

    // Show a loading/booting expression
    expressionSet(EXPR_THINKING);
    Serial.println("[Expressions] Set to THINKING (boot state)");

    // Connect to WiFi — blocks until connected
    // This is still done here (not in a task) so we can block safely during boot
    wifiConnect();

    // Connect to MQTT broker and subscribe to topics
    mqttSetup();

    // Check sprite server for updates
    spriteManagerInit();
    Serial.printf("[Sprites] Cached version: %s | Has sprites: %s\n",
                  spriteManagerCachedVersion(),
                  spriteManagerHasSprites() ? "yes" : "no (using programmatic face)");

    // Initialise the FreeRTOS message queues and create the two tasks
    threadingInit();

    Serial.println("=== DeskPet ready (Dual-Core) ===");
    Serial.println("[Task] Display task running on Core 1, Network task running on Core 0");

    // Note: we do NOT delete the loopTask here.
    // The Arduino framework's loopTask continues running on Core 0 at a lower
    // priority. Our networkTask will run alongside it at higher priority.
    // The loop() function below is effectively unused but left as a safety fallback.
}

// ---------------------------------------------------------------------------
// loop() — used as a fallback, but tasks dominate during runtime
// ---------------------------------------------------------------------------
// In the dual-core setup, this runs on Core 0 at low priority
// underneath the networkTask. It's kept here for Arduino compatibility,
// but the real work is done by FreeRTOS tasks.
// ---------------------------------------------------------------------------
void loop() {
    // This loop runs but is completely dominated by the higher-priority
    // FreeRTOS tasks. You could put safety checks here if needed, but
    // normally it just yields to the scheduler.
    vTaskDelay(100 / portTICK_PERIOD_MS);
}
