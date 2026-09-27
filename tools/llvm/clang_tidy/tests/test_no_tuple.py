from tidy_test_support import TidyTest, main


class NoTupleTests(TidyTest):
    check = "ioj-no-tuple"
    dependency_source = "#include <tuple>\ninline std::tuple<int, int> dependency() { return {}; }\n"

    def test_explicit_uses(self) -> None:
        self.assert_cases("#include <utility>\n#include <tuple>\n"
                         "template<template<class...> class T> struct Holder {};\n", (
            ("local", "std::tuple<int, float, bool> value;", 1),
            ("member", "struct S { std::tuple<int, float> value; };", 1),
            ("parameter", "void f(std::tuple<int, float>);", 1),
            ("return", "auto f() -> std::tuple<int, float>;", 1),
            ("alias", "using T = std::tuple<int, float>; T value;", 1),
            ("typedef", "typedef std::tuple<int, float> T;", 1),
            ("argument", "std::pair<std::tuple<int, float>, int> value;", 1),
            ("template_argument", "Holder<std::tuple> value;", 1),
            ("nested", "std::tuple<int, std::tuple<int, int>> value;", 2),
            ("temporary", "auto value = std::tuple<int, float>{1, 2.f};", 1),
            ("ctad", "auto value = std::tuple{1, 2.f};", 1),
            ("ctad_variable", "std::tuple value{1, 2.f};", 1),
            ("qualified", "std::tuple<int, float> const* value{};", 1),
            ("using_name", "using std::tuple; tuple<int, float> value;", 1),
            ("inferred", "auto value = std::make_tuple(1, 2.f);", 0),
            ("structured_binding", "auto [first, second] = std::make_tuple(1, 2.f);", 0),
            ("pair_binding", "auto [first, second] = std::make_pair(1, 2.f);", 0),
            ("user_type", "struct tuple { int first; float second; }; tuple value{};", 0),
            ("aggregate_binding", "struct S { int x; int y; }; auto [x,y] = S{};", 0),
            ("suppressed", "// NOLINTNEXTLINE(ioj-no-tuple)\nstd::tuple<int, float> value;", 0),
            ("nolint", "std::tuple<int, float> value; // NOLINT(ioj-no-tuple)", 0),
        ))


if __name__ == "__main__":
    main(NoTupleTests)
