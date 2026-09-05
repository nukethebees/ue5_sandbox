#include <kernel_codegen/compiler.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace kernel_codegen {
namespace {

constexpr std::string_view kernel_source = R"(
(kernel-module arithmetic
  (header "ArrayKernels.h")
  (source "ArrayKernels.cpp")
  (header-include "ArrayKernels.h")
  (namespace ml)
  (export COMPILE_FIXTURE_API)
  (type-set numeric float)
  (map multiply
    (types numeric)
    (operand lhs array)
    (operand rhs scalar)
    (output out)
    (expression (* lhs rhs))
    (variants
      (out-of-place multiply))))
)";

class TemporaryProject {
  public:
    explicit TemporaryProject(std::string_view const name)
        : root_{std::filesystem::temp_directory_path() /
                ("sandbox-kernel-codegen-" + std::string{name})} {
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

TEST(KernelCompiler, WritesChecksAndDetectsStaleOutputs) {
    TemporaryProject project{"check-mode"};
    project.write("manifest.json", R"({"entries":[{"input":"array_math.sbxkernel"}]})");
    project.write("array_math.sbxkernel", kernel_source);
    auto const options{CompileOptions{.manifest = project.path("manifest.json"),
                                      .output_root = project.path("generated")}};

    ASSERT_EQ(compile_manifest(options), 0);
    EXPECT_TRUE(project.read("generated/ArrayKernels.h").contains(
        "void COMPILE_FIXTURE_API multiply("));
    EXPECT_TRUE(project.read("generated/ArrayKernels.cpp").contains("lhs[i] * rhs"));
    EXPECT_TRUE(project.read("generated/.sandbox-codegen-outputs")
                    .contains("ArrayKernels.cpp"));
    EXPECT_EQ(compile_manifest(CompileOptions{.manifest = options.manifest,
                                               .output_root = options.output_root,
                                               .check = true}),
              0);

    project.write("generated/ArrayKernels.h", "stale\n");
    EXPECT_EQ(compile_manifest(CompileOptions{.manifest = options.manifest,
                                               .output_root = options.output_root,
                                               .check = true}),
              1);
}

TEST(KernelCompiler, RejectsOutputCollisionsAcrossManifestEntries) {
    TemporaryProject project{"duplicate-outputs"};
    project.write(
        "manifest.json",
        R"({"entries":[{"input":"first.sbxkernel"},{"input":"second.sbxkernel"}]})");
    project.write("first.sbxkernel", kernel_source);
    project.write("second.sbxkernel", kernel_source);

    EXPECT_THROW(static_cast<void>(compile_manifest(CompileOptions{
                     .manifest = project.path("manifest.json"),
                     .output_root = project.path("generated")})),
                 std::invalid_argument);
}

}
}
