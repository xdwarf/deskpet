#pragma once

// =============================================================================
// DeskPet — mqtt_client.h
// =============================================================================
// MQTT subscribe/publish interface.
//
// Uses the PubSubClient library (knolleary/PubSubClient).
// The client subscribes to the topics defined in config.h and calls
// expressionSet() / animationTrigger() when messages arrive.
//
// Publishing is used to:
//   - Announce "online" status on connect (and set up LWT for "offline")
//   - Confirm the currently displayed expression
// =============================================================================

// Call once in setup(), after WiFi is connected.
// Configures the MQTT server, sets up the callback, and connects.
void mqttSetup();

// Call every loop(). Maintains the connection and processes incoming messages.
// If the connection dropped, this reconnects before calling loop().
void mqttMaintain();

// Publish a message to a given topic. Returns true on success.
bool mqttPublish(const char* topic, const char* payload, bool retain = false);
