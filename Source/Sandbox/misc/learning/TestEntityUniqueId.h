#pragma once

#include <CoreMinimal.h>

#include <limits>

struct TestEntityUniqueId {
    using ThisClass = TestEntityUniqueId;

    using value_type = int32;

    static constexpr value_type NULL_ID{std::numeric_limits<value_type>::max()};

    constexpr bool is_valid() const { return id != ThisClass::NULL_ID; }

    value_type id{NULL_ID};
};
