from pathlib import Path
import tempfile

from tidy_test_support import TidyTest, main


DECLARATIONS = """
#include <utility>
#include <tuple>
#include <vector>
#include <optional>
#include <map>
#include <set>
#include <unordered_map>
template<class... Ts> struct Holder {};
template<class T> void consume(T const&);
namespace user {
template<class... Ts> struct pair { int x; int y; };
struct Point {};
template<std::size_t I> int get(Point);
}
namespace std {
template<> struct tuple_size<user::Point> : integral_constant<size_t, 2> {};
template<size_t I> struct tuple_element<I, user::Point> { using type = int; };
}
"""


class NoPairTests(TidyTest):
    check = "ioj-no-pair"
    dependency_source = """#include <utility>
#include <tuple>
using Borrowed = std::pair<int, float>;
extern Borrowed shared_value;
inline Borrowed dependency() { return {}; }
inline void implementation() { auto value = std::make_pair(1, 2.f); }
"""

    def test_semantic_uses(self) -> None:
        self.assert_cases(DECLARATIONS, (
            ("local", "std::pair<int, float> value;", 1),
            ("member", "struct S { std::pair<int, float> value; };", 1),
            ("parameter", "void f(std::pair<int, float>);", 1),
            ("return", "auto f() -> std::pair<int, float>;", 1),
            ("alias", "using P = std::pair<int, float>;", 1),
            ("alias_use", "using P = std::pair<int, float>; P value;", 2),
            ("nested_aliases", "using P = std::pair<int, float>; using Q = std::vector<P>; Q value;", 3),
            ("typedef", "typedef std::pair<int, float> P; P value;", 2),
            ("vector", "std::vector<std::pair<int, float>> values;", 1),
            ("map_defaults", "std::map<int, float> values; values.clear();", 0),
            ("map_default_identity", "std::map<int, float, std::less<int>, std::allocator<std::pair<int const, float>>> explicit_defaults; std::map<int, float> implicit_defaults;", 0),
            ("unordered_map_defaults", "std::unordered_map<int, float> values; values.reserve(10);", 0),
            ("map_iterator", "std::map<int, float> values; auto it = values.find(1);", 0),
            ("map_pair_value", "std::map<int, std::pair<int, float>> values;", 1),
            ("map_insert_result", "std::map<int, float> values; auto result = values.emplace(1, 2.f);", 1),
            ("optional", "std::optional<std::pair<int, float>> value;", 1),
            ("nested_pack", "Holder<int, Holder<std::pair<int, float>>> value;", 1),
            ("nested", "std::pair<int, std::pair<int, int>> value;", 1),
            ("temporary", "auto value = std::pair<int, float>{1, 2.f};", 1),
            ("ctad", "auto value = std::pair{1, 2.f};", 1),
            ("ctad_variable", "std::pair value{1, 2.f};", 1),
            ("qualified", "std::pair<int, float> const* value{};", 1),
            ("array", "std::pair<int, float> values[2];", 1),
            ("using_name", "using std::pair; pair<int, float> value;", 1),
            ("factory", "auto value = std::make_pair(1, 2.f);", 1),
            ("braced_factory", "auto const value{std::make_pair(1, 2.f)};", 1),
            ("reference_factory", "auto const& value = std::make_pair(1, 2.f);", 1),
            ("standalone", "consume(std::make_pair(1, 2.f));", 1),
            ("discarded_factory", "std::make_pair(1, 2.f);", 0),
            ("discarded_construction", "std::pair{1, 2.f};", 1),
            ("explicit_expression", "consume(std::pair<int, float>{1, 2.f});", 1),
            ("scalar_initializer", "auto value = std::get<0>(std::make_pair(1, 2.f));", 1),
            ("structured_binding", "auto [first, second] = std::make_pair(1, 2.f);", 1),
            ("separate_uses", "auto a = std::make_pair(1, 2.f); auto b = std::make_pair(3, 4.f);", 2),
            ("user_record", "struct pair { int x; float y; }; pair value{};", 0),
            ("user_template", "user::pair<int, int> value{}; auto const& ref = value; consume(ref);", 0),
            ("user_call", "user::pair<int, int> arbitrary(); auto value = arbitrary();", 0),
            ("aggregate_binding", "struct Point { int x; int y; }; auto [x,y] = Point{};", 0),
            ("user_binding", "auto [x,y] = user::pair<int, int>{};", 0),
            ("tuple_protocol", "auto [x,y] = user::Point{};", 0),
            ("suppressed", "// NOLINTNEXTLINE(ioj-no-pair)\nauto value = std::make_pair(1, 2.f);", 0),
            ("nolint", "auto value = std::make_pair(1, 2.f); // NOLINT(ioj-no-pair)", 0),
            ("suppressed_expression", "// NOLINTNEXTLINE(ioj-no-pair)\nconsume(std::make_pair(1, 2.f));", 0),
            ("suppressed_multiline", "// NOLINTNEXTLINE(ioj-no-pair)\nauto value =\n std::make_pair(1, 2.f);", 0),
        ))

    def test_call_consumption(self) -> None:
        self.assert_cases(DECLARATIONS, (
            ("discarded_map", "std::map<int, float> values; values.emplace(1, 2.f);", 0),
            ("discarded_set", "std::set<int> values; values.insert(1);", 0),
            ("discarded_unordered_map", "std::unordered_map<int, float> values; values.emplace(1, 2.f);", 0),
            ("stored", "std::map<int, float> values; auto result = values.emplace(1, 2.f);", 1),
            ("binding", "std::map<int, float> values; auto [it, inserted] = values.emplace(1, 2.f);", 1),
            ("member", "std::set<int> values; if (!values.insert(1).second) {}", 1),
            ("argument", "std::set<int> values; consume(values.insert(1));", 1),
            ("parentheses", "std::set<int> values; (values.insert(1));", 0),
            ("void_cast", "std::set<int> values; static_cast<void>(values.insert(1));", 0),
            ("loop_body", "std::set<int> values; for (int i{}; i < 2; ++i) values.insert(i);", 0),
            ("loop_increment", "std::set<int> values; for (int i{}; i < 2; values.insert(i++)) {}", 0),
            ("comma_discarded", "std::set<int> values; (values.insert(1), values.insert(2));", 0),
            ("comma_consumed", "std::set<int> values; consume((values.insert(1), values.insert(2)));", 1),
            ("conditional_discarded", "std::set<int> values; true ? values.insert(1) : values.insert(2);", 0),
            ("conditional_consumed", "std::set<int> values; auto result = true ? values.insert(1) : values.insert(2);", 1),
        ))

    def test_declaration_ownership(self) -> None:
        self.assert_cases(DECLARATIONS, (
            ("global", "auto value = std::make_pair(1, 2.f);", 1),
            ("static_member", "struct S { inline static auto value = std::make_pair(1, 2.f); };", 1),
            ("field_initializer", "struct S { std::pair<int, float> value = std::make_pair(1, 2.f); };", 1),
            ("default_parameter", "void f(std::pair<int, float> value = std::make_pair(1, 2.f));", 1),
            ("deduced_return", "auto f() { return std::make_pair(1, 2.f); }", 1),
            ("explicit_return", "std::pair<int, float> f() { return {1, 2.f}; }", 1),
            ("return_and_local", "auto f() { auto value = std::make_pair(1, 2.f); return value; }", 2),
            ("body_not_owned_by_return", "std::pair<int, float> f() { consume(std::make_pair(1, 2.f)); return {}; }", 2),
            ("lambda_return", "auto f = [] { return std::make_pair(1, 2.f); };", 1),
            ("lambda_boundary", "auto value = [] { consume(std::make_pair(1, 2.f)); return std::make_pair(3, 4.f); }();", 3),
            ("dependent", "template<class T> void f(Holder<std::pair<T, int>> value);", 1),
            ("dependent_expression", "template<class T> void f(T x) { consume(std::pair<T, int>{x, 1}); }", 1),
            ("function_pointer", "void (*callback)(std::pair<int, float>);", 1),
            ("function_pointer_alias", "using Callback = void (*)(std::pair<int, float>);", 1),
            ("function_pointer_field", "struct S { void (*callback)(std::pair<int, float>); };", 1),
            ("function_pointer_parameter", "void f(void (*callback)(std::pair<int, float>));", 1),
            ("function_pointer_return", "auto f() -> void (*)(std::pair<int, float>);", 1),
            ("independent_parameters", "void f(std::pair<int, float> a, std::pair<int, float> b);", 2),
            ("return_and_parameter", "std::pair<int, float> f(std::pair<int, float> value);", 2),
            ("member_pointer", "struct S; std::pair<int, float> S::* field;", 1),
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
                          "template<class... T> struct pair {}; } }", (
            ("inline_std", "std::pair<int, float> value;", 1),
        ))
        self.assert_cases("namespace user::std { template<class... T> struct pair {}; }", (
            ("nested_std", "user::std::pair<int, float> value;", 0),
        ))


if __name__ == "__main__":
    main(NoPairTests)
