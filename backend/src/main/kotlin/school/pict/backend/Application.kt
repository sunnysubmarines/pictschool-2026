package school.pict.backend

import io.ktor.http.ContentType
import io.ktor.http.HttpStatusCode
import io.ktor.http.HttpHeaders
import io.ktor.http.HttpMethod
import io.ktor.serialization.kotlinx.json.json
import io.ktor.server.application.Application
import io.ktor.server.application.ApplicationCall
import io.ktor.server.application.call
import io.ktor.server.application.install
import io.ktor.server.engine.embeddedServer
import io.ktor.server.netty.Netty
import io.ktor.server.plugins.contentnegotiation.ContentNegotiation
import io.ktor.server.plugins.cors.routing.CORS
import io.ktor.server.request.receive
import io.ktor.server.response.respond
import io.ktor.server.response.respondText
import io.ktor.server.response.respondTextWriter
import io.ktor.server.routing.get
import io.ktor.server.routing.post
import io.ktor.server.routing.routing
import kotlinx.coroutines.delay
import kotlinx.serialization.encodeToString

fun main() {
    val config = AppConfig.fromEnvironment()
    embeddedServer(Netty, port = config.httpPort, host = config.httpHost) {
        backendModule(config)
    }.start(wait = true)
}

fun Application.backendModule(
    config: AppConfig = AppConfig.fromEnvironment(),
    simulationTargets: SimulationTargets = SimulationTargets(config),
    tcpSender: TcpCommandSender = SocketTcpCommandSender(config, simulationTargets),
    store: GameStore = GameStore(),
    agentStatusStore: AgentStatusStore = AgentStatusStore(),
    simulationFallbackSettings: SimulationFallbackSettings = SimulationFallbackSettings(config)
) {
    install(ContentNegotiation) {
        json(backendJson)
    }

    install(CORS) {
        anyHost()
        allowHeader(HttpHeaders.Authorization)
        allowHeader(HttpHeaders.ContentType)
        allowMethod(HttpMethod.Get)
        allowMethod(HttpMethod.Post)
    }

    val engine = RoundEngine(store, tcpSender, config, simulationTargets, simulationFallbackSettings)
    val authService = if (config.authEnabled) AuthService(config) else null

    routing {
        get("/api/auth/config") {
            call.respond(AuthConfigResponse(config.authEnabled))
        }

        post("/api/auth/register") {
            val service = authService ?: return@post call.respondError(ApiError("auth_disabled", "Авторизация выключена."))
            val request = call.receive<AuthRequest>()
            val token = service.register(request.username, request.password).getOrElse { error ->
                return@post call.respondError(ApiError("auth_error", error.message ?: "Не удалось зарегистрироваться."))
            }
            call.respond(status = HttpStatusCode.Created, AuthResponse(token, request.username.trim().lowercase()))
        }

        post("/api/auth/login") {
            val service = authService ?: return@post call.respondError(ApiError("auth_disabled", "Авторизация выключена."))
            val request = call.receive<AuthRequest>()
            val token = service.login(request.username, request.password).getOrElse { error ->
                return@post call.respondError(ApiError("auth_error", error.message ?: "Не удалось войти."))
            }
            call.respond(AuthResponse(token, request.username.trim().lowercase()))
        }

        get("/api/auth/me") {
            val username = call.authenticatedUsername(authService)
            call.respond(CurrentUserResponse(username))
        }

        get("/api/docs") {
            call.respondText(swaggerHtml(), ContentType.Text.Html)
        }

        get("/api/docs/openapi.yaml") {
            val spec = requireNotNull({}.javaClass.getResource("/openapi.yaml")) {
                "openapi.yaml resource is missing"
            }.readText()
            call.respondText(spec, ContentType.parse("application/yaml"))
        }

        get("/api/round") {
            if (!call.requireAuth(authService)) return@get
            call.respond(RoundResponse(store.snapshot()))
        }

        get("/api/simulation/targets") {
            if (!call.requireAuth(authService)) return@get
            call.respond(simulationTargets.snapshot())
        }

        post("/api/simulation/targets") {
            if (!call.requireAuth(authService)) return@post
            val request = call.receive<SimulationTargetsRequest>()
            call.respond(simulationTargets.update(request))
        }

        get("/api/simulation/targets/check") {
            if (!call.requireAuth(authService)) return@get
            call.respond(simulationTargets.check(config.simTcpTimeoutMillis))
        }

        get("/api/simulation/fallback") {
            if (!call.requireAuth(authService)) return@get
            call.respond(simulationFallbackSettings.snapshot())
        }

        post("/api/simulation/fallback") {
            if (!call.requireAuth(authService)) return@post
            val request = call.receive<SimulationFallbackRequest>()
            call.respond(simulationFallbackSettings.update(request))
        }

        post("/api/round/start") {
            if (!call.requireAuth(authService)) return@post
            val request = runCatching { call.receive<StartRoundRequest>() }.getOrDefault(StartRoundRequest())
            val round = engine.startRound(request.scenarioId)
            call.respond(
                status = io.ktor.http.HttpStatusCode.Created,
                StartRoundResponse(round.id, round.status.wireName(), round.activeActor.wireName())
            )
        }

        post("/api/turn/submit") {
            if (!call.requireAuth(authService)) return@post
            val request = call.receive<TurnCommandRequest>()
            when (val result = engine.submitTurn(request)) {
                is SubmitTurnResult.Accepted -> call.respond(
                    status = io.ktor.http.HttpStatusCode.Accepted,
                    TurnAcceptedResponse(true, result.eventId, result.forwardedAs)
                )
                is SubmitTurnResult.Rejected -> call.respondError(result.error)
            }
        }

        get("/api/events") {
            if (!call.requireAuth(authService)) return@get
            call.respond(EventsResponse(store.events()))
        }

        get("/api/ai/agent/status") {
            if (!call.requireAuth(authService)) return@get
            call.respond(agentStatusStore.snapshot())
        }

        post("/api/ai/agent/status") {
            if (!call.requireAuth(authService)) return@post
            val request = call.receive<AgentStatusUpdateRequest>()
            call.respond(agentStatusStore.update(request))
        }

        get("/api/live") {
            if (!call.requireAuth(authService)) return@get
            call.respondTextWriter(contentType = ContentType.Text.EventStream) {
                var sentEvents = 0
                while (true) {
                    val events = store.events()
                    events.drop(sentEvents).forEach { event ->
                        write("event: ${event.type}\n")
                        write("data: ${backendJson.encodeToString(event)}\n\n")
                    }
                    sentEvents = events.size
                    flush()
                    delay(1_000)
                }
            }
        }

        post("/api/round/reset") {
            if (!call.requireAuth(authService)) return@post
            val round = engine.resetRound()
            call.respond(ResetRoundResponse(round.id, round.status.wireName(), true))
        }
    }
}

