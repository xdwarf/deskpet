// =============================================================================
// DeskPet — main.cpp
// =============================================================================
// Entry point for the ESP32-C3 firmware.
//
// Responsibilities:
//   - Initialise all subsystems (display, WiFi, MQTT)
//   - Run the main loop: keep WiFi/MQTT alive, tick animations, handle messages
//
// The code is intentionally split across multiple files so each concern
// (display, expressions, networking) can be developed and read independently.
// =============================================================================

#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "expressions.h"
#include "wifi_manager.h"
#include "mqtt_client.h"

// ---------------------------------------------------------------------------
// setup() — runs once at boot
// ---------------------------------------------------------------------------
void setup() {
    // Start serial for debug output — open the PlatformIO serial monitor at
    // 115200 baud to read these messages.
    Serial.begin(115200);
    delay(500); // Give USB serial time to enumerate on ESP32-C3
    Serial.println("\n=== DeskPet booting ===");

    // Initialise the GC9A01 display first so Kobo's face appears quickly,
    // even before WiFi connects. This gives visual feedback that the device
    // is alive.
    displayInit();
    Serial.println("[Display] Initialised");

    // Show a loading/booting expression while we connect to the network
    expressionSet(EXPR_THINKING);
    Serial.println("[Expressions] Set to THINKING (boot state)");

    // Connect to WiFi — this blocks until connected (with timeout + retry)
    wifiConnect();

    // Connect to MQTT broker and subscribe to our topics
    mqttSetup();

    Serial.println("=== DeskPet ready ===");

    // Switch to idle/neutral expression once fully connected
    expressionSet(EXPR_NEUTRAL);
}

// ---------------------------------------------------------------------------
// loop() — runs repeatedly after setup()
// ---------------------------------------------------------------------------
void loop() {
    // Keep WiFi alive — reconnects automatically if the connection drops
    wifiMaintain();

    // Keep MQTT alive — reconnects if needed, and processes incoming messages.
    // Incoming messages call back into onMqttMessage() in mqtt_client.cpp,
    // which then calls expressionSet() or animationTrigger().
    mqttMaintain();

    // Step the current animation frame.
    // expressionTick() checks millis() internally and only redraws when
    // the frame interval has elapsed — so calling it every loop() is safe
    // and won't thrash the display.
    expressionTick();
}
