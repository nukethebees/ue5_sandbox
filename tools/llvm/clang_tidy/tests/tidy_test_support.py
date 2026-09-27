from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest


VIEWS = """
namespace std {
template<class T> struct span { span(); span(T*, int); };
}
namespace ml {
template<class T> struct Vector3SoAView { Vector3SoAView(); Vector3SoAView(T*, T*, T*, int); };
namespace soa_storage_detail { template<bool Const, auto Require> struct CompactViewState {}; }
}
namespace ioj::sim {
struct Compact : ml::soa_storage_detail::CompactViewState<false, 0> { Compact(); };
struct LineTracesConstView { std::span<int> starts; std::span<int> ends; };
struct PlayerReadView { int snapshot; };
}
using View = ml::Vector3SoAView<float>;
struct OrdinaryView { OrdinaryView(); };
View resolve();
View const& borrowed();
void consume(View);
struct Items { View positions(); int misleading_view(); int ordinary(); };
"""


class TidyTest(unittest.TestCase):
    clang_tidy: str
    source_dir: Path
    check: str
    dependency_source = VIEWS + "\ninline void dependency() { for (;;) { View v; resolve(); } }\n"

    def run_tidy(self, *arguments: str) -> str:
        result = subprocess.run([self.clang_tidy, *arguments], capture_output=True,
                                text=True, encoding="utf-8", errors="replace")
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, output)
        self.assertNotIn("error:", output)
        return output

    def test_registration(self) -> None:
        self.assertIn(self.check, self.run_tidy(f"-checks=-*,{self.check}", "-list-checks"))

    def assert_cases(self, declarations: str, cases: tuple, checks: str | None = None,
                     *, at_namespace_scope: bool = False) -> None:
        with tempfile.TemporaryDirectory(prefix="ioj tidy semantics ") as directory:
            source = Path(directory) / "input.cpp"
            for name, body, warnings in cases:
                with self.subTest(case=name):
                    source.write_text(declarations + "\n" +
                                      (body if at_namespace_scope else "void test() {\n" + body + "\n}\n"),
                                      encoding="utf-8")
                    output = self.run_tidy(f"-checks=-*,{checks or self.check}", str(source), "--", "-std=c++23")
                    self.assertEqual(output.count(f"[{self.check}]"), warnings, output)

    def test_included_sources_are_not_diagnosed(self) -> None:
        with tempfile.TemporaryDirectory(prefix="ioj tidy headers ") as directory:
            root = Path(directory)
            (root / "dependency.h").write_text(
                self.dependency_source, encoding="utf-8")
            source = root / "input.cpp"
            source.write_text('#include "dependency.h"\n', encoding="utf-8")
            output = self.run_tidy(f"-checks=-*,{self.check}", "-header-filter=.*", str(source), "--", "-std=c++23")
            self.assertNotIn(f"[{self.check}]", output)


def main(test: type[TidyTest]) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--clang-tidy", required=True)
    parser.add_argument("--source-dir", required=True, type=Path)
    arguments = parser.parse_args()
    test.clang_tidy = arguments.clang_tidy
    test.source_dir = arguments.source_dir
    unittest.main(argv=[__file__], defaultTest=test.__name__)
