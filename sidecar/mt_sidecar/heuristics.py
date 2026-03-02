from __future__ import annotations

import json
import re
from functools import lru_cache
from pathlib import Path
from typing import Any, Dict

from .rules_validation import validate_rules
from .schema import clamp_delta

RULES_PATH = Path(__file__).with_name("heuristics_rules.json")


@lru_cache(maxsize=1)
def _load_rules() -> Dict[str, Any]:
    with RULES_PATH.open("r", encoding="utf-8") as f:
        parsed = json.load(f)
    if not isinstance(parsed, dict):
        raise RuntimeError(f"Invalid heuristics rules file: expected object at {RULES_PATH}")
    errors = validate_rules(parsed)
    if errors:
        joined = "; ".join(errors)
        raise RuntimeError(f"Invalid heuristics rules file {RULES_PATH}: {joined}")
    return parsed


def _apply_add_map(deltas: Dict[str, float], add_map: Dict[str, Any]) -> None:
    for name, amount in add_map.items():
        if not isinstance(name, str) or not isinstance(amount, (int, float)):
            continue
        deltas[name] = clamp_delta(deltas.get(name, 0.0) + float(amount))


def heuristic_interpret(text: str) -> Dict[str, float]:
    t = text.lower().strip()
    if not t:
        return {}

    rules = _load_rules()
    deltas: Dict[str, float] = {}

    for rule in rules.get("contains_rules", []):
        if not isinstance(rule, dict):
            continue
        terms = rule.get("terms", [])
        if not isinstance(terms, list):
            continue
        if any(isinstance(term, str) and term in t for term in terms):
            add_map = rule.get("add", {})
            if isinstance(add_map, dict):
                _apply_add_map(deltas, add_map)

    for rule in rules.get("contains_guard_rules", []):
        if not isinstance(rule, dict):
            continue
        terms = rule.get("terms", [])
        terms_absent = rule.get("terms_absent", [])
        if not isinstance(terms, list) or not isinstance(terms_absent, list):
            continue
        matches_present = any(isinstance(term, str) and term in t for term in terms)
        matches_absent = all(isinstance(term, str) and term not in t for term in terms_absent)
        if matches_present and matches_absent:
            add_map = rule.get("add", {})
            if isinstance(add_map, dict):
                _apply_add_map(deltas, add_map)

    synonyms = rules.get("synonyms", {})
    modifier_cfg = rules.get("generic_modifiers", {})
    more_terms = modifier_cfg.get("more_terms", [])
    less_terms = modifier_cfg.get("less_terms", [])
    without_terms = modifier_cfg.get("without_terms", [])
    not_macro_template = modifier_cfg.get("not_macro_template", "not {macro}")
    more_add = float(modifier_cfg.get("more_add", 6.0))
    less_add = float(modifier_cfg.get("less_add", -6.0))
    without_add = float(modifier_cfg.get("without_add", -6.0))

    for macro, aliases in synonyms.items():
        if not isinstance(macro, str) or not isinstance(aliases, list):
            continue
        if any(isinstance(alias, str) and alias in t for alias in aliases):
            if any(isinstance(term, str) and term in t for term in more_terms):
                _apply_add_map(deltas, {macro: more_add})
            if any(isinstance(term, str) and term in t for term in less_terms):
                _apply_add_map(deltas, {macro: less_add})
            if (isinstance(not_macro_template, str) and not_macro_template.format(macro=macro) in t) or any(
                isinstance(term, str) and term in t for term in without_terms
            ):
                _apply_add_map(deltas, {macro: without_add})

    for rule in rules.get("regex_rules", []):
        if not isinstance(rule, dict):
            continue
        pattern = rule.get("pattern")
        add_map = rule.get("add", {})
        if not isinstance(pattern, str) or not isinstance(add_map, dict):
            continue
        if re.search(pattern, t):
            _apply_add_map(deltas, add_map)

    if not deltas:
        fallback = rules.get("unknown_adjective_fallback", {})
        patterns = fallback.get("patterns", [])
        add_map = fallback.get("add", {})
        if isinstance(patterns, list) and isinstance(add_map, dict):
            if any(isinstance(pattern, str) and re.search(pattern, t) for pattern in patterns):
                _apply_add_map(deltas, add_map)

    return {k: v for k, v in deltas.items() if abs(v) >= 0.01}
