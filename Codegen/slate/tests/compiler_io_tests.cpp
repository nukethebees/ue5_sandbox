#include <slate_codegen/compiler.h>

#include "syntax.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sstream>
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
        return std::string{std::istreambuf_iterator<char>{input},
                           std::istreambuf_iterator<char>{}};
    }

    auto path(std::filesystem::path const& relative_path) const -> std::filesystem::path {
        return root_ / relative_path;
    }

  private:
    std::filesystem::path root_;
};

TEST(SlateCompiler, WritesMultipleOwnersAndQualifiedOutputPaths) {
    TemporaryProject project{"multiple-owners"};
    project.write("manifest.json", R"({"entries":[{"input":"panels.sbxslate"}]})");
    project.write("panels.sbxslate", R"(
(widget-class SFirstPanel
  (function Build
    (params)
    (SButton)))
(widget-class Example::SSecondPanel
  (function Build
    (params)
    (SImage)))
)");

    ASSERT_EQ(compile_manifest(CompileOptions{.manifest = project.path("manifest.json")}), 0);
    EXPECT_TRUE(project.read("generated/SFirstPanel.slate.generated.h")
                    .contains("struct SFirstPanelBuilder"));
    auto const qualified_output{
        project.read("generated/Example/SSecondPanel.slate.generated.h")};
    EXPECT_TRUE(qualified_output.contains("namespace SlateGenerated::Example"));
    EXPECT_TRUE(qualified_output.contains("using ThisClass = ::Example::SSecondPanel;"));

    auto const inventory{project.read("generated/.sandbox-codegen-outputs")};
    EXPECT_TRUE(inventory.contains("SFirstPanel.slate.generated.h"));
    EXPECT_TRUE(inventory.contains("Example/SSecondPanel.slate.generated.h"));
}

TEST(SlateCompiler, CheckModeDetectsStaleAndMissingOutputs) {
    TemporaryProject project{"check-mode"};
    project.write("manifest.json", R"({"entries":[{"input":"panel.sbxslate"}]})");
    project.write("panel.sbxslate", R"(
(widget-class SPanel
  (function Build
    (params)
    (SButton)))
)");
    auto const options{CompileOptions{.manifest = project.path("manifest.json")}};

    ASSERT_EQ(compile_manifest(options), 0);
    EXPECT_EQ(compile_manifest(CompileOptions{.manifest = options.manifest, .check = true}), 0);

    project.write("generated/SPanel.slate.generated.h", "stale\n");
    EXPECT_EQ(compile_manifest(CompileOptions{.manifest = options.manifest, .check = true}), 1);

    ASSERT_EQ(compile_manifest(options), 0);
    ASSERT_TRUE(std::filesystem::remove(project.path("generated/SPanel.slate.generated.h")));
    EXPECT_EQ(compile_manifest(CompileOptions{.manifest = options.manifest, .check = true}), 1);
}

TEST(SlateCompiler, RejectsDuplicateOwnersAcrossInputs) {
    TemporaryProject project{"duplicate-owners"};
    project.write(
        "manifest.json",
        R"({"entries":[{"input":"first.sbxslate"},{"input":"second.sbxslate"}]})");
    auto const source{R"(
(widget-class SPanel
  (function Build
    (params)
    (SButton)))
)"};
    project.write("first.sbxslate", source);
    project.write("second.sbxslate", source);

    try {
        static_cast<void>(
            compile_manifest(CompileOptions{.manifest = project.path("manifest.json")}));
        FAIL() << "Expected duplicate owner to be rejected";
    } catch (detail::SourceError const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("duplicate widget declaration 'SPanel'"));
    }
}

