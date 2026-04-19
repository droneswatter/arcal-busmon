#!/usr/bin/env python3
"""arcal-busmon web bridge.

Reads newline-delimited JSON metadata from a busmon FIFO, broadcasts it to
browser clients over WebSocket, and serves full message JSON from log files
on demand.

Usage:
    python server.py [--fifo PATH] [--log-dir DIR] [--port N]

Env vars (fallback):
    BUSMON_FIFO     path to the named pipe (default: /tmp/busmon.fifo)
    BUSMON_LOG_DIR  busmon log directory (default: ./busmon-out)
"""

import argparse
import asyncio
import json
import os
import threading
import time
from contextlib import asynccontextmanager
from pathlib import Path

import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, JSONResponse

# ── Config ────────────────────────────────────────────────────────────────────

parser = argparse.ArgumentParser(add_help=False)
parser.add_argument("--fifo",    default=os.environ.get("BUSMON_FIFO",    "/tmp/busmon.fifo"))
parser.add_argument("--log-dir", default=os.environ.get("BUSMON_LOG_DIR", "busmon-out"))
parser.add_argument("--port",    type=int, default=8765)
args, _ = parser.parse_known_args()

FIFO_PATH   = args.fifo
LOG_DIR     = Path(args.log_dir)
PORT        = args.port
BATCH_MS    = 50        # push to browsers every 50 ms (20 Hz)
MAX_HISTORY = 10_000    # ring of recent metadata kept for late-joining clients

# ── App ───────────────────────────────────────────────────────────────────────

@asynccontextmanager
async def lifespan(app: FastAPI):
    global _loop
    _loop = asyncio.get_event_loop()
    threading.Thread(target=_fifo_reader, daemon=True).start()
    asyncio.create_task(broadcaster())
    print(f"[bridge] listening on http://0.0.0.0:{PORT}")
    print(f"[bridge] fifo={FIFO_PATH}  log-dir={LOG_DIR}")
    yield


app = FastAPI(lifespan=lifespan)

_clients:  set[WebSocket]  = set()
_history:  list[dict]      = []        # ring buffer of raw metadata dicts
_queue:    asyncio.Queue   = asyncio.Queue(maxsize=200_000)
_loop:     asyncio.AbstractEventLoop | None = None
_total:    int             = 0


# ── Routes ────────────────────────────────────────────────────────────────────

@app.get("/")
async def index():
    return FileResponse(Path(__file__).parent / "static" / "index.html")


@app.websocket("/ws")
async def ws_endpoint(websocket: WebSocket):
    await websocket.accept()
    _clients.add(websocket)
    # Send recent history so the page isn't blank on load.
    if _history:
        try:
            await websocket.send_text(
                json.dumps({"batch": list(_history), "total": _total})
            )
        except Exception:
            pass
    try:
        # Keep the connection alive; data flows from broadcaster().
        while True:
            await asyncio.sleep(3600)
    except (WebSocketDisconnect, Exception):
        pass
    finally:
        _clients.discard(websocket)


@app.get("/message/{topic}/{seq}")
async def get_message(topic: str, seq: int):
    """Return the full JSON for a single logged message."""
    safe = _sanitize(topic)
    path = LOG_DIR / safe / f"{seq:010d}.json"
    if not path.exists():
        return JSONResponse({"error": "not found"}, status_code=404)
    try:
        return JSONResponse(json.loads(path.read_text()))
    except Exception as e:
        return JSONResponse({"error": str(e)}, status_code=500)


# ── Background tasks ──────────────────────────────────────────────────────────

async def broadcaster():
    """Drain the queue every BATCH_MS and push to all WebSocket clients."""
    global _total
    while True:
        await asyncio.sleep(BATCH_MS / 1000)

        batch: list[dict] = []
        while not _queue.empty() and len(batch) < 1000:
            try:
                batch.append(_queue.get_nowait())
            except asyncio.QueueEmpty:
                break

        if not batch:
            continue

        # Update ring buffer.
        _history.extend(batch)
        if len(_history) > MAX_HISTORY:
            del _history[: len(_history) - MAX_HISTORY]
        _total += len(batch)

        payload = json.dumps({"batch": batch, "total": _total})
        dead: set[WebSocket] = set()
        for client in list(_clients):
            try:
                await client.send_text(payload)
            except Exception:
                dead.add(client)
        _clients -= dead


def _fifo_reader():
    """Background thread: open FIFO, read NDJSON lines, enqueue metadata.

    Uses a plain blocking open() so the thread waits here until busmon opens
    the write end — no polling, no EOF cycling.  When busmon exits (write end
    closes) read() returns EOF and we loop back to wait for the next start.
    """
    while True:
        try:
            if not Path(FIFO_PATH).exists():
                time.sleep(0.5)
                continue

            # Blocking open — waits until busmon opens the write end.
            with open(FIFO_PATH, "r", buffering=1) as f:
                print(f"[bridge] connected to {FIFO_PATH}")
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        obj = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    if _loop is not None:
                        asyncio.run_coroutine_threadsafe(_queue.put(obj), _loop)

            print("[bridge] busmon disconnected, waiting...")

        except OSError as exc:
            print(f"[bridge] FIFO error: {exc}, retrying in 1 s...")
            time.sleep(1)


def _sanitize(name: str) -> str:
    """Mirror LogWriter::sanitize() from C++."""
    return name.replace("/", "_").replace(":", "_").replace("\\", "_")



if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=PORT)
