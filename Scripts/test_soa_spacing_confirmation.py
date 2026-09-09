"""Observable case-selection and acceptance-rule checks for the spacing experiment."""
from __future__ import annotations

import unittest
from pathlib import Path
from typing import Any

from soa_spacing_confirmation import confirmation_cases, field_paths, summarize

ROOT = Path(__file__).resolve().parent.parent


def records(gap64: float = 100., gap192: float = 100.) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for item in confirmation_cases():
        for repetition in range(3):
            mean = {0: 200., 64: gap64, 192: gap192}[item["gap"]]
            result.append({**item, "repetition": repetition, "mean_ns": mean, "samples": 100,
                           "extra_bytes": item["gap"] * 10, "layout_identity": str(item["gap"])})
    return result


class ConfirmationTests(unittest.TestCase):
    def test_case_matrix_and_schema_widths(self) -> None:
        cases = confirmation_cases()
        self.assertEqual(len(cases) * 3, 288)
        self.assertEqual(len({tuple(item.values()) for item in cases}), len(cases))
        self.assertEqual({item["live"] for item in cases}, {65535})
        self.assertEqual([len(field_paths(ROOT, schema)) for schema in range(4)], [53, 24, 42, 15])
        self.assertEqual(field_paths(ROOT, 3)[-1], "bundle2.aligned256")

    def test_prefers_smallest_passing_gap(self) -> None:
        _, decision = summarize(records(105., 100.))
        self.assertTrue(decision["complete"])
        self.assertEqual(decision["selected_gap"], 64)

    def test_chooses_192_when_64_misses_threshold(self) -> None:
        _, decision = summarize(records(120., 100.))
        self.assertEqual(decision["selected_gap"], 192)

    def test_archived_zero_gap_generated_controls_remain_complete(self) -> None:
        data = records()
        for row in data:
            if row["owner"] == 1:
                row["gap"] = 0
        _, decision = summarize(data)
        self.assertTrue(decision["complete"])

    def test_rejects_cross_schema_regressions(self) -> None:
        data = records()
        for row in data:
            if (row["schema"], row["gap"]) in ((1, 64), (2, 192)):
                row["mean_ns"] = 120.
        _, decision = summarize(data)
        self.assertIsNone(decision["selected_gap"])

    def test_incomplete_or_smoke_results_cannot_select_policy(self) -> None:
        data = records()
        _, incomplete = summarize(data[:-1])
        self.assertFalse(incomplete["complete"])
        self.assertIsNone(incomplete["selected_gap"])
        for row in data:
            row["samples"] = 1
        _, smoke = summarize(data)
        self.assertIsNone(smoke["selected_gap"])

    def test_equivalent_layouts_share_evidence_and_variation_is_visible(self) -> None:
        data = records(105., 100.)
        for row in data:
            if row["gap"] != 0:
                row["layout_identity"] = "same-offsets"
        data[3]["mean_ns"] = 1000.
        _, decision = summarize(data)
        self.assertEqual(decision["selected_gap"], 64)
        self.assertTrue(decision["equivalent_layouts"])
        self.assertTrue(decision["variable_groups"])


if __name__ == "__main__":
    unittest.main()
