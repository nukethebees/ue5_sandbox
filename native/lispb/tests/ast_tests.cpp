#include "compiler_test_context.h"
#include "test_utilities.h"

#include <codegen/ast/access.h>
#include <codegen/ast/access_indentation.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_file_flags.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
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
    !std::is_invocable_v<decltype(&CppIR::switch_case), CppIR&, std::optional<Expr>, NodeId>);

TEST(Ast, NodeListBuilderComposesSectionsAndSpacing) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    NodeListBuilder section{ir};
    section.add(ir.instance_member(types.intern("int32"), ir.strings.intern("first")), 1)
        .add(ir.instance_member(types.intern("int32"), ir.strings.intern("second")));

    NodeListBuilder builder{ir};
    auto nodes{builder.add(ir.access(Access::public_), 1)
                   .append(section.build())
                   .new_lines(2)
                   .add(ir.instance_member(types.intern("int32"), ir.strings.intern("third")))
                   .build()};

    EXPECT_EQ(ir.render_nodes(nodes, {}, 0),
              "public:\nint32 first;\nint32 second;\n\nint32 third;");
}

TEST(Ast, RendersFriendDeclarations) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    EXPECT_EQ(ir.render(ir.friend_declaration(ir.strings.intern("FOwner"))),
              "friend class FOwner;");
    EXPECT_EQ(ir.render(ir.friend_declaration(ir.strings.intern("FValue"), RecordKind::struct_)),
              "friend struct FValue;");
}

TEST(Ast, RendersComments) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    EXPECT_EQ(ir.render(ir.line_comment("Lifetime")), "// Lifetime");
    EXPECT_EQ(ir.render(ir.block_comment("****************************************")),
              "/* **************************************** */");
}

TEST(Ast, RendersStaticAssertionsWithOptionalMessages) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    NodeId const without_message{ir.static_assertion(ir.store_text("sizeof(Value) == 16"), {})};
    NodeId const with_message{ir.static_assertion(ir.store_text("valid<Value>"),
                                                  ir.store_text("Value must be \"valid\"."))};

    EXPECT_EQ(ir.render(without_message), "static_assert(sizeof(Value) == 16);");
    EXPECT_EQ(ir.render(with_message),
              "static_assert(valid<Value>, \"Value must be \\\"valid\\\".\");");
}

TEST(Ast, RendersTypedStmts) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    EXPECT_EQ(ir.render(ir.expression_statement(ir.raw_expression("apply(value)"))),
              "apply(value);");
    EXPECT_EQ(ir.render(ir.return_statement(ir.raw_expression("value"))), "return value;");
    EXPECT_EQ(ir.render(ir.return_statement()), "return;");
    EXPECT_EQ(ir.render(ir.assignment(ir.raw_expression("target"), ir.raw_expression("value"))),
              "target = value;");
    EXPECT_EQ(ir.render(ir.variable(types.intern("auto const"),
                                    ir.strings.intern("count"),
                                    ir.raw_expression("values.Num()"))),
              "auto const count{values.Num()};");
}

TEST(Ast, TypedStmtsReportDependencies) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const dependency{types.dependency("FValue", "Project/Value.h")};

    EXPECT_EQ(collected_dependencies(
                  ir, ir.expression_statement(ir.raw_expression("use_value()"), {dependency})),
              std::vector<TypeDependencyId>{dependency});
    EXPECT_EQ(collected_dependencies(
                  ir, ir.return_statement(ir.raw_expression("FValue{}"), {dependency})),
              std::vector<TypeDependencyId>{dependency});
    EXPECT_EQ(collected_dependencies(ir,
                                     ir.assignment(ir.raw_expression("target"),
                                                   ir.raw_expression("FValue{}"),
                                                   {dependency})),
              std::vector<TypeDependencyId>{dependency});
    EXPECT_EQ(collected_dependencies(ir,
                                     ir.variable(types.intern("FValue", {dependency}),
                                                 ir.strings.intern("value"),
                                                 ir.raw_expression("make_value()"))),
              std::vector<TypeDependencyId>{dependency});
}

