#include <SandboxCore/tick_countdown.h>


FTickCountdown::FTickCountdown(size_type const count, counter_type const initial_tick_value)
    : tick_value(initial_tick_value) {
    counters.Init(tick_value, count);
}

auto FTickCountdown::is_ready(counter_type const value) noexcept -> bool {
    return value <= 0;
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
    return {tick_value, counters};
}

auto FTickCountdown::get_view() const noexcept -> ConstView {
    return {tick_value, counters};
}

auto FTickCountdown::get_view(size_type const offset, size_type const count) noexcept -> View {
    return {tick_value, TArrayView<counter_type>{counters}.Slice(offset, count)};
}

auto FTickCountdown::get_view(size_type const offset, size_type const count) const noexcept
    -> ConstView {
    return {tick_value, TConstArrayView<counter_type>{counters}.Slice(offset, count)};
}

auto FTickCountdown::get_const_view(size_type const offset, size_type const count) const noexcept
    -> ConstView {
    return get_view(offset, count);
}
