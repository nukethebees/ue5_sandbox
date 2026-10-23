#include "compiler_test_context.h"
#include "test_utilities.h"

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_renderer.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/expr_kind.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/cpp_string.h>
#include <codegen/schema/declarations.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace lispb::codegen {
namespace {

TEST(Expr, RequiresExplicitRawAndSupportsIndependentCopies) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    static_assert(!std::is_convertible_v<std::string, Expr>);
    static_assert(!std::is_convertible_v<char const*, Expr>);
    auto original{ir.call(ir.named("lookup"), {ir.named("value")})};
    auto copy{original};
    original = ir.literal("false");

    EXPECT_EQ(CppRenderer{ir}.render(copy), "lookup(value)");
    EXPECT_EQ(CppRenderer{ir}.render(original), "false");
    EXPECT_EQ(copy.kind(), ExprKind::call);
}

TEST(Expr, RendersCallsCastsAndInitializerLists) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const lookup{ir.call(ir.named("get_name"), {ir.named("project::Mode::Ready")})};
    EXPECT_EQ(CppRenderer{ir}.render(ir.init_list({lookup}, types.intern("FString"))),
              "FString{get_name(project::Mode::Ready)}");
    EXPECT_EQ(CppRenderer{ir}.render(ir.init_list({ir.literal("1"), ir.literal("2")})), "{1, 2}");
    EXPECT_EQ(CppRenderer{ir}.render(ir.init_list({}, types.intern("FString"))), "FString{}");
    EXPECT_EQ(CppRenderer{ir}.render(ir.init_list({})), "{}");
    EXPECT_EQ(
        CppRenderer{ir}.render(ir.static_cast_expr(types.intern("int32"), ir.named("Mode::COUNT"))),
        "static_cast<int32>(Mode::COUNT)");
    EXPECT_EQ(CppRenderer{ir}.render(ir.call(ir.call(ir.named("factory")), {ir.literal("42")})),
              "factory()(42)");
}

TEST(Expr, PreservesBinaryGroupingAndPostfixBinding) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const inner{ir.binary(BinaryOperator::equal, ir.named("a"), ir.named("b"))};
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::equal, inner, ir.named("c"))),
              "a == b == c");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::equal, ir.named("c"), inner)),
              "c == (a == b)");
    EXPECT_EQ(CppRenderer{ir}.render(ir.call(inner)), "(a == b)()");
    EXPECT_EQ(CppRenderer{ir}.render(ir.call(ir.named("f"), {inner})), "f(a == b)");
    EXPECT_EQ(CppRenderer{ir}.render(ir.static_cast_expr(types.intern("bool"), inner)),
              "static_cast<bool>(a == b)");
}

TEST(Expr, RendersUnaryPointerAccessAndLogicalPrecedence) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const pointer{ir.named("state")};
    auto const data{ir.pointer_member_access(pointer, "data")};
    auto const missing{ir.binary(BinaryOperator::logical_or,
                                 ir.unary(UnaryOperator::logical_not, pointer),
                                 ir.unary(UnaryOperator::logical_not, data))};
    EXPECT_EQ(CppRenderer{ir}.render(missing), "!state || !state->data");
    EXPECT_EQ(CppRenderer{ir}.render(ir.unary(UnaryOperator::logical_not, missing)),
              "!(!state || !state->data)");
    EXPECT_EQ(CppRenderer{ir}.render(ir.call(ir.named("test"), {missing})),
              "test(!state || !state->data)");
    EXPECT_EQ(
        CppRenderer{ir}.render(ir.binary(BinaryOperator::equal, missing, ir.literal("false"))),
        "(!state || !state->data) == false");
    EXPECT_EQ(CppRenderer{ir}.render(
                  ir.member_access(ir.unary(UnaryOperator::dereference, pointer), "data")),
              "(*state).data");
    EXPECT_EQ(CppRenderer{ir}.render(ir.unary(UnaryOperator::address_of,
                                              ir.unary(UnaryOperator::address_of, pointer))),
              "&(&state)");
    EXPECT_EQ(
        CppRenderer{ir}.render(ir.binary(BinaryOperator::not_equal,
                                         ir.named("this"),
                                         ir.unary(UnaryOperator::address_of, ir.named("other")))),
        "this != &other");
    EXPECT_EQ(CppRenderer{ir}.render(
                  ir.binary(BinaryOperator::divide,
                            ir.named("a"),
                            ir.binary(BinaryOperator::multiply, ir.named("b"), ir.named("c")))),
              "a / (b * c)");
    auto const expression{
        ir.unary(UnaryOperator::logical_not,
                 ir.pointer_member_access(ir.named("state", {types.dependency("state", "State.h")}),
                                          "data"))};
    auto const deps{collected_dependencies(ir, expression)};
    ASSERT_EQ(deps.size(), 1);
    EXPECT_EQ(types.dependency_header(deps.front()), "State.h");
}