TEST(Ast, RendersAccessSpecifiersWithExplicitIndentation) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const render_context{RenderContext{.indent_level = 1, .indent_text = "    "}};

    EXPECT_EQ(ir.render(ir.access(Access::public_), render_context), "  public:");
    EXPECT_EQ(ir.render(ir.access(Access::private_, AccessIndentation::normal), render_context),
              "    private:");
    EXPECT_EQ(ir.render(ir.access(Access::private_, AccessIndentation::normal),
                        RenderContext{.indent_level = 1, .indent_text = "\t"}),
              "\tprivate:");
}

TEST(Ast, RendersBraceInitialisedTree) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const file{ir.files.append(
        ir.strings.add("Example.h"),
        {
            ir.include(ir.store_text("Example/Dependency.h"), IncludeStyle::quoted),
            ir.lines(2),
            ir.namespace_node(
                ir.strings.intern("example"),
                {

                    ir.record(
                        ir.strings.intern("FValue"),
                        {
                            ir.instance_member(types.intern("int32"), ir.strings.intern("value")),
                            ir.header_function([&] {
                                auto const created_function{ir.function(ir.strings.intern("g"
                                                                                          "e"
                                                                                          "t"),
                                                                        types.intern("auto"))};
                                ir.set_function_body(created_function, {ir.raw("return value;")});
                                ir.set_function_return_type(created_function,
                                                            types.intern("int32"),
                                                            ReturnSyntax::trailing);
                                ir.set_function_receiver(created_function,
                                                         FunctionReceiver::const_);
                                ir.function_flags(created_function).set_is_inline(true);
                                ir.function_flags(created_function).set_define_in_header(true);
                                return created_function;
                            }()),
                        },
                        RecordKind::struct_),
                }),
        })};

    EXPECT_EQ(ir.render(file), R"(// This file is autogenerated. Do not edit by hand.
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
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const element{types.dependency("FElement", "Project/Element.h")};
    auto const array{types.intern(
        "TArray<FElement>", {types.dependency("TArray<FElement>", "Containers/Array.h"), element})};
    auto const file{
        ir.files.append(ir.strings.add("Example.h"),
                        {
                            ir.include_dependencies(),
                            ir.lines(2),
                            ir.record(ir.strings.intern("FData"),
                                      {ir.instance_member(array, ir.strings.intern("first")),
                                       ir.instance_member(array, ir.strings.intern("second"))},
                                      RecordKind::struct_),
                        })};
    ir.files.set_include_order(file, {ir.strings.add("Project/")});

    auto const rendered{ir.render(file)};
    EXPECT_EQ(rendered.find("Project/Element.h"), rendered.rfind("Project/Element.h"));
    EXPECT_EQ(rendered.find("Containers/Array.h"), rendered.rfind("Containers/Array.h"));
    EXPECT_LT(rendered.find("Project/Element.h"), rendered.find("Containers/Array.h"));
}

TEST(Ast, SortsDependencyIncludesWithinConfiguredGroups) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const file{ir.files.append(
        ir.strings.add("Example.h"),
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
    ir.files.set_include_order(file, {ir.strings.add("Project/")});

    auto const rendered{ir.render(file)};
    EXPECT_LT(rendered.find("Project/A.h"), rendered.find("Project/Z.h"));
    EXPECT_LT(rendered.find("Project/Z.h"), rendered.find("Containers/Array.h"));
}

TEST(Ast, RendersExplicitIncludesAtTheirNodeLocation) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const file{
        ir.files.append(ir.strings.add("Example.cpp"),
                        {
                            ir.include(ir.store_text("Project/Explicit.h"), IncludeStyle::quoted),
                            ir.lines(2),
                            ir.raw("int value;"),
                        })};
    ir.files.flags[ir.files.index(file)].set_pragma_once(false);

    auto const rendered{ir.render(file)};
    EXPECT_LT(rendered.find("Project/Explicit.h"), rendered.find("int value;"));
    EXPECT_EQ(rendered.find("Project/Explicit.h"), rendered.rfind("Project/Explicit.h"));
}

