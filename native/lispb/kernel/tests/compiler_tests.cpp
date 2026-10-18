#include "parser.h"
#include "renderer.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <kernel_codegen/document.h>
#include <lispb/kernel_profile.h>
#include <lispb/strings.h>
#include <lispb/text_artifact.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
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

constexpr std::string_view component_standard_source = R"(
(kernel-module vector_lerp
  (emit standard
    (header "standard/VectorLerp.h")
    (source "standard/VectorLerp.cpp")
    (header-include "standard/VectorLerp.h")
    (namespace ml::standard))
  (type-set floating float)
  (component-map lerp_3d
    (components x y z)
    (types floating)
    (component-operand from array)
    (component-operand to array)
    (operand alpha (array scalar))
    (output out)
    (expression (+ from (* alpha (- to from))))
    (variants
      (out-of-place lerp_3d)
      (in-place from lerp_3d_in_place))))
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

struct ParsedDocument {
    lispb::Strings strings;
    lispb::codegen::sexpr::Sources sources{strings};
    lispb::codegen::sexpr::Syntax syntax{strings, sources};
    Document data{strings};
    lispb::codegen::CppTypes types{strings};
    lispb::codegen::CppIR cpp{strings, types};

    ParsedDocument(std::string_view const path, std::string_view const source) {
        auto const file{sources.add(path, source)};
        parse(syntax, data, file);
    }
};

auto parse_document(std::string_view const path, std::string_view const source) -> ParsedDocument {
    return ParsedDocument{path, source};
}

auto render_source(std::string_view const source) -> std::vector<lispb::TextArtifact> {
    auto document{parse_document("test.lispb", source)};
    return render(document.cpp, document.data, ModuleId{0}, lispb::KernelProfile::unreal);
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
    auto document{parse_document("test.lispb", valid_source)};

    ASSERT_EQ(document.data.modules.size(), 1);
    ASSERT_EQ(document.data.modules.operations(ModuleId{0}).size(), 1);
    EXPECT_EQ(
        document.data.operations.operands(document.data.modules.operations(ModuleId{0})[0]).size(),
        2);
    EXPECT_EQ(
        document.data.operations.variants(document.data.modules.operations(ModuleId{0})[0]).size(),
        2);
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
    EXPECT_NE(files[1].content.find("float const* lhs, float const* rhs, float* RESTRICT out"),
              std::string::npos);
    EXPECT_NE(files[1].content.find("multiply: out and lhs must not overlap"), std::string::npos);
    EXPECT_EQ(files[1].content.find("multiply: lhs and rhs must not overlap"), std::string::npos);
}

TEST(KernelRenderer, GeneratesFusedComponentMapBindings) {
    auto document{parse_document("test.lispb", component_standard_source)};
    auto const files{
        render(document.cpp, document.data, ModuleId{0}, lispb::KernelProfile::standard)};

    ASSERT_EQ(files.size(), 2);
    EXPECT_TRUE(files[0].content.contains("std::span<float const> from_x"));
    EXPECT_TRUE(files[0].content.contains("std::span<float const> from_y"));
    EXPECT_TRUE(files[0].content.contains("std::span<float const> from_z"));
    EXPECT_TRUE(files[0].content.contains("void lerp_3d_in_place("));
    EXPECT_TRUE(
        files[1].content.contains("from_x[i] = from_x[i] + alpha * (to_x[i] - from_x[i]);"));
    EXPECT_TRUE(
        files[1].content.contains("from_y[i] = from_y[i] + alpha * (to_y[i] - from_y[i]);"));
    EXPECT_TRUE(
        files[1].content.contains("from_z[i] = from_z[i] + alpha * (to_z[i] - from_z[i]);"));
}

