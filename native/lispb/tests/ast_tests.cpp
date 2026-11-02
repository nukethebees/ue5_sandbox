#include "cpp_ir_test.h"
#include "test_utilities.h"
#include "text_expectations.h"

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
using lispb::test_support::expect_contains_all;
namespace {

static_assert(
    !std::is_invocable_v<decltype(&CppIR::set_record_template), CppIR&, NodeId, TemplateId>);
static_assert(
    !std::is_invocable_v<decltype(&CppIR::switch_case), CppIR&, std::optional<Expr>, NodeId, bool>);
static_assert(
    std::is_invocable_v<decltype(&CppIR::switch_case), CppIR&, std::optional<Expr>, BlockId, bool>);

TEST_F(AstTest, NodeListBuilderComposesSectionsAndSpacing) {
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

TEST_F(AstTest, RendersFriendDeclarations) {
    expect_rendered(ir.friend_declaration(ir.strings.intern("FOwner")), "friend class FOwner;");
    expect_rendered(ir.friend_declaration(ir.strings.intern("FValue"), RecordKind::struct_),
                    "friend struct FValue;");
}

TEST_F(AstTest, RendersComments) {
    expect_rendered(ir.line_comment("Lifetime"), "// Lifetime");
    expect_rendered(ir.block_comment("****************************************"),
                    "/* **************************************** */");
}

TEST_F(AstTest, RendersStaticAssertionsWithOptionalMessages) {
    NodeId const without_message{ir.static_assertion(ir.store_text("sizeof(Value) == 16"), {})};
    NodeId const with_message{ir.static_assertion(ir.store_text("valid<Value>"),
                                                  ir.store_text("Value must be \"valid\"."))};

    expect_rendered(without_message, "static_assert(sizeof(Value) == 16);");
    expect_rendered(with_message, R"(static_assert(valid<Value>, "Value must be \"valid\".");)");

    auto const dependency{ir.types.dependency("a::valid", "a/validation.h")};
    auto const predicate{ir.call(
        ir.type_reference(ir.types.named("a::valid", NameOrigin::global, {dependency})), {})};
    auto const assertion{ir.static_assertion(predicate)};
    auto const shadowed{
        ir.record(ir.strings.intern("Value"),
                  {ir.instance_member(ir.types.std.int_, ir.strings.intern("valid")), assertion})};
    auto const output{
        renderer.render(ir.namespace_node(ir.strings.intern("a"), {assertion, shadowed}))};
    expect_contains_all(output, {"static_assert(valid());", "static_assert(a::valid());"});
    EXPECT_EQ(collected_dependencies(ir, assertion), std::vector<TypeDependencyId>{dependency});
}

TEST_F(AstTest, RendersTypedStmts) {
    expect_rendered(ir.expr_stmt(ir.raw_expr("apply(value)")), "apply(value);");
    expect_rendered(ir.return_stmt(ir.raw_expr("value")), "return value;");
    expect_rendered(ir.return_stmt(), "return;");
    expect_rendered(ir.assignment(ir.raw_expr("target"), ir.raw_expr("value")), "target = value;");
    expect_rendered(ir.variable(types.intern("auto const"),
                                ir.strings.intern("count"),
                                ir.raw_expr("values.Num()")),
                    "auto const count{values.Num()};");

    auto const left{ir.named("left")};
    auto const right{ir.named("right")};
    auto const comparison{ir.binary(BinaryOperator::three_way, left, right)};
    expect_rendered(ir.binary(BinaryOperator::less_equal, comparison, ir.literal("0")),
                    "left <=> right <= 0");
    expect_rendered(ir.binary(BinaryOperator::three_way,
                              ir.binary(BinaryOperator::less_equal, left, right),
                              ir.literal("0")),
                    "(left <= right) <=> 0");
    expect_rendered(ir.unary(UnaryOperator::negate, comparison), "-(left <=> right)");

    auto const conjunction{ir.binary(BinaryOperator::logical_and, left, right)};
    expect_rendered(ir.binary(BinaryOperator::logical_or, conjunction, ir.named("other")),
                    "(left && right) || other");
    expect_rendered(ir.binary(BinaryOperator::logical_or, ir.named("other"), conjunction),
                    "other || (left && right)");
    expect_rendered(ir.binary(BinaryOperator::logical_or, conjunction, conjunction),
                    "(left && right) || (left && right)");
    expect_rendered(ir.binary(BinaryOperator::logical_and, conjunction, ir.named("other")),
                    "left && right && other");
}

TEST_F(AstTest, TypedStmtsReportDependencies) {
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

TEST_F(AstTest, RendersAccessSpecifiersWithExplicitIndentation) {
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

TEST_F(AstTest, RendersBraceInitialisedTree) {
    FunctionDefinition getter{.name = ir.named(ir.strings.intern("get")),
                              .return_type = types.intern("int32"),
                              .return_syntax = ReturnSyntax::trailing,
                              .flags = {.is_inline = true, .define_in_header = true}};
    getter.implementation = {.kind = FunctionImplementationKind::body,
                             .body = {ir.raw("return value;")}};
    getter.receiver = FunctionReceiver::const_;

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
                                                ir.header_function(ir.add_function(getter)),
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

TEST_F(AstTest, CollectsNestedTypeDependenciesOnceAndInGroups) {
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

TEST_F(AstTest, SortsDependencyIncludesWithinConfiguredGroups) {
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

TEST_F(AstTest, RendersExplicitIncludesAtTheirNodeLocation) {
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

TEST_F(AstTest, InfersSystemIncludesFromExtensionlessPaths) {
    EXPECT_TRUE(include_is_system("vector", IncludeStyle::inferred));
    EXPECT_FALSE(include_is_system("Project/Value.h", IncludeStyle::inferred));
    EXPECT_FALSE(include_is_system("vector", IncludeStyle::quoted));
    EXPECT_TRUE(include_is_system("Project/Value.h", IncludeStyle::system));
}

TEST_F(AstTest, VisitsChildrenForNestedAndDefinedNodes) {
    auto const structure{
        ir.record(ir.strings.intern("FData"), {ir.raw("int value;")}, RecordKind::struct_)};
    NodeId const name_space{
        ir.namespace_node(ir.strings.intern("example"), {ir.raw("int value;")})};
    FunctionDefinition spec{.name = ir.named(ir.strings.intern("f")),
                            .return_type = types.std.void_};
    spec.implementation = {.kind = FunctionImplementationKind::body, .body = {ir.raw("return;")}};
    NodeId const function{ir.definition(ir.add_function(spec))};
    NodeId const declaration_node{ir.declaration(ir.add_function(FunctionDefinition{
        .name = ir.named(ir.strings.intern("f")), .return_type = types.std.void_}))};
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

TEST_F(AstTest, RendersClassInheritanceAndMemberInitializers) {
    auto const record{ir.record(
        ir.strings.intern("FDerived"),
        {
            ir.instance_member(types.std.int_, ir.strings.intern("value"), ir.raw_expr("42")),
            ir.constexpr_member(types.std.int_, ir.strings.intern("answer"), ir.raw_expr("42")),
            ir.constexpr_member(
                types.std.bool_,
                ir.strings.intern("supports_value"),
                ir.template_id(
                    ir.type_reference(ir.types.std.is_constructible_v),
                    {ir.type_reference(types.std.int_), ir.type_reference(types.intern("TArg"))}),
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
              "    static constexpr bool "
              "supports_value{std::is_constructible_v<int, TArg>};\n"
              "};");
}

TEST_F(AstTest, RendersTemplatedConstrainedFunctions) {
    FunctionDefinition spec{.name = ir.named(ir.strings.intern("append")),
                            .return_type = types.std.void_,
                            .flags = {.is_inline = true, .define_in_header = true}};
    spec.parameters = {ir.parameter(types.intern("Other const&"), ir.strings.intern("other"))};
    spec.implementation = {.kind = FunctionImplementationKind::body,
                           .body = {ir.raw("append_from(other);")}};
    spec.template_declaration =
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))});
    spec.requires_clause =
        ir.template_id(ir.named("Appendable"), {ir.type_reference(types.intern("Other"))});

    auto const rendered{CppRenderer{ir}.render(ir.header_function(ir.add_function(spec)))};
    expect_contains_all(
        rendered,
        {"template <typename Other>", "requires Appendable<Other>", "append_from(other);"});
}

TEST_F(AstTest, RendersTypedFunctionFormattingPolicies) {
    FunctionDefinition compact{.name = ir.named(ir.strings.intern("apply")),
                               .return_type = types.std.void_,
                               .flags = {.is_inline = true, .define_in_header = true}};
    compact.implementation = {.kind = FunctionImplementationKind::body,
                              .body = {ir.raw("func();")}};
    compact.template_declaration =
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))});
    compact.requires_clause =
        ir.template_id(ir.named("Invocable"), {ir.type_reference(types.intern("TFunc"))});
    compact.formatting = FunctionFormatting{
        .body_layout = FunctionBodyLayout::compact,
        .requires_placement = FunctionRequiresPlacement::trailing_same_line,
        .template_placement = FunctionTemplatePlacement::same_line,
    };
    expect_rendered(
        ir.record(ir.strings.intern("FValue"), {ir.header_function(ir.add_function(compact))}),
        "struct FValue {\n"
        "    template <typename TFunc> void apply() requires Invocable<TFunc> { func(); }\n"
        "};");

    auto prefixed{compact};
    prefixed.formatting = FunctionFormatting{
        .requires_placement = FunctionRequiresPlacement::before_signature,
    };
    expect_rendered(
        ir.record(ir.strings.intern("FValue"), {ir.header_function(ir.add_function(prefixed))}),
        "struct FValue {\n"
        "    template <typename TFunc>\n"
        "        requires Invocable<TFunc>\n"
        "    void apply() {\n"
        "        func();\n"
        "    }\n"
        "};");

    auto separate_brace{compact};
    separate_brace.template_declaration = {};
    separate_brace.formatting = FunctionFormatting{
        .opening_brace_placement = FunctionOpeningBracePlacement::separate_line,
    };
    expect_rendered(ir.record(ir.strings.intern("FValue"),
                              {ir.header_function(ir.add_function(separate_brace))}),
                    "struct FValue {\n"
                    "    void apply()\n"
                    "        requires Invocable<TFunc>\n"
                    "    {\n"
                    "        func();\n"
                    "    }\n"
                    "};");
}

TEST_F(AstTest, RendersConstrainedStructuresAndSpecialMembers) {
    auto const dependency{types.dependency("check", "CoreMinimal.h")};
    FunctionDefinition constructor{.name = ir.named(ir.strings.intern("TFixedData")),
                                   .kind = FunctionKind::constructor};
    constructor.exception_specification = {.kind = FunctionExceptionKind::noexcept_};
    constructor.implementation = {.kind = FunctionImplementationKind::defaulted};

    FunctionDefinition apply{.name = ir.named(ir.strings.intern("apply")),
                             .return_type = types.std.void_,
                             .flags = {.is_inline = true, .define_in_header = true}};
    apply.parameters = {ir.parameter(types.intern("TFunc&&"), ir.strings.intern("func"))};
    apply.implementation = {.kind = FunctionImplementationKind::body, .body = {ir.raw("func();")}};
    apply.template_declaration =
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))});
    apply.formatting = FunctionFormatting{
        .body_layout = FunctionBodyLayout::compact,
        .template_placement = FunctionTemplatePlacement::same_line,
    };

    auto const structure{ir.record(ir.strings.intern("TFixedData"),
                                   {ir.declaration(ir.add_function(constructor)),
                                    ir.lines(1),
                                    ir.header_function(ir.add_function(apply)),
                                    ir.lines(2),
                                    ir.access(Access::private_)},
                                   RecordKind::struct_)};
    ir.set_record_template(structure,
                           ir.template_declaration({ir.value_template_parameter(
                               types.std.int_, ir.strings.intern("Capacity"))}));
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