TEST(SlateCompiler, ExpandsIncludedMacrosWithoutChangingGeneratedCpp) {
    TemporaryProject project{"macro-equivalence"};
    project.write("manifest.json", R"({"include_directories":["shared"],"entries":[{"input":"panel.sbxslate"}]})");
    project.write("panel.sbxslate", R"(
(widget-class SPanel
  (function Build
    (params (value label))
    (vbox (auto :padding (0 0 0 10) (STextBlock :Text label)))))
)");
    auto const options{CompileOptions{.manifest = project.path("manifest.json")}};
    ASSERT_EQ(compile_manifest(options), 0);
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
    auto const expected{without_line_directives(project.read("generated/SPanel.slate.generated.h"))};
    project.write("shared/Common/Widgets.sbxslate", R"(
(include "Text.sbxslate")
(defmacro padded (padding child)
  (vbox (auto :padding $padding $child)))
(defmacro builder (name)
  (function $name
    (params (value label))
    (padded (0 0 0 10) (label-text label))))
)");
    project.write("shared/Common/Text.sbxslate", R"(
(defmacro label-text (label) (STextBlock :Text $label))
)");
    project.write("panel.sbxslate", R"(
(include "Common/Widgets.sbxslate")
(include "Common/./Text.sbxslate")
(widget-class SPanel (builder Build))
)");
    ASSERT_EQ(compile_manifest(options), 0);
    EXPECT_EQ(without_line_directives(project.read("generated/SPanel.slate.generated.h")), expected);
    project.write("shared/Common/Text.sbxslate", R"(
(defmacro label-text (label) (SButton :Text $label))
)");
    EXPECT_EQ(compile_manifest(CompileOptions{.manifest = options.manifest, .check = true}), 1);
}

TEST(SlateCompiler, ResolvesIncludesLocallyThenInDirectoryOrder) {
    TemporaryProject project{"include-order"};
    project.write("manifest.json", R"({"include_directories":["first","second"],"entries":[{"input":"local/panel.sbxslate"}]})");
    project.write("local/panel.sbxslate", R"(
(include "Widgets.sbxslate")
(widget-class SPanel (function Build (params) (content)))
)");
    project.write("local/Widgets.sbxslate", "(defmacro content () (STextBlock))");
    project.write("first/Widgets.sbxslate", "(defmacro content () (SButton))");
    project.write("second/Widgets.sbxslate", "(defmacro content () (SImage))");
    auto const options{CompileOptions{.manifest = project.path("manifest.json")}};
    ASSERT_EQ(compile_manifest(options), 0);
    EXPECT_TRUE(project.read("generated/SPanel.slate.generated.h").contains("SNew(STextBlock)"));
    ASSERT_TRUE(std::filesystem::remove(project.path("local/Widgets.sbxslate")));
    ASSERT_EQ(compile_manifest(options), 0);
    EXPECT_TRUE(project.read("generated/SPanel.slate.generated.h").contains("SNew(SButton)"));
    ASSERT_TRUE(std::filesystem::remove(project.path("first/Widgets.sbxslate")));
    ASSERT_EQ(compile_manifest(options), 0);
    EXPECT_TRUE(project.read("generated/SPanel.slate.generated.h").contains("SNew(SImage)"));
}

auto compile_error(TemporaryProject const& project) -> std::string {
    try {
        static_cast<void>(compile_manifest(CompileOptions{.manifest = project.path("manifest.json")}));
    } catch (std::exception const& error) {
        return error.what();
    }
    return {};
}

TEST(SlateCompiler, ReportsMissingIncludesAndCycles) {
    TemporaryProject project{"include-errors"};
    project.write("manifest.json", R"({"include_directories":["shared"],"entries":[{"input":"panel.sbxslate"}]})");
    project.write("panel.sbxslate", "(include \"Missing.sbxslate\")");
    auto error{compile_error(project)};
    EXPECT_TRUE(error.contains("include not found"));
    EXPECT_TRUE(error.contains("shared/Missing.sbxslate"));
    project.write("panel.sbxslate", "(include \"First.sbxslate\")");
    project.write("shared/First.sbxslate", "(include \"Second.sbxslate\")");
    project.write("shared/Second.sbxslate", "(include \"./First.sbxslate\")");
    error = compile_error(project);
    EXPECT_TRUE(error.contains("include cycle"));
    EXPECT_TRUE(error.contains("First.sbxslate"));
    EXPECT_TRUE(error.contains("Second.sbxslate"));
}

