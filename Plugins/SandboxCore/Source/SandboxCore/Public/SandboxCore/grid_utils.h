#pragma once

#include <HAL/Platform.h>

namespace ml {
[[nodiscard]] inline auto is_valid_grid_value_count(int32 const width,
                                                    int32 const height,
                                                    int32 const value_count) noexcept -> bool {
    if (width <= 0 || height <= 0) {
        return false;
    }

    auto const expected_value_count{static_cast<int64>(width) * static_cast<int64>(height)};
    return expected_value_count <= MAX_int32 && expected_value_count == value_count;
}
}
