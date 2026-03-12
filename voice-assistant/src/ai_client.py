"""
DeskPet Voice Assistant — ai_client.py
========================================
Thin HTTP client that delegates AI queries to the neo-brain server.
The server (192.168.2.14:8000) holds the Gemini API key and runs inference.

WHY SERVER-SIDE AI:
Centralising the API key on the Docker host means no secrets on the Pi.
Model or prompt changes are server-side only — no Pi redeployment needed.
"""

import logging
import requests


class AiClient:
    """
    Posts Danish questions to the neo-brain server's /ask endpoint
    and returns the response text.
    """

    def __init__(self, server_config: dict, log: logging.Logger):
        self._log        = log.getChild("ai")
        self._server_url = server_config["url"].rstrip("/")
        self._timeout    = int(server_config.get("timeout", 30))
        self._log.info(f"AI client initialised (server: {self._server_url})")

    def ask(self, question: str) -> str:
        """
        POST the question to /ask and return the response text.
        Returns a Danish fallback string on any network or server error
        so the TTS pipeline always has something to say.
        """
        self._log.info(f"Sending to server: {question!r}")
        try:
            resp = requests.post(
                f"{self._server_url}/ask",
                json={"text": question},
                timeout=self._timeout,
            )
            resp.raise_for_status()
            response_text = resp.json().get("response", "").strip()
            self._log.info(f"AI responded: {response_text!r}")
            return response_text
        except requests.RequestException as e:
            self._log.error(f"AI server unreachable: {e}")
            return "Beklager, jeg kunne ikke få svar fra min hjerne lige nu."
