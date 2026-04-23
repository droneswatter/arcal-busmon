from __future__ import annotations

import asyncio
import json
import time
from collections import deque
from pathlib import Path
from typing import Any

from .model import MessageMeta


class MessageStore:
    def __init__(self, log_dir: Path, max_history: int = 10_000) -> None:
        self.log_dir = log_dir
        self.max_history = max_history
        self._lock = asyncio.Lock()
        self._seq = 0
        self._history: deque[MessageMeta] = deque(maxlen=max_history)
        self._message_order: deque[int] = deque()
        self._messages: dict[int, Any] = {}

    async def add(self, topic: str, message_name: str, payload_text: str) -> MessageMeta:
        payload = json.loads(payload_text)
        encoded = json.dumps(payload, separators=(",", ":")).encode("utf-8")

        async with self._lock:
            self._seq += 1
            seq = self._seq
            meta = MessageMeta(
                seq=seq,
                topic=topic,
                type=message_name,
                bytes=len(encoded),
                ts_ms=int(time.time() * 1000),
            )
            self._history.append(meta)
            self._messages[seq] = payload
            self._message_order.append(seq)
            while len(self._message_order) > self.max_history:
                old_seq = self._message_order.popleft()
                self._messages.pop(old_seq, None)

        await asyncio.to_thread(self._write_log, topic, seq, payload)
        return meta

    async def history(self) -> list[dict[str, Any]]:
        async with self._lock:
            return [m.to_json() for m in self._history]

    async def total(self) -> int:
        async with self._lock:
            return self._seq

    async def get(self, seq: int) -> Any | None:
        async with self._lock:
            return self._messages.get(seq)

    def _write_log(self, topic: str, seq: int, payload: Any) -> None:
        topic_dir = self.log_dir / sanitize(topic)
        topic_dir.mkdir(parents=True, exist_ok=True)
        path = topic_dir / f"{seq:010d}.json"
        path.write_text(json.dumps(payload, indent=2) + "\n")


def sanitize(name: str) -> str:
    return name.replace("/", "_").replace(":", "_").replace("\\", "_")
