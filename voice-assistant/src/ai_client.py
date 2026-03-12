"""
DeskPet Voice Assistant — ai_client.py
========================================
Sends transcribed text to Claude (Anthropic API) and returns the response.

MODEL CHOICE:
We default to claude-haiku-4-5 for its speed and low cost — ideal for
short conversational exchanges. The model can be changed in config.yaml.

CONVERSATION HISTORY:
For simplicity in Stage 2, each query is stateless (no conversation history).
We include a system prompt that sets Neo's personality. Multi-turn
conversation memory can be added in Stage 3.

DANISH:
The system prompt is in Danish and instructs Claude to respond in Danish.
Claude handles Danish well with no special configuration needed.
"""

import logging
import anthropic


class AiClient:
    """
    Wraps the Anthropic Claude API for single-turn conversational queries.
    """

    def __init__(self, ai_config: dict, log: logging.Logger):
        self._log = log.getChild("ai")
        self._model         = ai_config.get("model", "claude-haiku-4-5-20251001")
        self._system_prompt = ai_config.get("system_prompt", "Du er Neo, en hjælpsom assistent. Svar altid på dansk.")
        self._client        = anthropic.Anthropic(api_key=ai_config["anthropic_api_key"])
        self._log.info(f"AI client initialised (model: {self._model})")

    def ask(self, question: str) -> str:
        """
        Send a question to Claude and return the response text.
        Uses a non-streaming request for simplicity — the response comes back
        as one block of text after the model finishes generating.

        For Stage 3 we could switch to streaming and update Neo's face
        in real-time as the response arrives.
        """
        self._log.info(f"Sending to Claude: {question!r}")

        try:
            message = self._client.messages.create(
                model=self._model,
                max_tokens=256,  # Keep responses short and spoken-word friendly
                system=self._system_prompt,
                messages=[
                    {"role": "user", "content": question}
                ]
            )
            # Extract the text content from the first content block
            response_text = message.content[0].text
            self._log.info(f"Claude responded: {response_text!r}")
            return response_text

        except anthropic.APIError as e:
            self._log.error(f"Claude API error: {e}")
            # Return a Danish fallback so TTS still has something to say
            return "Beklager, jeg kunne ikke få svar fra min hjerne lige nu."

        except Exception as e:
            self._log.error(f"Unexpected error querying Claude: {e}", exc_info=True)
            return "Der opstod en fejl. Prøv igen."
