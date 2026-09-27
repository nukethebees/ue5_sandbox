from pathlib import Path
import tempfile

from tidy_test_support import TidyTest, main


DECLARATIONS = """
#include <utility>
#include <tuple>
#include <vector>
#include <optional>
template<class... Ts> struct Holder {};
template<class T> void consume(T const&);
namespace user {
template<class... Ts> struct tuple { int x; int y; };
struct Point {};
template<std::size_t I> int get(Point);
}
namespace std {
template<> struct tuple_size<user::Point> : integral_constant<size_t, 2> {};
template<size_t I> struct tuple_element<I, user::Point> { using type = int; };
}
"""


class NoTupleTests(TidyTest):
    check = "ioj-no-tuple"
    dependency_source = """#include <utility>
#include <tuple>
using Borrowed = std::tuple<int, float>;
extern Borrowed shared_value;
inline Borrowed dependency() { return {}; }
inline void implementation() { auto value = std::make_tuple(1, 2.f); }
"""

    def test_semantic_uses(self) -> None:
        self.assert_cases(DECLARATIONS, (
            ("local", "std::tuple<int, float> value;", 1),
            ("member", "struct S { std::tuple<int, float> value; };", 1),
            ("parameter", "void f(std::tuple<int, float>);", 1),
            ("return", "auto f() -> std::tuple<int, float>;", 1),
            ("alias", "using P = std::tuple<int, float>;", 1),
            ("alias_use", "using P = std::tuple<int, float>; P value;", 2),
            ("nested_aliases", "using P = std::tuple<int, float>; using Q = std::vector<P>; Q value;", 3),
            ("typedef", "typedef std::tuple<int, float> P; P value;", 2),
            ("vector", "std::vector<std::tuple<int, float>> values;", 1),
            ("optional", "std::optional<std::tuple<int, float>> value;", 1),
            ("nested_pack", "Holder<int, Holder<std::tuple<int, float>>> value;", 1),
            ("nested", "std::tuple<int, std::tuple<int, int>> value;", 1),
            ("temporary", "auto value = std::tuple<int, float>{1, 2.f};", 1),
            ("ctad", "auto value = std::tuple{1, 2.f};", 1),
            ("ctad_variable", "std::tuple value{1, 2.f};", 1),
            ("qualified", "std::tuple<int, float> const* value{};", 1),
            ("array", "std::tuple<int, float> values[2];", 1),
            ("using_name", "using std::tuple; tuple<int, float> value;", 1),
            ("factory", "auto value = std::make_tuple(1, 2.f);", 1),
            ("braced_factory", "auto const value{std::make_tuple(1, 2.f)};", 1),
            ("reference_factory", "auto const& value = std::make_tuple(1, 2.f);", 1),
            ("standalone", "consume(std::make_tuple(1, 2.f));", 1),
            ("discarded_factory", "std::make_tuple(1, 2.f);", 0),
            ("discarded_construction", "std::tuple{1, 2.f};", 1),
            ("explicit_expression", "consume(std::tuple<int, float>{1, 2.f});", 1),
            ("scalar_initializer", "auto value = std::get<0>(std::make_tuple(1, 2.f));", 1),
            ("structured_binding", "auto [first, second] = std::make_tuple(1, 2.f);", 1),
            ("separate_uses", "auto a = std::make_tuple(1, 2.f); auto b = std::make_tuple(3, 4.f);", 2),
            ("user_record", "struct tuple { int x; float y; }; tuple value{};", 0),
            ("user_template", "user::tuple<int, int> value{}; auto const& ref = value; consume(ref);", 0),
            ("user_call", "user::tuple<int, int> arbitrary(); auto value = arbitrary();", 0),
            ("aggregate_binding", "struct Point { int x; int y; }; auto [x,y] = Point{};", 0),
            ("user_binding", "auto [x,y] = user::tuple<int, int>{};", 0),
            ("tuple_protocol", "auto [x,y] = user::Point{};", 0),
            ("suppressed", "// NOLINTNEXTLINE(ioj-no-tuple)\nauto value = std::make_tuple(1, 2.f);", 0),
            ("nolint", "auto value = std::make_tuple(1, 2.f); // NOLINT(ioj-no-tuple)", 0),
            ("suppressed_expression", "// NOLINTNEXTLINE(ioj-no-tuple)\nconsume(std::make_tuple(1, 2.f));", 0),
            ("suppressed_multiline", "// NOLINTNEXTLINE(ioj-no-tuple)\nauto value =\n std::make_tuple(1, 2.f);", 0),
        ))

    def test_declaration_ownership(self) -> None:
        self.assert_cases(DECLARATIONS, (
            ("global", "auto value = std::make_tuple(1, 2.f);", 1),
            ("static_member", "struct S { inline static auto value = std::make_tuple(1, 2.f); };", 1),
            ("field_initializer", "struct S { std::tuple<int, float> value = std::make_tuple(1, 2.f); };", 1),
            ("default_parameter", "void f(std::tuple<int, float> value = std::make_tuple(1, 2.f));", 1),
            ("deduced_return", "auto f() { return std::make_tuple(1, 2.f); }", 1),
            ("explicit_return", "std::tuple<int, float> f() { return {1, 2.f}; }", 1),
            ("return_and_local", "auto f() { auto value = std::make_tuple(1, 2.f); return value; }", 2),
            ("body_not_owned_by_return", "std::tuple<int, float> f() { consume(std::make_tuple(1, 2.f)); return {}; }", 2),
            ("lambda_return", "auto f = [] { return std::make_tuple(1, 2.f); };", 1),
            ("lambda_boundary", "auto value = [] { consume(std::make_tuple(1, 2.f)); return std::make_tuple(3, 4.f); }();", 3),
            ("dependent", "template<class T> void f(Holder<std::tuple<T, int>> value);", 1),
            ("dependent_expression", "template<class T> void f(T x) { consume(std::tuple<T, int>{x, 1}); }", 1),
            ("function_pointer", "void (*callback)(std::tuple<int, float>);", 1),
            ("function_pointer_alias", "using Callback = void (*)(std::tuple<int, float>);", 1),
            ("function_pointer_field", "struct S { void (*callback)(std::tuple<int, float>); };", 1),
            ("function_pointer_parameter", "void f(void (*callback)(std::tuple<int, float>));", 1),
            ("function_pointer_return", "auto f() -> void (*)(std::tuple<int, float>);", 1),
            ("independent_parameters", "void f(std::tuple<int, float> a, std::tuple<int, float> b);", 2),
            ("return_and_parameter", "std::tuple<int, float> f(std::tuple<int, float> value);", 2),
            ("member_pointer", "struct S; std::tuple<int, float> S::* field;", 1),
        ), at_namespace_scope=True)

    def test_dependency_type_uses(self) -> None:
        with tempfile.TemporaryDirectory(prefix="ioj semantic dependency ") as directory:
            root = Path(directory)
            (root / "dependency.h").write_text(self.dependency_source, encoding="utf-8")
            for body, warnings in (("Borrowed value;", 1), ("auto value = dependency();", 1),
                                   ("consume(dependency());", 1), ("auto [x,y] = dependency();", 1),
                                   ("consume(shared_value);", 1), ("dependency();", 0),
                                   ("(dependency());", 0), ("(void)dependency();", 0),
                                   ("shared_value;", 0)):
                with self.subTest(body=body):
                    source = root / "input.cpp"
                    source.write_text('#include "dependency.h"\n' +
                                      "template<class T> void consume(T const&);\nvoid f() { " +
                                      body + " }\n", encoding="utf-8")
                    output = self.run_tidy(f"-checks=-*,{self.check}", "-header-filter=.*",
                                           str(source), "--", "-std=c++23")
                    self.assertEqual(output.count(f"[{self.check}]"), warnings, output)

    def test_inline_namespace_identity(self) -> None:
        self.assert_cases("namespace std { inline namespace abi { "
                          "template<class... T> struct tuple {}; } }", (
            ("inline_std", "std::tuple<int, float> value;", 1),
        ))
        self.assert_cases("namespace user::std { template<class... T> struct tuple {}; }", (
            ("nested_std", "user::std::tuple<int, float> value;", 0),
        ))



    def test_tuple_factories(self) -> None:
        self.assert_cases(DECLARATIONS, (
            ("tie", "int x{}; float y{}; auto value = std::tie(x, y);", 1),
            ("forward", "auto value = std::forward_as_tuple(1, 2.f);", 1),
            ("cat", "auto value = std::tuple_cat(std::make_tuple(1), std::make_tuple(2.f));", 1),
            ("cat_expression", "consume(std::tuple_cat(std::make_tuple(1), std::make_tuple(2.f)));", 1),
            ("discarded_tie", "int x{}; float y{}; std::tie(x, y);", 0),
            ("discarded_forward", "std::forward_as_tuple(1, 2.f);", 0),
            ("discarded_cat", "std::tuple_cat();", 0),
            ("cat_consumes_arguments", "std::tuple_cat(std::make_tuple(1), std::make_tuple(2.f));", 2),
            ("tie_argument", "int x{}; consume(std::tie(x));", 1),
            ("forward_argument", "consume(std::forward_as_tuple(1, 2.f));", 1),
            ("pair_binding", "auto [x,y] = std::make_pair(1, 2.f);", 0),
        ))


if __name__ == "__main__":
    main(NoTupleTests)