TEST_F(AstTest, RawNodesReportExplicitDependencies) {
    auto const dependency{types.dependency("FValue", "Project/Value.h")};
    NodeId const node{ir.raw("use_value();", {dependency})};

    EXPECT_EQ(collected_dependencies(ir, node), std::vector<TypeDependencyId>{dependency});
}

TEST_F(AstTest, RendersDeclarationsAndOutOfLineDefinitionsFromOneSpec) {
    FunctionDefinition function{.name = ir.named(ir.strings.intern("set")),
                                .return_type = types.std.void_};
    function.parameters = {
        ir.parameter(types.intern("int32 const"), ir.strings.intern("value"), ir.literal("0"))};
    function.implementation = {.kind = FunctionImplementationKind::body,
                               .body = {ir.raw("stored = value;")}};
    function.exception_specification = {.kind = FunctionExceptionKind::noexcept_};

    auto const published_function{ir.add_function(function)};
    expect_rendered(ir.declaration(published_function),
                    "void set(int32 const value = 0) noexcept;");
    expect_rendered(ir.definition(published_function, types.intern("FValue")),
                    "void FValue::set(int32 const value) noexcept {\n"
                    "    stored = value;\n"
                    "}");
}

TEST_F(AstTest, CollectsTrailingReturnTypeDependencies) {
    auto const dependency{types.dependency("FResult", "Project/Result.h")};
    FunctionDefinition spec{.name = ir.named(ir.strings.intern("make")),
                            .return_type = types.intern("FResult", {dependency}),
                            .return_syntax = ReturnSyntax::trailing};
    NodeId const function{ir.declaration(ir.add_function(spec))};

    EXPECT_EQ(CppRenderer{ir}.render(function), "auto make() -> FResult;");
    EXPECT_EQ(collected_dependencies(ir, function), std::vector<TypeDependencyId>{dependency});
}

