package school.pict.backend

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs

class RoundEngineTest {
    @Test
    fun `successful robot turn applies simulation result and switches active actor`() {
        val sender = RecordingTcpSender()
        val store = GameStore()
        val engine = RoundEngine(store, sender, AppConfig())

        engine.startRound("default")
        val result = engine.submitTurn(TurnCommandRequest("robot", listOf(1, 1, 3, 1)))

        assertIs<SubmitTurnResult.Accepted>(result)
        assertEquals(listOf(1, 1, 3, 1), sender.requests.single().commands)
        assertEquals("robot", sender.requests.single().actor)
        assertEquals(ActorId.AGENT, store.snapshot().activeActor)
        assertEquals(2, store.snapshot().turnNumber)
    }

    @Test
    fun `more than five commands are rejected before tcp send`() {
        val sender = RecordingTcpSender()
        val store = GameStore()
        val engine = RoundEngine(store, sender, AppConfig())

        engine.startRound("default")
        val result = engine.submitTurn(TurnCommandRequest("robot", listOf(1, 1, 1, 1, 1, 1)))

        val rejected = assertIs<SubmitTurnResult.Rejected>(result)
        assertEquals("turn_limit_exceeded", rejected.error.code)
        assertEquals(emptyList(), sender.requests)
        assertEquals(ActorId.ROBOT, store.snapshot().activeActor)
    }

    @Test
    fun `unknown commands are rejected before tcp send`() {
        val sender = RecordingTcpSender()
        val store = GameStore()
        val engine = RoundEngine(store, sender, AppConfig())

        engine.startRound("default")
        val result = engine.submitTurn(TurnCommandRequest("robot", listOf(1, 7, 4)))

        val rejected = assertIs<SubmitTurnResult.Rejected>(result)
        assertEquals("unknown_command", rejected.error.code)
        assertEquals(emptyList(), sender.requests)
    }

    @Test
    fun `complex movement commands are rejected`() {
        val sender = RecordingTcpSender()
        val store = GameStore()
        val engine = RoundEngine(store, sender, AppConfig())

        engine.startRound("default")
        val result = engine.submitTurn(TurnCommandRequest("robot", listOf(10)))

        val rejected = assertIs<SubmitTurnResult.Rejected>(result)
        assertEquals("unknown_command", rejected.error.code)
        assertEquals(emptyList(), sender.requests)
    }

    @Test
    fun `tcp failure is converted to simulation error`() {
        val sender = RecordingTcpSender(Result.failure(IllegalStateException("port closed")))
        val store = GameStore()
        val engine = RoundEngine(store, sender, AppConfig())

        engine.startRound("default")
        val result = engine.submitTurn(TurnCommandRequest("robot", listOf(1)))

        val rejected = assertIs<SubmitTurnResult.Rejected>(result)
        assertEquals("simulation_error", rejected.error.code)
        assertEquals(listOf(listOf(1)), sender.requests.map { it.commands })
        assertEquals(ActorId.ROBOT, store.snapshot().activeActor)
        assertEquals("turn.failed", store.events().last().type)
    }

    @Test
    fun `tcp failure can fall back to local movement and switch actor`() {
        val sender = RecordingTcpSender(Result.failure(IllegalStateException("port closed")))
        val store = GameStore()
        val config = AppConfig(simFallbackOnError = true)
        val engine = RoundEngine(
            store,
            sender,
            config,
            simulationFallbackSettings = SimulationFallbackSettings(config)
        )

        engine.startRound("default")
        val result = engine.submitTurn(TurnCommandRequest("robot", listOf(1)))

        assertIs<SubmitTurnResult.Accepted>(result)
        assertEquals(ActorId.AGENT, store.snapshot().activeActor)
        assertEquals("simulation.fallback_used", store.events().first { it.type == "simulation.fallback_used" }.type)
        assertEquals("actor.moved", store.events().first { it.type == "actor.moved" }.type)
    }

    @Test
    fun `simulation result can collect duck`() {
        val sender = RecordingTcpSender(
            Result.success(
                SimulationCommandResult(
                    ok = true,
                    actor = "robot",
                    finalPosition = Position(1, 0),
                    finalDirection = Direction.E,
                    ducksCollected = listOf("duck-1")
                )
            )
        )
        val store = GameStore()
        val engine = RoundEngine(store, sender, AppConfig())

        engine.startRound("default")
        val result = engine.submitTurn(TurnCommandRequest("robot", listOf(1)))

        assertIs<SubmitTurnResult.Accepted>(result)
        assertEquals(1, store.snapshot().score.robot)
        assertEquals(9, store.snapshot().ducksLeft)
        assertEquals("duck.collected", store.events().first { it.type == "duck.collected" }.type)
    }
}

private class RecordingTcpSender(
    private val result: Result<SimulationCommandResult> = Result.success(
        SimulationCommandResult(
            ok = true,
            actor = "robot",
            finalPosition = Position(2, 0),
            finalDirection = Direction.N,
            ducksCollected = emptyList()
        )
    )
) : TcpCommandSender {
    val requests = mutableListOf<SimulationCommandRequest>()

    override fun send(request: SimulationCommandRequest): Result<SimulationCommandResult> {
        requests += request
        return result
    }
}
