#include "compiler_test_context.h"
#include "test_utilities.h"

#include <codegen/ast/access.h>
#include <codegen/ast/access_indentation.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_file_flags.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_renderer.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_formatting.h>
#include <codegen/ast/function_opening_brace_placement.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/function_requires_placement.h>
#include <codegen/ast/function_template_placement.h>
#include <codegen/ast/include.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/render_context.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/type_operation.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/schema/declarations.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace lispb::codegen {
namespace {

static_assert(
    !std::is_invocable_v<decltype(&CppIR::set_record_template), CppIR&, NodeId, TemplateId>);
static_assert(
    !std::is_invocable_v<decltype(&CppIR::switch_case), CppIR&, std::optional<Expr>, NodeId, bool>);
static_assert(
    std::is_invocable_v<decltype(&CppIR::switch_case), CppIR&, std::optional<Expr>, BlockId, bool>);

TEST(Ast, NodeListBuilderComposesSectionsAndSpacing) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    NodeListBuilder section{ir};
    section.add(ir.instance_member(types.intern("int32"), ir.strings.intern("first")), 1)
        .add(ir.instance_member(types.intern("int32"), ir.strings.intern("second")));

    NodeListBuilder builder{ir};
    auto nodes{builder.add(ir.access(Access::public_), 1)
                   .append(section.build())
                   .new_lines(2)
                   .add(ir.instance_member(types.intern("int32"), ir.strings.intern("third")))
                   .build()};

    EXPECT_EQ(CppRenderer{ir}.render_nodes(nodes, {}, 0),
              "public:\nint32 first;\nint32 second;\n\nint32 third;");
}

TEST(Ast, RendersFriendDeclarations) {
    CompilerTestContext context;
    auto& ir{context.ir};
    CppRenderer renderer{ir};
    EXPECT_EQ(renderer.render(ir.friend_declaration(ir.strings.intern("FOwner"))),
              "friend class FOwner;");
    EXPECT_EQ(
        renderer.render(ir.friend_declaration(ir.strings.intern("FValue"), RecordKind::struct_)),
        "friend struct FValue;");
}

TEST(Ast, RendersComments) {
    CompilerTestContext context;
    auto& ir{context.ir};
    CppRenderer renderer{ir};
    EXPECT_EQ(renderer.render(ir.line_comment("Lifetime")), "// Lifetime");
    EXPECT_EQ(renderer.render(ir.block_comment("****************************************")),
              "/* **************************************** */");
}

TEST(Ast, RendersStaticAssertionsWithOptionalMessages) {
    CompilerTestContext context;
    auto& ir{context.ir};
    CppRenderer renderer{ir};
    NodeId const without_message{ir.static_assertion(ir.store_text("sizeof(Value) == 16"), {})};
    NodeId const with_message{ir.static_assertion(ir.store_text("valid<Value>"),
                                                  ir.store_text("Value must be \"valid\"."))};

    EXPECT_EQ(renderer.render(without_message), "static_assert(sizeof(Value) == 16);");
    EXPECT_EQ(renderer.render(with_message),
              "static_assert(valid<Value>, \"Value must be \\\"valid\\\".\");");

    auto const dependency{ir.types.dependency("a::valid", "a/validation.h")};
    auto const predicate{ir.call(ir.type_reference(ir.types.named("a::valid", {dependency})), {})};
    auto const assertion{ir.static_assertion(predicate)};
    auto const shadowed{ir.record(
        ir.strings.intern("Value"),
        {ir.instance_member(ir.types.intern("int"), ir.strings.intern("valid")), assertion})};
    auto const output{
        renderer.render(ir.namespace_node(ir.strings.intern("a"), {assertion, shadowed}))};
    EXPECT_NE(output.find("static_assert(valid());"), std::string::npos);
    EXPECT_NE(output.find("static_assert(a::valid());"), std::string::npos);
    EXPECT_EQ(collected_dependencies(ir, assertion), std::vector<TypeDependencyId>{dependency});
}

TEST(Ast, RendersTypedStmts) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    CppRenderer renderer{ir};
    EXPECT_EQ(renderer.render(ir.expr_stmt(ir.raw_expr("apply(value)"))), "apply(value);");
    EXPECT_EQ(renderer.render(ir.return_stmt(ir.raw_expr("value"))), "return value;");
    EXPECT_EQ(renderer.render(ir.return_stmt()), "return;");
    EXPECT_EQ(renderer.render(ir.assignment(ir.raw_expr("target"), ir.raw_expr("value"))),
              "target = value;");
    EXPECT_EQ(renderer.render(ir.variable(types.intern("auto const"),
                                          ir.strings.intern("count"),
                                          ir.raw_expr("values.Num()"))),
              "auto const count{values.Num()};");

    auto const left{ir.named("left")};
    auto const right{ir.named("right")};
    auto const comparison{ir.binary(BinaryOperator::three_way, left, right)};
    EXPECT_EQ(renderer.render(ir.binary(BinaryOperator::less_equal, comparison, ir.literal("0"))),
              "left <=> right <= 0");
    EXPECT_EQ(renderer.render(ir.binary(BinaryOperator::three_way,
                                        ir.binary(BinaryOperator::less_equal, left, right),
                                        ir.literal("0"))),
              "(left <= right) <=> 0");
    EXPECT_EQ(renderer.render(ir.unary(UnaryOperator::negate, comparison)), "-(left <=> right)");

    auto const conjunction{ir.binary(BinaryOperator::logical_and, left, right)};
    EXPECT_EQ(
        renderer.render(ir.binary(BinaryOperator::logical_or, conjunction, ir.named("other"))),
        "(left && right) || other");
    EXPECT_EQ(
        renderer.render(ir.binary(BinaryOperator::logical_or, ir.named("other"), conjunction)),
        "other || (left && right)");
    EXPECT_EQ(renderer.render(ir.binary(BinaryOperator::logical_or, conjunction, conjunction)),
              "(left && right) || (left && right)");
    EXPECT_EQ(
        renderer.render(ir.binary(BinaryOperator::logical_and, conjunction, ir.named("other"))),
        "left && right && other");
}

