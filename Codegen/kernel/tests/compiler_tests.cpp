#include "parser.h"
#include "renderer.h"

#include <codegen/sexpr/lexer.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kernel_codegen::detail {
namespace {

constexpr std::string_view valid_source = R"(
(kernel-module arithmetic
  (emit unreal
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ml)
    (export COMPILE_FIXTURE_API))
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

constexpr std::string_view standard_source = R"(
(kernel-module arithmetic
  (emit standard
    (header "standard/ArrayKernels.h")
    (source "standard/ArrayKernels.cpp")
    (tests "standard/ArrayKernelsTests.cpp")
    (header-include "standard/ArrayKernels.h")
    (namespace ml::standard))
  (type-set numeric uint32 float)
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

constexpr std::string_view avx2_lab_source = R"(
(kernel-module arithmetic
  (emit unreal-avx2-lab
    (header "generated/add_scaled_avx2_lab.h")
    (source "generated/add_scaled_avx2_lab.cpp")
    (header-include "generated/add_scaled_avx2_lab.h")
    (namespace ml::kernel_benchmark)
    (select
      (operation add_scaled)
      (type float)
      (storage array array scalar)
      (variant out-of-place)))
  (type-set numeric float double)
  (map add_scaled
    (types numeric)
    (operand base array)
    (operand value array)
    (operand scale scalar)
    (output out)
    (aliasing pairwise-disjoint)
    (expression (+ base (* value scale)))
    (variants
      (out-of-place add_scaled))))
)";

constexpr std::string_view sum_lab_source = R"(
(kernel-module reductions
  (emit native-x86-simd-lab
    (header "native/DotProduct.h")
    (source "native/DotProductAvx2.cpp")
    (avx512-source "native/DotProductAvx512.cpp")
    (dispatch-source "native/DotProductDispatch.cpp")
    (relaxed-avx2-source "native/DotProductRelaxedAvx2.cpp")
    (relaxed-avx512-source "native/DotProductRelaxedAvx512.cpp")
    (header-include "native/DotProduct.h")
    (namespace ml::lab)
    (select
      (operation dot_product)
      (type float)
      (storage array array)
      (variant sum)))
  (type-set floating float)
  (sum dot_product
    (types floating)
    (operand lhs array)
    (operand rhs array)
    (aliasing pairwise-disjoint)
    (floating-point-modes strict relaxed)
    (expression (* lhs rhs))))
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
    return render(document.modules[0], Profile::unreal);
}

auto literal_source(std::string_view const types, std::string_view const literal) -> std::string {
    return std::string{R"((kernel-module literals
  (emit unreal
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ml)
    (export COMPILE_FIXTURE_API))
  (type-set numeric )"} +
           std::string{types} + R"()
  (map add_literal
    (types numeric)
    (operand data array)
    (output out)
    (expression (+ data )" +
           std::string{literal} + R"())
    (variants
      (out-of-place add_literal))))
)";
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

