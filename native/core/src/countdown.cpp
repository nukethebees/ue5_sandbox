#include "sandbox/core/countdown.h"

namespace ml {
void tick_countdowns(std::span<float> const remaining_times, float const dt) noexcept {
    for (auto& remaining : remaining_times) {
        remaining -= dt;
    }
}
}
