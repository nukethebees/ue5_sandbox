#include "GroupedEnums.h"
#include <ioj/enum_array.h>
#include <ioj/enum_strings.h>
#include <ioj/enum_traits.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>
#include <utility>

namespace codegen_compile_fixture::groups {
static_assert(is_actionable(Action::Kill));
static_assert(!is_actionable(Action::Timer));
static_assert(noexcept(is_actionable(Action::Kill)));
static_assert(sizeof(Action) == sizeof(std::uint8_t));
static_assert(std::to_underlying(Priority::Three) == 0);
static_assert(std::to_underlying(Priority::One) == 1);
static_assert(std::to_underlying(Priority::Four) == 2);
static_assert(std::to_underlying(Priority::Two) == 3);
static_assert(std::to_underlying(Priority::Five) == 4);
static_assert(std::to_underlying(Priority::Six) == 5);
static_assert(std::to_underlying(Manual::One) == 0);
static_assert(std::to_underlying(Manual::Six) == 5);
static_assert(std::to_underlying(Automatic::Three) != 5);

template <typename Enum>
class GeneratedOverlappingGroups : public testing::Test {};
using OrderingModes = testing::Types<Priority, Automatic, Manual>;
TYPED_TEST_SUITE(GeneratedOverlappingGroups, OrderingModes);

TYPED_TEST(GeneratedOverlappingGroups, MatchesAuthoredMembershipForEveryEnumerator) {
    using E = TypeParam;
    for (auto const value : ioj::EnumTraits<E>::values) {
        EXPECT_EQ(is_A(value),
                  value == E::One || value == E::Two || value == E::Three || value == E::Six);
        EXPECT_EQ(is_B(value), value == E::One || value == E::Three || value == E::Four);
        EXPECT_EQ(is_C(value), value == E::Two || value == E::Three || value == E::Five);
    }
    EXPECT_FALSE(is_A(static_cast<E>(255)));
    EXPECT_FALSE(is_B(static_cast<E>(255)));
    EXPECT_FALSE(is_C(static_cast<E>(255)));
}

TEST(GeneratedEnumGroups, OrderedHelpersAndPackedFieldsAgreeForEveryEnumerator) {
    ioj::EnumArray<int, Action> counts;
    for (auto const value : ioj::EnumTraits<Action>::values) {
        EXPECT_EQ(is_actionable(value),
                  value == Action::Kill || value == Action::Move || value == Action::Kills);
        counts[value] = static_cast<int>(std::to_underlying(value)) + 1;
        EXPECT_EQ(counts[value], static_cast<int>(std::to_underlying(value)) + 1);
        EXPECT_EQ(ActionHandle{value}.kind(), value);
        EXPECT_TRUE(ActionHandle{value}.is_valid());
        EXPECT_EQ(try_parse_serialized_action(ioj::to_serialized_string(value)), value);
    }
    EXPECT_EQ(ioj::enum_count<Action>(), 5U);
    EXPECT_EQ(ioj::to_string(Action::Kill), "Kill");
    EXPECT_EQ(ioj::to_serialized_string(Action::Move), "move");
    EXPECT_FALSE(is_actionable(static_cast<Action>(255)));
}

TEST(GeneratedEnumGroups, SparseSignedRangesDoNotAcceptHolesOrValuesOutsideTheEnum) {
    for (int value{-128}; value <= 127; ++value) {
        EXPECT_EQ(is_selected(static_cast<Sparse>(value)),
                  value == -3 || value == -2 || value == 5);
    }
    EXPECT_TRUE(is_selected(Sparse::Alias));
}

TEST(GeneratedEnumGroups, FragmentedAndSymbolicMembershipMatchEveryValue) {
    for (unsigned value{}; value < 256; ++value) {
        EXPECT_EQ(is_selected(static_cast<Fragmented>(value)), value < 10 && value % 2 == 0);
        EXPECT_EQ(is_selected(static_cast<Symbolic>(value)), value == 2);
    }
}
}
