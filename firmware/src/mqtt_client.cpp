// =============================================================================
// DeskPet — mqtt_client.cpp
// =============================================================================
// MQTT connection and message handling using PubSubClient.
//
// MESSAGE FLOW
// ============
//   Homey / voice assistant  →  broker  →  ESP32 (subscribed topics)
//   ESP32                    →  broker  →  (status topics, anyone listening)
//
// CALLBACK
// ========
// onMqttMessage() is called by PubSubClient when a message arrives on any
// subscribed topic. It parses the payload string and maps it to an expression
// or animation, then calls into expressions.cpp.
//
// LWT (Last Will and Testament)
// =============================
// We register a "will" message when connecting. If the ESP32 loses power or
// crashes, the broker automatically publishes "offline" to deskpet/status.
// This lets other devices (e.g. Homey) know the desk pet is unreachable.
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "mqtt_client.h"
#include "expressions.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static WiFiClient   s_wifiClient;
static PubSubClient s_mqtt(s_wifiClient);

static uint32_t s_lastReconnectAttempt = 0; // millis() of last reconnect try

// ---------------------------------------------------------------------------
// MQTT message callback — called by PubSubClient on incoming message
// ---------------------------------------------------------------------------
static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    // Convert payload bytes to a null-terminated string for easy comparison.
    // PubSubClient does not null-terminate the payload buffer.
    char msg[64] = {0};
    size_t copyLen = (length < sizeof(msg) - 1) ? length : sizeof(msg) - 1;
    memcpy(msg, payload, copyLen);

    Serial.printf("[MQTT] Received on '%s': %s\n", topic, msg);

    // --- deskpet/expression ---
    if (strcmp(topic, TOPIC_EXPRESSION) == 0) {
        Expression expr = EXPR_NEUTRAL; // default fallback

        if      (strcmp(msg, "happy")     == 0) expr = EXPR_HAPPY;
        else if (strcmp(msg, "sad")       == 0) expr = EXPR_SAD;
        else if (strcmp(msg, "surprised") == 0) expr = EXPR_SURPRISED;
        else if (strcmp(msg, "sleepy")    == 0) expr = EXPR_SLEEPY;
        else if (strcmp(msg, "excited")   == 0) expr = EXPR_EXCITED;
        else if (strcmp(msg, "thinking")  == 0) expr = EXPR_THINKING;
        else if (strcmp(msg, "neutral")   == 0) expr = EXPR_NEUTRAL;
        else {
            Serial.printf("[MQTT] Unknown expression: '%s' — ignoring\n", msg);
            return;
        }

        // Use EXPRESSION_HOLD_MS from config.h (0 = hold until next message)
        expressionSet(expr, EXPRESSION_HOLD_MS);

        // Confirm back to the broker what we're now showing
        mqttPublish(TOPIC_CURRENT_EXPR, msg, false);
    }

    // --- deskpet/animation ---
    else if (strcmp(topic, TOPIC_ANIMATION) == 0) {
        Animation anim = ANIM_NONE;

        if      (strcmp(msg, "bounce")  == 0) anim = ANIM_BOUNCE;
        else if (strcmp(msg, "blink")   == 0) anim = ANIM_BLINK;
        else if (strcmp(msg, "yawn")    == 0) anim = ANIM_YAWN;
        else if (strcmp(msg, "breathe") == 0) anim = ANIM_BREATHE;
        else {
            Serial.printf("[MQTT] Unknown animation: '%s' — ignoring\n", msg);
            return;
        }

        animationTrigger(anim);
    }

    // --- deskpet/command ---
    else if (strcmp(topic, TOPIC_COMMAND) == 0) {
        if (strcmp(msg, "restart") == 0) {
            Serial.println("[MQTT] Received restart command — rebooting...");
            delay(500);
            ESP.restart();
        } else if (strcmp(msg, "sleep") == 0) {
            // Dim backlight and show sleepy expression
            expressionSet(EXPR_SLEEPY);
        } else if (strcmp(msg, "wake") == 0) {
            expressionSet(EXPR_NEUTRAL);
        }
    }
}

