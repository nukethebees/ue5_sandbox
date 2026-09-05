#include "parser.h"
#include "renderer.h"

#include <codegen/sexpr/lexer.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>

namespace kernel_codegen::detail {
namespace {

constexpr std::string_view valid_source = R"(
(kernel-module arithmetic
  (header "ArrayKernels.h")
  (source "ArrayKernels.cpp")
  (header-include "ArrayKernels.h")
  (namespace ml)
  (export COMPILE_FIXTURE_API)
  (type-set numeric int32 float double)
  (map multiply
    (types numeric)
    (operand lhs array)
    (operand rhs (array scalar))
    (output out)
    (expression (* lhs rhs))
    (variants
      (out-of-place multiply)
      (in-place lhs multiply_in_place))))
)";

TEST(KernelParser, ParsesTypedMapDeclaration) {
    auto const document{parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", valid_source))};

    ASSERT_EQ(document.modules.size(), 1);
    ASSERT_EQ(document.modules[0].operations.size(), 1);
    EXPECT_EQ(document.modules[0].operations[0].operands.size(), 2);
    EXPECT_EQ(document.modules[0].operations[0].variants.size(), 2);
}

TEST(KernelRenderer, GeneratesConcreteOverloadsAndSourceLoops) {
    auto const document{parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", valid_source))};
    auto const files{render(document.modules[0])};

    ASSERT_EQ(files.size(), 2);
    EXPECT_EQ(files[0].content.find("template"), std::string::npos);
    EXPECT_NE(files[0].content.find("void COMPILE_FIXTURE_API multiply("), std::string::npos);
    EXPECT_NE(files[0].content.find("TConstArrayView<float> const rhs"), std::string::npos);
    EXPECT_NE(files[0].content.find("float const rhs"), std::string::npos);
    EXPECT_NE(files[0].content.find("TArray<float>& lhs"), std::string::npos);
    EXPECT_NE(files[1].content.find("for (int32 i{0}; i < count; ++i)"), std::string::npos);
    EXPECT_NE(files[1].content.find("lhs[i] * rhs[i]"), std::string::npos);
    EXPECT_NE(files[1].content.find("lhs[i] * rhs"), std::string::npos);
    EXPECT_NE(files[1].content.find(
                  "float const* lhs, float const* rhs, float* RESTRICT out"),
              std::string::npos);
    EXPECT_NE(files[1].content.find("multiply: out and lhs must not overlap"),
              std::string::npos);
}

TEST(KernelParser, RejectsUnknownExpressionOperatorWithLocation) {
    auto source{std::string{valid_source}};
    source.replace(source.find("(* lhs rhs)"), std::string{"(* lhs rhs)"}.size(), "(% lhs rhs)");

    try {
        static_cast<void>(parse("bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", source)));
        FAIL() << "Expected parsing to fail";
    } catch (std::runtime_error const& error) {
        EXPECT_NE(std::string{error.what()}.find("bad.sbxkernel:"), std::string::npos);
        EXPECT_NE(std::string{error.what()}.find("unsupported expression operator '%'"),
                  std::string::npos);
    }
}

TEST(KernelParser, RejectsDuplicateStructuralVariants) {
    auto source{std::string{valid_source}};
    source.replace(source.find("(out-of-place multiply)"),
                   std::string{"(out-of-place multiply)"}.size(),
                   "(out-of-place multiply) (out-of-place multiply_again)");

    EXPECT_THROW(
        static_cast<void>(
            parse("bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", source))),
        std::runtime_error);
}

}
}
