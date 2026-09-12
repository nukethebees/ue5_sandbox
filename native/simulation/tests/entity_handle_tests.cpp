#include "sandbox/simulation/entity_handle.h"

#include <gtest/gtest.h>

#include <type_traits>

TEST(EntityHandle, ValidityAndReset) {
    FRegistryEntityHandle handle{};
    EXPECT_TRUE(handle.is_null());
    EXPECT_FALSE(handle.is_valid());

    handle = {4, 2};
    EXPECT_TRUE(handle.is_valid());
    EXPECT_FALSE(handle.is_null());

    handle.reset();
    EXPECT_TRUE(handle.is_null());
}

TEST(EntityHandle, PreservesCompactTrivialLayout) {
    static_assert(sizeof(FRegistryEntityHandle) == sizeof(std::uint64_t));
    static_assert(std::is_trivially_copyable_v<FRegistryEntityHandle>);
}
