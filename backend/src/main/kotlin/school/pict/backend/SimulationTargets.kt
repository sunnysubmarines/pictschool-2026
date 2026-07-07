package school.pict.backend

import java.net.InetSocketAddress
import java.net.Socket

data class SimulationTarget(
    val host: String,
    val port: Int
)

class SimulationTargets(config: AppConfig) {
    private var robot = SimulationTarget(config.simRobotTcpHost, config.simRobotTcpCommandPort)
    private var agent = SimulationTarget(config.simAgentTcpHost, config.simAgentTcpCommandPort)

    @Synchronized
    fun forActor(actor: String): SimulationTarget = when (actor.lowercase()) {
        "agent" -> agent
        else -> robot
    }

    @Synchronized
    fun snapshot(): SimulationTargetsResponse = SimulationTargetsResponse(
        robot = SimulationTargetDto(robot.host, robot.port),
        agent = SimulationTargetDto(agent.host, agent.port)
    )

    @Synchronized
    fun update(request: SimulationTargetsRequest): SimulationTargetsResponse {
        request.robot?.let { robot = robot.copy(host = it.host.trim().ifBlank { robot.host }, port = it.port ?: robot.port) }
        request.agent?.let { agent = agent.copy(host = it.host.trim().ifBlank { agent.host }, port = it.port ?: agent.port) }
        return snapshot()
    }

    @Synchronized
    fun check(timeoutMillis: Int): SimulationTargetCheckResponse = SimulationTargetCheckResponse(
        robot = checkTarget(robot, timeoutMillis),
        agent = checkTarget(agent, timeoutMillis)
    )

    private fun checkTarget(target: SimulationTarget, timeoutMillis: Int): SimulationTargetCheck {
        if (target.host.isBlank()) {
            return SimulationTargetCheck(target.host, target.port, false, "Host is blank.")
        }
        return runCatching {
            Socket().use { socket ->
                socket.connect(InetSocketAddress(target.host, target.port), timeoutMillis)
            }
        }.fold(
            onSuccess = { SimulationTargetCheck(target.host, target.port, true) },
            onFailure = { SimulationTargetCheck(target.host, target.port, false, it.message ?: it::class.simpleName) }
        )
    }
}