TEST(KernelRenderer, GeneratesStandardLibraryBindingsAndTests) {
    auto const document{
        parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", standard_source))};
    auto const files{render(document.modules[0], Profile::standard)};

    ASSERT_EQ(files.size(), 3);
    EXPECT_TRUE(files[0].content.contains("std::span<std::uint32_t const> lhs"));
    EXPECT_TRUE(files[0].content.contains("std::span<float> out"));
    EXPECT_FALSE(files[0].content.contains("TArray"));
    EXPECT_FALSE(files[1].content.contains("CoreMinimal"));
    EXPECT_FALSE(files[1].content.contains("RESTRICT"));
    EXPECT_TRUE(files[1].content.contains("for (std::size_t i{}; i < count; ++i)"));
    EXPECT_TRUE(files[1].content.contains("std::abort()"));
    EXPECT_TRUE(files[2].content.contains("#include <gtest/gtest.h>"));
    EXPECT_TRUE(files[2].content.contains("constexpr std::size_t Count{31}"));
    EXPECT_TRUE(files[2].content.contains("std::array<float, Count> expected{"));
    EXPECT_TRUE(files[2].content.contains("EXPECT_FLOAT_EQ("));
    EXPECT_FALSE(files[2].content.contains("expected[i] ="));
}

TEST(KernelRenderer, GeneratesOneSelectedAvx2LabVariant) {
    auto const document{
        parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", avx2_lab_source))};
    auto const files{render(document.modules[0], Profile::unreal_avx2_lab)};

    ASSERT_EQ(files.size(), 2);
    EXPECT_EQ(occurrence_count(files[0].content, "void add_scaled_autovec_avx2("), 1);
    EXPECT_EQ(occurrence_count(files[0].content, "void add_scaled_avx2("), 1);
    EXPECT_EQ(occurrence_count(files[0].content, "void add_scaled_avx2_unrolled("), 1);
    EXPECT_TRUE(files[1].content.contains("_mm256_loadu_ps"));
    EXPECT_TRUE(files[1].content.contains("_mm256_mul_ps"));
    EXPECT_TRUE(files[1].content.contains("_mm256_add_ps"));
    EXPECT_TRUE(files[1].content.contains("_mm256_storeu_ps"));
    EXPECT_FALSE(files[1].content.contains("_mm256_fmadd"));
    EXPECT_FALSE(files[1].content.contains("_mm256_load_ps"));
    EXPECT_TRUE(files[1].content.contains(
        "float const* RESTRICT base, float const* RESTRICT value"));
    EXPECT_TRUE(files[1].content.contains("float* RESTRICT out"));
    EXPECT_TRUE(files[1].content.contains("for (; i < vectorized_count; i += 8)"));
    EXPECT_TRUE(files[1].content.contains("for (; i < unrolled_count; i += 32)"));
    EXPECT_FALSE(files[1].content.contains("double const*"));
}

TEST(KernelParser, ValidatesAvx2LabSelection) {
    for (auto const& [needle, replacement] :
         std::vector<std::pair<std::string_view, std::string_view>>{
             {"(operation add_scaled)", "(operation unknown)"},
             {"(type float)", "(type int32)"},
             {"(storage array array scalar)", "(storage array scalar scalar)"},
             {"(storage array array scalar)", "(storage array array)"},
             {"(variant out-of-place)", "(variant in-place)"}}) {
        auto source{std::string{avx2_lab_source}};
        source.replace(source.find(needle), needle.size(), replacement);
        EXPECT_THROW(static_cast<void>(
                         parse("bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", source))),
                     std::runtime_error);
    }
}

TEST(KernelParser, RequiresSelectionOnlyForAvx2LabProfile) {
    auto missing_selection{std::string{avx2_lab_source}};
    auto const selection_begin{missing_selection.find("    (select")};
    auto const selection_end{missing_selection.find("))\n  (type-set", selection_begin)};
    missing_selection.erase(selection_begin, selection_end + 2 - selection_begin);
    EXPECT_THROW(static_cast<void>(parse(
                     "bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", missing_selection))),
                 std::runtime_error);

    auto standard_with_selection{std::string{avx2_lab_source}};
    standard_with_selection.replace(standard_with_selection.find("unreal-avx2-lab"),
                                    std::string{"unreal-avx2-lab"}.size(),
                                    "standard");
    EXPECT_THROW(static_cast<void>(parse(
                     "bad.sbxkernel",
                     codegen::sexpr::lex("bad.sbxkernel", standard_with_selection))),
                 std::runtime_error);
}