TEST(Ast, RendersRefQualifiedMethodsAndInlineOutOfLineConstructors) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto spec{[&] {
        auto const created_function{ir.function(ir.strings.intern("view"), types.intern("auto"))};
        ir.set_function_return_type(created_function, types.intern("View"), ReturnSyntax::trailing);
        ir.set_function_receiver(created_function, FunctionReceiver::const_lvalue);
        ir.set_function_noexcept(created_function);
        return created_function;
    }()};
    EXPECT_EQ(CppRenderer{ir}.render(ir.declaration(spec)),
              "auto view() const & noexcept -> View;");
    ir.set_function_receiver(spec, FunctionReceiver::const_rvalue);
    ir.set_function_deleted(spec);
    EXPECT_EQ(CppRenderer{ir}.render(ir.definition(spec)),
              "auto view() const && noexcept -> View = delete;");
    auto const constructor{ir.definition(
        [&] {
            auto const created_function{ir.constructor(ir.strings.intern("View"))};
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            ir.function_formatting(created_function) = {.body_layout = FunctionBodyLayout::compact};
            return created_function;
        }(),
        ir.store_text("View"),
        {},
        true)};
    EXPECT_EQ(CppRenderer{ir}.render(constructor), "inline View::View() {  }");
}

TEST(Expr, RendersSizeofAndPreservesArithmeticGrouping) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const a{ir.named("a")};
    auto const b{ir.named("b")};
    auto const c{ir.named("c")};
    auto const sum{ir.binary(BinaryOperator::add, a, b)};
    auto const product{ir.binary(BinaryOperator::multiply, b, c)};
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::multiply, sum, c)), "(a + b) * c");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::add, a, product)), "a + b * c");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::multiply, a, product)),
              "a * (b * c)");
    EXPECT_EQ(CppRenderer{ir}.render(
                  ir.binary(BinaryOperator::add, a, ir.binary(BinaryOperator::subtract, b, c))),
              "a + (b - c)");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::multiply,
                                               ir.named("count"),
                                               ir.sizeof_type(types.intern("Element")))),
              "count * sizeof(Element)");
    EXPECT_EQ(CppRenderer{ir}.render(ir.static_cast_expr(types.intern("int"),
                                                         ir.sizeof_type(types.intern("Element")))),
              "static_cast<int>(sizeof(Element))");
}

TEST(Expr, CollectsSizeofTypeDependenciesThroughArithmetic) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const file{ir.files.append(
        ir.strings.add("Example.h"),
        {ir.include_dependencies(),
         ir.definition(
             [&] {
                 auto const created_function{
                     ir.function(ir.strings.intern("bytes"), types.intern("auto"))};
                 ir.set_function_body(
                     created_function,
                     {ir.return_stmt(ir.binary(
                         BinaryOperator::multiply,
                         ir.literal("2"),
                         ir.sizeof_type(types.intern(
                             "Element", {types.dependency("Element", "Project/Element.h")}))))});
                 return created_function;
             }(),
             {})})};
    auto const output{CppRenderer{ir}.render(file)};
    EXPECT_NE(output.find("#include \"Project/Element.h\""), std::string::npos);
    EXPECT_NE(output.find("return 2 * sizeof(Element);"), std::string::npos);
}

TEST(Expr, KeepsRawVerbatimAndProtectsNestedRawOperands) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    Expr const raw_expr{ir.raw_expr("left, right")};
    EXPECT_EQ(CppRenderer{ir}.render(raw_expr), "left, right");
    EXPECT_EQ(CppRenderer{ir}.render(ir.return_stmt(raw_expr)), "return left, right;");
    EXPECT_EQ(CppRenderer{ir}.render(ir.call(ir.named("f"), {raw_expr})), "f((left, right))");
    EXPECT_EQ(CppRenderer{ir}.render(ir.call(ir.raw_expr("*function_pointer"))),
              "(*function_pointer)()");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::equal, raw_expr, ir.literal("0"))),
              "(left, right) == 0");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::equal, ir.literal("0"), raw_expr)),
              "0 == (left, right)");
    EXPECT_EQ(CppRenderer{ir}.render(ir.static_cast_expr(types.intern("int"), raw_expr)),
              "static_cast<int>((left, right))");
    EXPECT_EQ(CppRenderer{ir}.render(ir.init_list({raw_expr})), "{(left, right)}");
}

