#include <ioj/structural_diff.h>
#include <ioj/testing/structural_diff.h>

#include <gtest/gtest.h>
#include <NestedSettings.h>
#include <ReadableRecords.h>
#include <Records.h>

#include <limits>
#include <stdexcept>

namespace lispb::tests {
using namespace codegen_compile_fixture;
using ioj::DiffContext;
using ioj::DiffResult;

template <typename T, auto field>
concept AccessibleField = requires(T& value) { T::template get_field<field>(value); };
template <typename T, auto field>
concept RvalueAccessibleField = requires { T::template get_field<field>(std::declval<T&&>()); };

static_assert(std::is_aggregate_v<ApiPair>);
static_assert(sizeof(ApiPair) == 2 * sizeof(float));
static_assert(
    std::same_as<decltype(ApiPair::get_field<ApiPair::Field::x>(std::declval<ApiPair&>())),
                 float&>);
static_assert(
    std::same_as<decltype(ApiPair::get_field<ApiPair::Field::x>(std::declval<ApiPair const&>())),
                 float const&>);
static_assert(!AccessibleField<ApiPair, ApiPair::Field::Count>);
static_assert(!AccessibleField<ApiPair, static_cast<ApiPair::Field>(255)>);
static_assert(!AccessibleField<EmptySettings, EmptySettings::Field::Count>);
static_assert(!RvalueAccessibleField<ApiPair, ApiPair::Field::x>);
static_assert(WeaponSettings::field_name(WeaponSettings::Field::turn_rate) == "turn_rate");
static_assert(std::same_as<decltype(ReadableRecord::get_field<ReadableRecord::Field::points>(
                               std::declval<ReadableRecord const&>())),
                           std::array<float, 3> const&>);

struct ForwardedVisitor {
    template <auto field>
    constexpr auto operator()(float& value) & -> int {
        value = 5.f;
        return 1;
    }

