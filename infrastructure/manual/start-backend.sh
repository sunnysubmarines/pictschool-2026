#!/usr/bin/env sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

cd "$ROOT_DIR/backend"
export SIM_TCP_TIMEOUT_MILLIS="${SIM_TCP_TIMEOUT_MILLIS:-5000}"
exec ./gradlew run
