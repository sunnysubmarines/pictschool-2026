from __future__ import annotations

import argparse
import logging
import time

from .client import BackendApiError, BackendClient
from .config import AgentSettings
from .planner import LLMPlanner
from .schemas import RoundState


LOGGER = logging.getLogger("agent_service")


class AgentRunner:
    def __init__(self, settings: AgentSettings, client: BackendClient, planner: LLMPlanner):
        self.settings = settings
        self.client = client
        self.planner = planner

    def run_forever(self) -> None:
        while True:
            self.run_once()
            time.sleep(self.settings.poll_interval_sec)

    def run_iterations(self, max_iterations: int) -> None:
        for _ in range(max_iterations):
            self.run_once()
            time.sleep(self.settings.poll_interval_sec)

    def run_once(self) -> None:
        try:
            round_response = self.client.get_round()
            round_state = round_response.round
            if not self._should_play(round_state):
                self._publish_status(
                    state="waiting",
                    rationale=f"Waiting for activeActor={self.settings.actor_id}; current activeActor={round_state.activeActor}, status={round_state.status}.",
                )
                LOGGER.debug(
                    "Skip turn: status=%s activeActor=%s", round_state.status, round_state.activeActor
                )
                return

            self._publish_status(state="planning", rationale=f"Planning turn {round_state.turnNumber}.")
            plan = self.planner.plan(round_state)
            self._publish_status(
                state="planned",
                source=plan.source,
                commands=plan.commands,
                rationale=plan.rationale,
                error=plan.error,
            )
            # LLM response can be slow; re-check turn ownership before submitting.
            latest_round_state = self.client.get_round().round
            if not self._should_play(latest_round_state):
                self._publish_status(
                    state="stale_plan",
                    source=plan.source,
                    commands=plan.commands,
                    rationale="Turn changed while planning; plan was not submitted.",
                    error=plan.error,
                )
                LOGGER.info(
                    "Skip stale plan: turn changed while planning (was turn=%s, now turn=%s, active=%s)",
                    round_state.turnNumber,
                    latest_round_state.turnNumber,
                    latest_round_state.activeActor,
                )
                return

            accepted = self.client.submit_turn(actor=self.settings.actor_id, commands=plan.commands)
            self._publish_status(
                state="submitted",
                source=plan.source,
                commands=plan.commands,
                rationale=plan.rationale,
                error=plan.error,
            )
            LOGGER.info(
                "Turn accepted: eventId=%s commands=%s source=%s rationale=%s",
                accepted.eventId,
                plan.commands,
                plan.source,
                plan.rationale,
            )
        except BackendApiError as error:
            self._publish_status(state="backend_error", error=str(error))
            LOGGER.warning("Backend error: %s", error)
        except Exception as error:
            self._publish_status(state="agent_error", error=str(error))
            LOGGER.exception("Unexpected runner error: %s", error)

    def _should_play(self, round_state: RoundState) -> bool:
        if round_state.status != "running":
            return False
        return round_state.activeActor == self.settings.actor_id

    def _publish_status(
        self,
        *,
        state: str,
        source: str | None = None,
        commands: list[int] | None = None,
        rationale: str | None = None,
        error: str | None = None,
    ) -> None:
        try:
            self.client.update_agent_status(
                state=state,
                actor=self.settings.actor_id,
                source=source,
                commands=commands or [],
                rationale=rationale,
                error=error,
                model=self.settings.llm_model if self.settings.llm_enabled else "disabled",
            )
        except BackendApiError:
            LOGGER.debug("Could not publish agent status.", exc_info=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the PICT school AI agent.")
    parser.add_argument("--once", action="store_true", help="run one polling/planning iteration and exit")
    parser.add_argument("--start-round", action="store_true", help="start a new backend round before running")
    parser.add_argument("--max-iterations", type=int, default=None, help="run at most this many iterations")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )
    settings = AgentSettings.from_env()

    client = BackendClient(
        base_url=settings.backend_url,
        auth_token=settings.auth_token,
        timeout_sec=settings.request_timeout_sec,
    )
    planner = LLMPlanner(settings=settings)
    runner = AgentRunner(settings=settings, client=client, planner=planner)

    LOGGER.info(
        "AI agent starting: backend=%s actor=%s llm_enabled=%s model=%s base_url=%s",
        settings.backend_url,
        settings.actor_id,
        settings.llm_enabled,
        settings.llm_model,
        settings.openai_base_url or "default",
    )
    runner._publish_status(
        state="started",
        rationale="AI agent process is running and polling backend.",
    )

    if args.start_round:
        client.start_round()

    if args.once:
        runner.run_once()
        return

    if args.max_iterations is not None:
        runner.run_iterations(args.max_iterations)
        return

    runner.run_forever()


if __name__ == "__main__":
    main()
