from __future__ import annotations

import json
import os
from typing import Dict, Iterable
from urllib import error, request

from ..schema import extract_json_object, normalize_deltas


def interpret_openai(text: str, profile: str, macros: Iterable[str]) -> Dict[str, float]:
    api_key = os.getenv("OPENAI_API_KEY", "").strip()
    if not api_key:
        return {}

    model = os.getenv("MAGICTRACK_OPENAI_MODEL", "gpt-4.1-mini")
    endpoint = os.getenv("MAGICTRACK_OPENAI_ENDPOINT", "https://api.openai.com/v1/chat/completions")

    system = (
        "You are an audio mixing assistant. Convert user text into macro deltas. "
        "Return JSON only with shape {\"deltas\": {macro: number}}. "
        "Use only these macros: " + ", ".join(macros) + ". "
        "Deltas are points in range -25..25. Prefer conservative moves."
    )

    user = f"profile={profile}\ntext={text}\nOutput JSON only."

    payload = {
        "model": model,
        "temperature": 0.2,
        "messages": [
            {"role": "system", "content": system},
            {"role": "user", "content": user},
        ],
    }

    req = request.Request(
        endpoint,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        },
        method="POST",
    )

    try:
        with request.urlopen(req, timeout=8) as resp:
            body = resp.read().decode("utf-8", errors="replace")
    except (error.URLError, TimeoutError, ValueError):
        return {}

    try:
        parsed = json.loads(body)
        content = parsed["choices"][0]["message"]["content"]
        if not isinstance(content, str):
            return {}
    except (KeyError, IndexError, TypeError, json.JSONDecodeError):
        return {}

    obj = extract_json_object(content)
    deltas = obj.get("deltas") if isinstance(obj, dict) else None
    if not isinstance(deltas, dict):
        deltas = obj
    if not isinstance(deltas, dict):
        return {}

    return normalize_deltas(deltas, macros)
