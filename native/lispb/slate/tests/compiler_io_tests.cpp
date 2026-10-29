#include "../../tests/temporary_project.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/lexer.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/compilation.h>
#include <lispb/diagnostics.h>
#include <lispb/output.h>
#include <lispb/publication_options.h>
#include <lispb/strings.h>
#include <slate_codegen/compiler.h>
#include <slate_codegen/document.h>
#include <slate_codegen/source_options.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace slate_codegen {
namespace {

using lispb::test_support::TemporaryProject;

auto compile(TemporaryProject const& project,
             std::vector<std::filesystem::path> inputs,
             std::vector<std::filesystem::path> include_directories = {},
             bool const check = false) -> int {
    auto const root{project.path({})};
    lispb::Strings strings;
    lispb::Diagnostics diagnostics{strings};
    lispb::codegen::sexpr::Sources sources{strings, diagnostics};
    lispb::codegen::sexpr::Syntax syntax{strings, sources};
    detail::Document document;
    lispb::codegen::CppTypes types{strings};
    lispb::codegen::CppIR ir{strings, types};
    lispb::Compilation compilation;
    compile_sources(syntax,
                    document,
                    ir,
                    compilation,
                    {.source_root = root,
                     .inputs = std::move(inputs),
                     .include_directories = std::move(include_directories)},
                    true);
    return lispb::publish(
        compilation,
        {.path_base = root / "generated", .output_root = root / "generated", .check_only = check});
}

auto expand(TemporaryProject const& project,
            std::vector<std::filesystem::path> inputs,
            std::vector<std::filesystem::path> include_directories = {}) -> std::string {
    lispb::Strings strings;
    lispb::Diagnostics diagnostics{strings};
    lispb::codegen::sexpr::Sources sources{strings, diagnostics};
    lispb::codegen::sexpr::Syntax syntax{strings, sources};
    return expand_sources(syntax,
                          {.source_root = project.path({}),
                           .inputs = std::move(inputs),
                           .include_directories = std::move(include_directories)});
}

TEST(SlateCompiler, WritesMultipleOwnersAndQualifiedOutputPaths) {
    TemporaryProject project{"slate", "multiple-owners"};
    project.write("panels.lispb", R"(
(widget-class SFirstPanel
  (function Build
    (params)
    (SButton)))
(widget-class Example::SSecondPanel
  (function Build
    (params)
    (SImage)))
)");

    ASSERT_EQ(compile(project, {"panels.lispb"}), 0);
    EXPECT_TRUE(project.read("generated/SFirstPanel.slate.generated.h")
                    .contains("struct SFirstPanelBuilder"));
    auto const qualified_output{project.read("generated/Example/SSecondPanel.slate.generated.h")};
    EXPECT_TRUE(qualified_output.contains("namespace SlateGenerated::Example"));
    EXPECT_TRUE(qualified_output.contains("using ThisClass = ::Example::SSecondPanel;"));

    auto const inventory{project.read("generated/.lispb-outputs")};
    EXPECT_TRUE(inventory.contains("SFirstPanel.slate.generated.h"));
    EXPECT_TRUE(inventory.contains("Example/SSecondPanel.slate.generated.h"));
}

TEST(SlateCompiler, CheckModeDetectsStaleAndMissingOutputs) {
    TemporaryProject project{"slate", "check-mode"};
    project.write("panel.lispb", R"(
(widget-class SPanel
  (function Build
    (params)
    (SButton)))
)");
    ASSERT_EQ(compile(project, {"panel.lispb"}), 0);
    EXPECT_EQ(compile(project, {"panel.lispb"}, {}, true), 0);

    project.write("generated/SPanel.slate.generated.h", "stale\n");
    EXPECT_EQ(compile(project, {"panel.lispb"}, {}, true), 1);

    ASSERT_EQ(compile(project, {"panel.lispb"}), 0);
    ASSERT_TRUE(std::filesystem::remove(project.path("generated/SPanel.slate.generated.h")));
    EXPECT_EQ(compile(project, {"panel.lispb"}, {}, true), 1);
}

TEST(SlateCompiler, RejectsDuplicateOwnersAcrossInputs) {
    TemporaryProject project{"slate", "duplicate-owners"};
    auto const source{R"(
(widget-class SPanel
  (function Build
    (params)
    (SButton)))
)"};
    project.write("first.lispb", source);
    project.write("second.lispb", source);

    try {
        static_cast<void>(compile(project, {"first.lispb", "second.lispb"}));
        FAIL() << "Expected duplicate owner to be rejected";
    } catch (lispb::codegen::sexpr::SourceError const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("duplicate widget declaration 'SPanel'"));
    }
}

