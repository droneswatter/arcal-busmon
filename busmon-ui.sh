#!/usr/bin/env bash
# busmon-ui.sh - start arlacal-server and print the static busmon UI URL.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARCAL_DIR="${ARCAL_DIR:-$SCRIPT_DIR/../arcal}"
ARCAL_BUILD_DIR="${ARCAL_BUILD_DIR:-$ARCAL_DIR/build}"
ARLACAL_BIN="$ARCAL_BUILD_DIR/lacal/arlacal-server"
CYCLONEDDS_XML="${CYCLONEDDS_XML:-$ARCAL_DIR/test/e2e/cyclonedds_localhost.xml}"

DOMAIN=0
LACAL_HOST="127.0.0.1"
LACAL_PORT=8766
START_ARLACAL=1

usage() {
  echo "usage: $0 [--domain ID] [--host ADDR] [--lacal-port N] [--lacal-url URL] [--no-start-arlacal]" >&2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --domain) DOMAIN="$2"; shift 2 ;;
    --host) LACAL_HOST="$2"; shift 2 ;;
    --lacal-port) LACAL_PORT="$2"; shift 2 ;;
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
BUSMON_UI_URL="file://$SCRIPT_DIR/ui/static/index.html?ws=$LACAL_URL"
WINDOWS_UI_URL=""

if [[ -n "${WSL_DISTRO_NAME:-}" ]] && command -v wslpath >/dev/null 2>&1; then
  WINDOWS_UI_PATH="$(wslpath -w "$SCRIPT_DIR/ui/static/index.html")"
  WINDOWS_UI_PATH="${WINDOWS_UI_PATH/#\\\\wsl.localhost\\/\\\\wsl$\\}"
  WINDOWS_UI_PATH="${WINDOWS_UI_PATH//\\//}"
  WINDOWS_UI_URL="file:$WINDOWS_UI_PATH?ws=$LACAL_URL"
fi

if [[ "$START_ARLACAL" -eq 1 && ! -x "$ARLACAL_BIN" ]]; then
  echo "error: $ARLACAL_BIN not found - run: cmake --build $ARCAL_BUILD_DIR --target arlacal-server" >&2
  exit 1
fi

PIDS=()
CLEANED_UP=0

cleanup() {
  if [[ "$CLEANED_UP" -eq 1 ]]; then
    return
  fi
  CLEANED_UP=1
  if [[ "${#PIDS[@]}" -eq 0 ]]; then
    return
  fi
  echo ""
  echo "[busmon-ui] shutting down..."
  for pid in "${PIDS[@]}"; do
    kill -TERM -- "-$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null || true
  done
  for _ in {1..20}; do
    local alive=0
    for pid in "${PIDS[@]}"; do
      kill -0 "$pid" 2>/dev/null && alive=1
    done
    [[ "$alive" -eq 0 ]] && break
    sleep 0.1
  done
  for pid in "${PIDS[@]}"; do
    kill -KILL -- "-$pid" 2>/dev/null || kill -KILL "$pid" 2>/dev/null || true
  done
  wait 2>/dev/null || true
  echo "[busmon-ui] done."
}

trap cleanup EXIT
trap 'cleanup; exit 130' INT
trap 'cleanup; exit 143' TERM

if [[ "$START_ARLACAL" -eq 1 ]]; then
  echo "[busmon-ui] starting arlacal-server on $LACAL_URL..."
  (
    cd "$ARCAL_DIR"
    CYCLONEDDS_URI="file://$CYCLONEDDS_XML" \
      exec setsid "$ARLACAL_BIN" --host "$LACAL_HOST" --port "$LACAL_PORT" --domain "$DOMAIN" 2>&1
  ) &
  PIDS+=($!)
  sleep 0.5
fi

echo ""
echo "arcal-busmon static UI"
echo ""
echo "Open this URL in a browser:"
if [[ -n "$WINDOWS_UI_URL" ]]; then
  echo "  $WINDOWS_UI_URL"
else
  echo "  $BUSMON_UI_URL"
fi
if [[ -n "$WINDOWS_UI_URL" ]]; then
  echo ""
  echo "Native path     -> $BUSMON_UI_URL"
fi
echo ""
echo "LA-CAL source   -> $LACAL_URL"
if [[ "$START_ARLACAL" -eq 1 ]]; then
  echo "Press Ctrl-C to stop arlacal-server"
else
  echo "Static page only; no local service started"
fi
echo ""

if [[ "$START_ARLACAL" -eq 1 ]]; then
  wait "${PIDS[-1]}"
fi
