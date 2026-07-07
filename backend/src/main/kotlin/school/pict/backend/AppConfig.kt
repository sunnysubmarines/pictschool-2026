package school.pict.backend

data class AppConfig(
    val httpHost: String = "0.0.0.0",
    val httpPort: Int = 8080,
    val simTcpHost: String = "127.0.0.1",
    val simRobotTcpHost: String = simTcpHost,
    val simAgentTcpHost: String = simTcpHost,
    val simTcpCommandPort: Int = 5055,
    val simRobotTcpCommandPort: Int = simTcpCommandPort,
    val simAgentTcpCommandPort: Int = simTcpCommandPort,
    val simTcpTelemetryPort: Int = 5056,
    val simTcpTimeoutMillis: Int = 1_000,
    val simFallbackOnError: Boolean = false,
    val authEnabled: Boolean = false,
    val databaseUrl: String = "jdbc:postgresql://localhost:5432/pictschool",
    val databaseUser: String = "pictschool",
    val databasePassword: String = "pictschool"
) {
    companion object {
        fun fromEnvironment(): AppConfig = AppConfig(
            httpHost = readConfig("HTTP_HOST") ?: "0.0.0.0",
            httpPort = readConfig("HTTP_PORT")?.toIntOrNull() ?: 8080,
            simTcpHost = readConfig("SIM_TCP_HOST") ?: "127.0.0.1",
            simRobotTcpHost = readConfig("SIM_ROBOT_TCP_HOST")
                ?: readConfig("ROBOT_ESP32_IP")
                ?: readConfig("SIM_TCP_HOST")
                ?: "127.0.0.1",
            simAgentTcpHost = readConfig("SIM_AGENT_TCP_HOST")
                ?: readConfig("AGENT_ESP32_IP")
                ?: readConfig("SIM_TCP_HOST")
                ?: "127.0.0.1",
            simTcpCommandPort = readConfig("SIM_TCP_COMMAND_PORT")?.toIntOrNull() ?: 5055,
            simRobotTcpCommandPort = readConfig("SIM_ROBOT_TCP_COMMAND_PORT")?.toIntOrNull()
                ?: readConfig("SIM_TCP_COMMAND_PORT")?.toIntOrNull()
                ?: 5055,
            simAgentTcpCommandPort = readConfig("SIM_AGENT_TCP_COMMAND_PORT")?.toIntOrNull()
                ?: readConfig("SIM_TCP_COMMAND_PORT")?.toIntOrNull()
                ?: 5055,
            simTcpTelemetryPort = readConfig("SIM_TCP_TELEMETRY_PORT")?.toIntOrNull() ?: 5056,
            simTcpTimeoutMillis = readConfig("SIM_TCP_TIMEOUT_MILLIS")?.toIntOrNull() ?: 1_000,
            simFallbackOnError = readConfig("SIM_FALLBACK_ON_ERROR")?.toBooleanStrictOrNull() ?: false,
            authEnabled = readConfig("AUTH_ENABLED")?.toBooleanStrictOrNull() ?: false,
            databaseUrl = readConfig("DATABASE_URL") ?: "jdbc:postgresql://localhost:5432/pictschool",
            databaseUser = readConfig("DATABASE_USER") ?: "pictschool",
            databasePassword = readConfig("DATABASE_PASSWORD") ?: "pictschool"
        )

        private fun readConfig(key: String): String? = System.getenv(key) ?: System.getProperty(key)
    }
}