TEST(SlateCompiler, ExpandsIncludedMacrosWithoutChangingGeneratedCpp) {
    TemporaryProject project{"slate", "macro-equivalence"};
    project.write("panel.lispb", R"(
(widget-class SPanel
  (function Build
    (params (value label))
    (vbox (auto :padding (0 0 0 10) (STextBlock :Text label)))))
)");
    ASSERT_EQ(compile(project, {"panel.lispb"}, {"shared"}), 0);
    auto const without_line_directives{[](std::string const& text) {
        std::istringstream input{text};
        std::string line;
        std::string result;
        while (std::getline(input, line)) {
            if (!line.starts_with("#line")) {
                result += line + '\n';
            }
        }
        return result;
    }};
    auto const expected{
        without_line_directives(project.read("generated/SPanel.slate.generated.h"))};
    project.write("shared/Common/Widgets.lispb", R"(
(include "Text.lispb")
(defmacro padded (padding child)
  (vbox (auto :padding $padding $child)))
(defmacro builder (name)
  (function $name
    (params (value label))
    (padded (0 0 0 10) (label-text label))))
)");
    project.write("shared/Common/Text.lispb", R"(
(defmacro label-text (label) (STextBlock :Text $label))
)");
    project.write("panel.lispb", R"(
(include "Common/Widgets.lispb")
(include "Common/./Text.lispb")
(widget-class SPanel (builder Build))
)");
    ASSERT_EQ(compile(project, {"panel.lispb"}, {"shared"}), 0);
    EXPECT_EQ(without_line_directives(project.read("generated/SPanel.slate.generated.h")),
              expected);
    project.write("shared/Common/Text.lispb", R"(
(defmacro label-text (label) (SButton :Text $label))
)");
    EXPECT_EQ(compile(project, {"panel.lispb"}, {"shared"}, true), 1);
}

TEST(SlateCompiler, ExpansionPreservesOpaqueRawMacroArguments) {
    TemporaryProject project{"slate", "raw-macro-expansion"};
    project.write("raw.lispb", R"lisp(
(defmacro passthrough (value) $value)
(passthrough #cpp{if ($value) {
    use("C:\\generated");
}}cpp#)
)lisp");

    EXPECT_EQ(expand(project, {"raw.lispb"}),
              "#cpp{if ($value) {\n"
              "    use(\"C:\\\\generated\");\n"
              "}}cpp#\n");
}

TEST(SlateCompiler, ResolvesIncludesLocallyThenInDirectoryOrder) {
    TemporaryProject project{"slate", "include-order"};
    project.write("local/panel.lispb", R"(
(include "Widgets.lispb")
(widget-class SPanel (function Build (params) (content)))
)");
    project.write("local/Widgets.lispb", "(defmacro content () (STextBlock))");
    project.write("first/Widgets.lispb", "(defmacro content () (SButton))");
    project.write("second/Widgets.lispb", "(defmacro content () (SImage))");
    ASSERT_EQ(compile(project, {"local/panel.lispb"}, {"first", "second"}), 0);
    EXPECT_TRUE(project.read("generated/SPanel.slate.generated.h").contains("SNew(STextBlock)"));
    ASSERT_TRUE(std::filesystem::remove(project.path("local/Widgets.lispb")));
    ASSERT_EQ(compile(project, {"local/panel.lispb"}, {"first", "second"}), 0);
    EXPECT_TRUE(project.read("generated/SPanel.slate.generated.h").contains("SNew(SButton)"));
    ASSERT_TRUE(std::filesystem::remove(project.path("first/Widgets.lispb")));
    ASSERT_EQ(compile(project, {"local/panel.lispb"}, {"first", "second"}), 0);
    EXPECT_TRUE(project.read("generated/SPanel.slate.generated.h").contains("SNew(SImage)"));
}

