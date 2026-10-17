#pragma once

#include <algorithm>
#include <concepts>
#include <span>

namespace ml {
template <std::signed_integral Counter>
void tick_countdowns(std::span<Counter> const counters,
                     Counter& cleaner_counter,
                     Counter const clean_interval) noexcept {
    for (auto& counter : counters) {
        --counter;
    }

    ++cleaner_counter;
    if (cleaner_counter < clean_interval) {
        return;
    }

    cleaner_counter = 0;
    for (auto& counter : counters) {
        counter = std::max(counter, Counter{0});
    }
}

template <std::signed_integral Counter>
void tick_periodic_countdowns(std::span<Counter> const remaining_ticks,
                              Counter const num_ticks = Counter{1}) noexcept {
    if (num_ticks <= 0) {
        return;
    }

    for (auto& remaining : remaining_ticks) {
        remaining =
            remaining <= num_ticks ? Counter{0} : static_cast<Counter>(remaining - num_ticks);
    }
}

template <std::signed_integral Counter>
void consume_ready_countdowns(std::span<Counter> const counters,
                              Counter const restart_value) noexcept {
    for (auto& counter : counters) {
        if (counter <= 0) {
            counter = restart_value;
        }
    }
}

void tick_countdowns(std::span<float> remaining_times, float dt) noexcept;
}
