"""Contract tests for the thin web-to-C bridge."""

import importlib.util
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "web_server.py"
SPEC = importlib.util.spec_from_file_location("web_server", MODULE_PATH)
assert SPEC and SPEC.loader
WEB = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WEB)


class WebBridgeTest(unittest.TestCase):
    def test_validation_rejects_out_of_range_values(self):
        with self.assertRaisesRegex(ValueError, "people"):
            WEB.validated({"people": 100})

    def test_scenario_is_repeatable_and_valid(self):
        settings = WEB.validated({"people": 8, "floors": 6, "lifts": 2, "capacity": 3, "seed": 7})
        self.assertEqual(WEB.scenario_text(settings), WEB.scenario_text(settings))
        self.assertEqual(len(WEB.scenario_text(settings).splitlines()), 13)

    def test_both_runs_use_identical_passengers(self):
        settings = WEB.validated({"people": 6, "floors": 5, "lifts": 2, "capacity": 3, "seed": 9})
        result = WEB.simulate(settings)
        left = [(p["id"], p["arr"], p["from"], p["to"]) for p in result["fcfs"]["passengers"]]
        right = [(p["id"], p["arr"], p["from"], p["to"]) for p in result["nearest"]["passengers"]]
        self.assertEqual(left, right)
        self.assertEqual(len(left), 6)
        self.assertEqual(result["fcfs"]["stats"]["delivered"], 6)
        self.assertEqual(result["nearest"]["stats"]["delivered"], 6)


if __name__ == "__main__":
    unittest.main()
