#!/usr/bin/env bash
# busmon-ui.sh — start arlacal-server + the LA-CAL bus monitor UI.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARCAL_DIR="$SCRIPT_DIR/../arcal"
ARCAL_BUILD_DIR="$ARCAL_DIR/build"
ARLACAL_BIN="$ARCAL_BUILD_DIR/lacal/arlacal-server"
CYCLONEDDS_XML="$ARCAL_DIR/test/e2e/cyclonedds_localhost.xml"

LOG_DIR="/tmp/busmon-out"
DOMAIN=0
PORT=8765
LACAL_HOST="127.0.0.1"
LACAL_PORT=8766
START_ARLACAL=1

usage() {
  echo "usage: $0 [--log-dir DIR] [--domain ID] [--port N] [--lacal-url URL] [--no-start-arlacal]" >&2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --log-dir) LOG_DIR="$2"; shift 2 ;;
    --domain) DOMAIN="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --lacal-url)
      LACAL_URL="$2"
      START_ARLACAL=0
      shift 2
      ;;
    --no-start-arlacal) START_ARLACAL=0; shift ;;
    *) usage; exit 1 ;;
  esac
done

LACAL_URL="${LACAL_URL:-ws://$LACAL_HOST:$LACAL_PORT}"

if [[ "$START_ARLACAL" -eq 1 && ! -x "$ARLACAL_BIN" ]]; then
  echo "error: $ARLACAL_BIN not found — run: cmake --build $ARCAL_BUILD_DIR --target arlacal-server" >&2
  exit 1
fi

WSL_IP="$(hostname -I | awk '{print $1}')"
PIDS=()
CLEANED_UP=0

cleanup() {
  if [[ "$CLEANED_UP" -eq 1 ]]; then
    return
  fi
  CLEANED_UP=1
  echo ""
  echo "[busmon-ui] shutting down..."
  for pid in "${PIDS[@]}"; do
    kill "$pid" 2>/dev/null || true
  done
  wait 2>/dev/null || true
  echo "[busmon-ui] done."
}
trap cleanup EXIT INT TERM

mkdir -p "$LOG_DIR"

if [[ "$START_ARLACAL" -eq 1 ]]; then
  echo "[busmon-ui] starting arlacal-server on $LACAL_URL..."
  (cd "$ARCAL_DIR" && \
    CYCLONEDDS_URI="file://$CYCLONEDDS_XML" \
    "$ARLACAL_BIN" --host "$LACAL_HOST" --port "$LACAL_PORT" --domain "$DOMAIN" 2>&1) &
  PIDS+=($!)
  sleep 0.5
fi

echo "[busmon-ui] starting web UI on port $PORT..."
(cd "$SCRIPT_DIR" && uv run python -m busmon.app \
  --lacal-url "$LACAL_URL" \
  --log-dir "$LOG_DIR" \
  --port "$PORT" \
  2>&1) &
PIDS+=($!)

echo ""
echo "  ┌─────────────────────────────────────────────────┐"
echo "  │  arcal-busmon LA-CAL UI                         │"
echo "  │                                                 │"
echo "  │  Windows browser → http://$WSL_IP:$PORT         │"
echo "  │  LA-CAL source   → $LACAL_URL"
echo "  │                                                 │"
echo "  │  Press Ctrl-C to stop                           │"
echo "  └─────────────────────────────────────────────────┘"
echo ""

wait "${PIDS[-1]}"
