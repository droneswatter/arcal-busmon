#!/usr/bin/env bash
# busmon-ui.sh — start arcal-busmon + web bridge, clean up on exit.
#
# Usage:
#   ./busmon-ui.sh [--log-dir DIR] [--domain ID] [--duration SECS] [--port N] [--debug-stream]
#
# Requires:
#   - arcal-busmon already built  (build/arcal-busmon)
#   - Python deps installed       (uv pip install fastapi "uvicorn[standard]")

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUSMON_BIN="$SCRIPT_DIR/build/arcal-busmon"
UI_DIR="$SCRIPT_DIR/ui"
CYCLONEDDS_XML="$SCRIPT_DIR/../arcal/test/e2e/cyclonedds_localhost.xml"
ARCAL_BUILD_DIR="$SCRIPT_DIR/../arcal/build"

# arcal externalizer plugins are loaded via dlopen with bare sonames; they must
# be on LD_LIBRARY_PATH so the dynamic linker can locate them at runtime.
export LD_LIBRARY_PATH="$ARCAL_BUILD_DIR:$ARCAL_BUILD_DIR/externalizer/cdr:$ARCAL_BUILD_DIR/externalizer/json${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

LOG_DIR="/tmp/busmon-out"
FIFO="/tmp/busmon.fifo"
DOMAIN=0
DURATION=0   # 0 = run until Ctrl-C
PORT=8765
DEBUG_STREAM=0

# ── Argument parsing ──────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case "$1" in
    --log-dir)  LOG_DIR="$2";  shift 2 ;;
    --domain)   DOMAIN="$2";   shift 2 ;;
    --duration) DURATION="$2"; shift 2 ;;
    --port)     PORT="$2";     shift 2 ;;
    --debug-stream) DEBUG_STREAM=1; shift ;;
    *) echo "usage: $0 [--log-dir DIR] [--domain ID] [--duration SECS] [--port N] [--debug-stream]" >&2; exit 1 ;;
  esac
done

# ── Sanity checks ─────────────────────────────────────────────────────────────
if [[ ! -x "$BUSMON_BIN" ]]; then
  echo "error: $BUSMON_BIN not found — run: ninja -C $SCRIPT_DIR/build arcal-busmon" >&2
  exit 1
fi

WSL_IP="$(hostname -I | awk '{print $1}')"

# ── Cleanup on exit ───────────────────────────────────────────────────────────
PIDS=()
cleanup() {
  echo ""
  echo "[busmon-ui] shutting down..."
  for pid in "${PIDS[@]}"; do
    kill "$pid" 2>/dev/null || true
  done
  wait 2>/dev/null || true
  rm -f "$FIFO"
  echo "[busmon-ui] done."
}
trap cleanup EXIT INT TERM

# ── Start Python bridge ───────────────────────────────────────────────────────
echo "[busmon-ui] starting web bridge on port $PORT..."
mkdir -p "$LOG_DIR"
(cd "$UI_DIR" && uv run python server.py \
  --fifo    "$FIFO"    \
  --log-dir "$LOG_DIR" \
  --port    "$PORT"    \
  2>&1) &
PIDS+=($!)
sleep 0.5   # give uvicorn time to bind

# ── Start arcal-busmon ────────────────────────────────────────────────────────
BUSMON_ARGS=(--log-dir "$LOG_DIR" --domain "$DOMAIN" --stream "$FIFO")
[[ "$DURATION" -gt 0 ]] && BUSMON_ARGS+=(--duration "$DURATION")
[[ "$DEBUG_STREAM" -eq 1 ]] && BUSMON_ARGS+=(--debug-stream)

echo "[busmon-ui] starting arcal-busmon (domain=$DOMAIN log-dir=$LOG_DIR)..."
(CYCLONEDDS_URI="file://$CYCLONEDDS_XML" "$BUSMON_BIN" "${BUSMON_ARGS[@]}" 2>&1) &
PIDS+=($!)

# ── Ready ─────────────────────────────────────────────────────────────────────
echo ""
echo "  ┌─────────────────────────────────────────────────┐"
echo "  │  arcal-busmon web UI                            │"
echo "  │                                                 │"
echo "  │  Windows browser → http://$WSL_IP:$PORT         │"
echo "  │                                                 │"
echo "  │  (localhost:$PORT also works on Windows 11+)    │"
echo "  │                                                 │"
echo "  │  Press Ctrl-C to stop                          │"
echo "  └─────────────────────────────────────────────────┘"
echo ""

wait "${PIDS[0]}"  # block until bridge exits (or Ctrl-C)