auto compile_error(TemporaryProject const& project,
                   std::vector<std::filesystem::path> inputs = {"panel.lispb"},
                   std::vector<std::filesystem::path> include_directories = {}) -> std::string {
    try {
        static_cast<void>(compile(project, std::move(inputs), std::move(include_directories)));
    } catch (std::exception const& error) {
        return error.what();
    }
    return {};
}

TEST(SlateCompiler, ReportsMissingIncludesAndCycles) {
    TemporaryProject project{"slate", "include-errors"};
    project.write("panel.lispb", "(include \"Missing.lispb\")");
    auto error{compile_error(project, {"panel.lispb"}, {"shared"})};
    EXPECT_TRUE(error.contains("include not found"));
    EXPECT_TRUE(error.contains("shared/Missing.lispb"));
    project.write("panel.lispb", "(include \"First.lispb\")");
    project.write("shared/First.lispb", "(include \"Second.lispb\")");
    project.write("shared/Second.lispb", "(include \"./First.lispb\")");
    error = compile_error(project, {"panel.lispb"}, {"shared"});
    EXPECT_TRUE(error.contains("include cycle"));
    EXPECT_TRUE(error.contains("First.lispb"));
    EXPECT_TRUE(error.contains("Second.lispb"));
}

TEST(SlateCompiler, RejectsInvalidMacroDeclarationsAndInvocations) {
    TemporaryProject project{"slate", "macro-errors"};
    struct Case {
        std::string_view source;
        std::string_view expected;
    };
    Case const cases[]{
        {"(defmacro thing () (SImage)) (defmacro thing () (SButton))", "duplicate macro"},
        {"(defmacro thing (x x) (SImage))", "duplicate macro parameter"},
        {"(defmacro thing (x) (SImage :Image $missing))", "unknown macro parameter"},
        {"(defmacro vbox () (SImage))", "built-in form"},
        {"(defmacro Thing () (SImage))", "lowercase letter"},
        {"(defmacro thing (x) $x) (thing)", "expects 1 arguments"},
        {"(defmacro thing () (thing)) (thing)", "recursive macro expansion"},
        {"(defmacro first () (second)) (defmacro second () (first)) (first)",
         "recursive macro expansion"},
        {"(widget-class SPanel (include \"Other.lispb\"))", "top-level source declarations"},
        {"$missing", "outside a template"},
    };
    for (auto const& test : cases) {
        SCOPED_TRACE(test.source);
        project.write("panel.lispb", test.source);
        EXPECT_TRUE(compile_error(project).contains(test.expected));
    }
}

TEST(SlateCompiler, ReportsMacroDefinitionAndInvocationForSemanticErrors) {
    TemporaryProject project{"slate", "macro-diagnostics"};
    project.write("Common.lispb", R"(
(defmacro broken (child) (vbox (auto :halign sideways $child)))
)");
    project.write("panel.lispb", R"(
(include "Common.lispb")
(widget-class SPanel (function Build (params) (broken (SImage))))
)");
    auto const error{compile_error(project)};
    EXPECT_TRUE(error.contains("Common.lispb:2:"));
    EXPECT_TRUE(error.contains("expanded at"));
    EXPECT_TRUE(error.contains("panel.lispb:3:"));
}

TEST(SlateCompiler, RestrictsIncludedFilesAndIsolatesMacrosBetweenInputs) {
    TemporaryProject project{"slate", "macro-isolation"};
    project.write("panel.lispb", "(include \"Common.lispb\")");
    project.write("Common.lispb", "(widget-class SPanel (function Build (params) (SImage)))");
    EXPECT_TRUE(compile_error(project).contains("included files may contain only"));
    project.write("panel.lispb", R"(
(defmacro builder () (function Build (params) (SImage)))
(widget-class SPanel (builder))
)");
    project.write("other.lispb", "(widget-class SOther (builder))");
    EXPECT_TRUE(compile_error(project, {"panel.lispb", "other.lispb"})
                    .contains("expected 'function' declaration"));
}