TEST(KernelRenderer, RejectsOperationsOutsideTheAvx2LabBoundary) {
    for (auto const& [needle, replacement] :
         std::vector<std::pair<std::string_view, std::string_view>>{
             {"(type float)", "(type double)"},
             {"(expression (+ base (* value scale)))",
              "(expression (- base (* value scale)))"},
             {"(aliasing pairwise-disjoint)", "(aliasing output-disjoint)"}}) {
        auto source{std::string{avx2_lab_source}};
        source.replace(source.find(needle), needle.size(), replacement);
        auto const document{
            parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", source))};
        EXPECT_THROW(static_cast<void>(render(document.modules[0], Profile::unreal_avx2_lab)),
                     std::invalid_argument);
    }
}

TEST(KernelRenderer, GeneratesIsolatedNativeSimdLabSources) {
    auto source{std::string{avx2_lab_source}};
    auto const insertion{source.find("  (type-set")};
    source.insert(insertion,
                  "  (emit native-x86-simd-lab\n"
                  "    (header \"native/Kernels.h\")\n"
                  "    (source \"native/KernelsAvx2.cpp\")\n"
                  "    (avx512-source \"native/KernelsAvx512.cpp\")\n"
                  "    (dispatch-source \"native/KernelsDispatch.cpp\")\n"
                  "    (header-include \"native/Kernels.h\")\n"
                  "    (namespace ml::lab)\n"
                  "    (select\n"
                  "      (operation add_scaled)\n"
                  "      (type float)\n"
                  "      (storage array array scalar)\n"
                  "      (variant out-of-place)))\n");
    auto const document{parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", source))};
    auto const files{render(document.modules[0], Profile::native_x86_simd_lab)};

    ASSERT_EQ(files.size(), 4);
    EXPECT_TRUE(files[0].content.contains("enum class X86SimdBackend"));
    EXPECT_TRUE(files[0].content.contains("add_scaled_scalar"));
    EXPECT_TRUE(files[0].content.contains("add_scaled_dispatch"));
    EXPECT_TRUE(files[1].content.contains("#pragma clang loop vectorize(disable)"));
    EXPECT_TRUE(files[1].content.contains("add_scaled_autovec_avx2"));
    EXPECT_TRUE(files[1].content.contains("add_scaled_avx2_unrolled"));
    EXPECT_TRUE(files[2].content.contains("_mm512_loadu_ps"));
    EXPECT_TRUE(files[2].content.contains("add_scaled_autovec_avx512"));
    EXPECT_TRUE(files[3].content.contains("cpu_features::GetX86Info()"));
    EXPECT_FALSE(files[1].content.contains("_mm512"));
    EXPECT_FALSE(files[3].content.contains("_mm512"));
}

TEST(KernelRenderer, GeneratesNativeSumReduction) {
    auto const document{
        parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", sum_lab_source))};
    auto const files{render(document.modules[0], Profile::native_x86_simd_lab)};

    ASSERT_EQ(files.size(), 6);
    EXPECT_TRUE(files[0].content.contains("float dot_product_scalar("));
    EXPECT_TRUE(files[0].content.contains("float dot_product_dispatch("));
    EXPECT_TRUE(files[0].content.contains("float dot_product_autovec_strict_avx2("));
    EXPECT_TRUE(files[0].content.contains("float dot_product_autovec_relaxed_avx2("));
    EXPECT_TRUE(files[0].content.contains("float dot_product_autovec_strict_avx512("));
    EXPECT_TRUE(files[0].content.contains("float dot_product_autovec_relaxed_avx512("));
    EXPECT_TRUE(files[0].content.contains(
        "float const* ML_KERNEL_LAB_RESTRICT lhs, float const* ML_KERNEL_LAB_RESTRICT rhs"));
    EXPECT_TRUE(files[1].content.contains("float dot_product_autovec_strict_avx2("));
    EXPECT_TRUE(files[1].content.contains("float dot_product_scalar("));
    EXPECT_TRUE(files[1].content.contains(
        "#pragma clang loop vectorize(disable) interleave(disable) unroll(disable)"));
    EXPECT_TRUE(files[1].content.contains("result += (lhs[i] * rhs[i])"));
    EXPECT_TRUE(files[1].content.contains("_mm256_setzero_ps"));
    EXPECT_TRUE(files[1].content.contains("_mm256_mul_ps"));
    EXPECT_TRUE(files[1].content.contains("accumulator_3"));
    EXPECT_TRUE(files[1].content.contains("for (; i < unrolled_count; i += 32)"));
    EXPECT_TRUE(files[1].content.contains("for (; i < vectorized_count; i += 8)"));
    EXPECT_TRUE(files[2].content.contains("_mm512_setzero_ps"));
    EXPECT_TRUE(files[2].content.contains("_mm512_mul_ps"));
    EXPECT_TRUE(files[2].content.contains("float dot_product_avx512_unrolled("));
    EXPECT_TRUE(files[2].content.contains("accumulator_3"));
    EXPECT_TRUE(files[2].content.contains("for (; i < unrolled_count; i += 64)"));
    EXPECT_TRUE(files[2].content.contains("for (; i < vectorized_count; i += 16)"));
    EXPECT_TRUE(files[3].content.contains("return selection().kernel(lhs, rhs, count)"));
    EXPECT_TRUE(files[4].content.contains("#pragma fp_contract(off)"));
    EXPECT_TRUE(files[4].content.contains("float dot_product_autovec_relaxed_avx2("));
    EXPECT_TRUE(files[5].content.contains("#pragma fp_contract(off)"));
    EXPECT_TRUE(files[5].content.contains("float dot_product_autovec_relaxed_avx512("));
}

TEST(KernelParser, RequiresRelaxedSourcesForRelaxedFloatingPointMode) {
    auto source{std::string{sum_lab_source}};
    auto const relaxed_avx2{source.find("    (relaxed-avx2-source")};
    auto const relaxed_avx2_end{source.find('\n', relaxed_avx2) + 1};
    source.erase(relaxed_avx2, relaxed_avx2_end - relaxed_avx2);
    auto const relaxed_avx512{source.find("    (relaxed-avx512-source")};
    auto const relaxed_avx512_end{source.find('\n', relaxed_avx512) + 1};
    source.erase(relaxed_avx512, relaxed_avx512_end - relaxed_avx512);

    EXPECT_THROW(static_cast<void>(
                     parse("bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", source))),
                 std::runtime_error);
}

TEST(KernelParser, KeepsSumReductionsInsideTheNarrowNativeLabBoundary) {
    auto scalar_operand{std::string{sum_lab_source}};
    auto const array_operand{scalar_operand.find("(operand rhs array)")};
    scalar_operand.replace(array_operand,
                           std::string{"(operand rhs array)"}.size(),
                           "(operand rhs scalar)");
    EXPECT_THROW(static_cast<void>(parse(
                     "bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", scalar_operand))),
                 std::runtime_error);

    constexpr std::string_view standard_emission = R"(
(kernel-module reductions
  (emit standard
    (header "DotProduct.h")
    (source "DotProduct.cpp")
    (header-include "DotProduct.h")
    (namespace ml::lab))
  (type-set floating float)
  (sum dot_product
    (types floating)
    (operand lhs array)
    (operand rhs array)
    (aliasing pairwise-disjoint)
    (expression (* lhs rhs))))
)";
    EXPECT_THROW(static_cast<void>(parse(
                     "bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", standard_emission))),
                 std::runtime_error);
}

