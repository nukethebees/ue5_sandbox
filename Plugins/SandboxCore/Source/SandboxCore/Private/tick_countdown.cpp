#include <SandboxCore/tick_countdown.h>


FTickCountdown::FTickCountdown(size_type const count, counter_type const initial_tick_value)
    : tick_value(initial_tick_value) {
    counters.Init(tick_value, count);
}

void FTickCountdown::tick() noexcept {
    for (auto& counter : counters) {
        --counter;
    }
}

auto FTickCountdown::is_ready(counter_type const value) noexcept -> bool {
    return value <= 0;
}

void FTickCountdown::consume(size_type const index) noexcept {
    auto& counter{counters[index]};
    if (is_ready(counter)) {
        counter = tick_value;
    }
}

void FTickCountdown::consume() noexcept {
    for (auto& counter : counters) {
        if (is_ready(counter)) {
            counter = tick_value;
        }
    }
}

void FTickCountdown::reset() {
    counters.Reset();
}

void FTickCountdown::reserve(size_type const count) {
    counters.Reserve(count);
}

auto FTickCountdown::num() const noexcept -> size_type {
    return counters.Num();
}

auto FTickCountdown::get_view() noexcept -> View {
    return counters;
}

auto FTickCountdown::get_view() const noexcept -> ConstView {
    return counters;
}
