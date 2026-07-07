package school.pict.backend

import kotlinx.serialization.json.add
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import kotlin.concurrent.thread

class RoundEngine(
    private val store: GameStore,
    private val tcpSender: TcpCommandSender,
    private val config: AppConfig,
    private val simulationTargets: SimulationTargets = SimulationTargets(config),
    private val simulationFallbackSettings: SimulationFallbackSettings = SimulationFallbackSettings(config)
) {
    private val allowedCommands = setOf(1, 2, 3, 4)

    fun startRound(scenarioId: String?): Round {
        val round = Scenario.defaultRound(status = RoundStatus.RUNNING)
        store.replace(round)
        store.clearEvents()
        store.append(
            "round.started",
            round.turnNumber,
            null,
            buildJsonObject {
                put("scenarioId", scenarioId ?: "default")
                put("activeActor", round.activeActor.wireName())
            }
        )
        return round
    }

    fun resetRound(): Round {
        val round = Scenario.defaultRound(status = RoundStatus.IDLE)
        store.replace(round)
        store.clearEvents()
        store.append("round.reset", round.turnNumber, null, buildJsonObject { put("readyForStart", true) })
        return round
    }

    fun submitTurn(request: TurnCommandRequest): SubmitTurnResult {
        val round = store.snapshot()
        val actor = ActorId.fromWire(request.actor)
            ?: return SubmitTurnResult.Rejected(ApiError("wrong_actor_turn", "Участник ${request.actor} не поддерживается."))

        validate(round, actor, request.commands)?.let { return SubmitTurnResult.Rejected(it) }

        val forwardedAs = request.commands.joinToString(" ")
        val simulationRequest = SimulationCommandRequest(actor.wireName(), request.commands, round)
        val simulationTarget = simulationTargets.forActor(actor.wireName())
        val submitted = store.append(
            "turn.submitted",
            round.turnNumber,
            actor,
            buildJsonObject {
                put("actor", actor.wireName())
                put("commands", forwardedAs)
            }
        )
        store.append(
            "simulation.command_sent",
            round.turnNumber,
            actor,
            buildJsonObject {
                put("actor", actor.wireName())
                put("tcpPayload", forwardedAs)
                put("host", simulationTarget.host)
                put("port", simulationTarget.port)
                put("telemetryPort", config.simTcpTelemetryPort)
            }
        )

        if (simulationFallbackSettings.isEnabled()) {
            thread(name = "esp-command-${actor.wireName()}-${round.turnNumber}", isDaemon = true) {
                tcpSender.send(simulationRequest)
            }
            store.append(
                "simulation.fallback_used",
                round.turnNumber,
                actor,
                buildJsonObject {
                    put("cause", "local fallback enabled; ESP response is not awaited")
                    put("mode", "local_movement_immediate")
                }
            )
            val afterMove = LocalMovement.apply(round, actor, expandCommandsForLocalFallback(request.commands))
            applyMovementResult(round, actor, afterMove)
            return SubmitTurnResult.Accepted(submitted.id, forwardedAs)
        }

        val tcpResult = tcpSender.send(simulationRequest)
        if (tcpResult.isFailure) {
            val message = tcpResult.exceptionOrNull()?.message ?: "Симуляция недоступна."
            store.append("turn.failed", round.turnNumber, actor, buildJsonObject { put("error", message) })
            return SubmitTurnResult.Rejected(
                ApiError(
                    "simulation_error",
                    "Не удалось отправить команды в симуляцию.",
                    buildJsonObject { put("cause", message) }
                )
            )
        }

        val simulationResult = tcpResult.getOrThrow()
        val simulationError = validateSimulationResult(simulationResult, actor)
        if (simulationError != null) {
            store.append("turn.failed", round.turnNumber, actor, buildJsonObject { put("error", simulationError) })
            return SubmitTurnResult.Rejected(
                ApiError(
                    "simulation_error",
                    "Симуляция вернула ошибку выполнения.",
                    buildJsonObject { put("cause", simulationError) }
                )
            )
        }

        val afterMove = LocalMovement.applySimulationResult(round, actor, simulationResult)
        applyMovementResult(round, actor, afterMove)
        return SubmitTurnResult.Accepted(submitted.id, forwardedAs)
    }

    private fun applyMovementResult(round: Round, actor: ActorId, afterMove: MovementResult) {
        store.replace(afterMove.round)
        store.append(
            "actor.moved",
            round.turnNumber,
            actor,
            buildJsonObject {
                put("actor", actor.wireName())
                put("finalPosition", afterMove.actorState.position.toJson())
                put("finalDirection", afterMove.actorState.direction.name)
            }
        )
        afterMove.collectedDucks.forEach { duckId ->
            store.append(
                "duck.collected",
                round.turnNumber,
                actor,
                buildJsonObject {
                    put("actor", actor.wireName())
                    put("duckId", duckId)
                    put("score", afterMove.round.score.toJson())
                }
            )
        }

        if (afterMove.round.status == RoundStatus.COMPLETED) {
            store.append("round.completed", round.turnNumber, actor, buildJsonObject { put("score", afterMove.round.score.toJson()) })
        } else {
            store.append(
                "turn.completed",
                round.turnNumber,
                actor,
                buildJsonObject {
                    put("nextActor", afterMove.round.activeActor.wireName())
                    put("turnNumber", afterMove.round.turnNumber)
                }
            )
        }
    }

    private fun validateSimulationResult(result: SimulationCommandResult, actor: ActorId): String? {
        if (!result.ok) {
            return result.error ?: "Симуляция вернула ok=false."
        }
        if (ActorId.fromWire(result.actor) != actor) {
            return "Симуляция вернула результат для другого участника: ${result.actor}."
        }
        if (result.finalPosition == null) {
            return "Симуляция не вернула finalPosition."
        }
        if (result.finalDirection == null) {
            return "Симуляция не вернула finalDirection."
        }
        return null
    }

    private fun expandCommandsForLocalFallback(commands: List<Int>): List<Int> = commands.flatMap { command ->
        when (command) {
            1 -> listOf(1)
            2 -> listOf(2)
            3 -> listOf(3)
            4 -> listOf(4)
            else -> emptyList()
        }
    }

    private fun validate(round: Round, actor: ActorId, commands: List<Int>): ApiError? {
        if (round.status != RoundStatus.RUNNING) {
            return ApiError("round_not_running", "Раунд не запущен.")
        }
        if (actor != round.activeActor) {
            return ApiError("wrong_actor_turn", "Сейчас ходит ${round.activeActor.wireName()}.")
        }
        if (commands.isEmpty() || commands.size > round.moveLimitPerTurn) {
            return ApiError("turn_limit_exceeded", "В ходе должно быть от 1 до ${round.moveLimitPerTurn} команд.")
        }
        val unknown = commands.firstOrNull { it !in allowedCommands }
        if (unknown != null) {
            return ApiError(
                "unknown_command",
                "Команда $unknown не поддерживается.",
                buildJsonObject {
                    put("allowed", buildJsonArray {
                        allowedCommands.sorted().forEach { add(it) }
                    })
                }
            )
        }
        return null
    }
}

sealed class SubmitTurnResult {
    data class Accepted(val eventId: String, val forwardedAs: String) : SubmitTurnResult()
    data class Rejected(val error: ApiError) : SubmitTurnResult()
}