TEST(Ast, TypedStmtsReportDependencies) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const dependency{types.dependency("FValue", "Project/Value.h")};

    EXPECT_EQ(collected_dependencies(ir, ir.expr_stmt(ir.raw_expr("use_value()"), {dependency})),
              std::vector<TypeDependencyId>{dependency});
    EXPECT_EQ(collected_dependencies(ir, ir.return_stmt(ir.raw_expr("FValue{}"), {dependency})),
              std::vector<TypeDependencyId>{dependency});
    EXPECT_EQ(collected_dependencies(
                  ir, ir.assignment(ir.raw_expr("target"), ir.raw_expr("FValue{}"), {dependency})),
              std::vector<TypeDependencyId>{dependency});
    EXPECT_EQ(collected_dependencies(ir,
                                     ir.variable(types.intern("FValue", {dependency}),
                                                 ir.strings.intern("value"),
                                                 ir.raw_expr("make_value()"))),
              std::vector<TypeDependencyId>{dependency});
}

TEST(Ast, RendersAccessSpecifiersWithExplicitIndentation) {
    CompilerTestContext context;
    auto& ir{context.ir};
    CppRenderer renderer{ir};
    auto const render_context{RenderContext{.indent_level = 1}};

    EXPECT_EQ(renderer.render(ir.access(Access::public_), render_context), "  public:");
    EXPECT_EQ(
        renderer.render(ir.access(Access::private_, AccessIndentation::normal), render_context),
        "    private:");
    CppRenderer tab_renderer{ir, "\t"};
    EXPECT_EQ(
        tab_renderer.render(ir.access(Access::private_, AccessIndentation::normal), render_context),
        "\tprivate:");
}

TEST(Ast, RendersBraceInitialisedTree) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const getter{ir.function(ir.strings.intern("get"), types.intern("auto"))};
    ir.set_function_body(getter, {ir.raw("return value;")});
    ir.set_function_return_type(getter, types.intern("int32"), ReturnSyntax::trailing);
    ir.set_function_receiver(getter, FunctionReceiver::const_);
    ir.function_flags(getter).set_is_inline(true);
    ir.function_flags(getter).set_define_in_header(true);

    auto const file{ir.files.append(
        ir.strings.intern("Example.h"),
        {
            ir.include(ir.store_text("Example/Dependency.h"), IncludeStyle::quoted),
            ir.lines(2),
            ir.namespace_node(ir.strings.intern("example"),
                              {

                                  ir.record(ir.strings.intern("FValue"),
                                            {
                                                ir.instance_member(types.intern("int32"),
                                                                   ir.strings.intern("value")),
                                                ir.header_function(getter),
                                            },
                                            RecordKind::struct_),
                              }),
        })};

    EXPECT_EQ(CppRenderer{ir}.render(file), R"(// This file is autogenerated. Do not edit by hand.
// Edit lispb/schema and regenerate it instead.

#pragma once

#include "Example/Dependency.h"

namespace example {
struct FValue {
    int32 value;

    auto get() const -> int32 {
        return value;
    }
};
} // namespace example
)");
}

TEST(Ast, CollectsNestedTypeDependenciesOnceAndInGroups) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const element{types.dependency("FElement", "Project/Element.h")};
    auto const array{types.intern(
        "TArray<FElement>", {types.dependency("TArray<FElement>", "Containers/Array.h"), element})};
    auto const file{
        ir.files.append(ir.strings.intern("Example.h"),
                        {
                            ir.include_dependencies(),
                            ir.lines(2),
                            ir.record(ir.strings.intern("FData"),
                                      {ir.instance_member(array, ir.strings.intern("first")),
                                       ir.instance_member(array, ir.strings.intern("second"))},
                                      RecordKind::struct_),
                        })};
    ir.files.set_include_order(file, {ir.strings.intern("Project/")});

    auto const rendered{CppRenderer{ir}.render(file)};
    EXPECT_EQ(rendered.find("Project/Element.h"), rendered.rfind("Project/Element.h"));
    EXPECT_EQ(rendered.find("Containers/Array.h"), rendered.rfind("Containers/Array.h"));
    EXPECT_LT(rendered.find("Project/Element.h"), rendered.find("Containers/Array.h"));
}

TEST(Ast, SortsDependencyIncludesWithinConfiguredGroups) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const file{ir.files.append(
        ir.strings.intern("Example.h"),
        {
            ir.include_dependencies(),
            ir.instance_member(types.intern("FZ", {types.dependency("FZ", "Project/Z.h")}),
                               ir.strings.intern("z")),
            ir.instance_member(types.intern("FA", {types.dependency("FA", "Project/A.h")}),
                               ir.strings.intern("a")),
            ir.instance_member(
                types.intern("TArray<int32>",
                             {types.dependency("TArray<int32>", "Containers/Array.h")}),
                ir.strings.intern("values")),
        })};
    ir.files.set_include_order(file, {ir.strings.intern("Project/")});

    auto const rendered{CppRenderer{ir}.render(file)};
    EXPECT_LT(rendered.find("Project/A.h"), rendered.find("Project/Z.h"));
    EXPECT_LT(rendered.find("Project/Z.h"), rendered.find("Containers/Array.h"));
}