TEST_F(AstTest, CollectsOpaqueDeclarationDependenciesWithoutIncludingTheBody) {
    auto const requirement{types.dependency("SupportsValue", "Project/Requirement.h")};
    auto const implementation{types.dependency("apply", "Project/Implementation.h")};
    FunctionDefinition spec{.name = ir.named(ir.strings.intern("use")),
                            .return_type = types.std.void_};
    spec.parameters = {ir.parameter(types.intern("T const&"), ir.strings.intern("value"))};
    spec.implementation = {
        .kind = FunctionImplementationKind::body,
        .body = {ir.expr_stmt(ir.call(ir.named("apply", {implementation}), {ir.named("value")}))}};
    spec.template_declaration =
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))});
    spec.requires_clause = ir.template_id(ir.named("SupportsValue", {requirement}),
                                          {ir.type_reference(types.intern("T"))});
    auto const published_spec{ir.add_function(spec)};
    auto const file{ir.files.append(ir.strings.intern("Example.h"),
                                    {ir.include_dependencies(), ir.declaration(published_spec)})};
    auto const header{renderer.render(file)};
    EXPECT_TRUE(header.contains("#include \"Project/Requirement.h\""));
    EXPECT_FALSE(header.contains("Project/Implementation.h"));
    EXPECT_TRUE(header.contains("requires SupportsValue<T>"));

    ir.files.set_nodes(file, {ir.files.nodes(file).front(), ir.definition(published_spec)});
    auto const source{renderer.render(file)};
    expect_contains_all(
        source, {"#include \"Project/Requirement.h\"", "#include \"Project/Implementation.h\""});
    EXPECT_EQ(source.find("Project/Requirement.h"), source.rfind("Project/Requirement.h"));
}

