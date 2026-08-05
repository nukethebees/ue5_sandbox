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

auto FTickCountdown::try_consume(size_type const index) noexcept -> bool {
    auto& counter{counters[index]};
    if (!is_ready(counter)) {
        return false;
    }

    counter = tick_value;
    return true;
}

void FTickCountdown::consume(size_type const index) noexcept {
    (void)try_consume(index);
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

void FTickCountdown::add_zeroed(size_type const count) {
    counters.AddZeroed(count);
}

void FTickCountdown::add_defaulted(size_type const count) {
    counters.AddDefaulted(count);
}

void FTickCountdown::add_uninitialised(size_type const count) {
    counters.AddUninitialized(count);
}

void FTickCountdown::remove_at_swap(size_type const index,
                                    size_type const count,
                                    EAllowShrinking const allow_shrinking) {
    counters.RemoveAtSwap(index, count, allow_shrinking);
}

void FTickCountdown::set_num(size_type const count, EAllowShrinking const allow_shrinking) {
    counters.SetNum(count, allow_shrinking);
}

void FTickCountdown::copy_element(size_type const dst_index,
                                  FTickCountdown const& src,
                                  size_type const src_index) {
    counters[dst_index] = src.counters[src_index];
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

auto FTickCountdown::get_view(size_type const offset, size_type const count) noexcept -> View {
    return View{counters}.Slice(offset, count);
}

auto FTickCountdown::get_view(size_type const offset, size_type const count) const noexcept
    -> ConstView {
    return ConstView{counters}.Slice(offset, count);
}

auto FTickCountdown::get_const_view(size_type const offset, size_type const count) const noexcept
    -> ConstView {
    return get_view(offset, count);
}
