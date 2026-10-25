#include <ioj/constrained_numbers.h>
#include <ioj/constrained_strings.h>
#include <ioj/s7/field_constraints.h>
#include <ioj/s7/interpreter.h>
#include <ioj/s7/reader.h>
#include <ioj/s7/source_format.h>
#include <ioj/testing/structural_diff.h>

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace ioj::s7::tests {
using ioj::testing::structurally_equal;
template <typename T>
auto read_source(std::string_view const source) -> ReadResult<T> {
    Interpreter interpreter;
    auto ast{interpreter.evaluate_ast(source)};
    if (!ast) {
        return std::unexpected{std::move(ast.error())};
    }
    return read<T>(Reader{*ast}, ast->root, "test");
}

enum class NumericType { Int32, Int64, UInt64, Float, Double };

struct NumericReaderCase {
    char const* name;
    NumericType type;
    std::vector<std::string_view> accepted;
    std::vector<std::string_view> rejected;
    AstErrorCode error_code{AstErrorCode::InvalidNumber};
};

template <typename T>
void check_numeric_sources(NumericReaderCase const& test, bool const accepted) {
    auto const& sources{accepted ? test.accepted : test.rejected};
    for (auto const source : sources) {
        SCOPED_TRACE(source);
        auto const result{read_source<T>(source)};
        EXPECT_EQ(result.has_value(), accepted);
        if (!accepted && !result) {
            ASSERT_EQ(result.error().size(), 1u);
            EXPECT_EQ(result.error().front().code, test.error_code);
            EXPECT_EQ(result.error().front().node_path, "test");
        }
    }
}

void check_numeric_sources(NumericReaderCase const& test, bool const accepted) {
    switch (test.type) {
        case NumericType::Int32:
            return check_numeric_sources<std::int32_t>(test, accepted);
        case NumericType::Int64:
            return check_numeric_sources<std::int64_t>(test, accepted);
        case NumericType::UInt64:
            return check_numeric_sources<std::uint64_t>(test, accepted);
        case NumericType::Float:
            return check_numeric_sources<float>(test, accepted);
        case NumericType::Double:
            return check_numeric_sources<double>(test, accepted);
    }
}

using SchemeNumericReader = ::testing::TestWithParam<NumericReaderCase>;

TEST_P(SchemeNumericReader, AcceptsValidSources) {
    check_numeric_sources(GetParam(), true);
}

TEST_P(SchemeNumericReader, RejectsInvalidSources) {
    check_numeric_sources(GetParam(), false);
}

INSTANTIATE_TEST_SUITE_P(
    NumericConversions,
    SchemeNumericReader,
    ::testing::Values(NumericReaderCase{"Int32",
                                        NumericType::Int32,
                                        {"4294967294/2"},
                                        {"2147483648",
                                         "-2147483649",
                                         "1.5",
                                         "3/2",
                                         literals::nan,
                                         literals::positive_infinity,
                                         literals::true_value}},
                      NumericReaderCase{"Int64",
                                        NumericType::Int64,
                                        {"9007199254740993", "-9223372036854775808"},
                                        {"9223372036854775808.0"}},
                      NumericReaderCase{"UInt64",
                                        NumericType::UInt64,
                                        {"18446744073709549568.0"},
                                        {"18446744073709551616.0", "-1"}},
                      NumericReaderCase{"Float", NumericType::Float, {"1/4"}, {"1e100"}},
                      NumericReaderCase{
                          "Double",
                          NumericType::Double,
                          {literals::positive_infinity, literals::negative_infinity, literals::nan},
                          {literals::true_value},
                          AstErrorCode::InvalidType}),
    [](auto const& info) { return info.param.name; });

TEST(SchemeReader, PreservesNumericValues) {
    EXPECT_EQ(read_source<std::int64_t>("9007199254740993"), INT64_C(9007199254740993));
    EXPECT_EQ(read_source<std::int64_t>("-9223372036854775808"),
              std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(read_source<std::int32_t>("4294967294/2"), std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(read_source<std::uint64_t>("18446744073709549568.0"), UINT64_C(18446744073709549568));
    EXPECT_EQ(read_source<float>("1/4"), 0.25f);
    EXPECT_EQ(read_source<double>(literals::positive_infinity),
              std::numeric_limits<double>::infinity());
}

struct RealFormattingCase {
    char const* name;
    double value;
};

using SchemeRealFormatting = ::testing::TestWithParam<RealFormattingCase>;

TEST_P(SchemeRealFormatting, PreservesValueAndType) {
    Interpreter interpreter;
    auto const value{GetParam().value};
    auto const source{format_real(value)};
    SCOPED_TRACE(source);
    auto const ast{interpreter.evaluate_ast(source)};
    ASSERT_TRUE(ast);
    ASSERT_EQ(ast->node(ast->root).kind, NodeKind::Real);
    auto const actual{ast->real(ast->root)};
    if (std::isnan(value)) {
        EXPECT_TRUE(std::isnan(actual));
    } else {
        EXPECT_EQ(actual, value);
        EXPECT_EQ(std::signbit(actual), std::signbit(value));
    }
}

INSTANTIATE_TEST_SUITE_P(
    RealValues,
    SchemeRealFormatting,
    ::testing::Values(
        RealFormattingCase{"Zero", 0.0},
        RealFormattingCase{"NegativeZero", -0.0},
        RealFormattingCase{"Whole", 1.0},
        RealFormattingCase{"Fractional", -1.25},
        RealFormattingCase{"Maximum", std::numeric_limits<double>::max()},
        RealFormattingCase{"PositiveInfinity", std::numeric_limits<double>::infinity()},
        RealFormattingCase{"NegativeInfinity", -std::numeric_limits<double>::infinity()},
        RealFormattingCase{"NaN", std::numeric_limits<double>::quiet_NaN()}),
    [](auto const& info) { return info.param.name; });

TEST(SchemeSourceFormatting, FormatsSubnormalValue) {
    EXPECT_EQ(format_real(std::numeric_limits<double>::denorm_min()), "5e-324");
}

TEST(SchemeReader, ReadsConstrainedValuesWithoutInvalidDefaults) {
    auto const positive{read_source<PositiveFinite<float>>("0.25")};
    ASSERT_TRUE(positive);
    EXPECT_EQ(positive->get(), 0.25f);
    EXPECT_FALSE(read_source<PositiveFinite<float>>("0"));
    EXPECT_FALSE(read_source<PositiveFinite<float>>(literals::positive_infinity));
    EXPECT_EQ(read_source<NonNegative<int>>("0")->get(), 0);
    EXPECT_FALSE(read_source<NonNegative<int>>("-1"));
    EXPECT_EQ(read_source<NonEmptyString>("\"name\"")->get_ref(), "name");
    EXPECT_FALSE(read_source<NonEmptyString>("\"\""));
    EXPECT_FALSE(read_source<std::string>("'name"));
    EXPECT_FALSE(read_source<bool>("0"));
    EXPECT_FALSE(*read_source<bool>(literals::false_value));
}

TEST(SchemeReader, ComposesContainersAndDistinguishesAbsence) {
    using Values = std::vector<std::array<std::optional<NonNegative<int>>, 2>>;
    auto const values{read_source<Values>("'((1 :none) (0 2))")};
    ASSERT_TRUE(values);
    Values const expected{{NonNegative<int>::try_make(1), std::nullopt},
                          {NonNegative<int>::try_make(0), NonNegative<int>::try_make(2)}};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *values);
    EXPECT_TRUE(read_source<std::vector<int>>("'()")->empty());
    EXPECT_TRUE((read_source<std::array<int, 0>>("'()")));
    EXPECT_FALSE((read_source<std::array<int, 2>>("'(1)")));
    auto const boolean{read_source<std::optional<bool>>(literals::false_value)};
    ASSERT_TRUE(boolean);
    EXPECT_PRED_FORMAT2(structurally_equal, std::optional{false}, *boolean);
    auto const empty{read_source<std::optional<std::vector<int>>>("'()")};
    ASSERT_TRUE(empty);
    EXPECT_PRED_FORMAT2(structurally_equal, std::optional{std::vector<int>{}}, *empty);
    auto const invalid{read_source<Values>("'((1 -2) (3 -4))")};
    ASSERT_FALSE(invalid);
    ASSERT_EQ(invalid.error().size(), 2u);
    EXPECT_EQ(invalid.error()[0].node_path, "test[0][1]");
    EXPECT_EQ(invalid.error()[1].node_path, "test[1][1]");
}

TEST(SchemeReader, AppliesConstraintsToValuesAndPresentOptionals) {
    EXPECT_FALSE(constraints::nonzero(0));
    EXPECT_TRUE(constraints::nonzero(-1));
    EXPECT_FALSE(constraints::positive(0.f));
    EXPECT_TRUE(constraints::nonnegative(0.f));
    EXPECT_FALSE(constraints::finite(std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(constraints::finite(3));
    EXPECT_FALSE(constraints::nonempty(std::string{}));
    EXPECT_TRUE(constraints::nonempty(*NonEmptyString::try_make("value")));
    EXPECT_TRUE(constraints::minimum(5)(std::optional<int>{}));
    EXPECT_FALSE(constraints::minimum(5)(std::optional{4}));
    EXPECT_TRUE(constraints::minimum(5)(5));
    EXPECT_TRUE(constraints::maximum(5)(5));
    EXPECT_FALSE(constraints::maximum(5)(6));
    EXPECT_FALSE(constraints::maximum(INT64_MAX)(std::numeric_limits<std::uint64_t>::max()));
    EXPECT_FALSE(constraints::minimum(0u)(-1));
    EXPECT_FALSE(constraints::minimum(std::ldexp(1.0, 63))(INT64_MAX));
    EXPECT_FALSE(constraints::maximum(UINT64_MAX)(std::ldexp(1.0, 64)));
    EXPECT_TRUE(constraints::maximum(std::ldexp(1.0, 63))(INT64_MAX));
    EXPECT_TRUE(constraints::minimum(UINT64_MAX)(std::ldexp(1.0, 64)));
    EXPECT_FALSE(constraints::minimum(127.1)(std::int8_t{127}));
    EXPECT_FALSE(constraints::maximum(-128.1)(std::int8_t{-128}));
    EXPECT_TRUE(constraints::maximum(4.5)(4));
    EXPECT_FALSE(constraints::minimum(4.5)(4));
    EXPECT_FALSE(constraints::maximum(std::numeric_limits<double>::quiet_NaN())(0));
}
}
