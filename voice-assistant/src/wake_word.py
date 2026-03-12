"""
DeskPet Voice Assistant — wake_word.py
========================================
Wake word detection using openwakeword.

openwakeword runs inference on a continuous stream of 16 kHz audio chunks.
Each chunk is scored against the chosen model. When the score exceeds the
configured threshold, we consider the wake word detected.

CURRENT STATE: Placeholder / Stage 2 stub.
The exact model name for "Hey Neo" will depend on whether we train a custom
model or use an available pre-trained model. See docs/hardware-notes.md.

PERFORMANCE NOTE:
On a Raspberry Pi 3B+, openwakeword with the "base" ONNX model runs in
real-time at ~10% CPU. The larger "large" models may not be fast enough.
"""

import logging
import numpy as np
import pyaudio
import openwakeword
from openwakeword.model import Model


class WakeWordDetector:
    """
    Listens continuously to the microphone and blocks until the wake word
    is detected above the configured confidence threshold.
    """

    def __init__(self, wake_config: dict, audio_config: dict, log: logging.Logger):
        self._log = log.getChild("wake_word")
        self._threshold    = float(wake_config.get("threshold", 0.5))
        self._model_name   = wake_config.get("model", "hey_jarvis")
        self._device_index = int(audio_config.get("mic_device_index", 0))
        self._sample_rate  = int(audio_config.get("sample_rate", 16000))
        self._chunk_size   = int(audio_config.get("chunk_size", 512))

        # Download pre-trained models on first run (requires internet)
        openwakeword.utils.download_models()

        self._model = Model(
            wakeword_models=[self._model_name],
            inference_framework="onnx"
        )
        self._pa = pyaudio.PyAudio()
        self._log.info(f"Wake word model loaded: {self._model_name} (threshold={self._threshold})")

    def wait_for_wake_word(self) -> None:
        """
        Blocks until the wake word is detected.
        Opens the microphone, streams audio into the model, and returns
        when the confidence score exceeds self._threshold.
        """
        stream = self._pa.open(
            format=pyaudio.paInt16,
            channels=1,
            rate=self._sample_rate,
            input=True,
            input_device_index=self._device_index,
            frames_per_buffer=self._chunk_size
        )

        self._log.debug("Microphone stream open — listening for wake word")

        try:
            while True:
                raw = stream.read(self._chunk_size, exception_on_overflow=False)
                # Convert raw bytes to numpy int16 array (openwakeword input format)
                audio_chunk = np.frombuffer(raw, dtype=np.int16)
                prediction = self._model.predict(audio_chunk)

                # prediction is a dict: {model_name: score, ...}
                score = prediction.get(self._model_name, 0.0)

                if score >= self._threshold:
                    self._log.debug(f"Wake word score: {score:.3f} ≥ {self._threshold}")
                    # Reset the model's internal state so we don't immediately
                    # re-trigger on the trailing audio
                    self._model.reset()
                    break
        finally:
            stream.stop_stream()
            stream.close()
