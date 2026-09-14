#include "ioj/sim/level_event_types.h"

#include <limits>

namespace ioj::sim {
auto try_to_level_event_count(std::int32_t const count, LevelEventCount& result) noexcept -> bool {
    if (count < 0 || count > std::numeric_limits<LevelEventCount>::max()) {
        return false;
    }
    result = static_cast<LevelEventCount>(count);
    return true;
}
} // namespace ioj::sim
