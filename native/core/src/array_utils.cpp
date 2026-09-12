#include <sandbox/core/array_utils.h>

namespace ml::kernel {
auto is_sorted_desc(std::int32_t const* const values, std::int32_t const count) noexcept -> bool {
    for (std::int32_t i{1}; i < count; ++i) {
        if (values[i - 1] < values[i]) {
            return false;
        }
    }

    return true;
}
}
