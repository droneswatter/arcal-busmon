from __future__ import annotations

from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True)
class SubscriptionInfo:
    stream_id: str
    subscription_id: str
    topic: str
    message_name: str


@dataclass(frozen=True)
class MessageMeta:
    seq: int
    topic: str
    type: str
    bytes: int
    ts_ms: int
    tag: str = ""

    def to_json(self) -> dict[str, Any]:
        return {
            "seq": self.seq,
            "topic": self.topic,
            "type": self.type,
            "bytes": self.bytes,
            "ts_ms": self.ts_ms,
            "tag": self.tag,
        }