TEST(KernelParser, RejectsInvalidComponentMapDeclarations) {
    auto duplicate_component_source{std::string{component_standard_source}};
    duplicate_component_source.replace(duplicate_component_source.find("(components x y z)"),
                                       std::string{"(components x y z)"}.size(),
                                       "(components x x)");

    EXPECT_THROW(static_cast<void>(parse_document("test.lispb", duplicate_component_source)),
                 lispb::codegen::sexpr::SourceError);

    auto scalar_target_source{std::string{component_standard_source}};
    scalar_target_source.replace(scalar_target_source.find("(in-place from lerp_3d_in_place)"),
                                 std::string{"(in-place from lerp_3d_in_place)"}.size(),
                                 "(in-place alpha lerp_3d_in_place)");

    EXPECT_THROW(static_cast<void>(parse_document("test.lispb", scalar_target_source)),
                 lispb::codegen::sexpr::SourceError);
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
        auto document{parse_document("test.lispb", source)};
        EXPECT_THROW(static_cast<void>(render(
                         document.cpp, document.data, ModuleId{0}, lispb::KernelProfile::standard)),
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

    auto document{parse_document("test.lispb", source)};
    auto const files{
        render(document.cpp, document.data, ModuleId{0}, lispb::KernelProfile::standard)};

    ASSERT_EQ(files.size(), 3);
    EXPECT_TRUE(files[2].content.contains("static_cast<std::int32_t>("));
    EXPECT_TRUE(files[2].content.contains("static_cast<std::uint32_t>("));
    EXPECT_TRUE(files[2].content.contains("static_cast<float>("));
    EXPECT_TRUE(files[2].content.contains("static_cast<double>("));
}

TEST(KernelRenderer, SkipsModulesWithoutTheSelectedProfile) {
    auto document{parse_document("test.lispb", valid_source)};

    EXPECT_TRUE(
        render(document.cpp, document.data, ModuleId{0}, lispb::KernelProfile::standard).empty());
}

TEST(KernelParser, RejectsInvalidEmissionProfiles) {
    auto unknown{std::string{valid_source}};
    unknown.replace(
        unknown.find("emit unreal"), std::string{"emit unreal"}.size(), "emit portable");
    EXPECT_THROW(static_cast<void>(parse_document("bad.lispb", unknown)), std::runtime_error);

    auto duplicate{std::string{valid_source}};
    auto const emission_begin{duplicate.find("  (emit unreal")};
    auto const emission_end{duplicate.find("\n  (type-set", emission_begin)};
    duplicate.insert(emission_end, duplicate.substr(emission_begin, emission_end - emission_begin));
    EXPECT_THROW(static_cast<void>(parse_document("bad.lispb", duplicate)), std::runtime_error);

    auto ignored_export{std::string{standard_source}};
    ignored_export.replace(ignored_export.find("    (namespace ml::standard)"),
                           std::string{"    (namespace ml::standard)"}.size(),
                           "    (namespace ml::standard)\n    (export UNUSED_API)");
    EXPECT_THROW(static_cast<void>(parse_document("bad.lispb", ignored_export)),
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
    EXPECT_TRUE(files[1].content.contains("a[i] - (b[i] + c)"));
    EXPECT_TRUE(files[1].content.contains("a[i] - (b + c)"));
    EXPECT_TRUE(files[1].content.contains("a - (b[i] + c)"));
    EXPECT_FALSE(files[1].content.contains("a - (b + c)"));
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
    auto document{parse_document("test.lispb", source)};
    auto const files{
        render(document.cpp, document.data, ModuleId{0}, lispb::KernelProfile::standard)};

    EXPECT_TRUE(
        files[1].content.contains("arithmetic_detail::require(!arithmetic_detail::ranges_overlap("
                                  "lhs.data(), lhs.size_bytes(), rhs.data(), rhs.size_bytes()))"));
    EXPECT_TRUE(
        files[1].content.contains("arithmetic_detail::require(!arithmetic_detail::ranges_overlap("
                                  "lhs.data(), lhs.size_bytes(), out.data(), out.size_bytes()))"));
    EXPECT_TRUE(
        files[1].content.contains("arithmetic_detail::require(!arithmetic_detail::ranges_overlap("
                                  "rhs.data(), rhs.size_bytes(), out.data(), out.size_bytes()))"));
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

    auto document{parse_document("test.lispb", source)};
    auto const standard_files{
        render(document.cpp, document.data, ModuleId{0}, lispb::KernelProfile::standard)};

    EXPECT_TRUE(standard_files[1].content.contains("#include <limits>"));
    EXPECT_TRUE(standard_files[1].content.contains("std::numeric_limits<float>::quiet_NaN()"));
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
         std::vector<std::pair<std::string_view, std::string_view>>{{"0", "0"},
                                                                    {"-12", "-12"},
                                                                    {"+2", "2"},
                                                                    {".25", "0.25"},
                                                                    {"1.", "1.0"},
                                                                    {"1e-4", "1e-4"}}) {
        auto const files{render_source(literal_source("float", literal))};
        EXPECT_TRUE(
            files[1].content.contains("static_cast<float>(" + std::string{normalized} + ")"));
    }
}

TEST(KernelParser, RejectsUnknownExpressionOperatorWithLocation) {
    auto source{std::string{valid_source}};
    source.replace(source.find("(* lhs rhs)"), std::string{"(* lhs rhs)"}.size(), "(% lhs rhs)");

    try {
        static_cast<void>(parse_document("bad.lispb", source));
        FAIL() << "Expected parsing to fail";
    } catch (std::runtime_error const& error) {
        EXPECT_NE(std::string{error.what()}.find("bad.lispb:"), std::string::npos);
        EXPECT_NE(std::string{error.what()}.find("unsupported expression operator '%'"),
                  std::string::npos);
    }
}

TEST(KernelParser, RejectsDuplicateStructuralVariants) {
    auto source{std::string{valid_source}};
    source.replace(source.find("(out-of-place multiply)"),
                   std::string{"(out-of-place multiply)"}.size(),
                   "(out-of-place multiply) (out-of-place multiply_again)");

    EXPECT_THROW(static_cast<void>(parse_document("bad.lispb", source)), std::runtime_error);
}

TEST(KernelParser, RejectsScalarOnlyInPlaceTargets) {
    auto source{std::string{valid_source}};
    source.replace(source.find("(operand lhs array)"),
                   std::string{"(operand lhs array)"}.size(),
                   "(operand lhs scalar)");

    EXPECT_THROW(static_cast<void>(parse_document("bad.lispb", source)), std::runtime_error);
}

TEST(KernelParser, RejectsNonFiniteNumericLiterals) {
    auto source{std::string{valid_source}};
    source.replace(source.find("(* lhs rhs)"), std::string{"(* lhs rhs)"}.size(), "(* lhs inf)");

    EXPECT_THROW(static_cast<void>(parse_document("bad.lispb", source)), std::runtime_error);
}

TEST(KernelParser, RejectsUnknownAndMalformedConstants) {
    for (auto const expression : {"(constant maximum)", "(constant nan extra)"}) {
        EXPECT_THROW(static_cast<void>(render_source(literal_source("float", expression))),
                     std::runtime_error);
    }
}

TEST(KernelParser, RejectsNonFiniteConstantsForIntegralTypeSets) {
    EXPECT_THROW(static_cast<void>(render_source(literal_source("int32 float", "(constant nan)"))),
                 std::runtime_error);
}

TEST(KernelParser, ValidatesUnsignedIntegerLiterals) {
    EXPECT_NO_THROW(static_cast<void>(render_source(literal_source("uint32", "4294967295"))));
    for (auto const literal : {"-1", "4294967296", "0.5"}) {
        EXPECT_THROW(static_cast<void>(render_source(literal_source("uint32", literal))),
                     std::runtime_error);
    }
    EXPECT_THROW(static_cast<void>(render_source(literal_source("uint32", "(constant infinity)"))),
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
    duplicate.replace(
        duplicate.find("(map multiply"), std::string{"(map multiply"}.size(), "(map product");
    duplicate.replace(duplicate.find("(in-place lhs multiply_in_place)"),
                      std::string{"(in-place lhs multiply_in_place)"}.size(),
                      "(in-place lhs product_in_place)");
    source.insert(source.rfind(')'), duplicate);

    EXPECT_THROW(static_cast<void>(render_source(source)), std::invalid_argument);
    auto document{parse_document("test.lispb", source)};
    EXPECT_THROW(validate(document.data, ModuleId{0}, lispb::KernelProfile::unreal),
                 std::invalid_argument);
}

}

}