private fun swaggerHtml(): String = """
    <!doctype html>
    <html lang="ru">
    <head>
        <meta charset="utf-8">
        <title>Duck Round Backend API</title>
        <link rel="stylesheet" href="https://unpkg.com/swagger-ui-dist@5/swagger-ui.css">
    </head>
    <body>
        <div id="swagger-ui"></div>
        <script src="https://unpkg.com/swagger-ui-dist@5/swagger-ui-bundle.js"></script>
        <script>
            window.onload = () => {
                window.ui = SwaggerUIBundle({
                    url: '/api/docs/openapi.yaml',
                    dom_id: '#swagger-ui'
                });
            };
        </script>
    </body>
    </html>
""".trimIndent()

private suspend fun ApplicationCall.respondError(error: ApiError) {
    val status = when (error.code) {
        "unknown_command", "turn_limit_exceeded", "wrong_actor_turn", "round_not_running", "auth_error", "auth_disabled" -> HttpStatusCode.BadRequest
        "unauthorized" -> HttpStatusCode.Unauthorized
        "simulation_error" -> HttpStatusCode.BadGateway
        else -> HttpStatusCode.InternalServerError
    }
    respond(status, ErrorResponse(error))
}

private suspend fun ApplicationCall.requireAuth(authService: AuthService?): Boolean {
    if (authService == null) return true
    if (authenticatedUsername(authService) != null) return true
    respondError(ApiError("unauthorized", "Нужна авторизация."))
    return false
}

private fun ApplicationCall.authenticatedUsername(authService: AuthService?): String? {
    if (authService == null) return null
    return authService.usernameByToken(authToken())
}

private fun ApplicationCall.authToken(): String? {
    val bearer = request.headers[HttpHeaders.Authorization]
        ?.removePrefix("Bearer ")
        ?.takeIf { it.isNotBlank() }
    return bearer ?: request.queryParameters["token"]
}
