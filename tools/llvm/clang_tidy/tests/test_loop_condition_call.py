from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


CHECK = "ioj-loop-condition-call"
DECLARATIONS = """
int get_count();
int make_start();
void update();
struct Items {
    int size() const;
    int* begin() const;
    int* end() const;
};
Items get_items();
struct Index {
    bool operator<(Index) const;
    bool operator!=(Index) const;
    bool operator==(Index) const;
    Index& operator++();
};
Index get_end();
struct Predicate { bool operator()() const; };
struct Condition { explicit operator bool() const; };
struct Bound { operator int() const; };
Condition get_condition();
struct ProxyIndex {
    Condition operator!=(ProxyIndex) const;
    ProxyIndex& operator++();
};
"""


class LoopConditionCallTests(unittest.TestCase):
    clang_tidy: str
    source_dir: Path

    def run_tidy(self, *arguments: str) -> str:
        result = subprocess.run(
            [self.clang_tidy, *arguments],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
        )
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, output)
        self.assertNotIn("error:", output)
        return output

    def test_registration(self) -> None:
        output = self.run_tidy(f"-checks=-*,{CHECK}", "-list-checks")
        self.assertIn(CHECK, output)

    def test_conditions_and_excluded_expressions(self) -> None:
        cases = (
            ("member", "for (int i = 0; i < items.size(); ++i) {}", 1),
            ("free", "for (int i = 0; i < get_count(); ++i) {}", 1),
            ("root_call", "for (; get_count();) {}", 1),
            ("condition_declaration", "for (; int count = get_count();) {}", 1),
            ("multiple_calls", "for (int i = 0; i < get_count() + items.size(); ++i) {}", 1),
            ("nested_calls", "for (int i = 0; i < get_items().size(); ++i) {}", 1),
            ("hoisted", "auto const count = items.size(); for (int i = 0; i < count; ++i) {}", 0),
            ("body", "for (int i = 0; i < 10; ++i) { update(); }", 0),
            ("initializer", "for (int i = make_start(); i < 10; ++i) {}", 0),
            ("increment", "for (int i = 0; i < 10; update()) {}", 0),
            ("range", "for (auto const& value : get_items()) {}", 0),
            ("while", "while (get_count()) {}", 0),
            ("do", "do {} while (get_count());", 0),
            ("empty_condition", "for (;;) {}", 0),
            ("operator_less", "Index end; for (Index i; i < end; ++i) {}", 0),
            ("operator_not_equal", "Index end; for (Index i; i != end; ++i) {}", 0),
            ("operator_equal", "Index end; for (Index i; i == end; ++i) {}", 0),
            ("operator_operand_call", "for (Index i; i < get_end(); ++i) {}", 1),
            ("explicit_operator_call", "Index end; for (Index i; i.operator<(end); ++i) {}", 1),
            ("function_object", "Predicate predicate; for (; predicate();) {}", 1),
            ("contextual_conversion", "Condition condition; for (; condition;) {}", 0),
            ("parenthesized_conversion", "Condition condition; for (; (condition);) {}", 0),
            ("comparison_conversion", "Bound bound; for (int i = 0; i < bound; ++i) {}", 0),
            ("comparison_proxy", "ProxyIndex end; for (ProxyIndex i; i != end; ++i) {}", 0),
            ("cast_conversion", "Condition condition; for (; static_cast<bool>(condition);) {}", 0),
            ("conversion_of_call", "for (; get_condition();) {}", 1),
            ("conversion_declaration", "for (; Condition condition = get_condition();) {}", 1),
            ("explicit_conversion", "Condition condition; for (; condition.operator bool();) {}", 1),
            ("explicit_bound_conversion", "Bound bound; for (int i = 0; i < bound.operator int(); ++i) {}", 1),
            ("suppressed_conversion", "Condition condition;\n"
             f"// NOLINTNEXTLINE({CHECK}) -- condition intentionally changes.\n"
             "for (; condition.operator bool();) {}", 0),
            ("nolint", f"for (; get_count();) {{}} // NOLINT({CHECK})", 0),
            ("nolint_block", f"// NOLINTBEGIN({CHECK})\nfor (; get_count();) {{}}\n"
             f"// NOLINTEND({CHECK})", 0),
            ("suppressed", f"// NOLINTNEXTLINE({CHECK}) -- bound intentionally changes.\n"
             "for (int i = 0;\n i < items.size(); ++i) {}", 0),
        )
        with tempfile.TemporaryDirectory(prefix="ioj tidy semantics ") as directory:
            source = Path(directory) / "input.cpp"
            for name, body, warnings in cases:
                with self.subTest(case=name):
                    source.write_text(DECLARATIONS + "void test(Items items) {\n" + body + "\n}\n", encoding="utf-8")
                    output = self.run_tidy(f"-checks=-*,{CHECK}", str(source), "--", "-std=c++23")
                    self.assertEqual(output.count(f"[{CHECK}]"), warnings, output)

    def test_nested_simulation_policy(self) -> None:
        with tempfile.TemporaryDirectory(prefix="ioj tidy policy ") as directory:
            native = Path(directory) / "native"
            simulation = native / "simulation"
            benchmark = native / "simulation_benchmark"
            simulation.mkdir(parents=True)
            benchmark.mkdir()
            shutil.copyfile(self.source_dir / "native/.clang-tidy", native / ".clang-tidy")
            shutil.copyfile(self.source_dir / "native/simulation/.clang-tidy", simulation / ".clang-tidy")
            for scope, expected in ((simulation, 1), (benchmark, 0)):
                with self.subTest(scope=scope.name):
                    source = scope / "input.cpp"
                    source.write_text(
                        "int get_count();\nvoid test() { for (int i = 0; i < get_count(); ++i) {} }\n",
                        encoding="utf-8",
                    )
                    output = self.run_tidy(str(source), "--", "--driver-mode=cl", "/std:c++latest")
                    self.assertEqual(output.count(f"[{CHECK}]"), expected, output)
            effective = self.run_tidy(str(simulation / "input.cpp"), "-list-checks")
            self.assertIn("clang-analyzer-core.CallAndMessage", effective)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--clang-tidy", default="clang-tidy")
    arguments = parser.parse_args()
    LoopConditionCallTests.clang_tidy = arguments.clang_tidy
    LoopConditionCallTests.source_dir = Path.cwd()
    unittest.main(argv=[__file__])
