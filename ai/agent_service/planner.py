from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Literal

from openai import OpenAI

from .config import AgentSettings
from .schemas import (
    ALLOWED_COMMANDS,
    LLMPlan,
    MAX_COMMANDS_PER_TURN,
    ActorState,
    Direction,
    Duck,
    RoundState,
)


@dataclass(frozen=True)
class PlannerResult:
    commands: list[int]
    rationale: str
    source: Literal["llm", "fallback"]
    error: str | None = None


class LLMPlanner:
    def __init__(self, settings: AgentSettings):
        self.settings = settings
        self._client: OpenAI | None = None
        if settings.llm_enabled and settings.openai_api_key:
            client_kwargs = {"api_key": settings.openai_api_key}
            if settings.openai_base_url:
                client_kwargs["base_url"] = settings.openai_base_url
            self._client = OpenAI(**client_kwargs)

    def plan(self, round_state: RoundState) -> PlannerResult:
        fallback_commands = fallback_commands_for_round(round_state)
        fallback = PlannerResult(
            commands=fallback_commands,
            rationale="Fallback strategy: deterministic safe move.",
            source="fallback",
        )

        if not self._client:
            import logging
            logging.getLogger(__name__).warning("No LLM client configured, using fallback.")
            return PlannerResult(
                commands=fallback.commands,
                rationale=fallback.rationale,
                source=fallback.source,
                error="No LLM client configured. Check OPENAI_API_KEY.",
            )

        try:
            prompt = self._build_prompt(round_state)
            content = self._request_plan_content(prompt)
            plan = LLMPlan.model_validate_json(content)

            max_commands = max(1, min(round_state.moveLimitPerTurn, MAX_COMMANDS_PER_TURN))
            commands = plan.commands[:max_commands]
            if not commands:
                return fallback
            if any(command not in ALLOWED_COMMANDS for command in commands):
                return fallback

            return PlannerResult(commands=commands, rationale=plan.rationale, source="llm")
        except Exception as e:
            import logging
            logging.getLogger(__name__).error(f"LLM request failed: {e}", exc_info=True)
            return PlannerResult(
                commands=fallback.commands,
                rationale=fallback.rationale,
                source=fallback.source,
                error=f"LLM request failed: {e}",
            )

    def _request_plan_content(self, prompt: str) -> str:
        assert self._client is not None

        messages = [
            {
                "role": "system",
                "content": (
                    "You are a tactical game agent. "
                    "Return only valid JSON with keys commands and rationale. "
                    "Commands must be integers in [1,2,3,4] with length 1..5. "
                    "Map: 1=Forward, 2=Backward, 3=TurnLeft, 4=TurnRight. "
                    "Note: Forward/Backward move you in your current direction. "
                    "TurnLeft/TurnRight change your direction by 90 degrees."
                ),
            },
            {"role": "user", "content": prompt},
        ]

        try:
            response = self._client.chat.completions.create(
                model=self.settings.llm_model,
                temperature=self.settings.llm_temperature,
                max_tokens=self.settings.llm_max_tokens,
                messages=messages,
                response_format={
                    "type": "json_schema",
                    "json_schema": {
                        "name": "agent_turn_plan",
                        "schema": LLMPlan.model_json_schema(),
                    },
                },
            )
            return response.choices[0].message.content or ""
        except Exception as schema_error:
            import logging
            logging.getLogger(__name__).warning(
                "LLM json_schema response_format failed, retrying with plain JSON: %s",
                schema_error,
            )

        response = self._client.chat.completions.create(
            model=self.settings.llm_model,
            temperature=self.settings.llm_temperature,
            max_tokens=self.settings.llm_max_tokens,
            messages=messages
            + [
                {
                    "role": "user",
                    "content": (
                        "Return exactly one JSON object, no markdown. "
                        'Example: {"commands":[1],"rationale":"move toward the closest duck"}'
                    ),
                }
            ],
        )
        content = response.choices[0].message.content or ""
        return _extract_json_object(content)

    def _build_prompt(self, round_state: RoundState) -> str:
        agent = round_state.actor("agent")
        robot = round_state.actor("robot")
        ducks = [duck for duck in round_state.field.ducks if duck.collectedBy is None]
        duck_descriptions = ", ".join(
            f"{duck.id}@({duck.position.x},{duck.position.y})" for duck in ducks[:12]
        )
        if not duck_descriptions:
            duck_descriptions = "none"

        obstacle_descriptions = ", ".join(
            f"{obs.id}@({obs.position.x},{obs.position.y})" for obs in round_state.field.obstacles[:16]
        )
        if not obstacle_descriptions:
            obstacle_descriptions = "none"

        return (
            "Plan commands for actor=agent.\n"
            f"Round status: {round_state.status}\n"
            f"Turn number: {round_state.turnNumber}\n"
            f"Active actor: {round_state.activeActor}\n"
            f"Move limit per turn: {round_state.moveLimitPerTurn}\n"
            f"Score robot-agent: {round_state.score.robot}-{round_state.score.agent}\n"
            f"Agent: pos=({agent.position.x},{agent.position.y}), dir={agent.direction}\n"
            f"Robot: pos=({robot.position.x},{robot.position.y}), dir={robot.direction}\n"
            f"Ducks left: {round_state.ducksLeft}; Ducks: {duck_descriptions}\n"
            f"Obstacles: {obstacle_descriptions}\n"
            "Goal: collect ducks efficiently and avoid wasting turns."
        )


def fallback_commands_for_round(round_state: RoundState) -> list[int]:
    agent = round_state.actor("agent")
    ducks = [duck for duck in round_state.field.ducks if duck.collectedBy is None]
    if not ducks:
        return [1]

    target = min(
        ducks,
        key=lambda duck: abs(duck.position.x - agent.position.x) + abs(duck.position.y - agent.position.y),
    )
    desired_direction = _desired_direction(agent, target)
    if desired_direction == agent.direction:
        return [1]
    return [_rotation_command(agent.direction, desired_direction)]


def _desired_direction(agent: ActorState, target: Duck) -> Direction:
    dx = target.position.x - agent.position.x
    dy = target.position.y - agent.position.y
    if abs(dx) >= abs(dy):
        if dx > 0:
            return "E"
        if dx < 0:
            return "W"
    if dy > 0:
        return "S"
    return "N"


def _rotation_command(current: Direction, target: Direction) -> int:
    order = ["N", "E", "S", "W"]
    current_index = order.index(current)
    target_index = order.index(target)
    right_turns = (target_index - current_index) % 4
    left_turns = (current_index - target_index) % 4
    if right_turns == 0:
        return 1
    if right_turns <= left_turns:
        return 4
    return 3


def _extract_json_object(content: str) -> str:
    stripped = content.strip()
    if stripped.startswith("{") and stripped.endswith("}"):
        return stripped

    start = stripped.find("{")
    end = stripped.rfind("}")
    if start >= 0 and end > start:
        candidate = stripped[start : end + 1]
        json.loads(candidate)
        return candidate

    raise ValueError(f"LLM did not return a JSON object: {content[:200]}")