TEST(KernelRenderer, RejectsInvalidIntegralReferenceFixtures) {
    constexpr std::string_view prefix = R"(
(kernel-module invalid_reference
  (emit standard
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (tests "ArrayKernelsTests.cpp")
    (header-include "ArrayKernels.h")
    (namespace ml))
  (type-set integral int32)
  (map calculate
    (types integral)
    (operand data array)
    (output out)
    (expression )";
    constexpr std::string_view suffix = R"()
    (variants
      (out-of-place calculate))))
)";

    for (auto const expression : {"(* (* data 50000) 50000)", "(/ data 0)"}) {
        auto const source{std::string{prefix} + expression + std::string{suffix}};
        auto const document{
            parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", source))};
        EXPECT_THROW(static_cast<void>(render(document.modules[0], Profile::standard)),
                     std::invalid_argument);
    }
}

TEST(KernelReferenceEvaluator, EvaluatesEveryOperatorForEveryNumericType) {
    constexpr std::string_view source = R"(
(kernel-module reference_operators
  (emit standard
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (tests "ArrayKernelsTests.cpp")
    (header-include "ArrayKernels.h")
    (namespace ml))
  (type-set numeric int32 uint32 float double)
  (map calculate
    (types numeric)
    (operand a array)
    (operand b array)
    (operand c array)
    (operand d array)
    (operand e array)
    (output out)
    (expression (+ (- a b) (* c (/ d e))))
    (variants
      (out-of-place calculate))))
)";

    auto const document{parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", source))};
    auto const files{render(document.modules[0], Profile::standard)};

    ASSERT_EQ(files.size(), 3);
    EXPECT_TRUE(files[2].content.contains("static_cast<std::int32_t>("));
    EXPECT_TRUE(files[2].content.contains("static_cast<std::uint32_t>("));
    EXPECT_TRUE(files[2].content.contains("static_cast<float>("));
    EXPECT_TRUE(files[2].content.contains("static_cast<double>("));
}