TEST(Ast, RendersExplicitIncludesAtTheirNodeLocation) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto const file{
        ir.files.append(ir.strings.intern("Example.cpp"),
                        {
                            ir.include(ir.store_text("Project/Explicit.h"), IncludeStyle::quoted),
                            ir.lines(2),
                            ir.raw("int value;"),
                        })};
    ir.files.flags[ir.files.index(file)].set_pragma_once(false);

    auto const rendered{CppRenderer{ir}.render(file)};
    EXPECT_LT(rendered.find("Project/Explicit.h"), rendered.find("int value;"));
    EXPECT_EQ(rendered.find("Project/Explicit.h"), rendered.rfind("Project/Explicit.h"));
}

TEST(Ast, InfersSystemIncludesFromExtensionlessPaths) {
    EXPECT_TRUE(include_is_system("vector", IncludeStyle::inferred));
    EXPECT_FALSE(include_is_system("Project/Value.h", IncludeStyle::inferred));
    EXPECT_FALSE(include_is_system("vector", IncludeStyle::quoted));
    EXPECT_TRUE(include_is_system("Project/Value.h", IncludeStyle::system));
}

TEST(Ast, VisitsChildrenForNestedAndDefinedNodes) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const structure{
        ir.record(ir.strings.intern("FData"), {ir.raw("int value;")}, RecordKind::struct_)};
    NodeId const name_space{
        ir.namespace_node(ir.strings.intern("example"), {ir.raw("int value;")})};
    auto const spec{ir.function(ir.strings.intern("f"), types.intern("void"))};
    ir.set_function_body(spec, {ir.raw("return;")});
    NodeId const function{ir.definition(spec)};
    NodeId const declaration_node{
        ir.declaration(ir.function(ir.strings.intern("f"), types.intern("void")))};
    NodeId const leaf{ir.raw("int value;")};

    auto child_count = [&](NodeId const& node) {
        std::size_t count{};
        ir.for_each_child(node, [&](NodeId const&) { ++count; });
        return count;
    };
    EXPECT_EQ(child_count(structure), 1);
    EXPECT_EQ(child_count(name_space), 1);
    EXPECT_EQ(child_count(function), 1);
    EXPECT_EQ(child_count(declaration_node), 0);
    EXPECT_EQ(child_count(leaf), 0);
}

TEST(Ast, RendersClassInheritanceAndMemberInitializers) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const record{ir.record(
        ir.strings.intern("FDerived"),
        {
            ir.instance_member(types.intern("int"), ir.strings.intern("value"), ir.raw_expr("42")),
            ir.constexpr_member(
                types.intern("int"), ir.strings.intern("answer"), ir.raw_expr("42")),
            ir.constexpr_member(
                types.intern("bool"),
                ir.strings.intern("supports_value"),
                ir.template_id(ir.named("std::is_constructible_v"),
                               {ir.type_reference(types.intern("int")),
                                ir.type_reference(types.intern("TArg"))}),
                true,
                ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TArg"))})),
        },
        RecordKind::class_)};
    ir.set_record_bases(record, {types.intern("FBase")});
    ir.set_record_export_specifier(record, ir.strings.intern("PROJECT_API"));
    auto const rendered{CppRenderer{ir}.render(record)};
    auto const forward{ir.forward_declaration(
        ir.strings.intern("FDerived"), RecordKind::class_, {}, ir.strings.intern("PROJECT_API"))};
    EXPECT_EQ(CppRenderer{ir}.render(forward), "class PROJECT_API FDerived;");

    EXPECT_EQ(rendered,
              "class PROJECT_API FDerived : FBase {\n"
              "    int value{42};\n"
              "\n"
              "    static constexpr int answer{42};\n"
              "\n"
              "    template <typename TArg>\n"
              "    inline static constexpr bool "
              "supports_value{std::is_constructible_v<int, TArg>};\n"
              "};");
}

TEST(Ast, RendersTemplatedConstrainedFunctions) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const spec{ir.function(ir.strings.intern("append"), types.intern("void"))};
    ir.set_function_parameters(
        spec, {ir.parameter(types.intern("Other const&"), ir.strings.intern("other"))});
    ir.set_function_body(spec, {ir.raw("append_from(other);")});
    ir.function_flags(spec).set_is_inline(true);
    ir.function_flags(spec).set_define_in_header(true);
    ir.set_function_template(
        spec, ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}));
    ir.set_function_requires_clause(
        spec, ir.template_id(ir.named("Appendable"), {ir.type_reference(types.intern("Other"))}));

    auto const rendered{CppRenderer{ir}.render(ir.header_function(spec))};
    EXPECT_NE(rendered.find("template <typename Other>"), std::string::npos);
    EXPECT_NE(rendered.find("requires Appendable<Other>"), std::string::npos);
    EXPECT_NE(rendered.find("append_from(other);"), std::string::npos);
}