TEST(Ast, InfersSystemIncludesFromExtensionlessPaths) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    EXPECT_TRUE(include_is_system("vector", IncludeStyle::inferred));
    EXPECT_FALSE(include_is_system("Project/Value.h", IncludeStyle::inferred));
    EXPECT_FALSE(include_is_system("vector", IncludeStyle::quoted));
    EXPECT_TRUE(include_is_system("Project/Value.h", IncludeStyle::system));
}

TEST(Ast, VisitsChildrenForNestedAndDefinedNodes) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const structure{
        ir.record(ir.strings.intern("FData"), {ir.raw("int value;")}, RecordKind::struct_)};
    NodeId const name_space{
        ir.namespace_node(ir.strings.intern("example"), {ir.raw("int value;")})};
    NodeId const function{ir.definition(
        [&] {
            auto const created_function{ir.function(ir.strings.intern("f"), types.intern("void"))};
            ir.set_function_body(created_function, {ir.raw("return;")});
            return created_function;
        }(),
        {})};
    NodeId const declaration_node{ir.declaration([&] {
        auto const created_function{ir.function(ir.strings.intern("f"), types.intern("void"))};
        return created_function;
    }())};
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
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const rendered{ir.render([&] {
        auto const created_record{ir.record(
            ir.strings.intern("FDerived"),
            {
                ir.instance_member(
                    types.intern("int"), ir.strings.intern("value"), ir.raw_expression("42")),
                ir.constexpr_member(
                    types.intern("int"), ir.strings.intern("answer"), ir.raw_expression("42")),
                ir.constexpr_member(types.intern("bool"),
                                    ir.strings.intern("supports_value"),
                                    ir.template_id(ir.named("std::is_constructible_v"),
                                                   {ir.type_reference(types.intern("int")),
                                                    ir.type_reference(types.intern("TArg"))}),
                                    true,
                                    ir.template_declaration(
                                        {ir.type_template_parameter(ir.strings.intern("TArg"))})),
            },
            RecordKind::class_)};
        ir.set_record_bases(created_record, {types.intern("FBase")});
        ir.set_record_export_specifier(created_record, ir.strings.intern("PROJECT_API"));
        return created_record;
    }())};

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
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const spec{[&] {
        auto const created_function{ir.function(ir.strings.intern("append"), types.intern("void"))};
        ir.set_function_parameters(
            created_function,
            {ir.parameter(types.intern("Other const&"), ir.strings.intern("other"))});
        ir.set_function_body(created_function, {ir.raw("append_from(other);")});
        ir.function_flags(created_function).set_is_inline(true);
        ir.function_flags(created_function).set_define_in_header(true);
        ir.set_function_template(
            created_function,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}));
        ir.set_function_requires_clause(
            created_function,
            ir.template_id(ir.named("Appendable"), {ir.type_reference(types.intern("Other"))}));
        return created_function;
    }()};

    auto const rendered{ir.render(ir.header_function(spec))};
    EXPECT_NE(rendered.find("template <typename Other>"), std::string::npos);
    EXPECT_NE(rendered.find("requires Appendable<Other>"), std::string::npos);
    EXPECT_NE(rendered.find("append_from(other);"), std::string::npos);
}

