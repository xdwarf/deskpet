"""
DeskPet Voice Assistant — ai_client.py
========================================
Sends transcribed text to Gemini (Google Generative AI) and returns the response.

MODEL CHOICE:
We use gemini-1.5-flash for its speed and generous free tier — ideal for
short conversational exchanges on a Pi 3B+. The model can be changed in config.yaml.

CONVERSATION HISTORY:
For simplicity in Stage 2, each query is stateless (no conversation history).
We include a system prompt that sets Neo's personality. Multi-turn
conversation memory can be added in Stage 3.

DANISH:
The system prompt is in Danish and instructs Gemini to respond in Danish.
"""

import logging
import google.generativeai as genai


class AiClient:
    """
    Wraps the Google Gemini API for single-turn conversational queries.
    """

    def __init__(self, ai_config: dict, log: logging.Logger):
        self._log = log.getChild("ai")
        model_name          = ai_config.get("model", "gemini-1.5-flash")
        self._system_prompt = ai_config.get("system_prompt", "Du er Neo, en hjælpsom assistent. Svar altid på dansk.")

        # Configure the SDK with the API key, then build a model instance
        # with the system prompt baked in so every request inherits it.
        genai.configure(api_key=ai_config["gemini_api_key"])
        self._model = genai.GenerativeModel(
            model_name=model_name,
            system_instruction=self._system_prompt,
        )
        self._log.info(f"AI client initialised (model: {model_name})")

    def ask(self, question: str) -> str:
        """
        Send a question to Gemini and return the response text.
        Uses a non-streaming request for simplicity — the response comes back
        as one block of text after the model finishes generating.

        For Stage 3 we could switch to streaming and update Neo's face
        in real-time as the response arrives.
        """
        self._log.info(f"Sending to Gemini: {question!r}")

        try:
            # max_output_tokens keeps responses short and spoken-word friendly
            response = self._model.generate_content(
                question,
                generation_config=genai.GenerationConfig(max_output_tokens=256),
            )
            response_text = response.text
            self._log.info(f"Gemini responded: {response_text!r}")
            return response_text

        except Exception as e:
            self._log.error(f"Gemini API error: {e}", exc_info=True)
            # Return a Danish fallback so TTS still has something to say
            return "Beklager, jeg kunne ikke få svar fra min hjerne lige nu."
