from __future__ import annotations

import json
import re
from typing import Dict, Iterable, Tuple

ALLOWED_MACROS: Tuple[str, ...] = (
    "level",
    "brightness",
    "harshness",
    "body",
    "tighten",
    "crunch",
    "width",
    "space",
)


def clamp_delta(value: float, low: float = -25.0, high: float = 25.0) -> float:
    return max(low, min(high, value))


def normalize_deltas(candidate: Dict[str, object], allowed: Iterable[str]) -> Dict[str, float]:
    out: Dict[str, float] = {}
    allowed_set = set(allowed)

    for key, raw in candidate.items():
        if key not in allowed_set:
            continue

        if not isinstance(raw, (int, float)):
            continue

        val = clamp_delta(float(raw))
        if abs(val) < 0.01:
            continue

        out[key] = round(val, 2)

    return out


def extract_json_object(text: str) -> Dict[str, object]:
    text = text.strip()
    if not text:
        return {}

    try:
        parsed = json.loads(text)
        if isinstance(parsed, dict):
            return parsed
    except json.JSONDecodeError:
        pass

    match = re.search(r"\{.*\}", text, flags=re.DOTALL)
    if not match:
        return {}

    try:
        parsed = json.loads(match.group(0))
        return parsed if isinstance(parsed, dict) else {}
    except json.JSONDecodeError:
        return {}


def parse_request_payload(payload: Dict[str, object]) -> tuple[str, str, list[str]]:
    text = str(payload.get("text", "")).strip()
    profile = str(payload.get("profile", "General")).strip()

    macros = payload.get("macros", list(ALLOWED_MACROS))
    if not isinstance(macros, list) or not macros:
        macros = list(ALLOWED_MACROS)

    allowed = [m for m in macros if isinstance(m, str) and m in ALLOWED_MACROS]
    if not allowed:
        allowed = list(ALLOWED_MACROS)

    return text, profile, allowed
