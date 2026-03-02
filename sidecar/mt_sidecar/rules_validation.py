from __future__ import annotations

from typing import Any, Dict, List


def _is_str_list(value: Any) -> bool:
    return isinstance(value, list) and all(isinstance(x, str) for x in value)


def _is_num(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _is_add_map(value: Any) -> bool:
    return isinstance(value, dict) and all(isinstance(k, str) and _is_num(v) for k, v in value.items())


def validate_rules(rules: Dict[str, Any]) -> List[str]:
    errors: List[str] = []

    if not isinstance(rules, dict):
        return ["rules must be an object"]

    if "synonyms" not in rules or not isinstance(rules["synonyms"], dict):
        errors.append("synonyms must be an object")
    else:
        for macro, aliases in rules["synonyms"].items():
            if not isinstance(macro, str):
                errors.append("synonyms keys must be strings")
                continue
            if not _is_str_list(aliases):
                errors.append(f"synonyms.{macro} must be an array of strings")

    def validate_rule_array(key: str, needs_terms: bool = True, needs_add: bool = True) -> None:
        value = rules.get(key)
        if value is None:
            return
        if not isinstance(value, list):
            errors.append(f"{key} must be an array")
            return
        for i, rule in enumerate(value):
            if not isinstance(rule, dict):
                errors.append(f"{key}[{i}] must be an object")
                continue
            if needs_terms and not _is_str_list(rule.get("terms")):
                errors.append(f"{key}[{i}].terms must be an array of strings")
            if "terms_absent" in rule and not _is_str_list(rule.get("terms_absent")):
                errors.append(f"{key}[{i}].terms_absent must be an array of strings")
            if "pattern" in rule and not isinstance(rule.get("pattern"), str):
                errors.append(f"{key}[{i}].pattern must be a string")
            if needs_add and not _is_add_map(rule.get("add")):
                errors.append(f"{key}[{i}].add must be an object of numeric macro deltas")

    validate_rule_array("contains_rules")
    validate_rule_array("contains_guard_rules")
    validate_rule_array("regex_rules", needs_terms=False)

    modifiers = rules.get("generic_modifiers")
    if modifiers is not None:
        if not isinstance(modifiers, dict):
            errors.append("generic_modifiers must be an object")
        else:
            for name in ["more_terms", "less_terms", "without_terms"]:
                if name in modifiers and not _is_str_list(modifiers[name]):
                    errors.append(f"generic_modifiers.{name} must be an array of strings")
            for name in ["more_add", "less_add", "without_add"]:
                if name in modifiers and not _is_num(modifiers[name]):
                    errors.append(f"generic_modifiers.{name} must be numeric")
            if "not_macro_template" in modifiers and not isinstance(modifiers["not_macro_template"], str):
                errors.append("generic_modifiers.not_macro_template must be a string")

    fallback = rules.get("unknown_adjective_fallback")
    if fallback is not None:
        if not isinstance(fallback, dict):
            errors.append("unknown_adjective_fallback must be an object")
        else:
            if "patterns" in fallback and not _is_str_list(fallback["patterns"]):
                errors.append("unknown_adjective_fallback.patterns must be an array of strings")
            if "add" in fallback and not _is_add_map(fallback["add"]):
                errors.append("unknown_adjective_fallback.add must be an object of numeric macro deltas")

    return errors
