from __future__ import annotations

from dataclasses import dataclass

import requests

from .schemas import ApiErrorEnvelope, RoundResponse, TurnAcceptedResponse, TurnCommandRequest


@dataclass
class BackendApiError(Exception):
    message: str
    status_code: int
    code: str | None = None
    details: dict | None = None

    def __str__(self) -> str:
        code_part = f" [{self.code}]" if self.code else ""
        return f"Backend API error{code_part} ({self.status_code}): {self.message}"


class BackendClient:
    def __init__(self, base_url: str, auth_token: str | None = None, timeout_sec: float = 8.0):
        self.base_url = base_url.rstrip("/")
        self.timeout_sec = timeout_sec
        self._session = requests.Session()
        self._session.headers.update({"Content-Type": "application/json"})
        if auth_token:
            self._session.headers.update({"Authorization": f"Bearer {auth_token}"})

    def get_round(self) -> RoundResponse:
        try:
            response = self._session.get(f"{self.base_url}/api/round", timeout=self.timeout_sec)
            self._raise_if_error(response)
            return RoundResponse.model_validate(response.json())
        except requests.RequestException as error:
            raise BackendApiError(message=str(error), status_code=0) from error

    def start_round(self, scenario_id: str = "default") -> None:
        try:
            response = self._session.post(
                f"{self.base_url}/api/round/start",
                json={"scenarioId": scenario_id},
                timeout=self.timeout_sec,
            )
            self._raise_if_error(response)
        except requests.RequestException as error:
            raise BackendApiError(message=str(error), status_code=0) from error

    def submit_turn(self, actor: str, commands: list[int]) -> TurnAcceptedResponse:
        payload = TurnCommandRequest(actor=actor, commands=commands)
        try:
            response = self._session.post(
                f"{self.base_url}/api/turn/submit",
                json=payload.model_dump(),
                timeout=self.timeout_sec,
            )
            self._raise_if_error(response)
            return TurnAcceptedResponse.model_validate(response.json())
        except requests.RequestException as error:
            raise BackendApiError(message=str(error), status_code=0) from error

    def update_agent_status(
        self,
        *,
        state: str,
        actor: str,
        source: str | None = None,
        commands: list[int] | None = None,
        rationale: str | None = None,
        error: str | None = None,
        model: str | None = None,
    ) -> None:
        payload = {
            "state": state,
            "actor": actor,
            "source": source,
            "commands": commands or [],
            "rationale": rationale,
            "error": error,
            "model": model,
        }
        try:
            response = self._session.post(
                f"{self.base_url}/api/ai/agent/status",
                json=payload,
                timeout=self.timeout_sec,
            )
            self._raise_if_error(response)
        except requests.RequestException as error:
            raise BackendApiError(message=str(error), status_code=0) from error

    def _raise_if_error(self, response: requests.Response) -> None:
        if response.ok:
            return

        message = f"HTTP {response.status_code}"
        code: str | None = None
        details: dict | None = None

        try:
            parsed = ApiErrorEnvelope.model_validate(response.json())
            message = parsed.error.message
            code = parsed.error.code
            if isinstance(parsed.error.details, dict):
                details = parsed.error.details
        except Exception:
            if response.text:
                message = response.text

        raise BackendApiError(
            message=message,
            status_code=response.status_code,
            code=code,
            details=details,
        )
