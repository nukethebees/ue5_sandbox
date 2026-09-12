#include <sandbox/core/array_math.h>

namespace ml::kernel {
template auto
    collect_indices_less_equal<float>(float const*, std::int32_t, float, std::int32_t*) noexcept
    -> std::int32_t;
}
