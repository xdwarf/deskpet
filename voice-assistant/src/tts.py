"""
DeskPet Voice Assistant — tts.py
==================================
Converts text to speech in Danish and plays it on the Google Nest speaker
via AirCast on the home server.

FLOW:
  1. Generate Danish audio from text using gTTS → save to MP3 file
  2. POST the MP3 file to AirCast's HTTP API
  3. AirCast casts it to the configured Google Nest speaker

AIRCAST API:
AirCast exposes a simple HTTP endpoint. The exact API depends on which
AirCast implementation is running on your server. The most common one
(hoanghoa/aircast on Docker) has:
  POST /play   — body: {"url": "http://...audio.mp3"}  or file upload

NOTE: The AirCast integration is a stub — the exact API endpoint and
authentication will need to be confirmed against your running instance.
See docs/hardware-notes.md for setup notes.

ALTERNATIVE:
If AirCast proves difficult, we can serve the MP3 from the Pi itself
(using a tiny HTTP server) and tell AirCast to fetch it by URL.
"""

import logging
import os
import requests
from gtts import gTTS


class TextToSpeech:
    """
    Danish text-to-speech via gTTS, with output pushed to Google Nest via AirCast.
    """

    def __init__(self, tts_config: dict, aircast_config: dict, log: logging.Logger):
        self._log         = log.getChild("tts")
        self._language    = tts_config.get("language", "da")
        self._output_file = tts_config.get("output_file", "/tmp/neo_response.mp3")
        self._aircast_url = (
            f"http://{aircast_config['server']}:{aircast_config.get('port', 5000)}"
        )
        self._speaker     = aircast_config.get("speaker_name", "")
        self._log.info(f"TTS initialised (lang={self._language}, AirCast={self._aircast_url})")

    def speak(self, text: str) -> None:
        """
        Convert text to MP3 and play it on the Google Nest speaker.
        Blocks until the file has been sent to AirCast (does not wait for
        playback to finish — AirCast handles that).
        """
        if not text.strip():
            self._log.warning("speak() called with empty text — skipping")
            return

        # Step 1: Generate audio file
        self._log.info(f"Generating TTS for: {text!r}")
        try:
            tts = gTTS(text=text, lang=self._language, slow=False)
            tts.save(self._output_file)
            self._log.debug(f"TTS saved to {self._output_file}")
        except Exception as e:
            self._log.error(f"gTTS generation failed: {e}")
            return

        # Step 2: Push to AirCast
        # AirCast needs a URL to fetch the audio from. Options:
        #   a) Serve the file from the Pi (simplest)
        #   b) Upload directly if AirCast supports it
        # For now we attempt a direct file POST — adjust if your AirCast differs.
        self._cast_audio()

    def _cast_audio(self) -> None:
        """
        Send the generated audio file to AirCast for playback on the Nest.
        NOTE: This is a placeholder — the exact AirCast API call depends on
        your setup. Uncomment/adjust the appropriate block below.
        """
        self._log.info(f"Casting audio via AirCast at {self._aircast_url}")

        # --- Option A: POST audio file as multipart upload ---
        # This works if AirCast exposes a /cast or /play endpoint that accepts files.
        try:
            with open(self._output_file, "rb") as f:
                response = requests.post(
                    f"{self._aircast_url}/cast",
                    files={"audio": ("response.mp3", f, "audio/mpeg")},
                    data={"speaker": self._speaker},
                    timeout=10
                )
            if response.status_code == 200:
                self._log.info("Audio cast successfully")
            else:
                self._log.warning(f"AirCast returned status {response.status_code}: {response.text}")
        except requests.RequestException as e:
            self._log.error(f"Failed to reach AirCast: {e}")
            self._log.info("Falling back to local playback (requires speakers on Pi)")
            self._local_playback()

    def _local_playback(self) -> None:
        """
        Fallback: play the audio locally on the Pi if AirCast is unavailable.
        Requires speakers connected to the Pi (not the Nest).
        """
        try:
            os.system(f"mpg123 -q {self._output_file}")
        except Exception as e:
            self._log.error(f"Local playback failed: {e}")
