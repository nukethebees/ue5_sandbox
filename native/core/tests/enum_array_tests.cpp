#include "sandbox/core/enum_array.h"

#include <gtest/gtest.h>

namespace {
enum class Key {
    First,
    Second,
};
}

template <>
constexpr auto ml::enum_count<Key>() noexcept -> std::size_t {
    return 2;
}

TEST(EnumArray, IndexesByEnum) {
    ml::EnumArray<Key, int> values;
    values[Key::First] = 10;
    values[Key::Second] = 20;

    EXPECT_EQ(values[Key::First], 10);
    EXPECT_EQ(values[Key::Second], 20);
    static_assert(values.size() == 2);
}
