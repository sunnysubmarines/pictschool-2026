#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
AI_DIR="$ROOT_DIR/ai"

if [[ -f "$AI_DIR/.env" ]]; then
  set -a
  # shellcheck disable=SC1090
  source "$AI_DIR/.env"
  set +a
fi

declare -a PYTHON_CMD=()
if command -v python3 >/dev/null 2>&1; then
  PYTHON_CMD=(python3)
elif command -v python >/dev/null 2>&1; then
  PYTHON_CMD=(python)
else
  echo "[ai-agent] python not found" >&2
  exit 1
fi

cd "$AI_DIR"

if [[ ! -d ".venv" ]]; then
  "${PYTHON_CMD[@]}" -m venv .venv
fi

# shellcheck disable=SC1091
source .venv/bin/activate
pip install -r requirements.txt >/dev/null

export AGENT_BACKEND_URL="${AGENT_BACKEND_URL:-http://127.0.0.1:8080}"

echo "[ai-agent] backend: $AGENT_BACKEND_URL"
echo "[ai-agent] actor: ${AGENT_ACTOR_ID:-agent}"
echo "[ai-agent] llm model: ${AGENT_LLM_MODEL:-gpt-4o-mini}"

exec python -m agent_service "$@"
