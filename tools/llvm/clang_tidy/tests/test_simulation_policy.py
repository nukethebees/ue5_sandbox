from pathlib import Path
import shutil
import tempfile

from tidy_test_support import TidyTest, VIEWS, main


class SimulationPolicyTests(TidyTest):
    check = "ioj-loop-condition-call"

    def test_nested_policy(self) -> None:
        checks = ("ioj-loop-condition-call", "ioj-loop-view-construction",
                  "ioj-loop-view-accessor-call", "ioj-no-pair", "ioj-no-tuple")
        with tempfile.TemporaryDirectory(prefix="ioj nested policy ") as directory:
            native = Path(directory) / "native"
            simulation = native / "simulation"
            benchmark = native / "simulation_benchmark"
            simulation.mkdir(parents=True)
            benchmark.mkdir()
            shutil.copyfile(self.source_dir / "native/.clang-tidy", native / ".clang-tidy")
            shutil.copyfile(self.source_dir / "native/simulation/.clang-tidy", simulation / ".clang-tidy")
            code = VIEWS + """
namespace std {
template<class A, class B> struct pair {};
template<class... T> struct tuple {};
}
int count();
void test() {
    std::pair<int, float> pair;
    std::tuple<int, float> tuple;
    for (int i = 0; i < count(); ++i) {
        View constructed;
        auto returned{resolve()};
    }
}
"""
            for scope, expected in ((simulation, 1), (benchmark, 0)):
                with self.subTest(scope=scope.name):
                    source = scope / "input.cpp"
                    source.write_text(code, encoding="utf-8")
                    output = self.run_tidy(str(source), "--", "--driver-mode=cl", "/std:c++latest")
                    effective = self.run_tidy(str(source), "-list-checks")
                    for check in checks:
                        self.assertEqual(output.count(f"[{check}]"), expected, output)
                        self.assertEqual(check in effective, bool(expected), effective)
                    self.assertIn("clang-analyzer-core.CallAndMessage", effective)
                    self.assertIn("performance-inefficient-algorithm", effective)


if __name__ == "__main__":
    main(SimulationPolicyTests)
