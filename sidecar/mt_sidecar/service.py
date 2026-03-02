from __future__ import annotations

from typing import Callable, Dict, Iterable

from .heuristics import heuristic_interpret
from .providers.ollama_provider import interpret_local_model
from .providers.openai_provider import interpret_openai
from .schema import normalize_deltas

ProviderFn = Callable[[str, str, Iterable[str]], Dict[str, float]]


def interpret_request(
    text: str,
    profile: str,
    allowed_macros: list[str],
    local_provider: ProviderFn = interpret_local_model,
    openai_provider: ProviderFn = interpret_openai,
) -> Dict[str, object]:
    attempted: list[str] = []

    # Deterministic first for known intents.
    deltas = normalize_deltas(heuristic_interpret(text), allowed_macros)
    source = "heuristic"

    if not deltas:
        attempted.append("qwen-local")
        deltas = local_provider(text, profile, allowed_macros)
        source = "qwen-local"

    if not deltas:
        attempted.append("openai")
        deltas = openai_provider(text, profile, allowed_macros)
        source = "openai"

    reason = "ok" if deltas else ("no_intent_after_llm" if attempted else "no_intent")
    return {
        "deltas": deltas,
        "source": source,
        "reason": reason,
        "attempted": attempted,
    }
