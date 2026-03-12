"""
DeskPet Voice Assistant — stt.py
==================================
Speech-to-text using OpenAI Whisper running locally on the Raspberry Pi.

WHY WHISPER?
- Runs fully offline — no API key needed, no cloud latency
- Good Danish (da) support in the "base" and "small" models
- The "base" model fits in ~150 MB RAM and runs in ~5s on Pi 3B+

RECORDING STRATEGY:
We record a fixed number of seconds after the wake word, then transcribe.
A more sophisticated approach (VAD — voice activity detection) would stop
recording when the user stops speaking. This is left as a future improvement.
"""

import logging
import io
import numpy as np
import pyaudio
import whisper


class SpeechToText:
    """
    Records audio from the microphone after the wake word fires,
    then transcribes it using Whisper.
    """

    def __init__(self, stt_config: dict, audio_config: dict, log: logging.Logger):
        self._log = log.getChild("stt")
        self._language     = stt_config.get("language", "da")
        self._model_name   = stt_config.get("model", "base")
        self._device_index = int(audio_config.get("mic_device_index", 0))
        self._sample_rate  = int(audio_config.get("sample_rate", 16000))
        self._chunk_size   = int(audio_config.get("chunk_size", 512))
        self._record_secs  = int(audio_config.get("record_seconds", 8))

        self._log.info(f"Loading Whisper model '{self._model_name}' — this may take a moment...")
        # Load model once at startup (not per utterance)
        self._model = whisper.load_model(self._model_name)
        self._log.info("Whisper model loaded")

        self._pa = pyaudio.PyAudio()

    def record_audio(self) -> np.ndarray:
        """
        Open the microphone, record for self._record_secs seconds, and
        return the raw audio as a float32 numpy array in the range [-1.0, 1.0].
        Whisper expects float32 at 16 kHz.
        """
        self._log.info(f"Recording for up to {self._record_secs}s...")
        stream = self._pa.open(
            format=pyaudio.paInt16,
            channels=1,
            rate=self._sample_rate,
            input=True,
            input_device_index=self._device_index,
            frames_per_buffer=self._chunk_size
        )

        frames = []
        num_chunks = int(self._sample_rate / self._chunk_size * self._record_secs)

        for _ in range(num_chunks):
            raw = stream.read(self._chunk_size, exception_on_overflow=False)
            frames.append(raw)

        stream.stop_stream()
        stream.close()
        self._log.info("Recording complete")

        # Convert to float32 in range [-1, 1] — the format Whisper expects
        raw_bytes = b"".join(frames)
        audio_int16 = np.frombuffer(raw_bytes, dtype=np.int16)
        audio_float32 = audio_int16.astype(np.float32) / 32768.0
        return audio_float32

    def transcribe(self, audio: np.ndarray) -> str:
        """
        Run Whisper transcription on the recorded audio.
        Returns the transcribed text string.
        Specifying language="da" skips language detection and is faster.
        """
        self._log.info("Transcribing...")
        result = self._model.transcribe(audio, language=self._language, fp16=False)
        # fp16=False because Pi 3B+ doesn't have FP16 acceleration
        text = result.get("text", "").strip()
        self._log.info(f"Transcription: {text!r}")
        return text
