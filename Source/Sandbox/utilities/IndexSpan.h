#pragma once

#include <HAL/Platform.h>

struct FIndexSpan {
    int32 offset{0};
    int32 count{0};

    bool is_empty() const noexcept { return count == 0; }
};
