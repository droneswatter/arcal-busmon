from __future__ import annotations

import asyncio
import json
from collections.abc import Awaitable, Callable

import websockets

from .model import SubscriptionInfo

MessageCallback = Callable[[SubscriptionInfo, str], Awaitable[None]]
StatusCallback = Callable[[str], Awaitable[None]]


class OwpClient:
    def __init__(
        self,
        url: str,
        service_id: str,
        schema: str,
        stream_id: str,
        on_message: MessageCallback,
        on_status: StatusCallback | None = None,
    ) -> None:
        self.url = url
        self.service_id = service_id
        self.schema = schema
        self.stream_id = stream_id
        self.on_message = on_message
        self.on_status = on_status
        self._subscriptions: dict[str, SubscriptionInfo] = {}
        self._stopped = asyncio.Event()

    async def stop(self) -> None:
        self._stopped.set()

    async def run_forever(self) -> None:
        while not self._stopped.is_set():
            try:
                await self._run_once()
            except asyncio.CancelledError:
                raise
            except Exception as exc:
                await self._status(f"disconnected: {exc}")
                await asyncio.sleep(1)

    async def _run_once(self) -> None:
        async with websockets.connect(self.url, subprotocols=["owp"]) as ws:
            if ws.subprotocol != "owp":
                raise RuntimeError(f"server selected unsupported subprotocol {ws.subprotocol!r}")

            self._subscriptions.clear()
            await self._status("connected")
            init = {
                "versions": ["1.0"],
                "schema": self.schema,
                "verbose": True,
                "service_id": self.service_id,
            }
            await ws.send("INIT " + json.dumps(init, separators=(",", ":")))
            await ws.send(f"XSUB {self.stream_id} *")

            async for raw in ws:
                if not isinstance(raw, str):
                    continue
                await self._handle_line(raw)

    async def _handle_line(self, line: str) -> None:
        if line == "+OK":
            return
        if line.startswith("-ERR "):
            await self._status(line)
            return
        if line.startswith("INFO "):
            await self._status("info")
            return
        if line.startswith("XSUBINFO "):
            fields = line.split(maxsplit=4)
            if len(fields) != 5:
                await self._status(f"bad XSUBINFO: {line}")
                return
            _, stream_id, sub_id, topic, message_name = fields
            self._subscriptions[sub_id] = SubscriptionInfo(
                stream_id=stream_id,
                subscription_id=sub_id,
                topic=topic,
                message_name=message_name,
            )
            return
        if line.startswith("MSG "):
            fields = line.split(maxsplit=2)
            if len(fields) != 3:
                await self._status(f"bad MSG: {line}")
                return
            _, sub_id, payload = fields
            sub = self._subscriptions.get(sub_id)
            if sub is None:
                await self._status(f"MSG for unknown subscription {sub_id}")
                return
            await self.on_message(sub, payload)
            return

        await self._status(f"unknown OWP operation: {line}")

    async def _status(self, value: str) -> None:
        if self.on_status is not None:
            await self.on_status(value)
