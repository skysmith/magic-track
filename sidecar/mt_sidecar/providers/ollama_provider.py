from __future__ import annotations

import json
import os
import re
import subprocess
from typing import Dict, Iterable
from urllib import error, request

from ..schema import extract_json_object, normalize_deltas


def interpret_local_model(text: str, profile: str, macros: Iterable[str]) -> Dict[str, float]:
    endpoint = os.getenv("MAGICTRACK_LOCAL_QWEN_OLLAMA_URL", "").strip()
    endpoint_candidates = [
        endpoint,
        "http://127.0.0.1:11434/api/chat",
        "http://127.0.0.1:11434/api/generate",
        "http://127.0.0.1:11434/v1/chat/completions",
    ]
    endpoint_candidates = [e for e in endpoint_candidates if e]
    if not endpoint_candidates:
        return {}

    model = os.getenv("MAGICTRACK_LOCAL_QWEN_MODEL", "llama3.2:3b").strip()
    if not model:
        return {}

    system = (
        "You are an audio mixing assistant. "
        "Convert user intent to conservative macro deltas. "
        "Return JSON only with shape {\"deltas\": {macro: number}}. "
        f"Allowed macros: {', '.join(macros)}. "
        "Delta range: -25..25."
    )
    user = f"profile={profile}\ntext={text}\nJSON only."
    content = ""

    for endpoint_url in endpoint_candidates:
        if endpoint_url.endswith("/api/generate"):
            payload = {
                "model": model,
                "prompt": system + "\n" + user,
                "stream": False,
                "options": {"temperature": 0.2},
            }
        elif endpoint_url.endswith("/v1/chat/completions"):
            payload = {
                "model": model,
                "temperature": 0.2,
                "messages": [
                    {"role": "system", "content": system},
                    {"role": "user", "content": user},
                ],
            }
        else:
            payload = {
                "model": model,
                "messages": [
                    {"role": "system", "content": system},
                    {"role": "user", "content": user},
                ],
                "stream": False,
                "options": {"temperature": 0.2},
            }

        req = request.Request(
            endpoint_url,
            data=json.dumps(payload).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )

        try:
            with request.urlopen(req, timeout=8) as resp:
                body = resp.read().decode("utf-8", errors="replace")
        except (error.URLError, TimeoutError, ValueError, error.HTTPError):
            continue

        try:
            parsed = json.loads(body)
        except json.JSONDecodeError:
            continue

        if endpoint_url.endswith("/api/generate"):
            maybe = parsed.get("response", "")
            if isinstance(maybe, str):
                content = maybe
                break
        elif endpoint_url.endswith("/v1/chat/completions"):
            try:
                maybe = parsed["choices"][0]["message"]["content"]
                if isinstance(maybe, str):
                    content = maybe
                    break
            except (KeyError, IndexError, TypeError):
                continue
        else:
            message = parsed.get("message", {})
            maybe = message.get("content", "") if isinstance(message, dict) else ""
            if isinstance(maybe, str) and maybe:
                content = maybe
                break

    if not content:
        # Some environments expose working `ollama run` but disabled HTTP generation routes.
        prompt = f"{system}\n{user}\nReturn JSON only.\n"
        try:
            proc = subprocess.run(
                ["ollama", "run", model, prompt],
                capture_output=True,
                text=True,
                timeout=20,
                check=False,
            )
            if proc.returncode == 0 and proc.stdout:
                cleaned = re.sub(r"\x1B\\[[0-9;?]*[A-Za-z]", "", proc.stdout)
                cleaned = re.sub(r"\x1B\\].*?\x07", "", cleaned)
                content = cleaned
        except (subprocess.SubprocessError, FileNotFoundError):
            return {}

    if not content:
        return {}

    obj = extract_json_object(content)
    deltas = obj.get("deltas") if isinstance(obj, dict) else None
    if not isinstance(deltas, dict):
        deltas = obj
    if not isinstance(deltas, dict):
        return {}

    return normalize_deltas(deltas, macros)
