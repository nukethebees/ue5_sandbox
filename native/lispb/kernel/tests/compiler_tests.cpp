#include "../../tests/text_expectations.h"
#include "parser.h"
#include "renderer.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <kernel_codegen/document.h>
#include <lispb/diagnostics.h>
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
using lispb::test_support::expect_contains_all;
namespace {

constexpr std::string_view valid_source = R"(
(kernel-module arithmetic
  (emit
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ioj))
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

constexpr std::string_view component_source = R"(
(kernel-module vector_lerp
  (emit
    (header "VectorLerp.h")
    (source "VectorLerp.cpp")
    (header-include "VectorLerp.h")
    (namespace ioj))
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
    lispb::Diagnostics diagnostics{strings};
    lispb::codegen::sexpr::Sources sources{strings, diagnostics};
    lispb::codegen::sexpr::Syntax syntax{strings, sources};
    Document data{strings};
    lispb::codegen::CppTypes types{strings};
    lispb::codegen::CppIR cpp{strings, types};

    ParsedDocument(std::string_view const path, std::string_view const source) {
        auto const file{sources.add(path, source)};
        parse(syntax, data, file, lispb::codegen::sexpr::read_forms(syntax, file));
    }
};

auto parse_document(std::string_view const path, std::string_view const source) -> ParsedDocument {
    return ParsedDocument{path, source};
}

auto render_document(ParsedDocument& document) -> std::vector<lispb::TextArtifact> {
    auto const analysis{analyze(document.syntax, document.data, ModuleId{0})};
    return render(document.cpp, document.data, analysis);
}

auto render_source(std::string_view const source) -> std::vector<lispb::TextArtifact> {
    auto document{parse_document("test.lispb", source)};
    return render_document(document);
}

auto literal_source(std::string_view const types, std::string_view const literal) -> std::string {
    return std::string{R"((kernel-module literals
  (emit
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ioj))
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
    auto const& header{files[0].content};
    auto const& source{files[1].content};
    EXPECT_FALSE(header.contains("template"));
    expect_contains_all(
        header,
        {"void multiply(", "std::span<float const> rhs", "float rhs", "std::span<float> lhs"});
    expect_contains_all(source,
                        {"for (std::size_t i{}; i < count; ++i)",
                         "lhs[i] * rhs[i]",
                         "lhs[i] * rhs",
                         "float const* lhs, float const* rhs, float* out"});

    auto const begin{source.find("void multiply(")};
    ASSERT_NE(begin, std::string::npos);
    auto const end{source.find("\n}", begin)};
    ASSERT_NE(end, std::string::npos);
    auto const out_of_place{std::string_view{source}.substr(begin, end - begin)};
    EXPECT_TRUE(
        out_of_place.contains("arithmetic_detail::require(!arithmetic_detail::ranges_overlap("
                              "out.data(), out.size_bytes(), lhs.data(), lhs.size_bytes()))"));
    EXPECT_FALSE(out_of_place.contains("arithmetic_detail::ranges_overlap(lhs.data(), "
                                       "lhs.size_bytes(), rhs.data(), rhs.size_bytes())"));
}

TEST(KernelRenderer, GeneratesFusedComponentMapBindings) {
    auto document{parse_document("test.lispb", component_source)};
    auto const files{render_document(document)};

    ASSERT_EQ(files.size(), 2);
    expect_contains_all(files[0].content,
                        {"std::span<float const> from_x",
                         "std::span<float const> from_y",
                         "std::span<float const> from_z",
                         "void lerp_3d_in_place("});
    expect_contains_all(files[1].content,
                        {"from_x[i] = from_x[i] + alpha * (to_x[i] - from_x[i]);",
                         "from_y[i] = from_y[i] + alpha * (to_y[i] - from_y[i]);",
                         "from_z[i] = from_z[i] + alpha * (to_z[i] - from_z[i]);"});
}

struct InvalidDeclarationCase {
    char const* name;
    std::string_view original;
    std::string_view replacement;
};

class KernelInvalidComponentMap : public testing::TestWithParam<InvalidDeclarationCase> {};

TEST_P(KernelInvalidComponentMap, RejectsInvalidDeclaration) {
    auto const& test{GetParam()};
    auto source{std::string{component_source}};
    source.replace(source.find(test.original), test.original.size(), test.replacement);
    EXPECT_THROW(static_cast<void>(parse_document("test.lispb", source)),
                 lispb::codegen::sexpr::SourceError);
}

INSTANTIATE_TEST_SUITE_P(KernelParser,
                         KernelInvalidComponentMap,
                         testing::Values(
                             InvalidDeclarationCase{
                                 "DuplicateComponent", "(components x y z)", "(components x x)"},
                             InvalidDeclarationCase{"ScalarInPlaceTarget",
                                                    "(in-place from lerp_3d_in_place)",
                                                    "(in-place alpha lerp_3d_in_place)"}),
                         [](auto const& info) { return info.param.name; });

struct InvalidReferenceCase {
    char const* name;
    std::string_view expression;
};

class KernelInvalidIntegralReference : public testing::TestWithParam<InvalidReferenceCase> {};

TEST_P(KernelInvalidIntegralReference, RejectsWithoutProducingArtifacts) {
    constexpr std::string_view prefix = R"(
(kernel-module invalid_reference
  (emit
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (tests "ArrayKernelsTests.cpp")
    (header-include "ArrayKernels.h")
    (namespace ioj))
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

    auto const source{std::string{prefix} + std::string{GetParam().expression} +
                      std::string{suffix}};
    auto document{parse_document("test.lispb", source)};
    EXPECT_THROW(static_cast<void>(analyze(document.syntax, document.data, ModuleId{0})),
                 lispb::codegen::sexpr::SourceError);
    EXPECT_TRUE(document.cpp.files.path.empty());
}

INSTANTIATE_TEST_SUITE_P(KernelAnalysis,
                         KernelInvalidIntegralReference,
                         testing::Values(InvalidReferenceCase{"Overflow",
                                                              "(* (* data 50000) 50000)"},
                                         InvalidReferenceCase{"DivisionByZero", "(/ data 0)"}),
                         [](auto const& info) { return info.param.name; });

TEST(KernelReferenceEvaluator, EvaluatesEveryOperatorForEveryNumericType) {
    constexpr std::string_view source = R"(
(kernel-module reference_operators
  (emit
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (tests "ArrayKernelsTests.cpp")
    (header-include "ArrayKernels.h")
    (namespace ioj))
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
    auto const files{render_document(document)};

    ASSERT_EQ(files.size(), 3);
    expect_contains_all(files[2].content,
                        {"make_array<std::int32_t>(",
                         "make_array<std::uint32_t>(",
                         "make_array<float>(",
                         "make_array<double>("});
}

TEST(KernelRenderer, QualifiesSharedFixturesWhenAnOperandShadowsTheirName) {
    auto source{literal_source("float", "1")};
    source.insert(source.find("(header-include"), "(tests \"ArrayKernelsTests.cpp\")\n    ");
    constexpr std::string_view operand{"fixture_values_0"};
    for (auto position{source.find("data")}; position != std::string::npos;
         position = source.find("data", position + operand.size())) {
        source.replace(position, std::string_view{"data"}.size(), operand);
    }

    auto const files{render_source(source)};

    ASSERT_EQ(files.size(), 3);
    auto const& tests{files[2].content};
    EXPECT_TRUE(tests.contains("inline constexpr auto fixture_values_0{make_array<float>("));
    EXPECT_TRUE(tests.contains("auto fixture_values_0{Generated_literals::fixture_values_0};"))
        << tests;
    EXPECT_FALSE(tests.contains("auto fixture_values_0{fixture_values_0};"));
}

TEST(KernelParser, RejectsInvalidEmissions) {
    auto unknown{std::string{valid_source}};
    unknown.insert(unknown.find("    (namespace ioj)"), "    (unsupported-option value)\n");
    EXPECT_THROW(static_cast<void>(parse_document("bad.lispb", unknown)), std::runtime_error);

    auto duplicate{std::string{valid_source}};
    auto const emission_begin{duplicate.find("  (emit")};
    auto const emission_end{duplicate.find("\n  (type-set", emission_begin)};
    duplicate.insert(emission_end, duplicate.substr(emission_begin, emission_end - emission_begin));
    EXPECT_THROW(static_cast<void>(parse_document("bad.lispb", duplicate)), std::runtime_error);

    auto missing{std::string{valid_source}};
    missing.erase(emission_begin, emission_end - emission_begin);
    EXPECT_THROW(static_cast<void>(parse_document("bad.lispb", missing)), std::runtime_error);
}

TEST(KernelRenderer, ExpandsStorageCartesianProductWithoutRewritingExpression) {
    constexpr std::string_view source = R"(
(kernel-module arithmetic
  (emit
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ioj))
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

    EXPECT_EQ(occurrence_count(files[0].content, "void difference("), 3);
    expect_contains_all(files[1].content,
                        {"a[i] - (b[i] + c)", "a[i] - (b + c)", "a - (b[i] + c)"});
    EXPECT_FALSE(files[1].content.contains("a - (b + c)"));
}

TEST(KernelRenderer, PairwiseDisjointChecksEveryArray) {
    auto source{std::string{valid_source}};
    source.replace(source.find("(variants"), 0, "(aliasing pairwise-disjoint)\n    ");
    auto document{parse_document("test.lispb", source)};
    auto const files{render_document(document)};

    expect_contains_all(files[1].content,
                        {"arithmetic_detail::require(!arithmetic_detail::ranges_overlap("
                         "lhs.data(), lhs.size_bytes(), rhs.data(), rhs.size_bytes()))",
                         "arithmetic_detail::require(!arithmetic_detail::ranges_overlap("
                         "lhs.data(), lhs.size_bytes(), out.data(), out.size_bytes()))",
                         "arithmetic_detail::require(!arithmetic_detail::ranges_overlap("
                         "rhs.data(), rhs.size_bytes(), out.data(), out.size_bytes()))"});
}

TEST(KernelRenderer, RendersNamedConstantsForEachConcreteFloatingType) {
    constexpr std::string_view source = R"(
(kernel-module constants
  (emit
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (tests "ArrayKernelsTests.cpp")
    (header-include "ArrayKernels.h")
    (namespace ioj))
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

    expect_contains_all(files[1].content,
                        {"#include <limits>",
                         "std::numeric_limits<float>::quiet_NaN()",
                         "std::numeric_limits<float>::infinity()",
                         "-std::numeric_limits<float>::infinity()",
                         "std::numeric_limits<double>::quiet_NaN()",
                         "-std::numeric_limits<double>::infinity()"});
}

TEST(KernelRenderer, TreatsBareNanAndInfAsOperandReferences) {
    constexpr std::string_view source = R"(
(kernel-module constants
  (emit
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ioj))
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

class KernelInvalidMap : public testing::TestWithParam<InvalidDeclarationCase> {};

TEST_P(KernelInvalidMap, RejectsInvalidDeclaration) {
    auto const& test{GetParam()};
    auto source{std::string{valid_source}};
    source.replace(source.find(test.original), test.original.size(), test.replacement);
    EXPECT_THROW(static_cast<void>(parse_document("bad.lispb", source)), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(
    KernelParser,
    KernelInvalidMap,
    testing::Values(InvalidDeclarationCase{"DuplicateStructuralVariant",
                                           "(out-of-place multiply)",
                                           "(out-of-place multiply) (out-of-place multiply_again)"},
                    InvalidDeclarationCase{
                        "ScalarInPlaceTarget", "(operand lhs array)", "(operand lhs scalar)"},
                    InvalidDeclarationCase{"BareInfinity", "(* lhs rhs)", "(* lhs inf)"}),
    [](auto const& info) { return info.param.name; });

struct InvalidLiteralCase {
    char const* name;
    std::string_view types;
    std::string_view expression;
};

class KernelInvalidLiteral : public testing::TestWithParam<InvalidLiteralCase> {};

TEST_P(KernelInvalidLiteral, RejectsInvalidOrUnrepresentableValue) {
    auto const& test{GetParam()};
    EXPECT_THROW(static_cast<void>(render_source(literal_source(test.types, test.expression))),
                 std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(
    KernelParser,
    KernelInvalidLiteral,
    testing::Values(InvalidLiteralCase{"UnknownConstant", "float", "(constant maximum)"},
                    InvalidLiteralCase{"MalformedConstant", "float", "(constant nan extra)"},
                    InvalidLiteralCase{"NanInMixedTypeSet", "int32 float", "(constant nan)"},
                    InvalidLiteralCase{"NegativeUnsigned", "uint32", "-1"},
                    InvalidLiteralCase{"UnsignedOverflow", "uint32", "4294967296"},
                    InvalidLiteralCase{"FractionalUnsigned", "uint32", "0.5"},
                    InvalidLiteralCase{"UnsignedInfinity", "uint32", "(constant infinity)"},
                    InvalidLiteralCase{"LeadingZero", "float", "08"},
                    InvalidLiteralCase{"RepeatedZero", "float", "00"},
                    InvalidLiteralCase{"MissingExponent", "float", "1e"},
                    InvalidLiteralCase{"FloatOverflow", "float", "1e100"},
                    InvalidLiteralCase{"FloatUnderflow", "float", "1e-100"},
                    InvalidLiteralCase{"FractionalInteger", "int32", "0.5"},
                    InvalidLiteralCase{"IntegerOverflow", "int32", "2147483648"}),
    [](auto const& info) { return info.param.name; });

TEST(KernelParser, AcceptsUnsignedIntegerMaximum) {
    EXPECT_NO_THROW(static_cast<void>(render_source(literal_source("uint32", "4294967295"))));
}

TEST(KernelAnalysis, RejectsCollidingPublicSignatures) {
    auto source{std::string{valid_source}};
    auto const map_position{source.find("(map multiply")};
    auto duplicate{source.substr(map_position, source.rfind(')') - map_position)};
    duplicate.replace(
        duplicate.find("(map multiply"), std::string{"(map multiply"}.size(), "(map product");
    duplicate.replace(duplicate.find("(in-place lhs multiply_in_place)"),
                      std::string{"(in-place lhs multiply_in_place)"}.size(),
                      "(in-place lhs product_in_place)");
    source.insert(source.rfind(')'), duplicate);

    EXPECT_THROW(static_cast<void>(render_source(source)), lispb::codegen::sexpr::SourceError);
    auto document{parse_document("test.lispb", source)};
    EXPECT_THROW(static_cast<void>(analyze(document.syntax, document.data, ModuleId{0})),
                 lispb::codegen::sexpr::SourceError);
    EXPECT_TRUE(document.cpp.files.path.empty());
}

}

}
