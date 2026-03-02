from __future__ import annotations

import json
import unittest
from pathlib import Path

from mt_sidecar.rules_validation import validate_rules


class RulesValidationTests(unittest.TestCase):
    def test_real_rules_file_is_valid(self) -> None:
        path = Path(__file__).resolve().parent.parent / "mt_sidecar" / "heuristics_rules.json"
        with path.open("r", encoding="utf-8") as f:
            rules = json.load(f)
        self.assertEqual(validate_rules(rules), [])

    def test_invalid_synonyms_shape(self) -> None:
        rules = {
            "synonyms": {"body": "not-a-list"},
            "contains_rules": [],
        }
        errors = validate_rules(rules)
        self.assertTrue(any("synonyms.body" in err for err in errors))

    def test_invalid_add_map(self) -> None:
        rules = {
            "synonyms": {"body": ["body"]},
            "contains_rules": [{"terms": ["muddy"], "add": {"body": "low"}}],
        }
        errors = validate_rules(rules)
        self.assertTrue(any("contains_rules[0].add" in err for err in errors))


if __name__ == "__main__":
    unittest.main()