TEST(Ast, RendersTypedFunctionFormattingPolicies) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    CppRenderer renderer{ir};
    auto compact{ir.function(ir.strings.intern("apply"), types.intern("void"))};
    ir.set_function_body(compact, {ir.raw("func();")});
    ir.function_flags(compact).set_is_inline(true);
    ir.function_flags(compact).set_define_in_header(true);
    ir.set_function_template(
        compact, ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))}));
    ir.set_function_requires_clause(
        compact, ir.template_id(ir.named("Invocable"), {ir.type_reference(types.intern("TFunc"))}));
    ir.function_formatting(compact) = FunctionFormatting{
        .body_layout = FunctionBodyLayout::compact,
        .requires_placement = FunctionRequiresPlacement::trailing_same_line,
        .template_placement = FunctionTemplatePlacement::same_line,
    };
    EXPECT_EQ(
        renderer.render(ir.record(ir.strings.intern("FValue"), {ir.header_function(compact)})),
        "struct FValue {\n"
        "    template <typename TFunc> void apply() requires Invocable<TFunc> { func(); }\n"
        "};");

    auto prefixed{compact};
    ir.function_formatting(prefixed) = FunctionFormatting{
        .requires_placement = FunctionRequiresPlacement::before_signature,
    };
    EXPECT_EQ(
        renderer.render(ir.record(ir.strings.intern("FValue"), {ir.header_function(prefixed)})),
        "struct FValue {\n"
        "    template <typename TFunc>\n"
        "        requires Invocable<TFunc>\n"
        "    void apply() {\n"
        "        func();\n"
        "    }\n"
        "};");

    auto separate_brace{compact};
    ir.set_function_template(separate_brace, {});
    ir.function_formatting(separate_brace) = FunctionFormatting{
        .opening_brace_placement = FunctionOpeningBracePlacement::separate_line,
    };
    EXPECT_EQ(renderer.render(
                  ir.record(ir.strings.intern("FValue"), {ir.header_function(separate_brace)})),
              "struct FValue {\n"
              "    void apply()\n"
              "        requires Invocable<TFunc>\n"
              "    {\n"
              "        func();\n"
              "    }\n"
              "};");
}

TEST(Ast, RendersConstrainedStructuresAndSpecialMembers) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const dependency{types.dependency("check", "CoreMinimal.h")};
    auto const constructor{ir.constructor(ir.strings.intern("TFixedData"))};
    ir.set_function_noexcept(constructor);
    ir.set_function_defaulted(constructor);

    auto const apply{ir.function(ir.strings.intern("apply"), types.intern("void"))};
    ir.set_function_parameters(apply,
                               {ir.parameter(types.intern("TFunc&&"), ir.strings.intern("func"))});
    ir.set_function_body(apply, {ir.raw("func();")});
    ir.function_flags(apply).set_is_inline(true);
    ir.function_flags(apply).set_define_in_header(true);
    ir.set_function_template(
        apply, ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))}));
    ir.function_formatting(apply) = FunctionFormatting{
        .body_layout = FunctionBodyLayout::compact,
        .template_placement = FunctionTemplatePlacement::same_line,
    };

    auto const structure{ir.record(ir.strings.intern("TFixedData"),
                                   {ir.declaration(constructor),
                                    ir.lines(1),
                                    ir.header_function(apply),
                                    ir.lines(2),
                                    ir.access(Access::private_)},
                                   RecordKind::struct_)};
    ir.set_record_template(structure,
                           ir.template_declaration({ir.value_template_parameter(
                               types.intern("int"), ir.strings.intern("Capacity"))}));
    ir.set_record_requires_clause(
        structure,
        ir.parenthesized(
            ir.binary(BinaryOperator::greater_equal, ir.named("Capacity"), ir.literal("0"))));
    ir.set_record_dependencies(structure, {dependency});

    EXPECT_EQ(CppRenderer{ir}.render(structure),
              "template <int Capacity>\n"
              "    requires (Capacity >= 0)\n"
              "struct TFixedData {\n"
              "    TFixedData() noexcept = default;\n"
              "    template <typename TFunc> void apply(TFunc&& func) { func(); }\n\n"
              "  private:\n"
              "};");
    EXPECT_EQ(collected_dependencies(ir, structure), std::vector<TypeDependencyId>{dependency});
}

TEST(Ast, RawNodesReportExplicitDependencies) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const dependency{types.dependency("FValue", "Project/Value.h")};
    NodeId const node{ir.raw("use_value();", {dependency})};

    EXPECT_EQ(collected_dependencies(ir, node), std::vector<TypeDependencyId>{dependency});
}

TEST(Ast, RendersDeclarationsAndOutOfLineDefinitionsFromOneSpec) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    CppRenderer renderer{ir};
    auto const function{ir.function(ir.strings.intern("set"), types.intern("void"))};
    ir.set_function_parameters(function,
                               {ir.parameter(types.intern("int32 const"),
                                             ir.strings.intern("value"),
                                             ir.store_text("0"))});
    ir.set_function_body(function, {ir.raw("stored = value;")});
    ir.set_function_noexcept(function);

    EXPECT_EQ(renderer.render(ir.declaration(function)),
              "void set(int32 const value = 0) noexcept;");
    EXPECT_EQ(renderer.render(ir.definition(function, types.intern("FValue"))),
              "void FValue::set(int32 const value) noexcept {\n"
              "    stored = value;\n"
              "}");
}

TEST(Ast, CollectsTrailingReturnTypeDependencies) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const dependency{types.dependency("FResult", "Project/Result.h")};
    auto const spec{ir.function(ir.strings.intern("make"), types.intern("auto"))};
    ir.set_function_return_type(
        spec, types.intern("FResult", {dependency}), ReturnSyntax::trailing);
    NodeId const function{ir.declaration(spec)};

    EXPECT_EQ(CppRenderer{ir}.render(function), "auto make() -> FResult;");
    EXPECT_EQ(collected_dependencies(ir, function), std::vector<TypeDependencyId>{dependency});
}