TEST(Expr, RendersMemberAccessAndSubscriptsWithPostfixBinding) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const other_values{ir.member_access(ir.named("other"), "values")};
    auto const element{ir.subscript(other_values, ir.named("index"))};
    EXPECT_EQ(CppRenderer{ir}.render(element), "other.values[index]");
    EXPECT_EQ(CppRenderer{ir}.render(ir.call(ir.member_access(element, "size"))),
              "other.values[index].size()");
    EXPECT_EQ(
        CppRenderer{ir}.render(ir.subscript(ir.call(ir.named("get_values")), ir.named("index"))),
        "get_values()[index]");
    EXPECT_EQ(CppRenderer{ir}.render(ir.call(ir.member_access(ir.named("values"), "set"),
                                             {ir.named("index"), ir.literal("42")})),
              "values.set(index, 42)");
    auto const difference{ir.binary(BinaryOperator::subtract, ir.named("end"), ir.named("count"))};
    EXPECT_EQ(CppRenderer{ir}.render(ir.subscript(other_values, difference)),
              "other.values[end - count]");
    EXPECT_EQ(CppRenderer{ir}.render(ir.subscript(difference, ir.named("index"))),
              "(end - count)[index]");
    EXPECT_EQ(CppRenderer{ir}.render(ir.member_access(difference, "value")), "(end - count).value");
    EXPECT_EQ(CppRenderer{ir}.render(ir.member_access(ir.raw_expr("*pointer"), "value")),
              "(*pointer).value");
    EXPECT_EQ(
        CppRenderer{ir}.render(ir.subscript(ir.raw_expr("*pointer"), ir.raw_expr("first, last"))),
        "(*pointer)[(first, last)]");
}

TEST(Expr, PreservesSubtractionAndComparisonPrecedence) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const a{ir.named("a")};
    auto const b{ir.named("b")};
    auto const c{ir.named("c")};
    auto const subtraction{ir.binary(BinaryOperator::subtract, a, b)};
    auto const comparison{ir.binary(BinaryOperator::greater_equal, a, b)};
    auto const equality{ir.binary(BinaryOperator::equal, a, b)};

    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::subtract, subtraction, c)),
              "a - b - c");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::subtract, c, subtraction)),
              "c - (a - b)");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::greater_equal, subtraction, c)),
              "a - b >= c");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::subtract, comparison, c)),
              "(a >= b) - c");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::subtract, c, comparison)),
              "c - (a >= b)");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::equal, comparison, c)),
              "a >= b == c");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::greater_equal, equality, c)),
              "(a == b) >= c");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::greater_equal, c, comparison)),
              "c >= (a >= b)");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::greater, subtraction, c)),
              "a - b > c");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::greater, equality, c)),
              "(a == b) > c");
    EXPECT_EQ(CppRenderer{ir}.render(
                  ir.binary(BinaryOperator::greater, a, ir.binary(BinaryOperator::greater, b, c))),
              "a > (b > c)");
    EXPECT_EQ(CppRenderer{ir}.render(ir.binary(BinaryOperator::subtract, c, ir.raw_expr("a - b"))),
              "c - (a - b)");
}

TEST(Expr, CollectsMemberObjectAndSubscriptIndexDependencies) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const object{types.dependency("get_rows", "Project/Rows.h")};
    auto const index{types.dependency("get_index", "Project/Index.h")};
    auto const expression{
        ir.subscript(ir.member_access(ir.call(ir.named("get_rows", {object})), "values"),
                     ir.call(ir.named("get_index", {index})))};
    EXPECT_EQ(collected_dependencies(ir, expression),
              (std::vector<TypeDependencyId>{object, index}));
    EXPECT_EQ(collected_dependencies(ir, ir.assignment(expression, ir.literal("42"))),
              (std::vector<TypeDependencyId>{object, index}));
}

