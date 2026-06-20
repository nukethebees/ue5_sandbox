#pragma once

#include <HAL/Platform.h>

#include <limits>

struct TestEntityUniqueId {
    using ThisClass = TestEntityUniqueId;

    using value_type = int32;

    static constexpr value_type NULL_ID{std::numeric_limits<value_type>::max()};

    constexpr auto operator+(value_type const delta) const -> ThisClass {
        return {
            .id = id + delta,
        };
    }

    constexpr bool is_valid() const { return id != ThisClass::NULL_ID; }

    value_type id{NULL_ID};
};
