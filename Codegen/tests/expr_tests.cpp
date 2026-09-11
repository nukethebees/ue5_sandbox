#include <codegen/ast.h>

#include <gtest/gtest.h>

#include <type_traits>

namespace codegen {
namespace {

TEST(Expr, RequiresExplicitRawAndSupportsIndependentCopies) {
    static_assert(!std::is_convertible_v<std::string, Expr>);
    static_assert(!std::is_convertible_v<char const*, Expr>);
    auto original{call(named("lookup"), {named("value")})};
    auto copy{original};
    original = literal("false");

    EXPECT_EQ(render(copy), "lookup(value)");
    EXPECT_EQ(render(original), "false");
    EXPECT_TRUE(std::holds_alternative<CallExpr>(copy.value()));
}

TEST(Expr, RendersCallsCastsAndInitializerLists) {
    auto const lookup{call(named("get_name"), {named("project::Mode::Ready")})};
    EXPECT_EQ(render(init_list({lookup}, CppType{"FString"})),
              "FString{get_name(project::Mode::Ready)}");
    EXPECT_EQ(render(init_list({literal("1"), literal("2")})), "{1, 2}");
    EXPECT_EQ(render(init_list({}, CppType{"FString"})), "FString{}");
    EXPECT_EQ(render(init_list({})), "{}");
    EXPECT_EQ(render(static_cast_expr("int32", named("Mode::COUNT"))),
              "static_cast<int32>(Mode::COUNT)");
    EXPECT_EQ(render(call(call(named("factory")), {literal("42")})), "factory()(42)");
}

TEST(Expr, PreservesBinaryGroupingAndPostfixBinding) {
    auto const inner{binary(BinaryOperator::equal, named("a"), named("b"))};
    EXPECT_EQ(render(binary(BinaryOperator::equal, inner, named("c"))), "a == b == c");
    EXPECT_EQ(render(binary(BinaryOperator::equal, named("c"), inner)), "c == (a == b)");
    EXPECT_EQ(render(call(inner)), "(a == b)()");
    EXPECT_EQ(render(call(named("f"), {inner})), "f(a == b)");
    EXPECT_EQ(render(static_cast_expr("bool", inner)), "static_cast<bool>(a == b)");
}

TEST(Expr, KeepsRawVerbatimAndProtectsNestedRawOperands) {
    Expr const raw_expression{RawExpr{"left, right"}};
    EXPECT_EQ(render(raw_expression), "left, right");
    EXPECT_EQ(render(Node{ReturnStatement{raw_expression}}), "return left, right;");
    EXPECT_EQ(render(call(named("f"), {raw_expression})), "f((left, right))");
    EXPECT_EQ(render(call(RawExpr{"*function_pointer"})), "(*function_pointer)()");
    EXPECT_EQ(render(binary(BinaryOperator::equal, raw_expression, literal("0"))),
              "(left, right) == 0");
    EXPECT_EQ(render(binary(BinaryOperator::equal, literal("0"), raw_expression)),
              "0 == (left, right)");
    EXPECT_EQ(render(static_cast_expr("int", raw_expression)), "static_cast<int>((left, right))");
    EXPECT_EQ(render(init_list({raw_expression})), "{(left, right)}");
}

TEST(Expr, EscapesStringContentsWithoutConsumingFollowingDigits) {
    EXPECT_EQ(render(string_literal("")), "\"\"");
    EXPECT_EQ(render(string_literal("a\"b\\c\n\r\t")), "\"a\\\"b\\\\c\\n\\r\\t\"");
    EXPECT_EQ(render(string_literal(std::string_view{"\0"
                                                     "7\001"
                                                     "f\177",
                                                     5})),
              "\"\\0007\\001f\\177\"");
    EXPECT_EQ(render(string_literal("caf\xc3\xa9")), "\"caf\xc3\xa9\"");
}

TEST(Expr, CollectsDependenciesFromEveryOperandAndType) {
    TypeDependency const callee{"make", "Project/Make.h", {}};
    TypeDependency const left{"left", "Project/Left.h", {}};
    TypeDependency const right{"right", "Project/Right.h", {}};
    TypeDependency const cast_type{"Count", "Project/Count.h", {}};
    TypeDependency const list_type{"Value", "Project/Value.h", {}};
    TypeDependency const raw_dependency{"macro", "Project/Macro.h", {}};
    auto const expression{init_list(
        {call(
            named("make", {callee}),
            {binary(BinaryOperator::equal, named("left", {left}), named("right", {right})),
             static_cast_expr(CppType{"Count", {cast_type}}, RawExpr{"MACRO", {raw_dependency}})})},
        CppType{"Value", {list_type}})};

    EXPECT_EQ(
        dependencies(expression),
        (std::vector<TypeDependency>{list_type, callee, left, right, cast_type, raw_dependency}));
}

TEST(Expr, StatementsCollectInitializersAndAssignmentOperands) {
    TypeDependency const make{"make", "Project/Make.h", {}};
    TypeDependency const target{"target", "Project/Target.h", {}};
    auto const expression{call(named("make", {make}))};
    EXPECT_EQ(dependencies(Node{ExpressionStatement{expression}}),
              std::vector<TypeDependency>{make});
    EXPECT_EQ(dependencies(Node{ReturnStatement{expression}}), std::vector<TypeDependency>{make});
    EXPECT_EQ(dependencies(Node{Member{"auto", "value", expression}}),
              std::vector<TypeDependency>{make});
    EXPECT_EQ(dependencies(Node{VariableDeclarationStatement{"auto", "value", expression}}),
              std::vector<TypeDependency>{make});
    EXPECT_EQ(dependencies(Node{AssignmentStatement{named("target", {target}), expression}}),
              (std::vector<TypeDependency>{target, make}));
}

TEST(Expr, PreservesAbsentEmptyAndStructuredInitializers) {
    EXPECT_EQ(render(Node{Member{"int", "value"}}), "int value;");
    EXPECT_EQ(render(Node{Member{"int", "value", RawExpr{""}}}), "int value{};");
    EXPECT_EQ(render(Node{Member{"int", "value", literal("42")}}), "int value{42};");
    EXPECT_EQ(render(Node{VariableDeclarationStatement{"int", "value", RawExpr{""}}}),
              "int value{};");
    EXPECT_EQ(render(Node{VariableDeclarationStatement{"auto const", "value", call(named("f"))}}),
              "auto const value{f()};");
    EXPECT_EQ(render(Node{ReturnStatement{}}), "return;");
}

TEST(Ast, RendersNestedControlFlowWithExplicitBreaksAndSpacing) {
    NodeListBuilder body;
    body.add(IfStatement{named("ready"),
                         Block{{ReturnStatement{literal("true")}}},
                         Block{{ExpressionStatement{call(named("retry"))}}}},
             2)
        .add(BreakStatement{});
    Node const statement{
        SwitchStatement{named("mode"),
                        {{named("Mode::Ready"), Block{body.build()}},
                         {std::nullopt, Block{{ReturnStatement{literal("false")}}}}}}};

    EXPECT_EQ(render(statement, {.indent_level = 1}),
              "    switch (mode) {\n"
              "    case Mode::Ready: {\n"
              "        if (ready) {\n"
              "            return true;\n"
              "        } else {\n"
              "            retry();\n"
              "        }\n"
              "\n"
              "        break;\n"
              "    }\n"
              "    default: {\n"
              "        return false;\n"
              "    }\n"
              "    }");
}

TEST(Ast, RendersEmptyControlFlowAndStandaloneBlocks) {
    EXPECT_EQ(render(Node{Block{}}), "{\n\n}");
    EXPECT_EQ(render(Node{Block{{ReturnStatement{}}}}), "{\n    return;\n}");
    EXPECT_EQ(render(Node{IfStatement{named("ready"), Block{}}}), "if (ready) {\n\n}");
    EXPECT_EQ(render(Node{SwitchStatement{named("mode"), {}}}), "switch (mode) {\n}");
    EXPECT_EQ(render(Node{SwitchStatement{named("mode"), {{std::nullopt, Block{}}}}}),
              "switch (mode) {\ndefault: {\n\n}\n}");
}

TEST(Ast, VisitsBothBranchesAndAllSwitchBodiesInOrder) {
    Node const branches{IfStatement{named("ready"), Block{{raw("then")}}, Block{{raw("else")}}}};
    Node const cases{SwitchStatement{named("mode"),
                                     {{literal("1"), Block{{raw("one")}}},
                                      {literal("2"), Block{{raw("two")}}},
                                      {std::nullopt, Block{{raw("default")}}}}}};
    std::vector<std::string> visited;
    auto visit = [&](Node const& node) { visited.push_back(node.get_if<Raw>()->text); };
    for_each_child(branches, visit);
    for_each_child(cases, visit);
    EXPECT_EQ(visited, (std::vector<std::string>{"then", "else", "one", "two", "default"}));
}

TEST(Ast, CollectsDeepControlFlowDependenciesOnlyForDefinitions) {
    auto symbol = [](std::string name) {
        auto const header{"Project/" + name + ".h"};
        return named(name, {TypeDependency{name, header, {}}});
    };
    auto const body_call{call(symbol("body_only"), {symbol("argument_only")})};
    SwitchStatement const selection{
        symbol("switch_only"),
        {{symbol("case_only"), Block{{ExpressionStatement{body_call}}}},
         {std::nullopt, Block{{Block{{ExpressionStatement{call(symbol("default_only"))}}}}}}},
    };
    FunctionSpec const spec{
        .name = "f",
        .return_type = "void",
        .body = {IfStatement{call(symbol("condition_only")),
                             Block{{selection}},
                             Block{{ReturnStatement{call(symbol("else_only"))}}}}},
    };
    CppFile file{.path = "Example.h", .nodes = {IncludeDependencies{}, declaration(spec)}};
    EXPECT_EQ(render(file).find("Project/"), std::string::npos);

    file.nodes.back() = Function{spec};
    auto const output{render(file)};
    for (auto const* name : {"condition_only",
                             "switch_only",
                             "case_only",
                             "body_only",
                             "argument_only",
                             "default_only",
                             "else_only"}) {
        auto const include{"#include \"Project/" + std::string{name} + ".h\""};
        EXPECT_NE(output.find(include), std::string::npos) << name;
        EXPECT_EQ(output.find(include), output.rfind(include)) << name;
    }
}

TEST(Ast, DeduplicatesSharedSymbolDependenciesAndTheirNestedHeaders) {
    TypeDependency const symbol{"make", "Project/Make.h", {{"Value", "Project/Value.h", {}}}};
    auto const expression{call(named("make", {symbol}))};
    CppFile const file{
        .path = "Example.h",
        .nodes = {IncludeDependencies{},
                  Member{"auto", "value", expression},
                  Function{FunctionSpec{
                      .name = "f", .return_type = "auto", .body = {ReturnStatement{expression}}}}},
    };
    auto const output{render(file)};
    for (auto const* header : {"Project/Make.h", "Project/Value.h"}) {
        EXPECT_NE(output.find(header), std::string::npos);
        EXPECT_EQ(output.find(header), output.rfind(header));
    }
}

} // namespace
} // namespace codegen