TEST(Expr, EscapesStringContentsWithoutConsumingFollowingDigits) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    EXPECT_EQ(CppRenderer{ir}.render(ir.string_literal("")), "\"\"");
    EXPECT_EQ(CppRenderer{ir}.render(ir.string_literal("a\"b\\c\n\r\t")),
              "\"a\\\"b\\\\c\\n\\r\\t\"");
    EXPECT_EQ(CppRenderer{ir}.render(ir.string_literal(std::string_view{"\0"
                                                                        "7\001"
                                                                        "f\177",
                                                                        5})),
              "\"\\0007\\001f\\177\"");
    EXPECT_EQ(CppRenderer{ir}.render(ir.string_literal("caf\xc3\xa9")), "\"caf\xc3\xa9\"");
    EXPECT_EQ(escape_cpp_string(std::string_view{"\0"
                                                 "7\001"
                                                 "f\177",
                                                 5}),
              "\\0007\\001f\\177");
}

TEST(Expr, CollectsDependenciesFromEveryOperandAndType) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const callee{types.dependency("make", "Project/Make.h")};
    auto const left{types.dependency("left", "Project/Left.h")};
    auto const right{types.dependency("right", "Project/Right.h")};
    auto const cast_type{types.dependency("Count", "Project/Count.h")};
    auto const list_type{types.dependency("Value", "Project/Value.h")};
    auto const raw_dependency{types.dependency("macro", "Project/Macro.h")};
    auto const expression{ir.init_list(
        {ir.call(
            ir.named("make", {callee}),
            {ir.binary(BinaryOperator::equal, ir.named("left", {left}), ir.named("right", {right})),
             ir.static_cast_expr(types.intern("Count", {cast_type}),
                                 ir.raw_expr("MACRO", {raw_dependency}))})},
        types.intern("Value", {list_type}))};

    EXPECT_EQ(
        collected_dependencies(ir, expression),
        (std::vector<TypeDependencyId>{list_type, callee, left, right, cast_type, raw_dependency}));
}

TEST(Expr, StmtsCollectInitializersAndAssignmentOperands) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const make{types.dependency("make", "Project/Make.h")};
    auto const target{types.dependency("target", "Project/Target.h")};
    auto const expression{ir.call(ir.named("make", {make}))};
    EXPECT_EQ(collected_dependencies(ir, ir.expr_stmt(expression)),
              std::vector<TypeDependencyId>{make});
    EXPECT_EQ(collected_dependencies(ir, ir.return_stmt(expression)),
              std::vector<TypeDependencyId>{make});
    EXPECT_EQ(
        collected_dependencies(
            ir, ir.instance_member(types.intern("auto"), ir.strings.intern("value"), expression)),
        std::vector<TypeDependencyId>{make});
    EXPECT_EQ(collected_dependencies(
                  ir, ir.variable(types.intern("auto"), ir.strings.intern("value"), expression)),
              std::vector<TypeDependencyId>{make});
    EXPECT_EQ(collected_dependencies(ir, ir.assignment(ir.named("target", {target}), expression)),
              (std::vector<TypeDependencyId>{target, make}));
}

TEST(Expr, PreservesAbsentEmptyAndStructuredInitializers) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    EXPECT_EQ(
        CppRenderer{ir}.render(ir.instance_member(types.intern("int"), ir.strings.intern("value"))),
        "int value;");
    EXPECT_EQ(CppRenderer{ir}.render(ir.instance_member(
                  types.intern("int"), ir.strings.intern("value"), ir.raw_expr(""))),
              "int value{};");
    EXPECT_EQ(CppRenderer{ir}.render(ir.instance_member(
                  types.intern("int"), ir.strings.intern("value"), ir.literal("42"))),
              "int value{42};");
    EXPECT_EQ(CppRenderer{ir}.render(
                  ir.variable(types.intern("int"), ir.strings.intern("value"), ir.raw_expr(""))),
              "int value{};");
    EXPECT_EQ(CppRenderer{ir}.render(ir.variable(
                  types.intern("auto const"), ir.strings.intern("value"), ir.call(ir.named("f")))),
              "auto const value{f()};");
    EXPECT_EQ(CppRenderer{ir}.render(ir.return_stmt()), "return;");
}