TEST(SlateCompiler, LibraryMigrationRemovesTheObsoleteOwnerHeader) {
    TemporaryProject project{"slate", "library-migration"};
    project.write("panel.lispb", "(widget-class FOwner (function Build (params) (SImage)))");
    ASSERT_EQ(compile(project, {"panel.lispb"}), 0);
    project.write("Common.lispb", "(defmacro image () (SImage))");
    project.write("panel.lispb", R"(
(include "Common.lispb")
(widget-library Example::Images (function Build (params) (image)))
)");
    ASSERT_EQ(compile(project, {"panel.lispb"}), 0);
    EXPECT_FALSE(std::filesystem::exists(project.path("generated/FOwner.slate.generated.h")));
    EXPECT_TRUE(
        project.read("generated/Example/Images.slate.generated.h").contains("inline auto Build()"));
    EXPECT_EQ(compile(project, {"panel.lispb"}, {}, true), 0);
}

TEST(SlateCompiler, ExpansionResolvesIncludesAndNeverWritesGeneratedFiles) {
    TemporaryProject project{"slate", "expand-read-only"};
    project.write("shared/Common.lispb", "(defmacro content () (SImage))");
    project.write("panel.lispb", R"(
(include "Common.lispb")
(widget-library Images (function Build (params) (content)))
)");
    auto const expanded{expand(project, {"panel.lispb"}, {"shared"})};
    EXPECT_EQ(expanded,
              "(widget-library Images\n  (function Build\n    (params)\n    (SImage)))\n");
    EXPECT_FALSE(std::filesystem::exists(project.path("generated")));

    ASSERT_EQ(compile(project, {"panel.lispb"}, {"shared"}), 0);
    auto const header{project.read("generated/Images.slate.generated.h")};
    auto const inventory{project.read("generated/.lispb-outputs")};
    project.write("shared/Common.lispb", "(defmacro content () (SButton))");
    EXPECT_TRUE(expand(project, {"panel.lispb"}, {"shared"}).contains("(SButton)"));
    EXPECT_EQ(project.read("generated/Images.slate.generated.h"), header);
    EXPECT_EQ(project.read("generated/.lispb-outputs"), inventory);
}

TEST(SlateCompiler, ExpansionPreservesStringsKeywordsAndSourceOrder) {
    TemporaryProject project{"slate", "expand-roundtrip"};
    std::string const first{R"(
(widget-library First
  (function Build (params)
    (SButton :Text (loc "Context" "Key" "Quotes: \" Backslash: \\ Newline: \n Tab: \t Return: \r")
      :Padding (0 0 0 10))))
)"};
    std::string const second{R"((widget-library Second (function Build (params) (SImage))))"};
    project.write("first.lispb", first);
    project.write("second.lispb", second);
    auto const expanded{expand(project, {"first.lispb", "second.lispb"})};
    lispb::Strings strings;
    lispb::Diagnostics diagnostics{strings};
    lispb::codegen::sexpr::Sources sources{strings, diagnostics};
    lispb::codegen::sexpr::Syntax syntax{strings, sources};
    auto const expected_file{sources.add("expected", first + second)};
    auto const actual_file{sources.add("expanded", expanded)};
    auto const expected{lispb::codegen::sexpr::lex(syntax, expected_file)};
    auto const actual{lispb::codegen::sexpr::lex(syntax, actual_file)};
    ASSERT_EQ(actual.count, expected.count);
    auto const count{expected.count};
    for (std::size_t index{}; index < count; ++index) {
        auto const actual_token{
            lispb::codegen::sexpr::TokenId{actual.offset + static_cast<std::uint32_t>(index)}};
        auto const expected_token{
            lispb::codegen::sexpr::TokenId{expected.offset + static_cast<std::uint32_t>(index)}};
        EXPECT_EQ(syntax.kind(actual_token), syntax.kind(expected_token));
        EXPECT_EQ(syntax.text(actual_token), syntax.text(expected_token));
    }
}

TEST(SlateCompiler, ExpansionCanInspectSemanticallyInvalidTrees) {
    TemporaryProject project{"slate", "expand-invalid-tree"};
    project.write("panel.lispb", R"(
(defmacro bad () (assign image_ SImage))
(widget-library Images (function Build (params) (bad)))
)");
    EXPECT_TRUE(expand(project, {"panel.lispb"}).contains("(assign image_ SImage)"));
    EXPECT_TRUE(compile_error(project).contains("requires a widget-class host"));
}

}
}
