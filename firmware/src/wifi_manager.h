#pragma once

// =============================================================================
// DeskPet — wifi_manager.h
// =============================================================================
// Manages WiFi connection for the ESP32-C3.
//
// The ESP32-C3 has one WiFi radio. We use station mode (connect to an AP).
// The manager handles:
//   - Initial connection in setup via wifiConnect()
//   - Reconnection in the main loop via wifiMaintain()
// =============================================================================

// Attempt to connect to the WiFi network defined in config.h.
// Blocks until connected or until WIFI_RETRY_DELAY_MS has elapsed,
// then loops and tries again. Prints status to Serial.
void wifiConnect();

// Call this every loop(). Checks if WiFi is still connected and
// reconnects if not. Non-blocking — returns immediately if connected.
void wifiMaintain();

// Returns true if WiFi is currently connected (IP obtained)
bool wifiIsConnected();