TEST(Ast, RendersNestedControlFlowWithExplicitBreaksAndSpacing) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    NodeListBuilder body{ir};
    body.add(ir.if_stmt(ir.named("ready"),
                        ir.block({ir.return_stmt(ir.literal("true"))}),
                        ir.block({ir.expr_stmt(ir.call(ir.named("retry")))})),
             2)
        .add(ir.break_stmt());
    NodeId const statement{ir.switch_stmt(
        ir.named("mode"),
        {ir.switch_case(ir.named("Mode::Ready"), ir.block(body.build())),
         ir.switch_case(std::nullopt, ir.block({ir.return_stmt(ir.literal("false"))}))})};

    EXPECT_EQ(CppRenderer{ir}.render(statement, {.indent_level = 1}),
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
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    EXPECT_EQ(CppRenderer{ir}.render(ir.block({})), "{\n\n}");
    EXPECT_EQ(CppRenderer{ir}.render(ir.block({ir.return_stmt()})), "{\n    return;\n}");
    EXPECT_EQ(CppRenderer{ir}.render(ir.if_stmt(ir.named("ready"), ir.block({}))),
              "if (ready) {\n\n}");
    EXPECT_EQ(CppRenderer{ir}.render(ir.switch_stmt(ir.named("mode"), {})), "switch (mode) {\n}");
    EXPECT_EQ(CppRenderer{ir}.render(
                  ir.switch_stmt(ir.named("mode"), {ir.switch_case(std::nullopt, ir.block({}))})),
              "switch (mode) {\ndefault: {\n\n}\n}");
}

TEST(Ast, VisitsBothBranchesAndAllSwitchBodiesInOrder) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    NodeId const branches{
        ir.if_stmt(ir.named("ready"), ir.block({ir.raw("then")}), ir.block({ir.raw("else")}))};
    NodeId const cases{
        ir.switch_stmt(ir.named("mode"),
                       {ir.switch_case(ir.literal("1"), ir.block({ir.raw("one")})),
                        ir.switch_case(ir.literal("2"), ir.block({ir.raw("two")})),
                        ir.switch_case(std::nullopt, ir.block({ir.raw("default")}))})};
    std::vector<std::string> visited;
    auto visit = [&](NodeId const& node) { visited.push_back(CppRenderer{ir}.render(node)); };
    ir.for_each_child(branches, visit);
    ir.for_each_child(cases, visit);
    EXPECT_EQ(visited, (std::vector<std::string>{"then", "else", "one", "two", "default"}));
}

TEST(Ast, CollectsDeepControlFlowDependenciesOnlyForDefinitions) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto symbol = [&](std::string const& name) {
        auto const header{"Project/" + name + ".h"};
        return ir.named(name, {types.dependency(name, header)});
    };
    auto const body_call{ir.call(symbol("body_only"), {symbol("argument_only")})};
    auto const selection{ir.switch_stmt(
        symbol("switch_only"),
        {ir.switch_case(symbol("case_only"), ir.block({ir.expr_stmt(body_call)})),
         ir.switch_case(std::nullopt,
                        ir.block({ir.block({ir.expr_stmt(ir.call(symbol("default_only")))})}))})};
    auto const spec{[&] {
        auto const created_function{ir.function(ir.strings.intern("f"), types.intern("void"))};
        ir.set_function_body(
            created_function,
            {ir.if_stmt(ir.call(symbol("condition_only")),
                        ir.block({selection}),
                        ir.block({ir.return_stmt(ir.call(symbol("else_only")))}))});
        return created_function;
    }()};
    auto const file{ir.files.append(ir.strings.add("Example.h"),
                                    {ir.include_dependencies(), ir.declaration(spec)})};
    EXPECT_EQ(CppRenderer{ir}.render(file).find("Project/"), std::string::npos);

    ir.files.set_nodes(file, {ir.files.nodes(file).front(), ir.definition(spec)});
    auto const output{CppRenderer{ir}.render(file)};
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
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const symbol{types.dependency("make", "Project/Make.h")};
    auto const value{types.dependency("Value", "Project/Value.h")};
    auto const expression{ir.call(ir.named("make", {symbol, value}))};
    auto const file{ir.files.append(
        ir.strings.add("Example.h"),
        {ir.include_dependencies(),
         ir.instance_member(types.intern("auto"), ir.strings.intern("value"), expression),
         ir.definition(
             [&] {
                 auto const created_function{
                     ir.function(ir.strings.intern("f"), types.intern("auto"))};
                 ir.set_function_body(created_function, {ir.return_stmt(expression)});
                 return created_function;
             }(),
             {})})};
    auto const output{CppRenderer{ir}.render(file)};
    for (auto const* header : {"Project/Make.h", "Project/Value.h"}) {
        EXPECT_NE(output.find(header), std::string::npos);
        EXPECT_EQ(output.find(header), output.rfind(header));
    }
}

} // namespace
} // namespace lispb::codegen
