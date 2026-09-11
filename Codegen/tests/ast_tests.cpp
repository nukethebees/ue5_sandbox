#include <codegen/ast.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace codegen {
namespace {

TEST(Ast, NodeProvidesVariantInterface) {
    Node node{Raw{"initial", {}}};

    EXPECT_TRUE(node.is<Raw>());
    ASSERT_NE(node.get_if<Raw>(), nullptr);
    EXPECT_EQ(node.get_if<Raw>()->text, "initial");
    EXPECT_EQ(std::visit([](auto const& value) { return sizeof(value); }, node), sizeof(Raw));

    node = NewLines{2};

    EXPECT_TRUE(node.is<NewLines>());
    EXPECT_EQ(node.get_if<NewLines>()->count, 2);
    EXPECT_EQ(node.get_if<Raw>(), nullptr);
}

TEST(Ast, NodeListBuilderComposesSectionsAndSpacing) {
    NodeListBuilder section;
    section.add(Member{"int32", "first"}, 1).add(Member{"int32", "second"});

    NodeListBuilder builder;
    auto nodes{builder.add(AccessSpecifier{"public"}, 1)
                   .append(section.build())
                   .new_lines(2)
                   .add(Member{"int32", "third"})
                   .build()};

    ASSERT_EQ(nodes.size(), 7);
    EXPECT_TRUE(nodes[0].is<AccessSpecifier>());
    EXPECT_EQ(nodes[1].get_if<NewLines>()->count, 1);
    EXPECT_EQ(nodes[2].get_if<Member>()->name, "first");
    EXPECT_EQ(nodes[3].get_if<NewLines>()->count, 1);
    EXPECT_EQ(nodes[4].get_if<Member>()->name, "second");
    EXPECT_EQ(nodes[5].get_if<NewLines>()->count, 2);
    EXPECT_EQ(nodes[6].get_if<Member>()->name, "third");
}

TEST(Ast, RendersFriendDeclarations) {
    EXPECT_EQ(render(Node{FriendDeclaration{"FOwner"}}), "friend class FOwner;");
    EXPECT_EQ(render(Node{FriendDeclaration{"FValue", "struct"}}), "friend struct FValue;");
}

TEST(Ast, RendersComments) {
    EXPECT_EQ(render(Node{LineComment{"Lifetime"}}), "// Lifetime");
    EXPECT_EQ(render(Node{BlockComment{"****************************************"}}),
              "/* **************************************** */");
}

TEST(Ast, RendersStaticAssertionsWithOptionalMessages) {
    Node const without_message{StaticAssert{"sizeof(Value) == 16", {}}};
    Node const with_message{StaticAssert{"valid<Value>", "Value must be \"valid\"."}};

    EXPECT_EQ(render(without_message), "static_assert(sizeof(Value) == 16);");
    EXPECT_EQ(render(with_message),
              "static_assert(valid<Value>, \"Value must be \\\"valid\\\".\");");
}

TEST(Ast, RendersTypedStmts) {
    EXPECT_EQ(render(Node{ExpressionStmt{RawExpr{"apply(value)"}}}), "apply(value);");
    EXPECT_EQ(render(Node{ReturnStmt{RawExpr{"value"}}}), "return value;");
    EXPECT_EQ(render(Node{ReturnStmt{}}), "return;");
    EXPECT_EQ(render(Node{AssignmentStmt{RawExpr{"target"}, RawExpr{"value"}}}), "target = value;");
    EXPECT_EQ(render(Node{VariableDeclarationStmt{"auto const", "count", RawExpr{"values.Num()"}}}),
              "auto const count{values.Num()};");
}

TEST(Ast, TypedStmtsReportDependencies) {
    TypeDependency const dependency{"FValue", "Project/Value.h", {}};

    EXPECT_EQ(dependencies(Node{ExpressionStmt{RawExpr{"use_value()"}, {dependency}}}),
              std::vector<TypeDependency>{dependency});
    EXPECT_EQ(dependencies(Node{ReturnStmt{RawExpr{"FValue{}"}, {dependency}}}),
              std::vector<TypeDependency>{dependency});
    EXPECT_EQ(
        dependencies(Node{AssignmentStmt{RawExpr{"target"}, RawExpr{"FValue{}"}, {dependency}}}),
        std::vector<TypeDependency>{dependency});
    EXPECT_EQ(dependencies(Node{VariableDeclarationStmt{
                  CppType{"FValue", {dependency}}, "value", RawExpr{"make_value()"}}}),
              std::vector<TypeDependency>{dependency});
}

TEST(Ast, RendersAccessSpecifiersWithExplicitIndentation) {
    auto const context{RenderContext{.indent_level = 1, .indent_text = "    "}};

    EXPECT_EQ(render(Node{AccessSpecifier{"public"}}, context), "  public:");
    EXPECT_EQ(
        render(Node{AccessSpecifier{"private", AccessSpecifier::Indentation::normal}}, context),
        "    private:");
}

TEST(Ast, RendersBraceInitialisedTree) {
    CppFile const file{
        .path = std::filesystem::path{"Example.h"},
        .nodes =
            {
                Include{"Example/Dependency.h", false},
                lines(2),
                Namespace{
                    "example",
                    {
                        Struct{
                            .name = "FValue",
                            .children =
                                {
                                    Member{"int32", "value"},
                                    header_function(FunctionSpec{
                                        .name = "get",
                                        .return_type = "auto",
                                        .body = {raw("return value;")},
                                        .qualifiers =
                                            {
                                                .trailing_return_type = CppType{"int32"},
                                                .is_const = true,
                                            },
                                        .is_inline = true,
                                    }),
                                },
                        },
                    },
                },
            },
    };

    EXPECT_EQ(render(file), R"(// This file is autogenerated. Do not edit by hand.
// Edit Codegen/manifests and regenerate it instead.

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
    TypeDependency const element{"FElement", "Project/Element.h", {}};
    CppType const array{
        "TArray<FElement>",
        {TypeDependency{"TArray<FElement>", "Containers/Array.h", {element}}},
    };
    CppFile const file{
        .path = "Example.h",
        .nodes =
            {
                IncludeDependencies{},
                lines(2),
                Struct{
                    .name = "FData",
                    .children = {Member{array, "first"}, Member{array, "second"}},
                },
            },
        .include_order = {"Project/"},
    };

    auto const rendered{render(file)};
    EXPECT_EQ(rendered.find("Project/Element.h"), rendered.rfind("Project/Element.h"));
    EXPECT_EQ(rendered.find("Containers/Array.h"), rendered.rfind("Containers/Array.h"));
    EXPECT_LT(rendered.find("Project/Element.h"), rendered.find("Containers/Array.h"));
}

TEST(Ast, SortsDependencyIncludesWithinConfiguredGroups) {
    CppFile const file{
        .path = "Example.h",
        .nodes =
            {
                IncludeDependencies{},
                Member{CppType{"FZ", "Project/Z.h"}, "z"},
                Member{CppType{"FA", "Project/A.h"}, "a"},
                Member{CppType{"TArray<int32>", "Containers/Array.h"}, "values"},
            },
        .include_order = {"Project/"},
    };

    auto const rendered{render(file)};
    EXPECT_LT(rendered.find("Project/A.h"), rendered.find("Project/Z.h"));
    EXPECT_LT(rendered.find("Project/Z.h"), rendered.find("Containers/Array.h"));
}

TEST(Ast, RendersExplicitIncludesAtTheirNodeLocation) {
    CppFile const file{
        .path = "Example.cpp",
        .nodes =
            {
                Include{"Project/Explicit.h", false},
                lines(2),
                raw("int value;"),
            },
        .pragma_once = false,
    };

    auto const rendered{render(file)};
    EXPECT_LT(rendered.find("Project/Explicit.h"), rendered.find("int value;"));
    EXPECT_EQ(rendered.find("Project/Explicit.h"), rendered.rfind("Project/Explicit.h"));
}

TEST(Ast, InfersSystemIncludesFromExtensionlessPaths) {
    EXPECT_TRUE(include_is_system(Include{"vector", std::nullopt}));
    EXPECT_FALSE(include_is_system(Include{"Project/Value.h", std::nullopt}));
    EXPECT_FALSE(include_is_system(Include{"vector", false}));
    EXPECT_TRUE(include_is_system(Include{"Project/Value.h", true}));
}

TEST(Ast, VisitsChildrenForNestedAndDefinedNodes) {
    Node const structure{Struct{.name = "FData", .children = {raw("int value;")}}};
    Node const name_space{Namespace{"example", {raw("int value;")}}};
    Node const function{Function{.spec = FunctionSpec{.name = "f", .body = {raw("return;")}}}};
    Node const declaration_node{Function{.spec = FunctionSpec{.name = "f"}, .declaration = true}};
    Node const leaf{raw("int value;")};

    auto child_count = [](Node const& node) {
        std::size_t count{};
        for_each_child(node, [&](Node const&) { ++count; });
        return count;
    };
    EXPECT_EQ(child_count(structure), 1);
    EXPECT_EQ(child_count(name_space), 1);
    EXPECT_EQ(child_count(function), 1);
    EXPECT_EQ(child_count(declaration_node), 0);
    EXPECT_EQ(child_count(leaf), 0);
}

TEST(Ast, RendersClassInheritanceAndMemberInitializers) {
    auto const rendered{render(Node{Struct{
        .name = "FDerived",
        .children =
            {
                Member{"int", "value", RawExpr{"42"}},
                Member{"int", "answer", RawExpr{"42"}, {.is_static = true, .is_constexpr = true}},
                Member{"bool",
                       "supports_value",
                       RawExpr{"std::is_constructible_v<int, TArg>"},
                       {.is_inline = true, .is_static = true, .is_constexpr = true},
                       "typename TArg"},
            },
        .bases = {CppType{"FBase"}},
        .export_specifier = "PROJECT_API",
        .record_kind = "class",
    }})};

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

TEST(Ast, RejectsInvalidMemberQualifierCombinations) {
    EXPECT_THROW(render(Node{Member{"int", "value", RawExpr{"42"}, {.is_constexpr = true}}}),
                 std::invalid_argument);
    EXPECT_THROW(render(Node{Member{
                     "int", "value", std::nullopt, {.is_static = true, .is_constexpr = true}}}),
                 std::invalid_argument);
    EXPECT_THROW(render(Node{Member{"int", "value", RawExpr{"42"}, {.is_inline = true}}}),
                 std::invalid_argument);
}

TEST(Ast, RendersTemplatedConstrainedFunctions) {
    FunctionSpec const spec{
        .name = "append",
        .return_type = "void",
        .parameters = {FunctionParameter{"Other const&", "other"}},
        .body = {raw("append_from(other);")},
        .is_inline = true,
        .template_parameters = "typename Other",
        .requires_clause = "Appendable<Other>",
    };

    auto const rendered{render(header_function(spec))};
    EXPECT_NE(rendered.find("template <typename Other>"), std::string::npos);
    EXPECT_NE(rendered.find("requires Appendable<Other>"), std::string::npos);
    EXPECT_NE(rendered.find("append_from(other);"), std::string::npos);
}

TEST(Ast, RendersTypedFunctionFormattingPolicies) {
    auto compact{FunctionSpec{
        .name = "apply",
        .return_type = "void",
        .body = {raw("func();")},
        .is_inline = true,
        .template_parameters = "typename TFunc",
        .requires_clause = "Invocable<TFunc>",
        .formatting =
            FunctionFormatting{
                .body_layout = FunctionFormatting::BodyLayout::compact,
                .requires_placement = FunctionFormatting::RequiresPlacement::trailing_same_line,
                .template_placement = FunctionFormatting::TemplatePlacement::same_line,
            },
    }};
    EXPECT_EQ(render(header_function(compact)),
              "template <typename TFunc> void apply() requires Invocable<TFunc> { func(); }");

    auto prefixed{compact};
    prefixed.formatting = FunctionFormatting{
        .requires_placement = FunctionFormatting::RequiresPlacement::before_signature,
    };
    EXPECT_EQ(render(header_function(prefixed)),
              "template <typename TFunc>\n"
              "    requires Invocable<TFunc>\n"
              "void apply() {\n"
              "    func();\n"
              "}");

    auto separate_brace{compact};
    separate_brace.template_parameters.reset();
    separate_brace.formatting = FunctionFormatting{
        .opening_brace_placement = FunctionFormatting::OpeningBracePlacement::separate_line,
    };
    EXPECT_EQ(render(header_function(separate_brace)),
              "void apply()\n"
              "    requires Invocable<TFunc>\n"
              "{\n"
              "    func();\n"
              "}");
}

TEST(Ast, RendersConstrainedStructuresAndSpecialMembers) {
    TypeDependency const dependency{"check", "CoreMinimal.h", {}};
    Node const structure{Struct{
        .name = "TFixedData",
        .children =
            {
                declaration(FunctionSpec{
                    .name = "TFixedData",
                    .qualifiers =
                        {
                            .is_noexcept = true,
                            .disposition = FunctionDisposition::defaulted,
                        },
                }),
                lines(1),
                header_function(FunctionSpec{
                    .name = "apply",
                    .return_type = "void",
                    .parameters = {FunctionParameter{"TFunc&&", "func"}},
                    .body = {raw("func();")},
                    .is_inline = true,
                    .template_parameters = "typename TFunc",
                    .formatting =
                        FunctionFormatting{
                            .body_layout = FunctionFormatting::BodyLayout::compact,
                            .template_placement = FunctionFormatting::TemplatePlacement::same_line,
                        },
                }),
                lines(2),
                AccessSpecifier{"private"},
            },
        .template_parameters = "int Capacity",
        .requires_clause = "(Capacity >= 0)",
        .dependencies = {dependency},
    }};

    EXPECT_EQ(render(structure),
              "template <int Capacity>\n"
              "    requires (Capacity >= 0)\n"
              "struct TFixedData {\n"
              "    TFixedData() noexcept = default;\n"
              "    template <typename TFunc> void apply(TFunc&& func) { func(); }\n\n"
              "  private:\n"
              "};");
    EXPECT_EQ(dependencies(structure), std::vector<TypeDependency>{dependency});
}

TEST(Ast, RawNodesReportExplicitDependencies) {
    TypeDependency const dependency{"FValue", "Project/Value.h", {}};
    Node const node{Raw{"use_value();", {dependency}}};

    EXPECT_EQ(dependencies(node), std::vector<TypeDependency>{dependency});
}

TEST(Ast, RendersDeclarationsAndOutOfLineDefinitionsFromOneSpec) {
    FunctionSpec const spec{
        .name = "set",
        .return_type = "void",
        .parameters = {FunctionParameter{"int32 const", "value", "0"}},
        .body = {raw("stored = value;")},
        .qualifiers = {.is_noexcept = true},
    };

    EXPECT_EQ(render(declaration(spec)), "void set(int32 const value = 0) noexcept;");
    EXPECT_EQ(render(definition(spec, "FValue")),
              "void FValue::set(int32 const value) noexcept {\n"
              "    stored = value;\n"
              "}");
}

TEST(Ast, CollectsTrailingReturnTypeDependencies) {
    TypeDependency const dependency{"FResult", "Project/Result.h", {}};
    Node const function{declaration(FunctionSpec{
        .name = "make",
        .return_type = "auto",
        .qualifiers =
            {
                .trailing_return_type = CppType{"FResult", {dependency}},
            },
    })};

    EXPECT_EQ(render(function), "auto make() -> FResult;");
    EXPECT_EQ(dependencies(function), std::vector<TypeDependency>{dependency});
}

TEST(Ast, CollectsOpaqueDeclarationDependenciesWithoutIncludingTheBody) {
    TypeDependency const requirement{"SupportsValue", "Project/Requirement.h", {}};
    TypeDependency const implementation{"apply", "Project/Implementation.h", {}};
    FunctionSpec const spec{
        .name = "use",
        .return_type = "void",
        .parameters = {FunctionParameter{"T const&", "value"}},
        .body = {ExpressionStmt{call(named("apply", {implementation}), {named("value")})}},
        .template_parameters = "typename T",
        .requires_clause = "SupportsValue<T>",
        .dependencies = {requirement},
    };
    CppFile file{.path = "Example.h", .nodes = {IncludeDependencies{}, declaration(spec)}};
    auto const header{render(file)};
    EXPECT_NE(header.find("#include \"Project/Requirement.h\""), std::string::npos);
    EXPECT_EQ(header.find("Project/Implementation.h"), std::string::npos);
    EXPECT_NE(header.find("requires SupportsValue<T>"), std::string::npos);

    file.nodes.back() = Function{spec};
    auto const source{render(file)};
    EXPECT_NE(source.find("#include \"Project/Requirement.h\""), std::string::npos);
    EXPECT_NE(source.find("#include \"Project/Implementation.h\""), std::string::npos);
    EXPECT_EQ(source.find("Project/Requirement.h"), source.rfind("Project/Requirement.h"));
}

TEST(Ast, RejectsInvalidFunctionQualifierCombinations) {
    auto expect_error = [](Node const& function, std::string_view expected) {
        try {
            static_cast<void>(render(function));
            FAIL() << "Expected function rendering to fail";
        } catch (std::invalid_argument const& error) {
            EXPECT_NE(std::string_view{error.what()}.find(expected), std::string_view::npos);
        }
    };

    expect_error(declaration(FunctionSpec{
                     .name = "conflicting_noexcept",
                     .return_type = "void",
                     .qualifiers = {.noexcept_condition = "condition", .is_noexcept = true},
                 }),
                 "cannot combine unconditional and conditional noexcept");

    expect_error(declaration(FunctionSpec{
                     .name = "non_placeholder_return",
                     .return_type = "int32",
                     .qualifiers = {.trailing_return_type = CppType{"int32"}},
                 }),
                 "trailing return type requires an 'auto' return type");

    EXPECT_EQ(render(declaration(FunctionSpec{
                  .name = "qualified_placeholder_return",
                  .return_type = "auto",
                  .qualifiers = {.trailing_return_type = CppType{"int32"}},
                  .is_static = true,
                  .is_constexpr = true,
              })),
              "static constexpr auto qualified_placeholder_return() -> int32;");

    expect_error(declaration(FunctionSpec{
                     .name = "static_const",
                     .return_type = "void",
                     .qualifiers = {.is_const = true},
                     .is_static = true,
                 }),
                 "static function cannot be const");

    expect_error(declaration(FunctionSpec{
                     .name = "deleted_with_body",
                     .body = {raw("return;")},
                     .qualifiers = {.disposition = FunctionDisposition::deleted},
                 }),
                 "defaulted or deleted function cannot have a body");

    expect_error(definition(
                     FunctionSpec{
                         .name = "removed",
                         .return_type = "void",
                         .qualifiers = {.disposition = FunctionDisposition::deleted},
                     },
                     "FValue"),
                 "deleted function cannot be an out-of-line definition");

    EXPECT_EQ(render(definition(
                  FunctionSpec{
                      .name = "FValue",
                      .qualifiers = {.disposition = FunctionDisposition::defaulted},
                  },
                  "FValue")),
              "FValue::FValue() = default;");
}

TEST(Ast, RejectsInvalidNewLineCounts) {
    EXPECT_THROW(lines(0), std::invalid_argument);
}

TEST(Ast, TypeOperationsAreStronglyTyped) {
    CppType type{"FContainer"};
    type.member_operations.emplace(TypeOperation::remove_at_swap, "remove_at_swap");

    EXPECT_EQ(type.operation(TypeOperation::remove_at_swap), "remove_at_swap");
    EXPECT_EQ(type.operation(static_cast<TypeOperation>(99)), std::nullopt);
}

TEST(Ast, RendersForwardUsingAndFilePolicyNodes) {
    CppFile const file{
        .path = "Example.cpp",
        .nodes =
            {
                ForwardDeclaration{"FValue", "struct"},
                lines(1),
                UsingDeclaration{"Value", CppType{"FValue"}},
            },
        .pragma_once = false,
        .clang_format_off = true,
        .prologue = {"// prologue"},
        .epilogue = {"// epilogue"},
    };

    auto const rendered{render(file)};
    EXPECT_TRUE(rendered.starts_with("// clang-format off\n"));
    EXPECT_EQ(rendered.find("#pragma once"), std::string::npos);
    EXPECT_NE(rendered.find("// prologue\n\nstruct FValue;\nusing Value = FValue;"),
              std::string::npos);
    EXPECT_NE(rendered.find("// epilogue\n// clang-format on\n"), std::string::npos);
}

TEST(Ast, AppliesStaticAndDefaultArgumentRulesToFunctionForms) {
    FunctionSpec const spec{
        .name = "make",
        .return_type = "int32",
        .parameters = {FunctionParameter{"int32 const", "value", "7"}},
        .body = {ReturnStmt{RawExpr{"value"}}},
        .is_static = true,
        .is_inline = true,
    };

    EXPECT_EQ(render(header_function(spec)),
              "static int32 make(int32 const value = 7) {\n"
              "    return value;\n"
              "}");
    EXPECT_EQ(render(definition(spec, "FValue")),
              "int32 FValue::make(int32 const value) {\n"
              "    return value;\n"
              "}");
}

TEST(Ast, RendersEnumsAndExportedFreeFunctions) {
    Enum const value{
        .name = "EMode",
        .underlying_type = CppType{"uint8", "CoreMinimal.h"},
        .values =
            {
                Enumerator{"First", "1"},
                Enumerator{"Second", std::nullopt, "UMETA(DisplayName = \"Second Value\")"},
            },
        .export_specifier = "PROJECT_API",
    };

    EXPECT_EQ(render(value),
              "enum class PROJECT_API EMode : uint8 {\n"
              "    First = 1,\n"
              "    Second UMETA(DisplayName = \"Second Value\"),\n"
              "};");
    auto const spec{FunctionSpec{
        .name = "to_string_view",
        .return_type = "auto",
        .qualifiers = {.trailing_return_type = CppType{"FStringView"}},
        .export_specifier = "PROJECT_API",
    }};
    EXPECT_EQ(render(declaration(spec)), "PROJECT_API auto to_string_view() -> FStringView;");
    EXPECT_EQ(render(Function{spec, std::nullopt, false, false}),
              "auto to_string_view() -> FStringView {\n\n}");
    EXPECT_EQ(render(Namespace{"", {raw("int value;")}}),
              "namespace {\nint value;\n} // namespace");
}

} // namespace
} // namespace codegen
