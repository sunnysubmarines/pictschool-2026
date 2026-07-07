const state = {
  backendUrl: `${window.location.protocol}//${window.location.hostname}:8080`,
  round: null,
  events: [],
  queue: [],
  source: null,
  pollTimer: null,
  targets: null,
  agentStatus: null,
};

const els = {
  backendUrl: document.getElementById("backendUrl"),
  robotEspIp: document.getElementById("robotEspIp"),
  agentEspIp: document.getElementById("agentEspIp"),
  saveTargetsButton: document.getElementById("saveTargetsButton"),
  testTargetsButton: document.getElementById("testTargetsButton"),
  fallbackToggle: document.getElementById("fallbackToggle"),
  connectButton: document.getElementById("connectButton"),
  startRoundButton: document.getElementById("startRoundButton"),
  status: document.getElementById("status"),
  turnNumber: document.getElementById("turnNumber"),
  activeActor: document.getElementById("activeActor"),
  robotScore: document.getElementById("robotScore"),
  agentScore: document.getElementById("agentScore"),
  ducksLeft: document.getElementById("ducksLeft"),
  robotPosition: document.getElementById("robotPosition"),
  agentPosition: document.getElementById("agentPosition"),
  duckPositions: document.getElementById("duckPositions"),
  obstaclePositions: document.getElementById("obstaclePositions"),
  agentState: document.getElementById("agentState"),
  agentSource: document.getElementById("agentSource"),
  agentModel: document.getElementById("agentModel"),
  agentCommands: document.getElementById("agentCommands"),
  agentRationale: document.getElementById("agentRationale"),
  agentError: document.getElementById("agentError"),
  agentHint: document.getElementById("agentHint"),
  board: document.getElementById("board"),
  queue: document.getElementById("queue"),
  undoButton: document.getElementById("undoButton"),
  clearButton: document.getElementById("clearButton"),
  submitButton: document.getElementById("submitButton"),
  eventLog: document.getElementById("eventLog"),
  moveLog: document.getElementById("moveLog"),
};

els.backendUrl.value = state.backendUrl;

function api(path, options = {}) {
  return fetch(`${state.backendUrl}${path}`, {
    headers: { "Content-Type": "application/json", ...(options.headers || {}) },
    ...options,
  }).then(async (response) => {
    const text = await response.text();
    const payload = text ? JSON.parse(text) : {};
    if (!response.ok) {
      throw payload;
    }
    return payload;
  });
}

function renderSummary() {
  if (!state.round) {
    els.turnNumber.textContent = "-";
    els.activeActor.textContent = "-";
    els.robotScore.textContent = "0";
    els.agentScore.textContent = "0";
    els.ducksLeft.textContent = "-";
    els.robotPosition.textContent = "-";
    els.agentPosition.textContent = "-";
    els.duckPositions.textContent = "-";
    els.obstaclePositions.textContent = "-";
    return;
  }
  els.turnNumber.textContent = String(state.round.turnNumber);
  els.activeActor.textContent = state.round.activeActor;
  els.robotScore.textContent = String(state.round.score.robot);
  els.agentScore.textContent = String(state.round.score.agent);
  els.ducksLeft.textContent = `${state.round.ducksLeft}/${state.round.ducksTotal}`;
  els.robotPosition.textContent = actorPositionText(currentActorState("robot"));
  els.agentPosition.textContent = actorPositionText(currentActorState("agent"));
  els.duckPositions.textContent = duckPositionsText(state.round.field.ducks);
  els.obstaclePositions.textContent = positionsText(state.round.field.obstacles);
}

function samePosition(a, b) {
  return a.x === b.x && a.y === b.y;
}

function boardColumnLabel(x) {
  const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if (x < alphabet.length) return alphabet[x];
  return `C${x + 1}`;
}

function boardRowLabel(y) {
  return String(y + 1);
}

function boardCellLabel(position) {
  return `${boardColumnLabel(position.x)}${boardRowLabel(position.y)}`;
}

function actorPositionText(actor) {
  if (!actor?.position) return "-";
  const { x, y } = actor.position;
  return `${boardCellLabel(actor.position)} / x=${x}, y=${y}, dir=${actor.direction}`;
}