    template <auto field>
    constexpr auto operator()(float& value) && -> int {
        value = 6.f;
        return 2;
    }
};

static_assert([] {
    ApiPair pair{};
    ForwardedVisitor visitor;
    ApiPair::visit_field(pair, ApiPair::Field::x, visitor);
    ApiPair::visit_field(pair, ApiPair::Field::y, ForwardedVisitor{});
    return pair.x == 5.f && pair.y == 6.f;
}());

TEST(StructuralDiff, AccessorsAndVisitorsReferToTheSameMembers) {
    ApiPair pair{1.f, 2.f};
    ApiPair::get_field<ApiPair::Field::x>(pair) = 3.f;
    EXPECT_EQ(pair.x, 3.f);
    ApiPair::visit_field(pair, ApiPair::Field::x, [&]<auto field>(auto& member) {
        EXPECT_EQ(&member, &ApiPair::get_field<field>(pair));
        member = 4.f;
    });
    EXPECT_EQ(pair.x, 4.f);
    DiffResult result;
    DiffContext context{result};
    context.diff("empty", EmptySettings{}, EmptySettings{});
    context.diff("pair", pair, pair);
    EXPECT_TRUE(result.empty()) << result.message();
}

TEST(StructuralDiff, GoogleTestPredicateRetainsExpressionsAndNestedDifferences) {
    ActorSettings expected;
    auto actual{expected};
    EXPECT_PRED_FORMAT2(ioj::testing::structurally_equal, expected, actual);
    actual.weapon.damage = 17;
    auto const failure{ioj::testing::structurally_equal("expected", "actual", expected, actual)};
    EXPECT_FALSE(failure);
    EXPECT_EQ(
        std::string_view{failure.message()},
        "Comparing actual with expected:\nactual.weapon.damage\n  Expected: 5\n  Actual:   17\n");
}

#ifndef NDEBUG
TEST(GeneratedRecordsDeathTest, RejectsInvalidRuntimeFields) {
    ApiPair pair{};
    EXPECT_DEATH(ApiPair::visit_field(pair, ApiPair::Field::Count, []<auto>(auto&) {}),
                 "Invalid record field");
    EXPECT_DEATH(ApiPair::field_name(static_cast<ApiPair::Field>(255)), "Invalid record field");
}
#endif

TEST(StructuralDiff, ReportsDeepRecordsSequencesAndPresenceWithoutLosingSiblingPaths) {
    std::vector<SettingsTarget> expected(2);
    auto actual{expected};
    actual[1].first.weapon.damage = 10;
    actual[1].first.optional_limit.reset();
    actual[1].second.weapon.turn_rate = 9.f;
    actual.emplace_back();
    DiffResult result;
    DiffContext context{result};
    context.diff("fighters", expected, actual);
    context.diff("next", 1, 2);
    EXPECT_EQ(
        result.differences,
        (std::vector<ioj::Difference>{{"fighters.size", "2", "3"},
                                      {"fighters[1].first.weapon.damage", "5", "10"},
                                      {"fighters[1].first.optional_limit", "present", "absent"},
                                      {"fighters[1].second.weapon.turn_rate", "2", "9"},
                                      {"next", "1", "2"}}));
}

TEST(StructuralDiff, DescendsThroughConstrainedRecordsArraysAndOptionalValues) {
    RangeSettings expected;
    RangeSettings actual{.range = *Range::try_make({1, 9})};
    DiffResult result;
    DiffContext context{result};
    context.diff("range", std::optional{expected}, std::optional{actual});
    context.diff("array", std::array{1, 2}, std::array{1, 3});
    context.diff("bits", std::vector<bool>{true}, std::vector<bool>{false});
    EXPECT_EQ(result.differences,
              (std::vector<ioj::Difference>{{"range.range.minimum", "0", "1"},
                                            {"range.range.maximum", "10", "9"},
                                            {"array[1]", "2", "3"},
                                            {"bits[0]", "true", "false"}}));
}

struct CustomComparison {
    int value;
    bool fail{};
};

void diff_members(CustomComparison const& expected,
                  CustomComparison const& actual,
                  DiffContext& context) {
    context.diff("value", expected.value, actual.value);
    if (actual.fail) {
        throw std::runtime_error{"custom comparison failed"};
    }
}

TEST(StructuralDiff, RestoresPathsAfterCustomComparisonsThrowAndOwnsDiagnosticStrings) {
    DiffResult result;
    DiffContext context{result};
    {
        std::string name{"temporary"};
        EXPECT_THROW(
            context.diff(std::string_view{name}, CustomComparison{1}, CustomComparison{2, true}),
            std::runtime_error);
    }
    context.diff("sibling", 3, 4);
    EXPECT_EQ(
        result.message(),
        "temporary.value\n  Expected: 1\n  Actual:   2\nsibling\n  Expected: 3\n  Actual:   4\n");
}

struct Opaque {
    auto operator==(Opaque const&) const -> bool = default;

    int value;
};

TEST(StructuralDiff, UsesExactLeafSemanticsAndBoundsDiagnosticStorage) {
    DiffResult result{.limit = 5};
    DiffContext context{result};
    context.diff("opaque", Opaque{1}, Opaque{2});
    context.diff(
        "nan", std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());
    context.diff("infinity",
                 std::numeric_limits<double>::infinity(),
                 std::numeric_limits<double>::infinity());
    context.diff("zero", -0.0, 0.0);
    context.diff("enum", ReaderEnum::Mixed, static_cast<ReaderEnum>(1));
    int first{};
    int second{};
    context.diff("pointer", &first, &second);
    context.diff("null", static_cast<char const*>(nullptr), static_cast<char const*>("text"));
    context.diff("limited", std::array{1, 2}, std::array{3, 4});
    ASSERT_EQ(result.differences.size(), 5u);
    EXPECT_EQ(result.differences[0].expected, "<unformattable value>");
    EXPECT_EQ(result.differences[1].path, "nan");
    EXPECT_EQ(result.differences[2].actual, "1");
    EXPECT_NE(result.differences[3].expected, result.differences[3].actual);
    EXPECT_EQ(result.omitted, 2u);
    EXPECT_TRUE(result.message().ends_with("2 further differences omitted\n"));
}
}