TEST(Ast, CollectsOpaqueDeclarationDependenciesWithoutIncludingTheBody) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    CppRenderer renderer{ir};
    auto const requirement{types.dependency("SupportsValue", "Project/Requirement.h")};
    auto const implementation{types.dependency("apply", "Project/Implementation.h")};
    auto const spec{ir.function(ir.strings.intern("use"), types.intern("void"))};
    ir.set_function_parameters(
        spec, {ir.parameter(types.intern("T const&"), ir.strings.intern("value"))});
    ir.set_function_body(
        spec, {ir.expr_stmt(ir.call(ir.named("apply", {implementation}), {ir.named("value")}))});
    ir.set_function_template(
        spec, ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))}));
    ir.set_function_requires_clause(spec,
                                    ir.template_id(ir.named("SupportsValue", {requirement}),
                                                   {ir.type_reference(types.intern("T"))}));
    auto const file{ir.files.append(ir.strings.intern("Example.h"),
                                    {ir.include_dependencies(), ir.declaration(spec)})};
    auto const header{renderer.render(file)};
    EXPECT_NE(header.find("#include \"Project/Requirement.h\""), std::string::npos);
    EXPECT_EQ(header.find("Project/Implementation.h"), std::string::npos);
    EXPECT_NE(header.find("requires SupportsValue<T>"), std::string::npos);

    ir.files.set_nodes(file, {ir.files.nodes(file).front(), ir.definition(spec)});
    auto const source{renderer.render(file)};
    EXPECT_NE(source.find("#include \"Project/Requirement.h\""), std::string::npos);
    EXPECT_NE(source.find("#include \"Project/Implementation.h\""), std::string::npos);
    EXPECT_EQ(source.find("Project/Requirement.h"), source.rfind("Project/Requirement.h"));
}

TEST(Ast, RendersQualifiedTrailingReturnAndDefaultedConstructor) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    CppRenderer renderer{ir};
    auto const function{
        ir.function(ir.strings.intern("qualified_placeholder_return"), types.intern("int32"))};
    ir.set_function_return_type(function, types.intern("int32"), ReturnSyntax::trailing);
    ir.set_function_receiver(function, FunctionReceiver::static_);
    ir.function_flags(function).set_is_constexpr(true);
    EXPECT_EQ(renderer.render(ir.declaration(function)),
              "static constexpr auto qualified_placeholder_return() -> int32;");

    auto const constructor{ir.constructor(ir.strings.intern("FValue"))};
    ir.set_function_defaulted(constructor);
    EXPECT_EQ(renderer.render(ir.definition(constructor, ir.types.intern("FValue"))),
              "FValue::FValue() = default;");

    auto const dependency{types.dependency("a::make_value", "a/value.h")};
    auto const factory{ir.call(ir.type_reference(types.named("a::make_value", {dependency})), {})};
    auto const initialized{ir.constructor(ir.strings.intern("Initialized"))};
    ir.set_function_parameters(initialized, {ir.parameter("int", "make_value")});
    ir.set_function_body(
        initialized, {}, {ir.member_initializer(ir.strings.intern("value_"), factory)});
    ir.function_flags(initialized).set_define_in_header(true);
    auto const record{
        ir.record(ir.strings.intern("Initialized"),
                  {ir.header_function(initialized),
                   ir.instance_member(types.intern("int"), ir.strings.intern("value_"))})};
    auto const output{renderer.render(ir.namespace_node(ir.strings.intern("a"), {record}))};
    EXPECT_NE(output.find("value_{a::make_value()}"), std::string::npos) << output;
    EXPECT_EQ(collected_dependencies(ir, ir.header_function(initialized)),
              std::vector<TypeDependencyId>{dependency});
    EXPECT_TRUE(collected_dependencies(ir, ir.declaration(initialized)).empty());
}

TEST(Ast, RejectsOutOfLineDeletedFunction) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto const function{ir.function(ir.strings.intern("removed"), ir.types.intern("void"))};
    ir.set_function_deleted(function);
    EXPECT_THROW(ir.definition(function, ir.types.intern("FValue")), std::invalid_argument);
}

TEST(Ast, TypeOperationsAreStronglyTyped) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const type{types.add("FContainer")};
    types.set_operation(type, TypeOperation::remove_at_swap, "remove_at_swap");

    EXPECT_EQ(types.operation(type, TypeOperation::remove_at_swap), "remove_at_swap");
}

TEST(Ast, RendersForwardUsingAndFilePolicyNodes) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const file{ir.files.append(
        ir.strings.intern("Example.cpp"),
        {
            ir.forward_declaration(ir.strings.intern("FValue"), RecordKind::struct_),
            ir.lines(1),
            ir.using_declaration(ir.strings.intern("Value"), types.intern("FValue")),
        })};
    auto& flags{ir.files.flags[ir.files.index(file)]};
    flags.set_pragma_once(false);
    flags.set_clang_format_off(true);
    ir.files.set_prologue(file, {ir.strings.intern("// prologue")});
    ir.files.set_epilogue(file, {ir.strings.intern("// epilogue")});

    auto const rendered{CppRenderer{ir}.render(file)};
    EXPECT_TRUE(rendered.starts_with("// clang-format off\n"));
    EXPECT_EQ(rendered.find("#pragma once"), std::string::npos);
    EXPECT_NE(rendered.find("// prologue\n\nstruct FValue;\nusing Value = FValue;"),
              std::string::npos);
    EXPECT_NE(rendered.find("// epilogue\n// clang-format on\n"), std::string::npos);
}

