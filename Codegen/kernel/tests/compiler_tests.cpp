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

auto occurrence_count(std::string_view text, std::string_view const value) -> std::size_t {
    std::size_t result{};
    while (true) {
        auto const position{text.find(value)};
        if (position == std::string_view::npos) {
            return result;
        }
        ++result;
        text.remove_prefix(position + value.size());
    }
}

auto render_source(std::string_view const source) -> std::vector<codegen::GeneratedFile> {
    auto const document{parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", source))};
    return render(document.modules[0]);
}

TEST(KernelParser, ParsesTypedMapDeclaration) {
    auto const document{parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", valid_source))};

    ASSERT_EQ(document.modules.size(), 1);
    ASSERT_EQ(document.modules[0].operations.size(), 1);
    EXPECT_EQ(document.modules[0].operations[0].operands.size(), 2);
    EXPECT_EQ(document.modules[0].operations[0].variants.size(), 2);
}

TEST(KernelRenderer, GeneratesConcreteOverloadsAndSourceLoops) {
    auto const files{render_source(valid_source)};

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
    EXPECT_EQ(files[1].content.find("multiply: lhs and rhs must not overlap"),
              std::string::npos);
}

TEST(KernelRenderer, ExpandsStorageCartesianProductWithoutRewritingExpression) {
    constexpr std::string_view source = R"(
(kernel-module arithmetic
  (header "ArrayKernels.h")
  (source "ArrayKernels.cpp")
  (header-include "ArrayKernels.h")
  (namespace ml)
  (export COMPILE_FIXTURE_API)
  (type-set numeric float)
  (map difference
    (types numeric)
    (operand a (array scalar))
    (operand b (array scalar))
    (operand c scalar)
    (output out)
    (expression (- a (+ b c)))
    (variants
      (out-of-place difference))))
)";

    auto const files{render_source(source)};

    EXPECT_EQ(occurrence_count(files[0].content, "void COMPILE_FIXTURE_API difference("), 3);
    EXPECT_TRUE(files[1].content.contains("(a[i] - (b[i] + c))"));
    EXPECT_TRUE(files[1].content.contains("(a[i] - (b + c))"));
    EXPECT_TRUE(files[1].content.contains("(a - (b[i] + c))"));
    EXPECT_FALSE(files[1].content.contains("(a - (b + c))"));
}

TEST(KernelRenderer, PairwiseDisjointRestrictsAndChecksEveryArray) {
    auto source{std::string{valid_source}};
    source.replace(source.find("(variants"), 0, "(aliasing pairwise-disjoint)\n    ");

    auto const files{render_source(source)};

    EXPECT_TRUE(files[1].content.contains(
        "float const* RESTRICT lhs, float const* RESTRICT rhs, float* RESTRICT out"));
    EXPECT_TRUE(files[1].content.contains("multiply: lhs and rhs must not overlap"));
    EXPECT_TRUE(files[1].content.contains("multiply: lhs and out must not overlap"));
    EXPECT_TRUE(files[1].content.contains("multiply: rhs and out must not overlap"));
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

TEST(KernelParser, RejectsScalarOnlyInPlaceTargets) {
    auto source{std::string{valid_source}};
    source.replace(source.find("(operand lhs array)"),
                   std::string{"(operand lhs array)"}.size(),
                   "(operand lhs scalar)");

    EXPECT_THROW(
        static_cast<void>(
            parse("bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", source))),
        std::runtime_error);
}

TEST(KernelParser, RejectsNonFiniteNumericLiterals) {
    auto source{std::string{valid_source}};
    source.replace(source.find("(* lhs rhs)"), std::string{"(* lhs rhs)"}.size(), "(* lhs inf)");

    EXPECT_THROW(
        static_cast<void>(
            parse("bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", source))),
        std::runtime_error);
}

TEST(KernelRenderer, RejectsCollidingPublicSignatures) {
    auto source{std::string{valid_source}};
    auto const map_position{source.find("(map multiply")};
    auto duplicate{source.substr(map_position, source.rfind(')') - map_position)};
    duplicate.replace(duplicate.find("(map multiply"),
                      std::string{"(map multiply"}.size(),
                      "(map product");
    duplicate.replace(duplicate.find("(in-place lhs multiply_in_place)"),
                      std::string{"(in-place lhs multiply_in_place)"}.size(),
                      "(in-place lhs product_in_place)");
    source.insert(source.rfind(')'), duplicate);

    EXPECT_THROW(static_cast<void>(render_source(source)), std::invalid_argument);
}

}
}
