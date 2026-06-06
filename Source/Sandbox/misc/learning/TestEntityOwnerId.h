#pragma once

#include <CoreMinimal.h>

#include <limits>

struct TestEntityOwnerId {
    using ThisClass = TestEntityOwnerId;

    static constexpr uint8 NULL_ID{std::numeric_limits<uint8>::max()};

    bool is_valid() const;

    uint8 id{NULL_ID};
};
