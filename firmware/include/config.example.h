#pragma once

// =============================================================================
// DeskPet — Configuration Template
// =============================================================================
// Copy this file to config.h and fill in your own values.
// config.h is gitignored — never commit credentials.
// =============================================================================

// -----------------------------------------------------------------------------
// WiFi
// -----------------------------------------------------------------------------
#define WIFI_SSID     "YourNetworkName"
#define WIFI_PASSWORD "YourWiFiPassword"

// How long to wait (ms) before retrying a failed WiFi connection
#define WIFI_RETRY_DELAY_MS 5000

// -----------------------------------------------------------------------------
// MQTT Broker
// -----------------------------------------------------------------------------
#define MQTT_BROKER_IP   "192.168.2.14"
#define MQTT_BROKER_PORT 1883

// Client ID — must be unique on the broker
#define MQTT_CLIENT_ID "deskpet-esp32"

// Optional: set to "" if your broker has no auth
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""

// How long to wait (ms) before retrying a failed MQTT connection
#define MQTT_RETRY_DELAY_MS 5000

// -----------------------------------------------------------------------------
// MQTT Topics
// -----------------------------------------------------------------------------
#define TOPIC_EXPRESSION        "deskpet/expression"
#define TOPIC_ANIMATION         "deskpet/animation"
#define TOPIC_COMMAND           "deskpet/command"
#define TOPIC_STATUS            "deskpet/status"
#define TOPIC_CURRENT_EXPR      "deskpet/current_expression"

// -----------------------------------------------------------------------------
// Display — GC9A01 SPI Pin Mapping
// These must match your physical wiring AND the TFT_eSPI User_Setup.h
// -----------------------------------------------------------------------------
#define PIN_TFT_SCLK  4   // SPI clock
#define PIN_TFT_MOSI  6   // SPI data (MOSI)
#define PIN_TFT_RST   8   // Display reset
#define PIN_TFT_DC    7   // Data / Command select
#define PIN_TFT_CS    5   // Chip select
// #define PIN_TFT_BL    2   // Backlight — uncomment if your display has a BL pin wired to GPIO2
                              // Leave commented if the display has no BL pin (backlight always on)

// Display resolution — GC9A01 is always 240x240
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 240

// -----------------------------------------------------------------------------
// Behaviour
// -----------------------------------------------------------------------------
// How often (ms) to step the idle animation (blink, breathe, etc.)
#define IDLE_ANIMATION_INTERVAL_MS 50

// How long (ms) a triggered expression lasts before returning to idle
// Set to 0 for "hold until next MQTT message"
#define EXPRESSION_HOLD_MS 8000