TEST(Ast, RendersTypedFunctionFormattingPolicies) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto compact{[&] {
        auto const created_function{ir.function(ir.strings.intern("apply"), types.intern("void"))};
        ir.set_function_body(created_function, {ir.raw("func();")});
        ir.function_flags(created_function).set_is_inline(true);
        ir.function_flags(created_function).set_define_in_header(true);
        ir.set_function_template(
            created_function,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))}));
        ir.set_function_requires_clause(
            created_function,
            ir.template_id(ir.named("Invocable"), {ir.type_reference(types.intern("TFunc"))}));
        ir.function_formatting(created_function) = FunctionFormatting{
            .body_layout = FunctionBodyLayout::compact,
            .requires_placement = FunctionRequiresPlacement::trailing_same_line,
            .template_placement = FunctionTemplatePlacement::same_line,
        };
        return created_function;
    }()};
    EXPECT_EQ(ir.render(ir.header_function(compact), {.record_scope = true}),
              "template <typename TFunc> void apply() requires Invocable<TFunc> { func(); }");

    auto prefixed{compact};
    ir.function_formatting(prefixed) = FunctionFormatting{
        .requires_placement = FunctionRequiresPlacement::before_signature,
    };
    EXPECT_EQ(ir.render(ir.header_function(prefixed), {.record_scope = true}),
              "template <typename TFunc>\n"
              "    requires Invocable<TFunc>\n"
              "void apply() {\n"
              "    func();\n"
              "}");

    auto separate_brace{compact};
    ir.set_function_template(separate_brace, {});
    ir.function_formatting(separate_brace) = FunctionFormatting{
        .opening_brace_placement = FunctionOpeningBracePlacement::separate_line,
    };
    EXPECT_EQ(ir.render(ir.header_function(separate_brace), {.record_scope = true}),
              "void apply()\n"
              "    requires Invocable<TFunc>\n"
              "{\n"
              "    func();\n"
              "}");
}

TEST(Ast, RendersConstrainedStructuresAndSpecialMembers) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const dependency{types.dependency("check", "CoreMinimal.h")};
    auto const structure{[&] {
        auto const created_record{ir.record(
            ir.strings.intern("TFixedData"),
            {
                ir.declaration([&] {
                    auto const created_function{ir.constructor(ir.strings.intern("TFixedData"))};
                    ir.set_function_noexcept(created_function);
                    ir.set_function_defaulted(created_function);
                    return created_function;
                }()),
                ir.lines(1),
                ir.header_function([&] {
                    auto const created_function{
                        ir.function(ir.strings.intern("apply"), types.intern("void"))};
                    ir.set_function_parameters(
                        created_function,
                        {ir.parameter(types.intern("TFunc&&"), ir.strings.intern("func"))});
                    ir.set_function_body(created_function, {ir.raw("func();")});
                    ir.function_flags(created_function).set_is_inline(true);
                    ir.function_flags(created_function).set_define_in_header(true);
                    ir.set_function_template(created_function,
                                             ir.template_declaration({ir.type_template_parameter(
                                                 ir.strings.intern("TFunc"))}));
                    ir.function_formatting(created_function) = FunctionFormatting{
                        .body_layout = FunctionBodyLayout::compact,
                        .template_placement = FunctionTemplatePlacement::same_line,
                    };
                    return created_function;
                }()),
                ir.lines(2),
                ir.access(Access::private_),
            },
            RecordKind::struct_)};
        ir.set_record_template(created_record,
                               ir.template_declaration({ir.value_template_parameter(
                                   types.intern("int"), ir.strings.intern("Capacity"))}));
        ir.set_record_requires_clause(
            created_record,
            ir.parenthesized(
                ir.binary(BinaryOperator::greater_equal, ir.named("Capacity"), ir.literal("0"))));
        ir.set_record_dependencies(created_record, {dependency});
        return created_record;
    }()};

    EXPECT_EQ(ir.render(structure),
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
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const dependency{types.dependency("FValue", "Project/Value.h")};
    NodeId const node{ir.raw("use_value();", {dependency})};

    EXPECT_EQ(collected_dependencies(ir, node), std::vector<TypeDependencyId>{dependency});
}

TEST(Ast, RendersDeclarationsAndOutOfLineDefinitionsFromOneSpec) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const spec{[&] {
        auto const created_function{ir.function(ir.strings.intern("set"), types.intern("void"))};
        ir.set_function_parameters(created_function,
                                   {ir.parameter(types.intern("int32 const"),
                                                 ir.strings.intern("value"),
                                                 ir.store_text("0"))});
        ir.set_function_body(created_function, {ir.raw("stored = value;")});
        ir.set_function_noexcept(created_function);
        return created_function;
    }()};

    auto const function{spec};
    EXPECT_EQ(ir.render(ir.declaration(function)), "void set(int32 const value = 0) noexcept;");
    EXPECT_EQ(ir.render(ir.definition(function, ir.store_text("FValue"))),
              "void FValue::set(int32 const value) noexcept {\n"
              "    stored = value;\n"
              "}");
}