TEST(Ast, AppliesStaticAndDefaultArgumentRulesToFunctionForms) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    CppRenderer renderer{ir};
    auto const function{ir.function(ir.strings.intern("make"), types.intern("int32"))};
    ir.set_function_parameters(function,
                               {ir.parameter(types.intern("int32 const"),
                                             ir.strings.intern("value"),
                                             ir.store_text("7"))});
    ir.set_function_body(function, {ir.return_stmt(ir.raw_expr("value"))});
    ir.set_function_receiver(function, FunctionReceiver::static_);
    ir.function_flags(function).set_is_inline(true);
    ir.function_flags(function).set_define_in_header(true);

    EXPECT_EQ(
        renderer.render(ir.record(ir.strings.intern("FValue"), {ir.header_function(function)})),
        "struct FValue {\n"
        "    static int32 make(int32 const value = 7) {\n"
        "        return value;\n"
        "    }\n"
        "};");
    EXPECT_EQ(renderer.render(ir.definition(function, ir.types.intern("FValue"))),
              "int32 FValue::make(int32 const value) {\n"
              "    return value;\n"
              "}");
}

TEST(Ast, RendersEmptyEnumsWithoutAnEnumerator) {
    CompilerTestContext context;
    auto& ir{context.ir};
    CppRenderer renderer{ir};

    EXPECT_EQ(renderer.render(ir.enumeration(ir.strings.intern("Empty"), {}, {})),
              "enum class Empty {\n\n};");
    EXPECT_EQ(renderer.render(ir.enumeration(ir.strings.intern("Empty"), {}, {}, {}, false)),
              "enum Empty {\n\n};");
}

TEST(Ast, RendersEnumsAndExportedFreeFunctions) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    CppRenderer renderer{ir};
    auto const value{
        ir.enumeration(ir.strings.intern("EMode"),
                       types.intern("uint8", {types.dependency("uint8", "CoreMinimal.h")}),
                       {
                           ir.enumerator(ir.strings.intern("First"), ir.store_text("1")),
                           ir.enumerator(ir.strings.intern("Second"),
                                         std::nullopt,
                                         ir.store_text("UMETA(DisplayName = \"Second Value\")")),
                       },
                       ir.strings.intern("PROJECT_API"))};

    EXPECT_EQ(renderer.render(value),
              "enum class PROJECT_API EMode : uint8 {\n"
              "    First = 1,\n"
              "    Second UMETA(DisplayName = \"Second Value\"),\n"
              "};");
    auto const function{ir.function(ir.strings.intern("to_string_view"), types.intern("auto"))};
    ir.set_function_return_type(function, types.intern("FStringView"), ReturnSyntax::trailing);
    ir.set_function_export_specifier(function, ir.strings.intern("PROJECT_API"));
    EXPECT_EQ(renderer.render(ir.declaration(function)),
              "PROJECT_API auto to_string_view() -> FStringView;");
    EXPECT_EQ(renderer.render(ir.definition(function)),
              "auto to_string_view() -> FStringView {\n\n}");
    EXPECT_EQ(renderer.render(ir.namespace_node(ir.strings.intern(""), {ir.raw("int value;")})),
              "namespace {\nint value;\n} // namespace");
}

struct NamespaceCase {
    std::string_view current;
    std::string_view target;
    std::string_view expected;
    std::vector<std::string_view> hidden;
};

auto in_namespace(CppIR& ir, std::string_view name, NodeId node, bool nested = false) -> NodeId {
    if (!nested) {
        return ir.namespace_node(ir.strings.intern(name), {node});
    }
    auto const separator{name.find("::")};
    return separator == std::string_view::npos
             ? ir.namespace_node(ir.strings.intern(name), {node})
             : ir.namespace_node(ir.strings.intern(name.substr(0, separator)),
                                 {in_namespace(ir, name.substr(separator + 2), node, true)});
}

class NamespaceRendering : public testing::TestWithParam<NamespaceCase> {};

TEST_P(NamespaceRendering, ShortensOnlyVisibleNamespaceComponents) {
    auto const& test{GetParam()};
    for (auto const nested : {false, true}) {
        CompilerTestContext context;
        auto& ir{context.ir};
        auto& types{ir.types};
        NodeListBuilder members{ir};
        for (auto const name : test.hidden) {
            members.add(ir.instance_member(types.intern("int"), ir.strings.intern(name)));
        }
        members.add(ir.instance_member(types.named(test.target), ir.strings.intern("value")));
        auto const record{ir.record(ir.strings.intern("Holder"), members.build())};
        auto const output{CppRenderer{ir}.render(in_namespace(ir, test.current, record, nested))};
        EXPECT_NE(output.find(std::string{test.expected} + " value;"), std::string::npos) << output;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Namespaces,
    NamespaceRendering,
    testing::Values(NamespaceCase{"a::b", "a::b::Token", "Token", {}},
                    NamespaceCase{"a::b", "a::Token", "Token", {}},
                    NamespaceCase{"a::b", "a::c::Token", "c::Token", {}},
                    NamespaceCase{"a::b", "other::Token", "other::Token", {}},
                    NamespaceCase{"a::bee", "a::b::Token", "b::Token", {}},
                    NamespaceCase{"a::b", "a::b::Token::Nested", "Token::Nested", {}},
                    NamespaceCase{"a::b", "a::b::Token", "b::Token", {"Token"}},
                    NamespaceCase{"a::b", "a::Token", "a::Token", {"Token"}},
                    NamespaceCase{"a::b", "a::c::Token", "a::c::Token", {"c"}},
                    NamespaceCase{"a::b", "a::b::Token", "::a::b::Token", {"Token", "b", "a"}},
                    NamespaceCase{"a::b", "::a::b::Token", "::a::b::Token", {}}));

TEST(Ast, RestoresNamespaceContextAndRetainsAnonymousLookup) {
    CompilerTestContext context;
    auto& ir{context.ir};
    CppRenderer renderer{ir};
    auto const type{ir.types.named("a::b::Token")};
    auto const member{ir.variable(type, "value")};
    auto const local{in_namespace(ir, "a::b", ir.namespace_node(ir.strings.intern(""), {member}))};
    auto const output{renderer.render_nodes(std::vector<NodeId>{local, member}, {}, 1)};
    EXPECT_NE(output.find("namespace {\nToken value{};"), std::string::npos);
    EXPECT_TRUE(output.ends_with("a::b::Token value{};"));
    EXPECT_EQ(renderer.render(member), "a::b::Token value{};");
    EXPECT_EQ(renderer.render(local), renderer.render(local));
}

TEST(Ast, OmitsImportsIntoTheirOwningNamespaceAndRetainsOtherImports) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto const dependency{ir.types.dependency("a::lookup", "Lookup.h")};
    auto const imported{ir.using_import(ir.types.named("a::lookup", {dependency}))};
    CppRenderer renderer{ir};
    EXPECT_EQ(renderer.render(in_namespace(ir, "a", imported)),
              "namespace a {\n\n} // namespace a");
    EXPECT_NE(renderer.render(in_namespace(ir, "b", imported)).find("using a::lookup;"),
              std::string::npos);
    EXPECT_NE(renderer.render(in_namespace(ir, "a::nested", imported)).find("using a::lookup;"),
              std::string::npos);
    EXPECT_EQ(collected_dependencies(ir, imported), std::vector{dependency});
}

