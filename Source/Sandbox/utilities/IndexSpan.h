#pragma once

#include <HAL/Platform.h>

#include <compare>

struct FIndexSpan {
    int32 offset{0};
    int32 count{0};

    auto operator<=>(FIndexSpan const&) const noexcept = default;

    bool is_empty() const noexcept { return count == 0; }
    auto end() const noexcept { return offset + count; }
};