TEST(Ast, CollectsTrailingReturnTypeDependencies) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const dependency{types.dependency("FResult", "Project/Result.h")};
    NodeId const function{ir.declaration([&] {
        auto const created_function{ir.function(ir.strings.intern("make"), types.intern("auto"))};
        ir.set_function_return_type(
            created_function, types.intern("FResult", {dependency}), ReturnSyntax::trailing);
        return created_function;
    }())};

    EXPECT_EQ(ir.render(function), "auto make() -> FResult;");
    EXPECT_EQ(collected_dependencies(ir, function), std::vector<TypeDependencyId>{dependency});
}

TEST(Ast, CollectsOpaqueDeclarationDependenciesWithoutIncludingTheBody) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const requirement{types.dependency("SupportsValue", "Project/Requirement.h")};
    auto const implementation{types.dependency("apply", "Project/Implementation.h")};
    auto const spec{[&] {
        auto const created_function{ir.function(ir.strings.intern("use"), types.intern("void"))};
        ir.set_function_parameters(
            created_function, {ir.parameter(types.intern("T const&"), ir.strings.intern("value"))});
        ir.set_function_body(created_function,
                             {ir.expression_statement(ir.call(ir.named("apply", {implementation}),
                                                              {ir.named("value")}))});
        ir.set_function_template(
            created_function,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))}));
        ir.set_function_requires_clause(created_function,
                                        ir.template_id(ir.named("SupportsValue", {requirement}),
                                                       {ir.type_reference(types.intern("T"))}));
        return created_function;
    }()};
    auto const file{ir.files.append(ir.strings.add("Example.h"),
                                    {ir.include_dependencies(), ir.declaration(spec)})};
    auto const header{ir.render(file)};
    EXPECT_NE(header.find("#include \"Project/Requirement.h\""), std::string::npos);
    EXPECT_EQ(header.find("Project/Implementation.h"), std::string::npos);
    EXPECT_NE(header.find("requires SupportsValue<T>"), std::string::npos);

    ir.files.set_nodes(file, {ir.files.nodes(file).front(), ir.definition(spec)});
    auto const source{ir.render(file)};
    EXPECT_NE(source.find("#include \"Project/Requirement.h\""), std::string::npos);
    EXPECT_NE(source.find("#include \"Project/Implementation.h\""), std::string::npos);
    EXPECT_EQ(source.find("Project/Requirement.h"), source.rfind("Project/Requirement.h"));
}

TEST(Ast, RendersQualifiedTrailingReturnAndDefaultedConstructor) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto& types{context.storage.cpp_types};
    auto const function{
        ir.function(ir.strings.intern("qualified_placeholder_return"), types.intern("int32"))};
    ir.set_function_return_type(function, types.intern("int32"), ReturnSyntax::trailing);
    ir.set_function_receiver(function, FunctionReceiver::static_);
    ir.function_flags(function).set_is_constexpr(true);
    EXPECT_EQ(ir.render(ir.declaration(function)),
              "static constexpr auto qualified_placeholder_return() -> int32;");

    auto const constructor{ir.constructor(ir.strings.intern("FValue"))};
    ir.set_function_defaulted(constructor);
    EXPECT_EQ(ir.render(ir.definition(constructor, ir.store_text("FValue"))),
              "FValue::FValue() = default;");
}

TEST(Ast, RejectsOutOfLineDeletedFunction) {
    CompilerTestContext context;
    auto& ir{context.ir};
    auto const function{
        ir.function(ir.strings.intern("removed"), context.storage.cpp_types.intern("void"))};
    ir.set_function_deleted(function);
    EXPECT_THROW(ir.definition(function, ir.store_text("FValue")), std::invalid_argument);
}

