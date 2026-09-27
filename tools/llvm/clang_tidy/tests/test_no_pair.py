from tidy_test_support import TidyTest, main


class NoPairTests(TidyTest):
    check = "ioj-no-pair"
    dependency_source = "#include <utility>\ninline std::pair<int, int> dependency() { return {}; }\n"

    def test_explicit_uses(self) -> None:
        self.assert_cases("#include <utility>\n#include <tuple>\n", (
            ("local", "std::pair<int, float> value;", 1),
            ("member", "struct S { std::pair<int, float> value; };", 1),
            ("parameter", "void f(std::pair<int, float>);", 1),
            ("return", "auto f() -> std::pair<int, float>;", 1),
            ("alias", "using P = std::pair<int, float>; P value;", 1),
            ("typedef", "typedef std::pair<int, float> P;", 1),
            ("argument", "std::tuple<std::pair<int, float>> value;", 1),
            ("temporary", "auto value = std::pair<int, float>{1, 2.f};", 1),
            ("ctad", "auto value = std::pair{1, 2.f};", 1),
            ("ctad_variable", "std::pair value{1, 2.f};", 1),
            ("qualified", "std::pair<int, float> const* value{};", 1),
            ("using_name", "using std::pair; pair<int, float> value;", 1),
            ("inferred", "auto value = std::make_pair(1, 2.f);", 0),
            ("structured_binding", "auto [first, second] = std::make_pair(1, 2.f);", 0),
            ("user_type", "struct pair { int first; float second; }; pair value{};", 0),
            ("aggregate_binding", "struct S { int x; int y; }; auto [x,y] = S{};", 0),
            ("suppressed", "// NOLINTNEXTLINE(ioj-no-pair)\nstd::pair<int, float> value;", 0),
            ("nolint", "std::pair<int, float> value; // NOLINT(ioj-no-pair)", 0),
        ))


if __name__ == "__main__":
    main(NoPairTests)
