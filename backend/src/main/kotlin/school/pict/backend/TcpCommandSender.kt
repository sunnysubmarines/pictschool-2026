package school.pict.backend

import kotlinx.serialization.decodeFromString
import kotlinx.serialization.encodeToString
import java.net.InetSocketAddress
import java.net.Socket

interface TcpCommandSender {
    fun send(request: SimulationCommandRequest): Result<SimulationCommandResult>
}

class SocketTcpCommandSender(
    private val config: AppConfig,
    private val targets: SimulationTargets = SimulationTargets(config)
) : TcpCommandSender {
    override fun send(request: SimulationCommandRequest): Result<SimulationCommandResult> = runCatching {
        val payload = backendJson.encodeToString(request)
        val target = targets.forActor(request.actor)
        Socket().use { socket ->
            socket.connect(InetSocketAddress(target.host, target.port), config.simTcpTimeoutMillis)
            socket.soTimeout = config.simTcpTimeoutMillis

            val output = socket.getOutputStream()
            output.write(payload.encodeToByteArray())
            output.write('\n'.code)
            output.flush()
            socket.shutdownOutput()

            val response = socket.getInputStream().bufferedReader().readLine()?.trim().orEmpty()
            require(response.isNotBlank()) {
                "Симуляция закрыла соединение без ответа."
            }

            backendJson.decodeFromString<SimulationCommandResult>(response)
        }
    }
}
