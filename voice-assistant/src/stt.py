"""
DeskPet Voice Assistant — stt.py
==================================
Records audio on the Pi and sends it to the neo-brain server for transcription.

WHY SERVER-SIDE STT:
Vosk runs on the Docker host (192.168.2.14:8000) where CPU/RAM aren't
constrained. The Pi only captures audio and POSTs the WAV over the LAN,
which is far faster than running Whisper locally (~5s on Pi 3B+).

RECORDING STRATEGY:
Fixed-duration recording after the wake word. VAD (stop on silence) is a
future improvement.
"""

import io
import wave
import logging

import pyaudio
import requests


class SpeechToText:
    """
    Records audio from the Pi microphone, wraps it in a WAV container,
    and sends it to the neo-brain server's /transcribe endpoint.
    """

    def __init__(self, server_config: dict, audio_config: dict, log: logging.Logger):
        self._log          = log.getChild("stt")
        self._server_url   = server_config["url"].rstrip("/")
        self._timeout      = int(server_config.get("timeout", 30))
        self._device_index = int(audio_config.get("mic_device_index", 0))
        self._sample_rate  = int(audio_config.get("sample_rate", 16000))
        self._chunk_size   = int(audio_config.get("chunk_size", 512))
        self._record_secs  = int(audio_config.get("record_seconds", 8))
        self._pa           = pyaudio.PyAudio()
        self._log.info(f"STT initialised (server: {self._server_url})")

    def record_audio(self) -> bytes:
        """
        Open the microphone, record for self._record_secs seconds, and return
        the audio wrapped in a WAV container (16 kHz, mono, 16-bit PCM).
        WAV format is required by the server's Vosk endpoint.
        """
        self._log.info(f"Recording for up to {self._record_secs}s...")
        stream = self._pa.open(
            format=pyaudio.paInt16,
            channels=1,
            rate=self._sample_rate,
            input=True,
            input_device_index=self._device_index,
            frames_per_buffer=self._chunk_size,
        )

        frames = []
        num_chunks = int(self._sample_rate / self._chunk_size * self._record_secs)
        for _ in range(num_chunks):
            raw = stream.read(self._chunk_size, exception_on_overflow=False)
            frames.append(raw)

        stream.stop_stream()
        stream.close()
        self._log.info("Recording complete")

        # Wrap the raw PCM bytes in a WAV container so the server's
        # wave.open() can parse sample rate and channel count correctly
        buf = io.BytesIO()
        with wave.open(buf, "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)          # 16-bit PCM = 2 bytes per sample
            wf.setframerate(self._sample_rate)
            wf.writeframes(b"".join(frames))
        return buf.getvalue()

    def transcribe(self, wav_bytes: bytes) -> str:
        """
        POST the WAV bytes to the neo-brain server's /transcribe endpoint
        and return the Danish transcript string.
        Returns empty string on failure so the main loop can handle it gracefully.
        """
        self._log.info("Sending audio to server for transcription...")
        try:
            resp = requests.post(
                f"{self._server_url}/transcribe",
                files={"audio": ("recording.wav", wav_bytes, "audio/wav")},
                timeout=self._timeout,
            )
            resp.raise_for_status()
            text = resp.json().get("text", "").strip()
            self._log.info(f"Transcript: {text!r}")
            return text
        except requests.RequestException as e:
            self._log.error(f"STT server unreachable: {e}")
            return ""
