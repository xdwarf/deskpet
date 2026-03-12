"""
DeskPet Voice Assistant — main.py
===================================
Entry point for the voice assistant pipeline running on the Raspberry Pi 3B+.

Pipeline overview:
  1. Connect to MQTT broker (to signal Neo's face state)
  2. Start listening for the wake word
  3. On wake word detection:
     a. Tell ESP32 → "thinking" face (via MQTT)
     b. Record microphone until silence or max duration
     c. Run speech-to-text (Whisper, Danish)
     d. Send transcribed text to Gemini AI
     e. Run text-to-speech on the response (gTTS, Danish)
     f. Cast audio directly to Google Nest via pychromecast
     g. Tell ESP32 → "happy" face
  4. Return to wake word listening

This file wires the modules together. The actual logic lives in:
  wake_word.py  — wake word detection
  stt.py        — speech to text
  ai_client.py  — Gemini API
  tts.py        — text to speech
  mqtt_bridge.py — MQTT publish/subscribe

NOTE: This is a Stage 2 skeleton. Some modules are stubs until the
hardware is in hand and dependencies are confirmed on the Pi.
"""

import sys
import logging
import yaml
import colorlog

from mqtt_bridge import MqttBridge
from wake_word import WakeWordDetector
from stt import SpeechToText
from ai_client import AiClient
from tts import TextToSpeech


def load_config(path: str = "config/config.yaml") -> dict:
    """Load YAML config file and return as dict."""
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def setup_logging(level: str = "INFO") -> logging.Logger:
    """Configure coloured console logging."""
    handler = colorlog.StreamHandler()
    handler.setFormatter(colorlog.ColoredFormatter(
        "%(log_color)s%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%H:%M:%S"
    ))
    logger = logging.getLogger("deskpet")
    logger.addHandler(handler)
    logger.setLevel(getattr(logging, level.upper(), logging.INFO))
    return logger


def main():
    # -------------------------------------------------------------------------
    # Load config
    # -------------------------------------------------------------------------
    try:
        config = load_config()
    except FileNotFoundError:
        print("ERROR: config/config.yaml not found.")
        print("Copy config/config.example.yaml to config/config.yaml and fill in your values.")
        sys.exit(1)

    log = setup_logging(config.get("logging", {}).get("level", "INFO"))
    log.info("=== DeskPet Voice Assistant starting ===")

    # -------------------------------------------------------------------------
    # Initialise modules
    # -------------------------------------------------------------------------
    mqtt = MqttBridge(config["mqtt"], log)
    wake = WakeWordDetector(config["wake_word"], config["audio"], log)
    stt  = SpeechToText(config["stt"], config["audio"], log)
    ai   = AiClient(config["ai"], log)
    tts  = TextToSpeech(config["tts"], config["chromecast"], log)

    mqtt.connect()
    mqtt.publish_expression("neutral")

    log.info("Listening for wake word...")

    # -------------------------------------------------------------------------
    # Main loop
    # -------------------------------------------------------------------------
    while True:
        try:
            # Block until we hear the wake word
            wake.wait_for_wake_word()
            log.info("Wake word detected!")

            # Signal Neo to look like he's thinking
            mqtt.publish_expression("thinking")
            mqtt.publish_voice_status("listening")

            # Record and transcribe the user's question
            audio_data = stt.record_audio()
            mqtt.publish_voice_status("processing")

            transcript = stt.transcribe(audio_data)
            log.info(f"Transcribed: {transcript!r}")

            if not transcript.strip():
                log.warning("Empty transcription — ignoring")
                mqtt.publish_expression("neutral")
                mqtt.publish_voice_status("idle")
                continue

            # Ask Claude
            response = ai.ask(transcript)
            log.info(f"AI response: {response!r}")

            # Speak the response
            mqtt.publish_voice_status("speaking")
            tts.speak(response)

            # Neo looks happy after responding
            mqtt.publish_expression("happy")
            mqtt.publish_voice_status("idle")

        except KeyboardInterrupt:
            log.info("Shutting down (KeyboardInterrupt)")
            mqtt.publish_expression("neutral")
            mqtt.disconnect()
            break
        except Exception as e:
            log.error(f"Unexpected error in main loop: {e}", exc_info=True)
            mqtt.publish_expression("neutral")
            mqtt.publish_voice_status("idle")
            # Continue rather than crash — the desk pet should keep running


if __name__ == "__main__":
    main()
