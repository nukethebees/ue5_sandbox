#pragma once

namespace ioj::presentation {
struct CapitalExplosionTiming {
    constexpr auto small_delay(int index) const -> float {
        return first_small_delay + interval * static_cast<float>(index);
    }

    constexpr auto last_small_delay() const -> float {
        return small_count > 0 ? small_delay(small_count - 1) : first_small_delay;
    }

    constexpr auto main_delay(float delay, bool after_small_explosions) const -> float {
        return after_small_explosions && small_count > 0 ? last_small_delay() + delay : delay;
    }

    float first_small_delay;
    float interval;
    int small_count;
};
}
