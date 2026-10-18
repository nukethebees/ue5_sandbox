#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <kernel_codegen/compiler.h>
#include <kernel_codegen/document.h>
#include <lispb/compilation.h>
#include <lispb/diagnostics.h>
#include <lispb/kernel_profile.h>
#include <lispb/output.h>
#include <lispb/publication_options.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kernel_codegen {
namespace {

constexpr std::string_view kernel_source = R"(
(kernel-module arithmetic
  (emit unreal
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ml)
    (export COMPILE_FIXTURE_API))
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
        write(".clang-format", "BasedOnStyle: LLVM\nLineEnding: LF\n");
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
             lispb::KernelProfile const profile = lispb::KernelProfile::unreal,
             bool const check = false) -> int {
    auto const root{project.path({})};
    lispb::Strings strings;
    lispb::Diagnostics diagnostics{strings};
    lispb::codegen::sexpr::Sources sources{strings, diagnostics};
    lispb::codegen::sexpr::Syntax syntax{strings, sources};
    detail::Document document{strings};
    lispb::codegen::CppTypes types{strings};
    lispb::codegen::CppIR cpp{strings, types};
    lispb::Compilation compilation;
    std::vector<lispb::codegen::sexpr::SourceFileId> files;
    for (auto const& input : inputs) {
        files.push_back(sources.add(input.generic_string(), project.read(input)));
    }
    kernel_codegen::compile(syntax, document, cpp, compilation, files, profile, true);
    return lispb::publish(
        compilation,
        {.path_base = root / "generated", .output_root = root / "generated", .check_only = check});
}

TEST(KernelCompiler, WritesChecksAndDetectsStaleOutputs) {
    TemporaryProject project{"check-mode"};
    project.write("array_math.lispb", kernel_source);
    ASSERT_EQ(compile(project, {"array_math.lispb"}), 0);
    EXPECT_TRUE(
        project.read("generated/ArrayKernels.h").contains("void COMPILE_FIXTURE_API multiply("));
    EXPECT_TRUE(project.read("generated/ArrayKernels.cpp").contains("lhs[i] * rhs"));
    EXPECT_TRUE(project.read("generated/.lispb-outputs").contains("ArrayKernels.cpp"));
    EXPECT_EQ(compile(project, {"array_math.lispb"}, lispb::KernelProfile::unreal, true), 0);

    project.write("generated/ArrayKernels.h", "stale\n");
    EXPECT_EQ(compile(project, {"array_math.lispb"}, lispb::KernelProfile::unreal, true), 1);
}

TEST(KernelCompiler, ValidationDoesNotProduceArtifacts) {
    lispb::Strings strings;
    lispb::Diagnostics diagnostics{strings};
    lispb::codegen::sexpr::Sources sources{strings, diagnostics};
    lispb::codegen::sexpr::Syntax syntax{strings, sources};
    detail::Document document{strings};
    lispb::codegen::CppTypes types{strings};
    lispb::codegen::CppIR cpp{strings, types};
    lispb::Compilation compilation;
    auto const file{sources.add("array_math.lispb", kernel_source)};
    kernel_codegen::compile(syntax,
                            document,
                            cpp,
                            compilation,
                            std::span{&file, 1},
                            lispb::KernelProfile::unreal,
                            false);
    EXPECT_TRUE(compilation.artifacts.empty());
}

TEST(KernelCompiler, RejectsOutputCollisionsAcrossInputs) {
    TemporaryProject project{"duplicate-outputs"};
    project.write("first.lispb", kernel_source);
    project.write("second.lispb", kernel_source);

    EXPECT_THROW(static_cast<void>(compile(project, {"first.lispb", "second.lispb"})),
                 std::invalid_argument);
}

TEST(KernelCompiler, EmitsOnlyTheSelectedProfile) {
    TemporaryProject project{"profile-selection"};
    auto source{std::string{kernel_source}};
    auto const insertion{source.find("  (type-set")};
    source.insert(insertion,
                  "  (emit standard\n"
                  "    (header \"standard/Kernels.h\")\n"
                  "    (source \"standard/Kernels.cpp\")\n"
                  "    (tests \"standard/KernelsTests.cpp\")\n"
                  "    (header-include \"standard/Kernels.h\")\n"
                  "    (namespace ml))\n");
    project.write("array_math.lispb", source);

    ASSERT_EQ(compile(project, {"array_math.lispb"}, lispb::KernelProfile::standard), 0);
    EXPECT_TRUE(project.read("generated/standard/Kernels.h").contains("std::span<float const>"));
    EXPECT_TRUE(project.read("generated/standard/KernelsTests.cpp").contains("TEST("));
    EXPECT_FALSE(std::filesystem::exists(project.path("generated/ArrayKernels.h")));
}

}
}
