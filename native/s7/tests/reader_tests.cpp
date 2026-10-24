#include <ioj/constrained_numbers.h>
#include <ioj/constrained_strings.h>
#include <ioj/s7/field_constraints.h>
#include <ioj/s7/interpreter.h>
#include <ioj/s7/reader.h>

#include <gtest/gtest.h>

namespace ioj::s7::tests {
template <typename T>
auto read_source(std::string_view const source) -> ReadResult<T> {
    Interpreter interpreter;
    auto ast{interpreter.evaluate_ast(source)};
    if (!ast) {
        return std::unexpected{std::move(ast.error())};
    }
    return read<T>(Reader{*ast}, ast->root, "test");
}

TEST(SchemeReader, ChecksNumericConversionBeforeCasting) {
    EXPECT_EQ(*read_source<std::int64_t>("9007199254740993"), INT64_C(9007199254740993));
    EXPECT_EQ(*read_source<std::int64_t>("-9223372036854775808"),
              std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(*read_source<std::int32_t>("4294967294/2"), std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(*read_source<std::uint64_t>("18446744073709549568.0"),
              UINT64_C(18446744073709549568));
    for (auto const source :
         {"2147483648", "-2147483649", "1.5", "3/2", "+nan.0", "+inf.0", "#t"}) {
        SCOPED_TRACE(source);
        auto const result{read_source<std::int32_t>(source)};
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().front().code, AstErrorCode::InvalidNumber);
        EXPECT_EQ(result.error().front().node_path, "test");
    }
    EXPECT_FALSE(read_source<std::int64_t>("9223372036854775808.0"));
    EXPECT_FALSE(read_source<std::uint64_t>("18446744073709551616.0"));
    EXPECT_FALSE(read_source<std::uint64_t>("-1"));
    EXPECT_FALSE(read_source<float>("1e100"));
    EXPECT_FLOAT_EQ(*read_source<float>("1/4"), 0.25f);
    EXPECT_TRUE(std::isinf(*read_source<double>("+inf.0")));
}

TEST(SchemeReader, ReadsConstrainedValuesWithoutInvalidDefaults) {
    auto const positive{read_source<PositiveFinite<float>>("0.25")};
    ASSERT_TRUE(positive);
    EXPECT_EQ(positive->get(), 0.25f);
    EXPECT_FALSE(read_source<PositiveFinite<float>>("0"));
    EXPECT_FALSE(read_source<PositiveFinite<float>>("+inf.0"));
    EXPECT_EQ(read_source<NonNegative<int>>("0")->get(), 0);
    EXPECT_FALSE(read_source<NonNegative<int>>("-1"));
    EXPECT_EQ(read_source<NonEmptyString>("\"name\"")->get_ref(), "name");
    EXPECT_FALSE(read_source<NonEmptyString>("\"\""));
    EXPECT_FALSE(read_source<std::string>("'name"));
    EXPECT_FALSE(read_source<bool>("0"));
    EXPECT_FALSE(*read_source<bool>("#f"));
}

TEST(SchemeReader, ComposesContainersAndDistinguishesAbsence) {
    using Values = std::vector<std::array<std::optional<NonNegative<int>>, 2>>;
    auto const values{read_source<Values>("'((1 :none) (0 2))")};
    ASSERT_TRUE(values);
    ASSERT_EQ(values->size(), 2u);
    EXPECT_EQ((*values)[0][0]->get(), 1);
    EXPECT_FALSE((*values)[0][1]);
    EXPECT_EQ((*values)[1][0]->get(), 0);
    EXPECT_EQ((*values)[1][1]->get(), 2);
    EXPECT_TRUE(read_source<std::vector<int>>("'()")->empty());
    EXPECT_TRUE((read_source<std::array<int, 0>>("'()")));
    EXPECT_FALSE((read_source<std::array<int, 2>>("'(1)")));
    auto const boolean{read_source<std::optional<bool>>("#f")};
    ASSERT_TRUE(boolean);
    ASSERT_TRUE(*boolean);
    EXPECT_FALSE(**boolean);
    auto const empty{read_source<std::optional<std::vector<int>>>("'()")};
    ASSERT_TRUE(empty);
    ASSERT_TRUE(*empty);
    EXPECT_TRUE((*empty)->empty());
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
    EXPECT_TRUE(constraints::minimum(std::optional<int>{}, 5));
    EXPECT_FALSE(constraints::minimum(std::optional{4}, 5));
    EXPECT_TRUE(constraints::minimum(5, 5));
    EXPECT_TRUE(constraints::maximum(5, 5));
    EXPECT_FALSE(constraints::maximum(6, 5));
    EXPECT_FALSE(constraints::maximum(std::numeric_limits<std::uint64_t>::max(), INT64_MAX));
    EXPECT_FALSE(constraints::minimum(-1, 0u));
    EXPECT_FALSE(constraints::minimum(INT64_MAX, std::ldexp(1.0, 63)));
    EXPECT_FALSE(constraints::maximum(std::ldexp(1.0, 64), UINT64_MAX));
    EXPECT_TRUE(constraints::maximum(INT64_MAX, std::ldexp(1.0, 63)));
    EXPECT_TRUE(constraints::minimum(std::ldexp(1.0, 64), UINT64_MAX));
    EXPECT_FALSE(constraints::minimum(std::int8_t{127}, 127.1));
    EXPECT_FALSE(constraints::maximum(std::int8_t{-128}, -128.1));
    EXPECT_TRUE(constraints::maximum(4, 4.5));
    EXPECT_FALSE(constraints::minimum(4, 4.5));
    EXPECT_FALSE(constraints::maximum(0, std::numeric_limits<double>::quiet_NaN()));
}
}