TEST_F(AstTest, RendersQualifiedTrailingReturnAndDefaultedConstructor) {
    FunctionDefinition function{.name = ir.named(ir.strings.intern("qualified_placeholder_return")),
                                .return_type = types.intern("int32"),
                                .return_syntax = ReturnSyntax::trailing,
                                .flags = {.is_constexpr = true}};
    function.receiver = FunctionReceiver::static_;
    expect_rendered(ir.declaration(ir.add_function(function)),
                    "static constexpr auto qualified_placeholder_return() -> int32;");

    FunctionDefinition constructor{.name = ir.named(ir.strings.intern("FValue")),
                                   .kind = FunctionKind::constructor};
    constructor.implementation = {.kind = FunctionImplementationKind::defaulted};
    expect_rendered(ir.definition(ir.add_function(constructor), ir.types.intern("FValue")),
                    "FValue::FValue() = default;");

    auto const dependency{types.dependency("a::make_value", "a/value.h")};
    auto const factory{ir.call(
        ir.type_reference(types.named("a::make_value", NameOrigin::global, {dependency})), {})};
    FunctionDefinition initialized{.name = ir.named(ir.strings.intern("Initialized")),
                                   .kind = FunctionKind::constructor,
                                   .flags = {.define_in_header = true}};
    initialized.parameters = {ir.parameter(ir.types.std.int_, "make_value")};
    initialized.implementation = {
        .kind = FunctionImplementationKind::body,
        .body = {},
        .initializers = {ir.member_initializer(ir.strings.intern("value_"), factory)}};
    auto const published_initialized{ir.add_function(initialized)};
    auto const record{ir.record(ir.strings.intern("Initialized"),
                                {ir.header_function(published_initialized),
                                 ir.instance_member(types.std.int_, ir.strings.intern("value_"))})};
    auto const output{renderer.render(ir.namespace_node(ir.strings.intern("a"), {record}))};
    EXPECT_TRUE(output.contains("value_{a::make_value()}")) << output;
    EXPECT_EQ(collected_dependencies(ir, ir.header_function(published_initialized)),
              std::vector<TypeDependencyId>{dependency});
    EXPECT_TRUE(collected_dependencies(ir, ir.declaration(published_initialized)).empty());
}