TEST(Ast, TypeOperationsAreStronglyTyped) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const type{types.add("FContainer")};
    types.set_operation(type, TypeOperation::remove_at_swap, "remove_at_swap");

    EXPECT_EQ(types.operation(type, TypeOperation::remove_at_swap), "remove_at_swap");
}

TEST(Ast, RendersForwardUsingAndFilePolicyNodes) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const file{ir.files.append(
        ir.strings.add("Example.cpp"),
        {
            ir.forward_declaration(ir.strings.intern("FValue"), RecordKind::struct_),
            ir.lines(1),
            ir.using_declaration(ir.strings.intern("Value"), types.intern("FValue")),
        })};
    ir.files.flags[ir.files.index(file)].set_pragma_once(false);
    ir.files.flags[ir.files.index(file)].set_clang_format_off(true);
    ir.files.set_prologue(file, {ir.strings.add("// prologue")});
    ir.files.set_epilogue(file, {ir.strings.add("// epilogue")});

    auto const rendered{ir.render(file)};
    EXPECT_TRUE(rendered.starts_with("// clang-format off\n"));
    EXPECT_EQ(rendered.find("#pragma once"), std::string::npos);
    EXPECT_NE(rendered.find("// prologue\n\nstruct FValue;\nusing Value = FValue;"),
              std::string::npos);
    EXPECT_NE(rendered.find("// epilogue\n// clang-format on\n"), std::string::npos);
}

TEST(Ast, AppliesStaticAndDefaultArgumentRulesToFunctionForms) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
    auto const spec{[&] {
        auto const created_function{ir.function(ir.strings.intern("make"), types.intern("int32"))};
        ir.set_function_parameters(created_function,
                                   {ir.parameter(types.intern("int32 const"),
                                                 ir.strings.intern("value"),
                                                 ir.store_text("7"))});
        ir.set_function_body(created_function, {ir.return_statement(ir.raw_expression("value"))});
        ir.set_function_receiver(created_function, FunctionReceiver::static_);
        ir.function_flags(created_function).set_is_inline(true);
        ir.function_flags(created_function).set_define_in_header(true);
        return created_function;
    }()};

    auto const function{spec};
    EXPECT_EQ(ir.render(ir.header_function(function), {.record_scope = true}),
              "static int32 make(int32 const value = 7) {\n"
              "    return value;\n"
              "}");
    EXPECT_EQ(ir.render(ir.definition(function, ir.store_text("FValue"))),
              "int32 FValue::make(int32 const value) {\n"
              "    return value;\n"
              "}");
}

TEST(Ast, RendersEnumsAndExportedFreeFunctions) {
    CompilerTestContext context;
    [[maybe_unused]] auto& ir{context.ir};
    [[maybe_unused]] auto& types{context.storage.cpp_types};
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

    EXPECT_EQ(ir.render(value),
              "enum class PROJECT_API EMode : uint8 {\n"
              "    First = 1,\n"
              "    Second UMETA(DisplayName = \"Second Value\"),\n"
              "};");
    auto const spec{[&] {
        auto const created_function{
            ir.function(ir.strings.intern("to_string_view"), types.intern("auto"))};
        ir.set_function_return_type(
            created_function, types.intern("FStringView"), ReturnSyntax::trailing);
        ir.set_function_export_specifier(created_function, ir.strings.intern("PROJECT_API"));
        return created_function;
    }()};
    auto const function{spec};
    EXPECT_EQ(ir.render(ir.declaration(function)),
              "PROJECT_API auto to_string_view() -> FStringView;");
    EXPECT_EQ(ir.render(ir.definition(spec)), "auto to_string_view() -> FStringView {\n\n}");
    EXPECT_EQ(ir.render(ir.namespace_node(ir.strings.intern(""), {ir.raw("int value;")})),
              "namespace {\nint value;\n} // namespace");
}

} // namespace
} // namespace lispb::codegen