function positionText(position) {
  if (!position) return "-";
  return `${boardCellLabel(position)}(x=${position.x},y=${position.y})`;
}

function positionsText(items) {
  if (!items?.length) return "-";
  return items.map((item) => positionText(item.position || item)).join("; ");
}

function duckPositionsText(ducks) {
  if (!ducks?.length) return "-";
  return ducks
    .map((duck) => {
      const status = duck.collectedBy ? ` -> ${duck.collectedBy}` : "";
      return `${duck.id}: ${positionText(duck.position)}${status}`;
    })
    .join("; ");
}

function latestMovedActorState(actorName) {
  for (let index = state.events.length - 1; index >= 0; index -= 1) {
    const event = state.events[index];
    if (event.type !== "actor.moved" || event.actor !== actorName) continue;
    const payload = event.payload || {};
    if (!payload.finalPosition) continue;
    return {
      id: actorName,
      position: payload.finalPosition,
      direction: payload.finalDirection || "?",
    };
  }
  return null;
}

function currentActorState(actorName) {
  return state.round?.actors?.[actorName] || latestMovedActorState(actorName);
}

function renderBoard() {
  els.board.innerHTML = "";
  if (!state.round) return;
  const { width, height, ducks, obstacles } = state.round.field;
  els.board.style.gridTemplateColumns = `28px repeat(${width}, minmax(34px, 1fr))`;

  const corner = document.createElement("div");
  corner.className = "coord-header coord-corner";
  els.board.append(corner);

  for (let x = 0; x < width; x += 1) {
    const header = document.createElement("div");
    header.className = "coord-header";
    header.textContent = boardColumnLabel(x);
    els.board.append(header);
  }

  for (let y = 0; y < height; y += 1) {
    const rowHeader = document.createElement("div");
    rowHeader.className = "coord-header row-header";
    rowHeader.textContent = boardRowLabel(y);
    els.board.append(rowHeader);

    for (let x = 0; x < width; x += 1) {
      const cell = document.createElement("div");
      cell.className = "cell";
      let mark = "";
      const pos = { x, y };
      const robot = state.round.actors.robot;
      const agent = state.round.actors.agent;
      if (samePosition(robot.position, pos)) {
        mark = `R${robot.direction}`;
      } else if (samePosition(agent.position, pos)) {
        mark = `A${agent.direction}`;
      } else if (obstacles.some((o) => samePosition(o.position, pos))) {
        mark = "#";
      } else if (ducks.some((d) => !d.collectedBy && samePosition(d.position, pos))) {
        mark = "D";
      }

      const coord = document.createElement("span");
      coord.className = "cell-coord";
      coord.textContent = boardCellLabel(pos);
      cell.append(coord);

      const content = document.createElement("span");
      content.className = "cell-content";
      content.textContent = mark;
      cell.append(content);

      els.board.append(cell);
    }
  }
}

const commandLabels = {
  1: "F",
  2: "B",
  3: "L",
  4: "R",
};

function renderQueue() {
  els.queue.innerHTML = "";
  state.queue.forEach((command) => {
    const chip = document.createElement("span");
    chip.className = "chip";
    chip.textContent = commandLabels[command] || command;
    els.queue.append(chip);
  });
  const robotTurn = state.round?.status === "running" && state.round.activeActor === "robot";
  els.submitButton.disabled = !robotTurn || state.queue.length === 0;
}

