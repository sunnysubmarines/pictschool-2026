package school.pict.backend

class SimulationFallbackSettings(config: AppConfig) {
    private var enabled = config.simFallbackOnError

    @Synchronized
    fun snapshot(): SimulationFallbackResponse = SimulationFallbackResponse(enabled)

    @Synchronized
    fun update(request: SimulationFallbackRequest): SimulationFallbackResponse {
        enabled = request.enabled
        return snapshot()
    }

    @Synchronized
    fun isEnabled(): Boolean = enabled
}