// ---------------------------------------------------------------------------
// Connect to the broker — called on first connect and on reconnect
// ---------------------------------------------------------------------------
static bool mqttConnect() {
    Serial.printf("[MQTT] Connecting to %s:%d as '%s'\n",
                  MQTT_BROKER_IP, MQTT_BROKER_PORT, MQTT_CLIENT_ID);

    bool connected;

    if (strlen(MQTT_USERNAME) > 0) {
        // Authenticated connection with LWT
        connected = s_mqtt.connect(
            MQTT_CLIENT_ID,
            MQTT_USERNAME, MQTT_PASSWORD,
            TOPIC_STATUS, 0,           // LWT topic, QoS 0
            true,                      // LWT retain
            "offline"                  // LWT payload
        );
    } else {
        // Anonymous connection with LWT
        connected = s_mqtt.connect(
            MQTT_CLIENT_ID,
            nullptr, nullptr,
            TOPIC_STATUS, 0,
            true,
            "offline"
        );
    }

    if (connected) {
        Serial.println("[MQTT] Connected!");

        // Announce we're online (retained so new subscribers see it immediately)
        s_mqtt.publish(TOPIC_STATUS, "online", true);

        // Subscribe to our input topics
        s_mqtt.subscribe(TOPIC_EXPRESSION);
        s_mqtt.subscribe(TOPIC_ANIMATION);
        s_mqtt.subscribe(TOPIC_COMMAND);

        Serial.println("[MQTT] Subscribed to topics");
    } else {
        Serial.printf("[MQTT] Connection failed, rc=%d\n", s_mqtt.state());
        // PubSubClient error codes:
        //  -4 = MQTT_CONNECTION_TIMEOUT
        //  -3 = MQTT_CONNECTION_LOST
        //  -2 = MQTT_CONNECT_FAILED
        //  -1 = MQTT_DISCONNECTED
        //   1 = MQTT_CONNECT_BAD_PROTOCOL
        //   2 = MQTT_CONNECT_BAD_CLIENT_ID
        //   3 = MQTT_CONNECT_UNAVAILABLE
        //   4 = MQTT_CONNECT_BAD_CREDENTIALS
        //   5 = MQTT_CONNECT_UNAUTHORIZED
    }

    return connected;
}

// ---------------------------------------------------------------------------
void mqttSetup() {
    s_mqtt.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
    s_mqtt.setCallback(onMqttMessage);

    // PubSubClient default buffer size is 256 bytes — increase if you plan
    // to send larger JSON payloads in Stage 3
    s_mqtt.setBufferSize(512);

    mqttConnect();
}

// ---------------------------------------------------------------------------
void mqttMaintain() {
    if (!s_mqtt.connected()) {
        uint32_t now = millis();
        // Don't hammer the broker — wait MQTT_RETRY_DELAY_MS between attempts
        if (now - s_lastReconnectAttempt >= MQTT_RETRY_DELAY_MS) {
            s_lastReconnectAttempt = now;
            mqttConnect();
        }
        return; // Don't call loop() if not connected
    }

    // Give PubSubClient CPU time to:
    //   - Process incoming messages (calls onMqttMessage)
    //   - Send keepalive PINGs to the broker
    s_mqtt.loop();
}

// ---------------------------------------------------------------------------
bool mqttPublish(const char* topic, const char* payload, bool retain) {
    if (!s_mqtt.connected()) {
        Serial.println("[MQTT] Cannot publish — not connected");
        return false;
    }
    bool ok = s_mqtt.publish(topic, payload, retain);
    if (!ok) {
        Serial.printf("[MQTT] Publish failed to '%s'\n", topic);
    }
    return ok;
}