function renderAgentStatus() {
  const status = state.agentStatus;
  if (!status) {
    els.agentState.textContent = "not_connected";
    els.agentSource.textContent = "-";
    els.agentModel.textContent = "-";
    els.agentCommands.textContent = "-";
    els.agentRationale.textContent = "AI agent has not reported yet.";
    els.agentError.textContent = "Start it with ./infrastructure/manual/start-ai-agent.sh";
    els.agentHint.textContent = "";
    return;
  }

  const updatedAtMs = status.updatedAt ? Date.parse(status.updatedAt) : NaN;
  const stale = Number.isFinite(updatedAtMs) && Date.now() - updatedAtMs > 6000;
  els.agentState.textContent = `${status.state || "unknown"}${stale ? " (stale)" : ""}`;
  els.agentSource.textContent = status.source || "-";
  els.agentModel.textContent = status.model || "-";
  els.agentCommands.textContent = (status.commands || []).join(" ") || "-";
  els.agentRationale.textContent = status.rationale || "No rationale.";
  els.agentError.textContent = stale
    ? `Agent status is old. Last update: ${status.updatedAt}. Check the Python agent process.`
    : status.error || "";

  const hints = [];
  if (state.round?.status === "running" && state.round.activeActor === "robot") {
    hints.push("Now it is robot turn. Submit robot turn first; then AI agent can move.");
  }
  if (state.targets?.robot?.host && state.targets?.agent?.host && state.targets.robot.host === state.targets.agent.host) {
    hints.push("Robot ESP IP and Agent ESP IP are the same. This is OK only if that ESP is flashed as the active actor; for two physical platforms use two different IPs.");
  }
  if (status.state === "not_connected") {
    hints.push("Run the Python agent service: ./infrastructure/manual/start-ai-agent.sh");
  }
  els.agentHint.textContent = hints.join(" ");
}

function renderEvents() {
  els.eventLog.innerHTML = "";
  state.events
    .slice(-40)
    .reverse()
    .forEach((event) => {
      const li = document.createElement("li");
      const details = eventDetails(event);
      li.textContent =
        `${event.type} (turn ${event.turnNumber}${event.actor ? `, ${event.actor}` : ""})` +
        `${details ? `: ${details}` : ""}`;
      els.eventLog.append(li);
    });
}

function eventDetails(event) {
  const payload = event.payload || {};
  if (event.type === "turn.failed") {
    return payload.error || payload.cause || "";
  }
  if (event.type === "simulation.command_sent") {
    return `${payload.actor || event.actor || "?"} -> ${payload.host || "?"}:${payload.port || "?"}, payload=${payload.tcpPayload || "-"}`;
  }
  if (event.type === "simulation.fallback_used") {
    return `local movement fallback, cause=${payload.cause || "unknown"}`;
  }
  if (event.type === "turn.submitted") {
    return payload.commands || "";
  }
  return "";
}

function renderMoves() {
  els.moveLog.innerHTML = "";

  const commandsByTurnAndActor = new Map();
  state.events.forEach((event) => {
    if (event.type === "turn.submitted" && event.actor) {
      commandsByTurnAndActor.set(`${event.turnNumber}:${event.actor}`, event.payload?.commands || "-");
    }
  });

  state.events
    .filter((event) => event.type === "actor.moved" && event.actor)
    .slice(-40)
    .reverse()
    .forEach((event) => {
      const li = document.createElement("li");
      const payload = event.payload || {};
      const pos = payload.finalPosition || {};
      const direction = payload.finalDirection || "?";
      const commands = commandsByTurnAndActor.get(`${event.turnNumber}:${event.actor}`) || "-";
      li.textContent =
        `turn ${event.turnNumber}, ${event.actor}: ` +
        `to (${pos.x ?? "?"}, ${pos.y ?? "?"}), dir=${direction}, commands=${commands}`;
      els.moveLog.append(li);
    });
}

function renderAll() {
  renderSummary();
  renderBoard();
  renderQueue();
  renderAgentStatus();
  renderEvents();
  renderMoves();
}

async function refreshState() {
  const [roundPayload, eventsPayload, agentStatus] = await Promise.all([
    api("/api/round"),
    api("/api/events"),
    api("/api/ai/agent/status").catch(() => null),
  ]);
  state.round = roundPayload.round;
  state.events = eventsPayload.events || [];
  state.agentStatus = agentStatus;
  renderAll();
}

function renderTargets() {
  if (!state.targets) return;
  els.robotEspIp.value = state.targets.robot?.host || "";
  els.agentEspIp.value = state.targets.agent?.host || "";
}

async function refreshTargets() {
  const [targets, fallback] = await Promise.all([
    api("/api/simulation/targets"),
    api("/api/simulation/fallback").catch(() => ({ enabled: false })),
  ]);
  state.targets = targets;
  els.fallbackToggle.checked = Boolean(fallback.enabled);
  renderTargets();
}