TEST(Ast, RetainsTypeStructureAndPoliciesAcrossCompositionAndDependencies) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{ir.types};
    auto const dependency{types.dependency("a::Token", "Token.h")};
    auto const registered{types.add("a::Token", {dependency}, ParameterPassing::value)};
    types.set_operation(registered, TypeOperation::remove_at_swap, "erase");
    auto const named{types.reference(registered)};
    auto const pointer{types.decorate(named, "", " const*")};
    auto const array{types.template_type(types.named("std::array"), {pointer, types.intern("2")})};
    auto const copied{types.with_dependencies(pointer, std::vector{dependency})};
    auto const emit = [&](CppTypeId type) {
        return CppRenderer{ir}.render(in_namespace(ir, "a", ir.variable(type, "value")));
    };
    EXPECT_NE(emit(array).find("std::array<Token const*, 2> value{};"), std::string::npos);
    EXPECT_NE(emit(copied).find("Token const* value{};"), std::string::npos);
    EXPECT_EQ(types.spelling(array), "std::array<a::Token const*, 2>");
    EXPECT_EQ(types.operation(copied, TypeOperation::remove_at_swap), "erase");
    EXPECT_EQ(types.parameter_passing(copied), ParameterPassing::value);
    EXPECT_EQ(collected_dependencies(ir, ir.variable(array, "value")), std::vector{dependency});
    EXPECT_EQ(types.template_type(types.named("std::array"), {pointer, types.intern("2")}), array);
    EXPECT_EQ(types.derive(named, types.spelling(named)), named);
    EXPECT_NE(named, types.intern("a::Token", {dependency}, ParameterPassing::value));
    EXPECT_NE(emit(types.intern("a::Token")).find("a::Token value{};"), std::string::npos);
}

TEST(Ast, PreservesQualificationAcrossOpaqueScopesUnlessBindingsAreProvided) {
    CompilerTestContext context;
    auto& ir{context.ir};
    CppRenderer renderer{ir};
    auto const raw{ir.raw("using Hidden = int;")};
    auto const member{ir.instance_member(ir.types.named("a::Token"), ir.strings.intern("value"))};
    auto const record{ir.record(ir.strings.intern("Holder"), {raw, member})};
    auto const wrapped{in_namespace(ir, "a", record)};
    EXPECT_NE(renderer.render(wrapped).find("a::Token value;"), std::string::npos);
    ir.set_scope_bindings(raw, std::vector{ir.strings.intern("Hidden")});
    EXPECT_NE(renderer.render(wrapped).find("\n    Token value;"), std::string::npos);
    ir.set_record_bases(record, {ir.types.intern("ExternalBase")});
    EXPECT_NE(renderer.render(wrapped).find("a::Token value;"), std::string::npos);
}

TEST(Ast, AccountsForReopenedAndAnonymousNamespaceBindings) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto const token{ir.types.named("a::Token")};
    auto const declaration{ir.record(ir.strings.intern("Token"))};
    auto const member{ir.variable(token, "value")};
    for (auto const anonymous : {false, true}) {
        auto const binding{anonymous ? ir.namespace_node(ir.strings.intern(""), {declaration})
                                     : static_cast<NodeId>(declaration)};
        auto const output{
            CppRenderer{ir}.render_nodes(std::vector<NodeId>{in_namespace(ir, "a::b", binding),
                                                             in_namespace(ir, "a::b", member)},
                                         {},
                                         1)};
        EXPECT_NE(output.find("a::Token value{};"), std::string::npos) << output;
    }
}

