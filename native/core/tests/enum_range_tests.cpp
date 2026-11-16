#include <ioj/enum_range_array.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>

namespace enum_range_tests {
enum class Key : std::int8_t { First = -2, Middle = -1, Last = 0, Positive = 4, End = 6 };
inline constexpr ioj::EnumeratorRange signed_range{Key::Last, Key::First};
inline constexpr ioj::EnumeratorRange positive_range{Key::Positive, Key::End};
inline constexpr ioj::EnumeratorRange singleton{Key::Middle, Key::Middle};

TEST(EnumeratorRange, NormalizesOrderedReversedAndEqualEndpoints) {
    static_assert(signed_range.first() == Key::First);
    static_assert(signed_range.last() == Key::Last);
    static_assert(singleton.first() == singleton.last());
    static_assert(noexcept(ioj::EnumeratorRange{Key::First, Key::Last}));
    for (auto const first : {Key::First, Key::Last}) {
        auto const last{first == Key::First ? Key::Last : Key::First};
        ioj::EnumeratorRange const range{first, last};
        EXPECT_EQ(range.first(), Key::First);
        EXPECT_EQ(range.last(), Key::Last);
    }
}

TEST(EnumRangeArray, UsesOnlyArrayStorageAndIndexesInclusiveBoundaries) {
    using Values = ioj::EnumRangeArray<int, signed_range>;
    static_assert(sizeof(Values) == sizeof(std::array<int, 3>));
    static_assert(std::is_same_v<decltype(std::declval<Values&>()[Key::First]), int&>);
    static_assert(std::is_same_v<decltype(std::declval<Values const&>()[Key::First]), int const&>);
    constexpr Values values{std::array{1, 2, 3}};
    static_assert(values[Key::First] == 1);
    static_assert(values[Key::Middle] == 2);
    static_assert(values[Key::Last] == 3);
    static_assert(values.size() == 3);
    EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));

    ioj::EnumRangeArray<int, positive_range> positive;
    positive[Key::Positive] = 4;
    positive[Key::End] = 6;
    EXPECT_TRUE(std::ranges::equal(positive, std::array{4, 0, 6}));
    constexpr ioj::EnumRangeArray<int, singleton> one{7};
    static_assert(one.size() == 1 && one[Key::Middle] == 7);
}

TEST(EnumRangeArray, FillsAndIteratesMutableAndConstArrays) {
    using Values = ioj::EnumRangeArray<int, signed_range>;
    constexpr Values filled{8};
    static_assert(filled[Key::First] == 8 && filled[Key::Last] == 8);
    Values values{4};
    for (auto& value : values) {
        ++value;
    }
    EXPECT_EQ(values, Values{5});
    values.fill(9);
    EXPECT_TRUE(std::ranges::equal(std::as_const(values), std::array{9, 9, 9}));
}
}
