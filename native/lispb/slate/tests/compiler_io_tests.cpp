#include <slate_codegen/compiler.h>

#include "syntax.h"

#include <codegen/sexpr/lexer.h>
#include <gtest/gtest.h>
#include <lispb/output.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace slate_codegen {
namespace {

class TemporaryProject {
  public:
    explicit TemporaryProject(std::string_view const name)
        : root_{std::filesystem::temp_directory_path() /
                ("sandbox-slate-codegen-" + std::string{name})} {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
        std::filesystem::create_directories(root_);
    }

    ~TemporaryProject() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    void write(std::filesystem::path const& relative_path, std::string_view const content) const {
        auto const path{root_ / relative_path};
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output{path, std::ios::binary};
        if (!output) {
            throw std::runtime_error{"Cannot write test file: " + path.string()};
        }
        output << content;
    }

    auto read(std::filesystem::path const& relative_path) const -> std::string {
        auto const path{root_ / relative_path};
        std::ifstream input{path, std::ios::binary};
        if (!input) {
            throw std::runtime_error{"Cannot read test file: " + path.string()};
        }
        return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }

    auto path(std::filesystem::path const& relative_path) const -> std::filesystem::path {
        return root_ / relative_path;
    }
  private:
    std::filesystem::path root_;
};

auto compile(TemporaryProject const& project,
             std::vector<std::filesystem::path> inputs,
             std::vector<std::filesystem::path> include_directories = {},
             bool const check = false) -> int {
    auto const root{project.path({})};
    auto const compilation{
        compile_sources({.source_root = root,
                         .inputs = std::move(inputs),
                         .include_directories = std::move(include_directories)})};
    return lispb::publish(
        compilation,
        {.path_base = root / "generated", .output_root = root / "generated", .check_only = check});
}

auto expand(TemporaryProject const& project,
            std::vector<std::filesystem::path> inputs,
            std::vector<std::filesystem::path> include_directories = {}) -> std::string {
    return expand_sources({.source_root = project.path({}),
                           .inputs = std::move(inputs),
                           .include_directories = std::move(include_directories)});
}

TEST(SlateCompiler, WritesMultipleOwnersAndQualifiedOutputPaths) {
    TemporaryProject project{"multiple-owners"};
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
    TemporaryProject project{"check-mode"};
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
    TemporaryProject project{"duplicate-owners"};
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
    } catch (detail::SourceError const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("duplicate widget declaration 'SPanel'"));
    }
}

TEST(SlateCompiler, ExpandsIncludedMacrosWithoutChangingGeneratedCpp) {
    TemporaryProject project{"macro-equivalence"};
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

TEST(SlateCompiler, ResolvesIncludesLocallyThenInDirectoryOrder) {
    TemporaryProject project{"include-order"};
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
    TemporaryProject project{"include-errors"};
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
    TemporaryProject project{"macro-errors"};
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
    TemporaryProject project{"macro-diagnostics"};
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
    TemporaryProject project{"macro-isolation"};
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
    TemporaryProject project{"library-migration"};
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
    TemporaryProject project{"expand-read-only"};
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
    TemporaryProject project{"expand-roundtrip"};
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
    auto const expected{codegen::sexpr::lex("expected", first + second)};
    auto const actual{codegen::sexpr::lex("expanded", expanded)};
    ASSERT_EQ(actual.size(), expected.size());
    auto const count{expected.size()};
    for (std::size_t index{}; index < count; ++index) {
        EXPECT_EQ(actual[index].kind, expected[index].kind);
        EXPECT_EQ(actual[index].text, expected[index].text);
    }
}

TEST(SlateCompiler, ExpansionCanInspectSemanticallyInvalidTrees) {
    TemporaryProject project{"expand-invalid-tree"};
    project.write("panel.lispb", R"(
(defmacro bad () (assign image_ SImage))
(widget-library Images (function Build (params) (bad)))
)");
    EXPECT_TRUE(expand(project, {"panel.lispb"}).contains("(assign image_ SImage)"));
    EXPECT_TRUE(compile_error(project).contains("requires a widget-class host"));
}

}
}