TEST_F(AstTest, RejectsOutOfLineDeletedFunction) {
    FunctionDefinition function{.name = ir.named(ir.strings.intern("removed")),
                                .return_type = ir.types.std.void_};
    function.implementation = {.kind = FunctionImplementationKind::deleted};
    EXPECT_THROW(ir.definition(ir.add_function(function), ir.types.intern("FValue")),
                 std::invalid_argument);
}

TEST_F(AstTest, TypeOperationsAreStronglyTyped) {
    auto const type{types.add("FContainer")};
    types.set_operation(type, TypeOperation::remove_at_swap, "remove_at_swap");

    EXPECT_EQ(types.operation(type, TypeOperation::remove_at_swap), "remove_at_swap");
}

TEST_F(AstTest, RendersForwardUsingAndFilePolicyNodes) {
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
    EXPECT_FALSE(rendered.contains("#pragma once"));
    expect_contains_all(rendered,
                        {"// prologue\n\nstruct FValue;\nusing Value = FValue;",
                         "// epilogue\n// clang-format on\n"});
}

TEST_F(AstTest, AppliesStaticAndDefaultArgumentRulesToFunctionForms) {
    FunctionDefinition function{.name = ir.named(ir.strings.intern("make")),
                                .return_type = types.intern("int32"),
                                .flags = {.is_inline = true, .define_in_header = true}};
    function.parameters = {
        ir.parameter(types.intern("int32 const"), ir.strings.intern("value"), ir.literal("7"))};
    function.implementation = {.kind = FunctionImplementationKind::body,
                               .body = {ir.return_stmt(ir.raw_expr("value"))}};
    function.receiver = FunctionReceiver::static_;

    auto const published_function{ir.add_function(function)};
    expect_rendered(
        ir.record(ir.strings.intern("FValue"), {ir.header_function(published_function)}),
        "struct FValue {\n"
        "    static int32 make(int32 const value = 7) {\n"
        "        return value;\n"
        "    }\n"
        "};");
    expect_rendered(ir.definition(published_function, ir.types.intern("FValue")),
                    "int32 FValue::make(int32 const value) {\n"
                    "    return value;\n"
                    "}");
}

TEST_F(AstTest, RendersEmptyEnumsWithoutAnEnumerator) {

    expect_rendered(ir.enumeration(ir.strings.intern("Empty"), {}, {}), "enum class Empty {\n\n};");
    expect_rendered(ir.enumeration(ir.strings.intern("Empty"), {}, {}, {}, false),
                    "enum Empty {\n\n};");
}