TEST(Ast, UsesTemplateAndMemberScopesInSignaturesAndExpressions) {
    CompilerTestContext context;
    auto& ir{context.ir};
    CppRenderer renderer{ir};
    auto const token{ir.types.named("a::Token")};
    auto const make{ir.function("make", token)};
    ir.set_function_parameters(make, {ir.parameter(token, "argument")});
    ir.set_function_body(make, {ir.return_stmt(ir.init_list({}, token))});
    auto const record{
        ir.record(ir.strings.intern("Holder"),
                  {ir.using_declaration(ir.strings.intern("Token"), ir.types.intern("int")),
                   ir.declaration(make)})};
    auto const definition{ir.definition(make, ir.types.intern("Holder"))};
    auto const output{renderer.render_nodes(
        std::vector<NodeId>{in_namespace(ir, "a", record), in_namespace(ir, "a", definition)},
        {},
        1)};
    EXPECT_NE(output.find("a::Token make(a::Token argument);"), std::string::npos);
    EXPECT_NE(output.find("Token Holder::make(a::Token argument)"), std::string::npos);
    EXPECT_NE(output.find("return a::Token{};"), std::string::npos);

    auto const template_function{ir.function("use", token)};
    ir.set_function_return_type(template_function, token, ReturnSyntax::trailing);
    ir.set_function_template(
        template_function,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Token"))}));
    ir.set_function_body(template_function,
                         {ir.variable(token, "value", ir.init_list({}, token)),
                          ir.expr_stmt(ir.sizeof_type(token)),
                          ir.return_stmt(ir.static_cast_expr(token, ir.named("value")))});
    auto const templated{
        renderer.render(in_namespace(ir, "a", ir.definition(template_function, {}, {}, true)))};
    for (auto const expected : {"-> a::Token",
                                "a::Token value{a::Token{}};",
                                "sizeof(a::Token)",
                                "static_cast<a::Token>"}) {
        EXPECT_NE(templated.find(expected), std::string::npos) << templated;
    }
}

TEST(Ast, DistinguishesDeclarationsWhenNodesAreReusedAcrossNamespaces) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto const alias{ir.using_declaration(ir.strings.intern("Token"), ir.types.intern("int"))};
    auto const record{
        ir.record(ir.strings.intern("Holder"),
                  {ir.instance_member(ir.types.named("a::Token"), ir.strings.intern("value"))})};
    auto const output{CppRenderer{ir}.render_nodes(
        std::vector{ir.namespace_node(ir.strings.intern("a"), {alias, record}),
                    ir.namespace_node(ir.strings.intern("b"), {alias, record})},
        {},
        1)};

    EXPECT_NE(output.find("namespace a {\nusing Token = int;\n\nstruct Holder {\n    Token value;"),
              std::string::npos)
        << output;
    EXPECT_NE(
        output.find("namespace b {\nusing Token = int;\n\nstruct Holder {\n    a::Token value;"),
        std::string::npos)
        << output;
}

TEST(Ast, TracksLocalHidingAtTheDeclarationAndRestoresEnclosingLookup) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto const token{ir.types.named("a::Token")};
    auto const function{ir.function("use", "void")};
    ir.set_function_body(function,
                         {ir.variable(token, "before"),
                          ir.block({ir.variable("int", "Token"), ir.variable(token, "inside")}),
                          ir.variable(token, "after"),
                          ir.variable("int", "Token"),
                          ir.variable(token, "hidden")});
    auto const output{CppRenderer{ir}.render(in_namespace(ir, "a", ir.definition(function)))};

    for (auto const expected :
         {"Token before{};", "a::Token inside{};", "Token after{};", "a::Token hidden{};"}) {
        EXPECT_NE(output.find(expected), std::string::npos) << output;
    }
}

TEST(Ast, ResolvesNestedClassOwnersAndForwardDeclaredInjectedNames) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto const token{ir.types.named("a::Token")};
    auto const make{ir.function("make", token)};
    ir.set_function_parameters(make, {ir.parameter(token, "value")});
    ir.set_function_body(make, {ir.return_stmt(ir.init_list({}, token))});
    auto const inner{
        ir.record(ir.strings.intern("Inner"),
                  {ir.using_declaration(ir.strings.intern("Token"), ir.types.intern("int")),
                   ir.declaration(make)})};
    auto const outer{ir.record(ir.strings.intern("Outer"), {inner})};
    auto const self{
        ir.record(ir.strings.intern("Self"),
                  {ir.instance_member(ir.types.decorate(ir.types.named("a::Self"), "", " const*"),
                                      ir.strings.intern("next"))})};
    auto const output{CppRenderer{ir}.render(
        ir.namespace_node(ir.strings.intern("a"),
                          {outer,
                           ir.definition(make, ir.types.intern("Outer::Inner")),
                           ir.forward_declaration(ir.strings.intern("Self")),
                           self}))};

    EXPECT_NE(output.find("Token Outer::Inner::make(a::Token value)"), std::string::npos) << output;
    EXPECT_NE(output.find("return a::Token{};"), std::string::npos) << output;
    EXPECT_NE(output.find("Self const* next;"), std::string::npos) << output;
}

TEST(Ast, KeepsTemplateHeadersIndependentOfFunctionParameters) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto const token{ir.types.named("a::Token")};
    auto const function{ir.function("use", token)};
    ir.set_function_template(
        function,
        ir.template_declaration({ir.value_template_parameter(token, ir.strings.intern("N"))}));
    ir.set_function_parameters(function,
                               {ir.parameter(token, "Token"), ir.parameter(token, "other")});
    ir.set_function_return_type(function, token, ReturnSyntax::trailing);
    auto const output{CppRenderer{ir}.render(in_namespace(ir, "a", ir.declaration(function)))};

    EXPECT_NE(output.find("template <Token N>"), std::string::npos) << output;
    EXPECT_NE(output.find("use(Token Token, a::Token other) -> a::Token;"), std::string::npos)
        << output;
}

TEST(Ast, PreservesQualificationForAuthoredRecordNames) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto const record{
        ir.record(ir.raw_expr("Holder"),
                  {ir.instance_member(ir.types.named("a::Token"), ir.strings.intern("value"))})};

    EXPECT_NE(CppRenderer{ir}.render(in_namespace(ir, "a", record)).find("a::Token value;"),
              std::string::npos);
}

} // namespace
} // namespace lispb::codegen
