// =============================================================================
// DeskPet — wifi_manager.cpp
// =============================================================================
// WiFi connection management for the ESP32-C3.
//
// ESP32-C3 uses the same WiFi API as the classic ESP32 via the Arduino core.
// We use the simple WiFi.begin() / WiFi.status() pattern here.
//
// The connection is blocking on first connect — we don't want the rest of
// the system (MQTT etc.) to start before we have an IP address.
// Subsequent reconnections in wifiMaintain() are non-blocking checks.
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include "wifi_manager.h"
#include "config.h"

// ---------------------------------------------------------------------------
void wifiConnect() {
    Serial.printf("[WiFi] Connecting to SSID: %s\n", WIFI_SSID);

    // Set to station mode explicitly (not AP or AP+STA)
    WiFi.mode(WIFI_STA);

    // Disable WiFi power save — improves MQTT latency and reduces dropped
    // packets at the cost of slightly higher power draw (acceptable here)
    WiFi.setSleep(false);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Block until connected, printing dots to serial so you can see progress
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println(); // newline after the dots
    Serial.printf("[WiFi] Connected! IP address: %s\n", WiFi.localIP().toString().c_str());
}

// ---------------------------------------------------------------------------
void wifiMaintain() {
    if (WiFi.status() == WL_CONNECTED) {
        return; // All good
    }

    // Connection lost — attempt reconnect
    Serial.println("[WiFi] Connection lost — reconnecting...");

    // WiFi.reconnect() attempts to reconnect with the stored credentials.
    // If this doesn't work within a few seconds, we fall back to a full
    // WiFi.begin() call.
    WiFi.reconnect();

    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < WIFI_RETRY_DELAY_MS) {
        delay(200);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.printf("[WiFi] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println();
        Serial.println("[WiFi] Reconnect attempt timed out — will retry next loop");
    }
}

// ---------------------------------------------------------------------------
bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}