async function saveTargets() {
  const robotHost = els.robotEspIp.value.trim();
  const agentHost = els.agentEspIp.value.trim();
  state.targets = await api("/api/simulation/targets", {
    method: "POST",
    body: JSON.stringify({
      robot: { host: robotHost, port: 5055 },
      agent: { host: agentHost, port: 5055 },
    }),
  });
  renderTargets();
  els.status.textContent = "ESP IPs saved";
}

async function testTargets() {
  const result = await api("/api/simulation/targets/check");
  const format = (name, item) => `${name} ${item.host}:${item.port} ${item.ok ? "OK" : `FAIL (${item.error || "unknown"})`}`;
  els.status.textContent = `${format("robot", result.robot)}; ${format("agent", result.agent)}`;
}

async function setFallback(enabled) {
  const result = await api("/api/simulation/fallback", {
    method: "POST",
    body: JSON.stringify({ enabled }),
  });
  els.fallbackToggle.checked = Boolean(result.enabled);
  els.status.textContent = result.enabled
    ? "Local fallback enabled: failed ESP moves will be simulated locally"
    : "Local fallback disabled";
}

function connectSse() {
  if (state.source) state.source.close();
  state.source = new EventSource(`${state.backendUrl}/api/live`);
  state.source.onopen = () => {
    els.status.textContent = "SSE connected";
  };
  state.source.onerror = () => {
    els.status.textContent = "SSE disconnected";
  };
  state.source.onmessage = () => {
    refreshState().catch((error) => {
      els.status.textContent = error?.error?.message || "Refresh error";
    });
  };
}

function startPolling() {
  if (state.pollTimer) {
    clearInterval(state.pollTimer);
  }
  state.pollTimer = setInterval(() => {
    refreshState().catch(() => {});
  }, 1500);
}

async function connect() {
  state.backendUrl = els.backendUrl.value.replace(/\/$/, "");
  await refreshTargets();
  await refreshState();
  connectSse();
  startPolling();
  els.status.textContent = "Connected";
}

async function startRound() {
  await api("/api/round/start", {
    method: "POST",
    body: JSON.stringify({ scenarioId: "default" }),
  });
  state.queue = [];
  await refreshState();
}

async function submitRobotTurn() {
  if (state.queue.length === 0) return;
  await api("/api/turn/submit", {
    method: "POST",
    body: JSON.stringify({ actor: "robot", commands: state.queue }),
  });
  state.queue = [];
  await refreshState();
}

document.querySelectorAll("[data-command]").forEach((button) => {
  button.addEventListener("click", () => {
    if (state.queue.length >= 5) return;
    state.queue.push(Number(button.dataset.command));
    renderQueue();
  });
});

els.undoButton.addEventListener("click", () => {
  state.queue.pop();
  renderQueue();
});
els.clearButton.addEventListener("click", () => {
  state.queue = [];
  renderQueue();
});
els.submitButton.addEventListener("click", () => {
  submitRobotTurn().catch((error) => {
    els.status.textContent = error?.error?.message || "Submit error";
  });
});
els.saveTargetsButton.addEventListener("click", () => {
  saveTargets().catch((error) => {
    els.status.textContent = error?.error?.message || "Save ESP IPs error";
  });
});
els.testTargetsButton.addEventListener("click", () => {
  testTargets().catch((error) => {
    els.status.textContent = error?.error?.message || "Test ESPs error";
  });
});
els.fallbackToggle.addEventListener("change", () => {
  setFallback(els.fallbackToggle.checked).catch((error) => {
    els.status.textContent = error?.error?.message || "Fallback toggle error";
    els.fallbackToggle.checked = !els.fallbackToggle.checked;
  });
});
els.connectButton.addEventListener("click", () => {
  connect().catch((error) => {
    els.status.textContent = error?.error?.message || "Connect error";
  });
});
els.startRoundButton.addEventListener("click", () => {
  startRound().catch((error) => {
    els.status.textContent = error?.error?.message || "Start round error";
  });
});

connect().catch(() => {
  els.status.textContent = "Start backend stack first";
});
