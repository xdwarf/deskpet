"""
DeskPet Voice Assistant — mqtt_bridge.py
=========================================
MQTT publish/subscribe wrapper for the voice assistant.

The voice assistant only needs to PUBLISH (it tells the ESP32 what expression
to show, and broadcasts its own status). It does not currently need to
subscribe to anything, but the subscription scaffolding is here for Stage 3
if we want the Pi to react to smart home events too.
"""

import logging
import paho.mqtt.client as mqtt


class MqttBridge:
    """
    Thin wrapper around paho-mqtt for the DeskPet voice assistant.

    Publish topics:
      deskpet/expression      — set Neo's face
      deskpet/voice/status    — report pipeline state
    """

    def __init__(self, config: dict, log: logging.Logger):
        self._config = config
        self._log = log.getChild("mqtt")
        self._client = mqtt.Client(client_id=config.get("client_id", "deskpet-voice"))

        username = config.get("username", "")
        password = config.get("password", "")
        if username:
            self._client.username_pw_set(username, password)

        self._client.on_connect    = self._on_connect
        self._client.on_disconnect = self._on_disconnect

    # -------------------------------------------------------------------------

    def connect(self):
        """Connect to the MQTT broker. Blocks until connected."""
        broker = self._config["broker"]
        port   = int(self._config.get("port", 1883))
        self._log.info(f"Connecting to {broker}:{port}")
        self._client.connect(broker, port, keepalive=60)
        # Start the background network loop so paho handles keepalives
        self._client.loop_start()

    def disconnect(self):
        self._client.loop_stop()
        self._client.disconnect()
        self._log.info("Disconnected from MQTT broker")

    # -------------------------------------------------------------------------

    def publish_expression(self, expression: str):
        """
        Tell the ESP32 desk pet to display an expression.
        Valid values: neutral, happy, sad, surprised, sleepy, excited, thinking
        """
        self._publish("deskpet/expression", expression)

    def publish_voice_status(self, status: str):
        """
        Broadcast the voice pipeline state.
        Valid values: idle, listening, processing, speaking
        """
        self._publish("deskpet/voice/status", status)

    # -------------------------------------------------------------------------

    def _publish(self, topic: str, payload: str, retain: bool = False):
        result = self._client.publish(topic, payload, qos=0, retain=retain)
        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            self._log.error(f"Failed to publish to {topic}: rc={result.rc}")
        else:
            self._log.debug(f"Published {payload!r} → {topic}")

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self._log.info("Connected to MQTT broker")
        else:
            self._log.error(f"MQTT connection failed, rc={rc}")

    def _on_disconnect(self, client, userdata, rc):
        if rc != 0:
            self._log.warning(f"Unexpected MQTT disconnect, rc={rc}")
