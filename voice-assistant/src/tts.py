"""
DeskPet Voice Assistant — tts.py
==================================
Converts text to speech in Danish and plays it on the Google Nest speaker
directly via pychromecast (no AirCast intermediary needed).

FLOW:
  1. Generate Danish audio from text using gTTS → save to MP3 file
  2. Serve the MP3 from a lightweight HTTP server running on the Pi
  3. Tell the Nest to play the URL via pychromecast MediaController
  4. Wait for playback to finish before returning

WHY A LOCAL HTTP SERVER:
Chromecast devices fetch audio by URL — they pull from a server rather than
accepting a file push. We spin up a minimal HTTP server on the Pi at startup
so the Nest can reach the generated MP3 over the local network.

DIRECT CONNECTION:
pychromecast supports connecting directly by IP + port, bypassing mDNS
discovery. This is more reliable on a headless Pi where mDNS can be flaky.
"""

import logging
import os
import time
import threading
import http.server
import socketserver
from pathlib import Path
from gtts import gTTS
import pychromecast


class _SilentHandler(http.server.SimpleHTTPRequestHandler):
    """HTTP handler that serves files without printing access logs."""

    def log_message(self, format, *args):
        pass  # Suppress default stdout logging — we use our own logger

    def log_error(self, format, *args):
        pass


class TextToSpeech:
    """
    Danish text-to-speech via gTTS, cast directly to a Google Nest speaker
    using pychromecast.
    """

    def __init__(self, tts_config: dict, chromecast_config: dict, log: logging.Logger):
        self._log         = log.getChild("tts")
        self._language    = tts_config.get("language", "da")
        self._output_file = Path(tts_config.get("output_file", "/tmp/neo_response.mp3"))

        # Chromecast / Nest speaker connection details
        self._cast_host   = chromecast_config["host"]   # Nest speaker IP
        self._cast_port   = chromecast_config.get("port", 8009)

        # The Pi's own LAN IP — the Nest fetches audio from here
        self._serve_host  = chromecast_config["serve_host"]
        self._serve_port  = chromecast_config.get("serve_port", 8765)

        # Start a background HTTP server so the Nest can fetch the MP3
        self._start_file_server()

        self._log.info(
            f"TTS initialised (lang={self._language}, "
            f"Nest={self._cast_host}:{self._cast_port}, "
            f"serving on {self._serve_host}:{self._serve_port})"
        )

    def _start_file_server(self) -> None:
        """
        Spin up a simple HTTP server in a daemon thread.
        Serves the directory that contains the output MP3 so the Nest can
        fetch it by URL. Daemon thread exits automatically when the main
        process exits.
        """
        serve_dir = str(self._output_file.parent)

        # Change the handler's working directory to our serve path
        class _Handler(_SilentHandler):
            def __init__(self, *args, **kwargs):
                super().__init__(*args, directory=serve_dir, **kwargs)

        # allow_reuse_address prevents "address already in use" on restart
        socketserver.TCPServer.allow_reuse_address = True
        self._http_server = socketserver.TCPServer(("0.0.0.0", self._serve_port), _Handler)

        thread = threading.Thread(target=self._http_server.serve_forever, daemon=True)
        thread.start()
        self._log.debug(f"File server started, serving {serve_dir} on port {self._serve_port}")

    def speak(self, text: str) -> None:
        """
        Convert text to MP3, serve it from the Pi, and cast to the Nest.
        Blocks until playback finishes (or times out after 60 s).
        """
        if not text.strip():
            self._log.warning("speak() called with empty text — skipping")
            return

        # Step 1: Generate audio file
        self._log.info(f"Generating TTS for: {text!r}")
        try:
            tts = gTTS(text=text, lang=self._language, slow=False)
            tts.save(str(self._output_file))
            self._log.debug(f"TTS saved to {self._output_file}")
        except Exception as e:
            self._log.error(f"gTTS generation failed: {e}")
            return

        # Step 2: Cast the file to the Nest speaker
        self._cast_audio()

    def _cast_audio(self) -> None:
        """
        Connect to the Nest speaker directly by IP and play the MP3.
        Waits for playback to complete before returning so the caller
        can safely move on to the next expression update.
        """
        # URL the Nest will use to fetch the audio from the Pi
        filename = self._output_file.name
        audio_url = f"http://{self._serve_host}:{self._serve_port}/{filename}"
        self._log.info(f"Casting {audio_url} → {self._cast_host}:{self._cast_port}")

        try:
            # Connect directly by IP — no mDNS discovery needed
            cast = pychromecast.Chromecast(self._cast_host, port=self._cast_port)
            cast.wait()  # Block until the socket connection is established

            mc = cast.media_controller
            mc.play_media(audio_url, "audio/mp3")
            mc.block_until_active(timeout=15)  # Wait until the player starts

            # Poll until playback ends (PLAYING → IDLE) or we time out
            timeout = 60  # seconds — generous upper bound for TTS responses
            start   = time.time()
            while time.time() - start < timeout:
                cast.media_controller.update_status()
                state = cast.media_controller.status.player_state
                if state in ("IDLE", "UNKNOWN", None):
                    break
                time.sleep(0.5)

            self._log.info("Playback finished")

        except pychromecast.error.ChromecastConnectionError as e:
            self._log.error(f"Could not connect to Nest at {self._cast_host}:{self._cast_port}: {e}")
        except Exception as e:
            self._log.error(f"Cast failed: {e}", exc_info=True)
