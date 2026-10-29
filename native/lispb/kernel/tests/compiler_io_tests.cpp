#include "../../tests/temporary_project.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <kernel_codegen/compiler.h>
#include <kernel_codegen/document.h>
#include <lispb/compilation.h>
#include <lispb/diagnostics.h>
#include <lispb/output.h>
#include <lispb/publication_options.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kernel_codegen {
namespace {

constexpr std::string_view kernel_source = R"(
(kernel-module arithmetic
  (emit
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ioj))
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

using lispb::test_support::TemporaryProject;

auto compile(TemporaryProject const& project,
             std::vector<std::filesystem::path> const& inputs,
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
    kernel_codegen::compile(syntax, document, cpp, compilation, files, true);
    return lispb::publish(
        compilation,
        {.path_base = root / "generated", .output_root = root / "generated", .check_only = check});
}

TEST(KernelCompiler, WritesChecksAndDetectsStaleOutputs) {
    TemporaryProject project{"kernel", "check-mode"};
    project.write("array_math.lispb", kernel_source);
    ASSERT_EQ(compile(project, {"array_math.lispb"}), 0);
    EXPECT_TRUE(
        project.read("generated/ArrayKernels.h").contains("multiply(std::span<float const>"));
    EXPECT_TRUE(project.read("generated/ArrayKernels.cpp").contains("lhs[i] * rhs"));
    EXPECT_TRUE(project.read("generated/.lispb-outputs").contains("ArrayKernels.cpp"));
    EXPECT_EQ(compile(project, {"array_math.lispb"}, true), 0);

    project.write("generated/ArrayKernels.h", "stale\n");
    EXPECT_EQ(compile(project, {"array_math.lispb"}, true), 1);
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
    kernel_codegen::compile(syntax, document, cpp, compilation, std::span{&file, 1}, false);
    EXPECT_TRUE(compilation.artifacts.empty());
}

TEST(KernelCompiler, RejectsOutputCollisionsAcrossInputs) {
    TemporaryProject project{"kernel", "duplicate-outputs"};
    project.write("first.lispb", kernel_source);
    project.write("second.lispb", kernel_source);

    EXPECT_THROW(static_cast<void>(compile(project, {"first.lispb", "second.lispb"})),
                 lispb::codegen::sexpr::SourceError);
}

TEST(KernelCompiler, EmitsReferenceTestsWhenRequested) {
    TemporaryProject project{"kernel", "reference-tests"};
    auto source{std::string{kernel_source}};
    source.insert(source.find("    (header-include"), "    (tests \"ArrayKernelsTests.cpp\")\n");
    project.write("array_math.lispb", source);

    ASSERT_EQ(compile(project, {"array_math.lispb"}), 0);
    EXPECT_TRUE(project.read("generated/ArrayKernels.h").contains("std::span<float const>"));
    EXPECT_TRUE(project.read("generated/ArrayKernelsTests.cpp").contains("TEST("));
    EXPECT_TRUE(project.read("generated/.lispb-outputs").contains("ArrayKernelsTests.cpp"));
}

}
}
