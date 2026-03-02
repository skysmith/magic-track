from __future__ import annotations

import unittest

from mt_sidecar.heuristics import heuristic_interpret
from mt_sidecar.service import interpret_request

ALLOWED = ["level", "brightness", "harshness", "body", "tighten", "crunch", "width", "space"]


class HeuristicTests(unittest.TestCase):
    def test_muddy_reduces_body_and_tightens(self) -> None:
        deltas = heuristic_interpret("too muddy")
        self.assertLess(deltas.get("body", 0.0), 0.0)
        self.assertGreater(deltas.get("tighten", 0.0), 0.0)

    def test_unknown_adjective_fallback(self) -> None:
        deltas = heuristic_interpret("too floofy")
        self.assertLess(deltas.get("body", 0.0), 0.0)
        self.assertGreater(deltas.get("tighten", 0.0), 0.0)


class ServiceTests(unittest.TestCase):
    def test_heuristic_preferred_for_known_phrase(self) -> None:
        called = {"local": 0, "openai": 0}

        def local_provider(*_args):
            called["local"] += 1
            return {"brightness": 4.0}

        def openai_provider(*_args):
            called["openai"] += 1
            return {"brightness": 4.0}

        result = interpret_request("too muddy", "General", ALLOWED, local_provider, openai_provider)
        self.assertEqual(result["source"], "heuristic")
        self.assertEqual(result["attempted"], [])
        self.assertEqual(called["local"], 0)
        self.assertEqual(called["openai"], 0)

    def test_provider_chain_used_when_no_heuristic_intent(self) -> None:
        called = {"local": 0, "openai": 0}

        def local_provider(*_args):
            called["local"] += 1
            return {}

        def openai_provider(*_args):
            called["openai"] += 1
            return {"brightness": 3.0}

        result = interpret_request("flarny glorp", "General", ALLOWED, local_provider, openai_provider)
        self.assertEqual(result["source"], "openai")
        self.assertEqual(result["reason"], "ok")
        self.assertEqual(result["attempted"], ["qwen-local", "openai"])
        self.assertGreater(result["deltas"].get("brightness", 0.0), 0.0)

    def test_no_intent_after_llm(self) -> None:
        result = interpret_request(
            "flarny glorp",
            "General",
            ALLOWED,
            local_provider=lambda *_args: {},
            openai_provider=lambda *_args: {},
        )
        self.assertEqual(result["source"], "openai")
        self.assertEqual(result["reason"], "no_intent_after_llm")
        self.assertEqual(result["deltas"], {})


if __name__ == "__main__":
    unittest.main()