TEST(KernelRenderer, SkipsModulesWithoutTheSelectedProfile) {
    auto const document{parse("test.sbxkernel",
                              codegen::sexpr::lex("test.sbxkernel", valid_source))};

    EXPECT_TRUE(render(document.modules[0], Profile::standard).empty());
}

TEST(KernelParser, RejectsInvalidEmissionProfiles) {
    auto unknown{std::string{valid_source}};
    unknown.replace(unknown.find("emit unreal"),
                    std::string{"emit unreal"}.size(),
                    "emit portable");
    EXPECT_THROW(
        static_cast<void>(parse("bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", unknown))),
        std::runtime_error);

    auto duplicate{std::string{valid_source}};
    auto const emission_begin{duplicate.find("  (emit unreal")};
    auto const emission_end{duplicate.find("\n  (type-set", emission_begin)};
    duplicate.insert(emission_end, duplicate.substr(emission_begin, emission_end - emission_begin));
    EXPECT_THROW(
        static_cast<void>(parse("bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", duplicate))),
        std::runtime_error);

    auto ignored_export{std::string{standard_source}};
    ignored_export.replace(ignored_export.find("    (namespace ml::standard)"),
                           std::string{"    (namespace ml::standard)"}.size(),
                           "    (namespace ml::standard)\n    (export UNUSED_API)");
    EXPECT_THROW(static_cast<void>(
                     parse("bad.sbxkernel", codegen::sexpr::lex("bad.sbxkernel", ignored_export))),
                 std::runtime_error);
}

TEST(KernelRenderer, ExpandsStorageCartesianProductWithoutRewritingExpression) {
    constexpr std::string_view source = R"(
(kernel-module arithmetic
  (emit unreal
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ml)
    (export COMPILE_FIXTURE_API))
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

TEST(KernelRenderer, StandardPairwiseDisjointChecksEveryArray) {
    auto source{std::string{standard_source}};
    source.replace(source.find("(variants"), 0, "(aliasing pairwise-disjoint)\n    ");
    auto const document{parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", source))};
    auto const files{render(document.modules[0], Profile::standard)};

    EXPECT_TRUE(files[1].content.contains(
        "require(!ranges_overlap(lhs.data(), lhs.size_bytes(), rhs.data(), rhs.size_bytes()))"));
    EXPECT_TRUE(files[1].content.contains(
        "require(!ranges_overlap(lhs.data(), lhs.size_bytes(), out.data(), out.size_bytes()))"));
    EXPECT_TRUE(files[1].content.contains(
        "require(!ranges_overlap(rhs.data(), rhs.size_bytes(), out.data(), out.size_bytes()))"));
}

TEST(KernelRenderer, RendersNamedConstantsForEachConcreteFloatingType) {
    constexpr std::string_view source = R"(
(kernel-module constants
  (emit unreal
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ml)
    (export COMPILE_FIXTURE_API))
  (emit standard
    (header "standard/ArrayKernels.h")
    (source "standard/ArrayKernels.cpp")
    (tests "standard/ArrayKernelsTests.cpp")
    (header-include "standard/ArrayKernels.h")
    (namespace ml))
  (type-set floating float double)
  (map classify
    (types floating)
    (operand data array)
    (output out)
    (expression (+ data (+ (constant nan)
                           (+ (constant infinity) (constant negative-infinity)))))
    (variants
      (out-of-place classify))))
)";

    auto const files{render_source(source)};

    EXPECT_TRUE(files[1].content.contains("#include <limits>"));
    EXPECT_TRUE(files[1].content.contains("std::numeric_limits<float>::quiet_NaN()"));
    EXPECT_TRUE(files[1].content.contains("std::numeric_limits<float>::infinity()"));
    EXPECT_TRUE(files[1].content.contains("-std::numeric_limits<float>::infinity()"));
    EXPECT_TRUE(files[1].content.contains("std::numeric_limits<double>::quiet_NaN()"));
    EXPECT_TRUE(files[1].content.contains("-std::numeric_limits<double>::infinity()"));

    auto const document{parse("test.sbxkernel", codegen::sexpr::lex("test.sbxkernel", source))};
    auto const standard_files{render(document.modules[0], Profile::standard)};

    EXPECT_TRUE(standard_files[1].content.contains("#include <limits>"));
    EXPECT_TRUE(
        standard_files[1].content.contains("std::numeric_limits<float>::quiet_NaN()"));
    EXPECT_TRUE(standard_files[2].content.contains("std::isnan(expected[i])"));
}

