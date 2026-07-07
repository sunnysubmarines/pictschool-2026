package school.pict.backend

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.JsonObject

@Serializable
data class StartRoundRequest(val scenarioId: String? = "default")

@Serializable
data class TurnCommandRequest(val actor: String, val commands: List<Int>)

@Serializable
data class RoundResponse(val round: Round)

@Serializable
data class StartRoundResponse(val roundId: String, val status: String, val activeActor: String)

@Serializable
data class TurnAcceptedResponse(val accepted: Boolean, val eventId: String, val forwardedAs: String)

@Serializable
data class EventsResponse(val events: List<GameEvent>)

@Serializable
data class ResetRoundResponse(val roundId: String, val status: String, val readyForStart: Boolean)

@Serializable
data class SimulationTargetDto(val host: String, val port: Int? = null)

@Serializable
data class SimulationTargetsRequest(
    val robot: SimulationTargetDto? = null,
    val agent: SimulationTargetDto? = null
)

@Serializable
data class SimulationTargetsResponse(
    val robot: SimulationTargetDto,
    val agent: SimulationTargetDto
)

@Serializable
data class SimulationTargetCheckResponse(
    val robot: SimulationTargetCheck,
    val agent: SimulationTargetCheck
)

@Serializable
data class SimulationTargetCheck(
    val host: String,
    val port: Int,
    val ok: Boolean,
    val error: String? = null
)

@Serializable
data class SimulationFallbackRequest(val enabled: Boolean)

@Serializable
data class SimulationFallbackResponse(val enabled: Boolean)

@Serializable
data class AgentStatusUpdateRequest(
    val state: String,
    val actor: String = "agent",
    val source: String? = null,
    val commands: List<Int> = emptyList(),
    val rationale: String? = null,
    val error: String? = null,
    val model: String? = null
)

@Serializable
data class AgentStatusResponse(
    val state: String,
    val actor: String,
    val source: String?,
    val commands: List<Int>,
    val rationale: String?,
    val error: String?,
    val model: String?,
    val updatedAt: String
)

@Serializable
data class ErrorResponse(val error: ApiError)

@Serializable
data class ApiError(
    val code: String,
    val message: String,
    val details: JsonObject = JsonObject(emptyMap())
)

@Serializable
data class AuthConfigResponse(val enabled: Boolean)

@Serializable
data class AuthRequest(val username: String, val password: String)

@Serializable
data class AuthResponse(val token: String, val username: String)

@Serializable
data class CurrentUserResponse(val username: String?)
