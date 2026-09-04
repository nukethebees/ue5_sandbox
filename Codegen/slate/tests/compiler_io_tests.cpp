#include <slate_codegen/compiler.h>

#include "syntax.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
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
        EXPECT_TRUE(std::string{error.what()}.contains("duplicate widget class declaration 'SPanel'"));
    }
}

}
}