TEST(KernelRenderer, TreatsBareNanAndInfAsOperandReferences) {
    constexpr std::string_view source = R"(
(kernel-module constants
  (emit unreal
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ml)
    (export COMPILE_FIXTURE_API))
  (type-set floating float)
  (map add
    (types floating)
    (operand nan array)
    (operand inf scalar)
    (output out)
    (expression (+ nan inf))
    (variants
      (out-of-place add))))
)";

    auto const files{render_source(source)};

    EXPECT_TRUE(files[1].content.contains("nan[i] + inf"));
    EXPECT_FALSE(files[1].content.contains("#include <limits>"));
}

TEST(KernelRenderer, NormalizesAndTypesFiniteDecimalLiterals) {
    for (auto const& [literal, normalized] :
         std::vector<std::pair<std::string_view, std::string_view>>{
             {"0", "0"}, {"-12", "-12"}, {"+2", "2"}, {".25", "0.25"},
             {"1.", "1.0"}, {"1e-4", "1e-4"}}) {
        auto const files{render_source(literal_source("float", literal))};
        EXPECT_TRUE(files[1].content.contains("static_cast<float>(" +
                                              std::string{normalized} + ")"));
    }
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

TEST(KernelParser, RejectsUnknownAndMalformedConstants) {
    for (auto const expression : {"(constant maximum)", "(constant nan extra)"}) {
        EXPECT_THROW(
            static_cast<void>(render_source(literal_source("float", expression))),
            std::runtime_error);
    }
}

TEST(KernelParser, RejectsNonFiniteConstantsForIntegralTypeSets) {
    EXPECT_THROW(
        static_cast<void>(render_source(literal_source("int32 float", "(constant nan)"))),
        std::runtime_error);
}

TEST(KernelParser, ValidatesUnsignedIntegerLiterals) {
    EXPECT_NO_THROW(static_cast<void>(render_source(literal_source("uint32", "4294967295"))));
    for (auto const literal : {"-1", "4294967296", "0.5"}) {
        EXPECT_THROW(static_cast<void>(render_source(literal_source("uint32", literal))),
                     std::runtime_error);
    }
    EXPECT_THROW(
        static_cast<void>(render_source(literal_source("uint32", "(constant infinity)"))),
        std::runtime_error);
}

TEST(KernelParser, RejectsInvalidOrUnrepresentableDecimalLiterals) {
    for (auto const literal : {"08", "00", "1e", "1e100", "1e-100"}) {
        EXPECT_THROW(static_cast<void>(render_source(literal_source("float", literal))),
                     std::runtime_error);
    }
    for (auto const literal : {"0.5", "2147483648"}) {
        EXPECT_THROW(static_cast<void>(render_source(literal_source("int32", literal))),
                     std::runtime_error);
    }
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
