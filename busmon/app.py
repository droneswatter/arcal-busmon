from __future__ import annotations

import argparse
import asyncio
import json
from contextlib import asynccontextmanager, suppress
from pathlib import Path
from typing import Any

import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, JSONResponse

from .message_store import MessageStore
from .model import MessageMeta, SubscriptionInfo
from .owp_client import OwpClient


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lacal-url", default="ws://127.0.0.1:8766")
    parser.add_argument("--schema", default="2.5.0")
    parser.add_argument("--service-id", default="busmon")
    parser.add_argument("--stream-id", default="busmon")
    parser.add_argument("--log-dir", default="/tmp/busmon-out")
    parser.add_argument("--port", type=int, default=8765)
    return parser.parse_args()


args = parse_args()
store = MessageStore(Path(args.log_dir))
queue: asyncio.Queue[dict[str, Any]] = asyncio.Queue(maxsize=200_000)
clients: set[WebSocket] = set()
owp_task: asyncio.Task[None] | None = None

BATCH_MS = 50
STATIC_DIR = Path(__file__).resolve().parents[1] / "ui" / "static"


async def on_lacal_message(sub: SubscriptionInfo, payload: str) -> None:
    meta = await store.add(sub.topic, sub.message_name, payload)
    await queue.put(meta.to_json())


async def on_lacal_status(value: str) -> None:
    print(f"[busmon] lacal {value}")


@asynccontextmanager
async def lifespan(app: FastAPI):
    global owp_task
    Path(args.log_dir).mkdir(parents=True, exist_ok=True)
    client = OwpClient(
        url=args.lacal_url,
        service_id=args.service_id,
        schema=args.schema,
        stream_id=args.stream_id,
        on_message=on_lacal_message,
        on_status=on_lacal_status,
    )
    owp_task = asyncio.create_task(client.run_forever())
    broadcaster_task = asyncio.create_task(broadcaster())
    print(f"[busmon] listening on http://0.0.0.0:{args.port}")
    print(f"[busmon] lacal={args.lacal_url} log-dir={args.log_dir}")
    try:
        yield
    finally:
        await client.stop()
        if owp_task:
            owp_task.cancel()
            with suppress(asyncio.CancelledError):
                await owp_task
        broadcaster_task.cancel()
        with suppress(asyncio.CancelledError):
            await broadcaster_task


app = FastAPI(lifespan=lifespan)


@app.get("/")
async def index():
    return FileResponse(STATIC_DIR / "index.html")


@app.websocket("/ws")
async def ws_endpoint(websocket: WebSocket):
    await websocket.accept()
    clients.add(websocket)
    history = await store.history()
    if history:
        await websocket.send_text(json.dumps({"batch": history, "total": await store.total()}))
    try:
        while True:
            await asyncio.sleep(3600)
    except (WebSocketDisconnect, asyncio.CancelledError):
        pass
    finally:
        clients.discard(websocket)


@app.get("/message/{seq}")
async def get_message_by_seq(seq: int):
    payload = await store.get(seq)
    if payload is None:
        return JSONResponse({"error": "not found"}, status_code=404)
    return JSONResponse(payload)


@app.get("/message/{topic}/{seq}")
async def get_message_by_topic_seq(topic: str, seq: int):
    del topic
    return await get_message_by_seq(seq)


async def broadcaster() -> None:
    total = 0
    while True:
        await asyncio.sleep(BATCH_MS / 1000)
        batch: list[dict[str, Any]] = []
        while not queue.empty() and len(batch) < 1000:
            batch.append(queue.get_nowait())
        if not batch:
            continue
        total = await store.total()
        payload = json.dumps({"batch": batch, "total": total})
        dead: set[WebSocket] = set()
        for client in list(clients):
            try:
                await client.send_text(payload)
            except Exception:
                dead.add(client)
        clients.difference_update(dead)


def main() -> None:
    uvicorn.run(app, host="0.0.0.0", port=args.port)


if __name__ == "__main__":
    main()