TEST(SlateCompiler, RejectsInvalidMacroDeclarationsAndInvocations) {
    TemporaryProject project{"macro-errors"};
    project.write("manifest.json", R"({"entries":[{"input":"panel.sbxslate"}]})");
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
        {"(defmacro first () (second)) (defmacro second () (first)) (first)", "recursive macro expansion"},
        {"(widget-class SPanel (include \"Other.sbxslate\"))", "top-level source declarations"},
        {"$missing", "outside a template"},
    };
    for (auto const& test : cases) {
        SCOPED_TRACE(test.source);
        project.write("panel.sbxslate", test.source);
        EXPECT_TRUE(compile_error(project).contains(test.expected));
    }
}

TEST(SlateCompiler, ReportsMacroDefinitionAndInvocationForSemanticErrors) {
    TemporaryProject project{"macro-diagnostics"};
    project.write("manifest.json", R"({"entries":[{"input":"panel.sbxslate"}]})");
    project.write("Common.sbxslate", R"(
(defmacro broken (child) (vbox (auto :halign sideways $child)))
)");
    project.write("panel.sbxslate", R"(
(include "Common.sbxslate")
(widget-class SPanel (function Build (params) (broken (SImage))))
)");
    auto const error{compile_error(project)};
    EXPECT_TRUE(error.contains("Common.sbxslate:2:"));
    EXPECT_TRUE(error.contains("expanded at"));
    EXPECT_TRUE(error.contains("panel.sbxslate:3:"));
}

TEST(SlateCompiler, RestrictsIncludedFilesAndIsolatesMacrosBetweenInputs) {
    TemporaryProject project{"macro-isolation"};
    project.write("manifest.json", R"({"entries":[{"input":"panel.sbxslate"}]})");
    project.write("panel.sbxslate", "(include \"Common.sbxslate\")");
    project.write("Common.sbxslate", "(widget-class SPanel (function Build (params) (SImage)))");
    EXPECT_TRUE(compile_error(project).contains("included files may contain only"));
    project.write("manifest.json", R"({"entries":[{"input":"panel.sbxslate"},{"input":"other.sbxslate"}]})");
    project.write("panel.sbxslate", R"(
(defmacro builder () (function Build (params) (SImage)))
(widget-class SPanel (builder))
)");
    project.write("other.sbxslate", "(widget-class SOther (builder))");
    EXPECT_TRUE(compile_error(project).contains("expected 'function' declaration"));
}

TEST(SlateCompiler, RejectsInvalidIncludeDirectories) {
    TemporaryProject project{"include-directories"};
    for (auto const directories : {"null", "true", "1", "\"shared\"", "[1]", "[\"\"]"}) {
        project.write("manifest.json", std::string{"{\"entries\":[{\"input\":\"panel.sbxslate\"}],\"include_directories\":"} + directories + "}");
        EXPECT_FALSE(compile_error(project).empty());
    }
}

TEST(SlateCompiler, LibraryMigrationRemovesTheObsoleteOwnerHeader) {
    TemporaryProject project{"library-migration"};
    project.write("manifest.json", R"({"entries":[{"input":"panel.sbxslate"}]})");
    project.write("panel.sbxslate", "(widget-class FOwner (function Build (params) (SImage)))");
    auto const options{CompileOptions{.manifest = project.path("manifest.json")}};
    ASSERT_EQ(compile_manifest(options), 0);
    project.write("Common.sbxslate", "(defmacro image () (SImage))");
    project.write("panel.sbxslate", R"(
(include "Common.sbxslate")
(widget-library Example::Images (function Build (params) (image)))
)");
    ASSERT_EQ(compile_manifest(options), 0);
    EXPECT_FALSE(std::filesystem::exists(project.path("generated/FOwner.slate.generated.h")));
    EXPECT_TRUE(project.read("generated/Example/Images.slate.generated.h").contains("inline auto Build()"));
    EXPECT_EQ(compile_manifest(CompileOptions{.manifest = options.manifest, .check = true}), 0);
}

}
}
