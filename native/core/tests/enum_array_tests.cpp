#include "ioj/enum_array.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <type_traits>
#include <utility>

namespace {
enum class Key {
    First,
    Second,
};
enum class Column {
    Left,
    Middle,
    Right,
};
enum class Layer {
    Front,
    Back,
};
}

template <>
constexpr auto ioj::enum_count<Key>() noexcept -> std::size_t {
    return 2;
}

template <>
constexpr auto ioj::enum_count<Column>() noexcept -> std::size_t {
    return 3;
}

template <>
constexpr auto ioj::enum_count<Layer>() noexcept -> std::size_t {
    return 2;
}

TEST(EnumArray, IndexesByEnum) {
    ioj::EnumArray<int, Key> values;
    values[Key::First] = 10;
    values[Key::Second] = 20;

    EXPECT_EQ(values[Key::First], 10);
    EXPECT_EQ(values[Key::Second], 20);
    static_assert(values.size() == 2);
}

TEST(EnumArray, IndexesMultipleDimensionsAndIteratesInRowMajorOrder) {
    ioj::EnumArray<int, Key, Column, Layer> values;
    int next{};
    for (auto const key : {Key::First, Key::Second}) {
        for (auto const column : {Column::Left, Column::Middle, Column::Right}) {
            for (auto const layer : {Layer::Front, Layer::Back}) {
                values[key, column, layer] = next++;
            }
        }
    }

    static_assert(values.size() == 12);
    EXPECT_TRUE(std::ranges::equal(values, std::array{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}));
    EXPECT_EQ((values[Key::Second, Column::Middle, Layer::Front]), 8);
    EXPECT_EQ((values[Key::Second, Column::Right, Layer::Back]), 11);
}

TEST(EnumArray, PreservesConstReferencesAndSupportsConstexprConstruction) {
    using Values = ioj::EnumArray<int, Key, Column>;
    static_assert(
        std::is_same_v<decltype(std::declval<Values&>()[Key::First, Column::Left]), int&>);
    static_assert(std::is_same_v<decltype(std::declval<Values const&>()[Key::First, Column::Left]),
                                 int const&>);
    constexpr Values values{std::array{1, 2, 3, 4, 5, 6}};
    static_assert(values[Key::First, Column::Right] == 3);
    static_assert(values[Key::Second, Column::Left] == 4);
    EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5, 6}));

    Values filled{7};
    EXPECT_TRUE(std::ranges::all_of(filled, [](int const value) { return value == 7; }));
    filled.fill(9);
    EXPECT_EQ(filled, Values{9});
}

TEST(EnumArray, SupportsNestedRowAccess) {
    using Row = ioj::EnumArray<int, Column>;
    ioj::EnumArray<Row, Key> values;
    auto& second{values[Key::Second]};
    second[Column::Middle] = 8;

    EXPECT_EQ(values[Key::Second][Column::Middle], 8);
    EXPECT_EQ(values[Key::First][Column::Middle], 0);
}
