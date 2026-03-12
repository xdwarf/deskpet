"""
DeskPet — neo-brain server
===========================
FastAPI server running on the home Docker host (192.168.2.14:8000).
The Pi offloads the CPU-heavy work here so it stays lightweight.

ENDPOINTS:
  GET  /health      — liveness check
  POST /transcribe  — WAV audio → Danish transcript text (Vosk)
  POST /ask         — Danish text → AI response text (Gemini)

WHY HERE AND NOT ON THE PI:
- Vosk models (~50 MB+) and inference are too slow on a Pi 3B+
- Gemini API key lives on the server — cleaner than putting secrets on the Pi
- Server-side updates (model swaps, prompt tuning) don't require Pi redeployment

VOSK MODEL:
Download before first run — see docker-compose.yaml for instructions.
Model used: vosk-model-small-da-0.22 (Danish, ~48 MB)
"""

import os
import io
import json
import wave
import logging
import tempfile

from fastapi import FastAPI, UploadFile, File, HTTPException
from pydantic import BaseModel
from vosk import Model, KaldiRecognizer
import google.generativeai as genai


# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
log = logging.getLogger("neo-brain")


# ---------------------------------------------------------------------------
# Vosk — loaded once at startup (takes a few seconds on first load)
# ---------------------------------------------------------------------------
VOSK_MODEL_PATH = os.getenv("VOSK_MODEL_PATH", "/models/vosk-model-small-da-0.22")

if not os.path.isdir(VOSK_MODEL_PATH):
    raise RuntimeError(
        f"Vosk model not found at {VOSK_MODEL_PATH}. "
        "Download it first — see the docker-compose.yaml for instructions."
    )

log.info(f"Loading Vosk model from {VOSK_MODEL_PATH} ...")
vosk_model = Model(VOSK_MODEL_PATH)
log.info("Vosk model ready")


# ---------------------------------------------------------------------------
# Gemini — loaded once at startup
# ---------------------------------------------------------------------------
GEMINI_API_KEY = os.environ["GEMINI_API_KEY"]   # Hard fail if missing — catch it early
GEMINI_MODEL   = os.getenv("GEMINI_MODEL", "gemini-1.5-flash")
SYSTEM_PROMPT  = os.getenv(
    "SYSTEM_PROMPT",
    (
        "Du er Neo, en venlig og hjælpsom desktop-assistent. "
        "Du taler altid dansk og giver korte, klare svar. "
        "Du har adgang til smart home-data og kan hjælpe med dagligdags spørgsmål. "
        "Hold svar under 3 sætninger medmindre der specifikt bedt om mere."
    ),
)

genai.configure(api_key=GEMINI_API_KEY)
gemini = genai.GenerativeModel(
    model_name=GEMINI_MODEL,
    system_instruction=SYSTEM_PROMPT,
)
log.info(f"Gemini model ready ({GEMINI_MODEL})")


# ---------------------------------------------------------------------------
# FastAPI app
# ---------------------------------------------------------------------------
app = FastAPI(title="neo-brain", description="DeskPet STT + AI backend")


class AskRequest(BaseModel):
    text: str


@app.get("/health")
def health():
    """Simple liveness check — useful for Docker healthchecks and debugging."""
    return {"status": "ok", "vosk_model": VOSK_MODEL_PATH, "gemini_model": GEMINI_MODEL}


@app.post("/transcribe")
async def transcribe(audio: UploadFile = File(...)):
    """
    Accept a WAV audio file and return the Danish transcript.

    The audio must be 16 kHz, mono, 16-bit PCM — exactly what the Pi records.
    The Pi wraps its raw PCM capture in a WAV container before posting here.
    """
    raw = await audio.read()

    # Write to a temp file so wave.open() can seek through the WAV headers
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
        tmp.write(raw)
        tmp_path = tmp.name

    try:
        with wave.open(tmp_path, "rb") as wf:
            if wf.getnchannels() != 1:
                raise HTTPException(status_code=400, detail="Audio must be mono (1 channel)")
            sample_rate = wf.getframerate()

            # KaldiRecognizer is created per-request (cheap, stateless)
            rec = KaldiRecognizer(vosk_model, sample_rate)
            rec.SetWords(False)  # We only need the transcript, not word timestamps

            # Feed audio in 4096-frame chunks — Vosk streams the recognition
            while True:
                data = wf.readframes(4096)
                if not data:
                    break
                rec.AcceptWaveform(data)

            result = json.loads(rec.FinalResult())
            text = result.get("text", "").strip()

    finally:
        os.unlink(tmp_path)

    log.info(f"Transcribed: {text!r}")
    return {"text": text}


@app.post("/ask")
async def ask(req: AskRequest):
    """
    Accept a Danish question and return Neo's AI response text.

    The Pi posts the transcript here after /transcribe, gets text back,
    then generates TTS and casts it to the Nest itself.
    """
    if not req.text.strip():
        raise HTTPException(status_code=400, detail="text must not be empty")

    log.info(f"Asking Gemini: {req.text!r}")

    try:
        response = gemini.generate_content(
            req.text,
            generation_config=genai.GenerationConfig(max_output_tokens=256),
        )
        response_text = response.text.strip()
        log.info(f"Gemini responded: {response_text!r}")
        return {"response": response_text}

    except Exception as e:
        log.error(f"Gemini error: {e}", exc_info=True)
        raise HTTPException(status_code=502, detail="AI backend error")
