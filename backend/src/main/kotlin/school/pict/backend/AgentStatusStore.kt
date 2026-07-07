package school.pict.backend

import java.time.Instant

class AgentStatusStore {
    private var status = AgentStatusResponse(
        state = "not_connected",
        actor = "agent",
        source = null,
        commands = emptyList(),
        rationale = "AI agent has not reported yet.",
        error = "Start the Python agent service: cd ai && source .venv/bin/activate && python -m agent_service",
        model = null,
        updatedAt = Instant.now().toString()
    )

    @Synchronized
    fun snapshot(): AgentStatusResponse = status

    @Synchronized
    fun update(request: AgentStatusUpdateRequest): AgentStatusResponse {
        status = AgentStatusResponse(
            state = request.state,
            actor = request.actor,
            source = request.source,
            commands = request.commands,
            rationale = request.rationale,
            error = request.error,
            model = request.model,
            updatedAt = Instant.now().toString()
        )
        return status
    }
}