TEST_F(AstTest, RendersEnumsAndExportedFreeFunctions) {
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

    expect_rendered(value,
                    "enum class PROJECT_API EMode : uint8 {\n"
                    "    First = 1,\n"
                    "    Second UMETA(DisplayName = \"Second Value\"),\n"
                    "};");
    auto function{FunctionDefinition{.name = ir.named(ir.strings.intern("to_string_view")),
                                     .return_type = types.intern("FStringView"),
                                     .return_syntax = ReturnSyntax::trailing}};
    function.export_specifier = ir.strings.intern("PROJECT_API");
    function.implementation = {.kind = FunctionImplementationKind::body, .body = {}};
    auto const published_function{ir.add_function(function)};
    expect_rendered(ir.declaration(published_function),
                    "PROJECT_API auto to_string_view() -> FStringView;");
    expect_rendered(ir.definition(published_function),
                    "auto to_string_view() -> FStringView {\n\n}");
    expect_rendered(ir.namespace_node(ir.strings.intern(""), {ir.raw("int value;")}),
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
        CompilerTestContext ctx;
        auto& ir{ctx.ir};
        auto& types{ir.types};
        NodeListBuilder members{ir};
        for (auto const name : test.hidden) {
            members.add(ir.instance_member(types.std.int_, ir.strings.intern(name)));
        }
        members.add(ir.instance_member(types.named(test.target, NameOrigin::global),
                                       ir.strings.intern("value")));
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

TEST_F(AstTest, RestoresNamespaceContextAndRetainsAnonymousLookup) {
    auto const type{ir.types.named("a::b::Token", NameOrigin::global)};
    auto const member{ir.variable(type, "value")};
    auto const local{in_namespace(ir, "a::b", ir.namespace_node(ir.strings.intern(""), {member}))};
    auto const output{renderer.render_nodes(std::vector<NodeId>{local, member}, {}, 1)};
    EXPECT_TRUE(output.contains("namespace {\nToken value{};"));
    EXPECT_TRUE(output.ends_with("a::b::Token value{};"));
    expect_rendered(member, "a::b::Token value{};");
    expect_rendered(local, renderer.render(local));
}

TEST_F(AstTest, OmitsImportsIntoTheirOwningNamespaceAndRetainsOtherImports) {
    auto const dependency{ir.types.dependency("a::lookup", "Lookup.h")};
    auto const imported{
        ir.using_import(ir.types.named("a::lookup", NameOrigin::global, {dependency}))};
    expect_rendered(in_namespace(ir, "a", imported), "namespace a {\n\n} // namespace a");
    EXPECT_NE(renderer.render(in_namespace(ir, "b", imported)).find("using a::lookup;"),
              std::string::npos);
    EXPECT_NE(renderer.render(in_namespace(ir, "a::nested", imported)).find("using a::lookup;"),
              std::string::npos);
    EXPECT_EQ(collected_dependencies(ir, imported), std::vector{dependency});
}

TEST_F(AstTest, RetainsTypeStructureAndPoliciesAcrossCompositionAndDependencies) {
    auto const dependency{types.dependency("a::Token", "Token.h")};
    auto const registered{types.add("a::Token", {dependency}, ParameterPassing::value)};
    types.set_operation(registered, TypeOperation::remove_at_swap, "erase");
    auto const named{types.reference(registered)};
    auto const pointer{types.decorate(named, "", " const*")};
    auto const array{types.template_type(types.std.array, {pointer, types.intern("2")})};
    auto const copied{types.with_dependencies(pointer, std::vector{dependency})};
    auto const emit = [&](CppTypeId type) {
        return CppRenderer{ir}.render(in_namespace(ir, "a", ir.variable(type, "value")));
    };
    EXPECT_NE(emit(array).find("std::array<Token const*, 2> value{};"), std::string::npos);
    EXPECT_NE(emit(copied).find("Token const* value{};"), std::string::npos);
    EXPECT_EQ(types.spelling(array), "std::array<a::Token const*, 2>");
    EXPECT_EQ(types.operation(copied, TypeOperation::remove_at_swap), "erase");
    EXPECT_EQ(types.parameter_passing(copied), ParameterPassing::value);
    EXPECT_EQ(collected_dependencies(ir, ir.variable(array, "value")),
              (std::vector{types.dependency("std::array", "array"), dependency}));
    EXPECT_EQ(types.template_type(types.std.array, {pointer, types.intern("2")}), array);
    EXPECT_EQ(types.derive(named, types.spelling(named)), named);
    EXPECT_NE(named, types.intern("a::Token", {dependency}, ParameterPassing::value));
    EXPECT_NE(emit(types.intern("a::Token")).find("a::Token value{};"), std::string::npos);
}

TEST_F(AstTest, PreservesQualificationAcrossOpaqueScopesUnlessBindingsAreProvided) {
    auto const raw{ir.raw("using Hidden = int;")};
    auto const member{ir.instance_member(ir.types.named("a::Token", NameOrigin::global),
                                         ir.strings.intern("value"))};
    auto const record{ir.record(ir.strings.intern("Holder"), {raw, member})};
    auto const wrapped{in_namespace(ir, "a", record)};
    EXPECT_NE(renderer.render(wrapped).find("a::Token value;"), std::string::npos);
    ir.set_scope_bindings(raw, std::vector{ir.strings.intern("Hidden")});
    EXPECT_NE(renderer.render(wrapped).find("\n    Token value;"), std::string::npos);
    ir.set_record_bases(record, {ir.types.intern("ExternalBase")});
    EXPECT_NE(renderer.render(wrapped).find("a::Token value;"), std::string::npos);
}

TEST_F(AstTest, AccountsForReopenedAndAnonymousNamespaceBindings) {
    auto const token{ir.types.named("a::Token", NameOrigin::global)};
    auto const declaration{ir.record(ir.strings.intern("Token"))};
    auto const member{ir.variable(token, "value")};
    for (auto const nested : {false, true}) {
        for (auto const anonymous : {false, true}) {
            auto const binding{anonymous ? ir.namespace_node(ir.strings.intern(""), {declaration})
                                         : static_cast<NodeId>(declaration)};
            std::vector<NodeId> const original{in_namespace(ir, "a::b", binding, nested),
                                               ir.lines(2),
                                               in_namespace(ir, "a::b", member, nested)};
            auto const original_text{renderer.render_nodes(original, {}, 1)};
            auto const file{ir.files.append(ir.strings.intern("Reopened.h"), original)};
            ir.merge_adjacent_namespaces(file);

            auto const output{renderer.render(file)};
            EXPECT_TRUE(output.contains("a::Token value{};")) << output;
            ASSERT_EQ(ir.files.nodes(file).size(), 1);
            auto const merged{ir.files.nodes(file).front()};
            EXPECT_NE(merged, original.front());
            if (nested) {
                std::vector<NodeId> children;
                ir.for_each_child(merged, [&](NodeId child) { children.push_back(child); });
                ASSERT_EQ(children.size(), 1);
            }
            EXPECT_EQ(renderer.render_nodes(original, {}, 1), original_text);

            ir.merge_adjacent_namespaces(file);
            ASSERT_EQ(ir.files.nodes(file).size(), 1);
            EXPECT_EQ(ir.files.nodes(file).front(), merged);
            EXPECT_EQ(renderer.render(file), output);
        }
    }

    for (auto const separator : {ir.line_comment("boundary"),
                                 ir.raw("#pragma once"),
                                 static_cast<NodeId>(declaration),
                                 in_namespace(ir, "other", member)}) {
        std::vector<NodeId> const original{in_namespace(ir, "a", declaration),
                                           ir.lines(2),
                                           separator,
                                           ir.lines(2),
                                           in_namespace(ir, "a", member)};
        auto const file{ir.files.append(ir.strings.intern("Separated.h"), original)};
        ir.merge_adjacent_namespaces(file);
        auto const roots{ir.files.nodes(file)};
        EXPECT_EQ((std::vector<NodeId>{roots.begin(), roots.end()}), original);
    }
}

TEST_F(AstTest, UsesTemplateAndMemberScopesInSignaturesAndExpressions) {
    auto const token{ir.types.named("a::Token", NameOrigin::global)};
    FunctionDefinition make{.name = ir.named("make"), .return_type = token};
    make.parameters = {ir.parameter(token, "argument")};
    make.implementation = {.kind = FunctionImplementationKind::body,
                           .body = {ir.return_stmt(ir.init_list({}, token))}};
    auto const published_make{ir.add_function(make)};
    auto const record{
        ir.record(ir.strings.intern("Holder"),
                  {ir.using_declaration(ir.strings.intern("Token"), ir.types.std.int_),
                   ir.declaration(published_make)})};
    auto const definition{ir.definition(published_make, ir.types.intern("Holder"))};
    auto const output{renderer.render_nodes(
        std::vector<NodeId>{in_namespace(ir, "a", record), in_namespace(ir, "a", definition)},
        {},
        1)};
    expect_contains_all(output,
                        {"a::Token make(a::Token argument);",
                         "Token Holder::make(a::Token argument)",
                         "return a::Token{};"});

    FunctionDefinition template_function{
        .name = ir.named("use"), .return_type = token, .return_syntax = ReturnSyntax::trailing};
    template_function.template_declaration =
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Token"))});
    template_function.implementation = {
        .kind = FunctionImplementationKind::body,
        .body = {ir.variable(token, "value", ir.init_list({}, token)),
                 ir.expr_stmt(ir.sizeof_type(token)),
                 ir.return_stmt(ir.static_cast_expr(token, ir.named("value")))}};
    auto const templated{renderer.render(
        in_namespace(ir, "a", ir.definition(ir.add_function(template_function), {}, {}, true)))};
    for (auto const expected : {"-> a::Token",
                                "a::Token value{a::Token{}};",
                                "sizeof(a::Token)",
                                "static_cast<a::Token>"}) {
        EXPECT_NE(templated.find(expected), std::string::npos) << templated;
    }
}

TEST_F(AstTest, DistinguishesDeclarationsWhenNodesAreReusedAcrossNamespaces) {
    auto const alias{ir.using_declaration(ir.strings.intern("Token"), ir.types.std.int_)};
    auto const record{ir.record(ir.strings.intern("Holder"),
                                {ir.instance_member(ir.types.named("a::Token", NameOrigin::global),
                                                    ir.strings.intern("value"))})};
    auto const output{CppRenderer{ir}.render_nodes(
        std::vector{ir.namespace_node(ir.strings.intern("a"), {alias, record}),
                    ir.namespace_node(ir.strings.intern("b"), {alias, record})},
        {},
        1)};

    EXPECT_TRUE(
        output.contains("namespace a {\nusing Token = int;\n\nstruct Holder {\n    Token value;"))
        << output;
    EXPECT_TRUE(output.contains(
        "namespace b {\nusing Token = int;\n\nstruct Holder {\n    a::Token value;"))
        << output;
}

TEST_F(AstTest, TracksLocalHidingAtTheDeclarationAndRestoresEnclosingLookup) {
    auto const token{ir.types.named("a::Token", NameOrigin::global)};
    FunctionDefinition function{.name = ir.named("use"), .return_type = ir.types.std.void_};
    function.implementation = {
        .kind = FunctionImplementationKind::body,
        .body = {ir.variable(token, "before"),
                 ir.block({ir.variable(ir.types.std.int_, "Token"), ir.variable(token, "inside")}),
                 ir.variable(token, "after"),
                 ir.variable(ir.types.std.int_, "Token"),
                 ir.variable(token, "hidden")}};
    auto const output{
        CppRenderer{ir}.render(in_namespace(ir, "a", ir.definition(ir.add_function(function))))};

    for (auto const expected :
         {"Token before{};", "a::Token inside{};", "Token after{};", "a::Token hidden{};"}) {
        EXPECT_NE(output.find(expected), std::string::npos) << output;
    }
}

TEST_F(AstTest, ResolvesNestedClassOwnersAndForwardDeclaredInjectedNames) {
    auto const token{ir.types.named("a::Token", NameOrigin::global)};
    FunctionDefinition make{.name = ir.named("make"), .return_type = token};
    make.parameters = {ir.parameter(token, "value")};
    make.implementation = {.kind = FunctionImplementationKind::body,
                           .body = {ir.return_stmt(ir.init_list({}, token))}};
    auto const published_make{ir.add_function(make)};
    auto const inner{ir.record(ir.strings.intern("Inner"),
                               {ir.using_declaration(ir.strings.intern("Token"), ir.types.std.int_),
                                ir.declaration(published_make)})};
    auto const outer{ir.record(ir.strings.intern("Outer"), {inner})};
    auto const self{ir.record(
        ir.strings.intern("Self"),
        {ir.instance_member(
            ir.types.decorate(ir.types.named("a::Self", NameOrigin::global), "", " const*"),
            ir.strings.intern("next"))})};
    auto const output{CppRenderer{ir}.render(
        ir.namespace_node(ir.strings.intern("a"),
                          {outer,
                           ir.definition(published_make, ir.types.intern("Outer::Inner")),
                           ir.forward_declaration(ir.strings.intern("Self")),
                           self}))};

    EXPECT_TRUE(output.contains("Token Outer::Inner::make(a::Token value)")) << output;
    EXPECT_TRUE(output.contains("return a::Token{};")) << output;
    EXPECT_TRUE(output.contains("Self const* next;")) << output;
}

TEST_F(AstTest, KeepsTemplateHeadersIndependentOfFunctionParameters) {
    auto const token{ir.types.named("a::Token", NameOrigin::global)};
    FunctionDefinition function{
        .name = ir.named("use"), .return_type = token, .return_syntax = ReturnSyntax::trailing};
    function.template_declaration =
        ir.template_declaration({ir.value_template_parameter(token, ir.strings.intern("N"))});
    function.parameters = {ir.parameter(token, "Token"), ir.parameter(token, "other")};
    auto const output{
        CppRenderer{ir}.render(in_namespace(ir, "a", ir.declaration(ir.add_function(function))))};

    EXPECT_TRUE(output.contains("template <Token N>")) << output;
    EXPECT_TRUE(output.contains("use(Token Token, a::Token other) -> a::Token;")) << output;
}

TEST_F(AstTest, PreservesQualificationForAuthoredRecordNames) {
    auto const record{ir.record(ir.raw_expr("Holder"),
                                {ir.instance_member(ir.types.named("a::Token", NameOrigin::global),
                                                    ir.strings.intern("value"))})};

    EXPECT_NE(CppRenderer{ir}.render(in_namespace(ir, "a", record)).find("a::Token value;"),
              std::string::npos);
}

} // namespace
} // namespace lispb::codegen
