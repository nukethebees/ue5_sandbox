#include <SandboxCore/tick_countdown.h>


FTickCountdownView::FTickCountdownView(FTickCountdown& countdown,
                                       size_type const offset,
                                       size_type const length) noexcept
    : countdown_{&countdown}, offset_{offset}, length_{length} {
    check(offset_ >= 0);
    check(length_ >= 0);
    check(offset_ + length_ <= countdown_->num());
}

auto FTickCountdownView::num() const noexcept -> size_type {
    return length_;
}

auto FTickCountdownView::operator[](size_type const index) const noexcept -> counter_type {
    check(index >= 0);
    check(index < length_);
    return countdown_->counters()[offset_ + index];
}

auto FTickCountdownView::try_consume(size_type const index) noexcept -> bool {
    check(index >= 0);
    check(index < length_);
    return countdown_->try_consume(offset_ + index);
}

FTickCountdownConstView::FTickCountdownConstView(FTickCountdown const& countdown,
                                                 size_type const offset,
                                                 size_type const length) noexcept
    : countdown_{&countdown}, offset_{offset}, length_{length} {
    check(offset_ >= 0);
    check(length_ >= 0);
    check(offset_ + length_ <= countdown_->num());
}

auto FTickCountdownConstView::num() const noexcept -> size_type {
    return length_;
}

auto FTickCountdownConstView::operator[](size_type const index) const noexcept -> counter_type {
    check(index >= 0);
    check(index < length_);
    return countdown_->counters()[offset_ + index];
}

FTickCountdown::FTickCountdown(size_type const count, counter_type const initial_tick_value)
    : tick_value_{initial_tick_value} {
    counters_.Init(tick_value_, count);
}

void FTickCountdown::tick() noexcept {
    for (auto& counter : counters_) {
        --counter;
    }
}

auto FTickCountdown::is_ready(counter_type const value) noexcept -> bool {
    return value <= 0;
}

auto FTickCountdown::try_consume(size_type const index) noexcept -> bool {
    auto& counter{counters_[index]};
    if (!is_ready(counter)) {
        return false;
    }

    counter = tick_value_;
    return true;
}

void FTickCountdown::consume(size_type const index) noexcept {
    (void)try_consume(index);
}

void FTickCountdown::consume() noexcept {
    for (auto& counter : counters_) {
        if (is_ready(counter)) {
            counter = tick_value_;
        }
    }
}

void FTickCountdown::reset() {
    counters_.Reset();
}

void FTickCountdown::reserve(size_type const count) {
    counters_.Reserve(count);
}

void FTickCountdown::add_zeroed(size_type const count) {
    counters_.AddZeroed(count);
}

void FTickCountdown::add_defaulted(size_type const count) {
    counters_.AddDefaulted(count);
}

void FTickCountdown::add_uninitialised(size_type const count) {
    counters_.AddUninitialized(count);
}

void FTickCountdown::remove_at_swap(size_type const index,
                                    size_type const count,
                                    EAllowShrinking const allow_shrinking) {
    counters_.RemoveAtSwap(index, count, allow_shrinking);
}

void FTickCountdown::set_num(size_type const count,
                             EAllowShrinking const allow_shrinking) {
    counters_.SetNum(count, allow_shrinking);
}

void FTickCountdown::copy_element(size_type const dst_index,
                                  FTickCountdown const& src,
                                  size_type const src_index) {
    counters_[dst_index] = src.counters_[src_index];
}

auto FTickCountdown::num() const noexcept -> size_type {
    return counters_.Num();
}

auto FTickCountdown::tick_value() const noexcept -> counter_type {
    return tick_value_;
}

void FTickCountdown::set_tick_value(counter_type const value) noexcept {
    tick_value_ = value;
}

auto FTickCountdown::counters() noexcept -> TArrayView<counter_type> {
    return counters_;
}

auto FTickCountdown::counters() const noexcept -> TConstArrayView<counter_type> {
    return counters_;
}

auto FTickCountdown::get_view() noexcept -> View {
    return {*this, 0, num()};
}

auto FTickCountdown::get_view() const noexcept -> ConstView {
    return {*this, 0, num()};
}

auto FTickCountdown::get_view(size_type const offset, size_type const count) noexcept -> View {
    return {*this, offset, count};
}

auto FTickCountdown::get_view(size_type const offset, size_type const count) const noexcept
    -> ConstView {
    return {*this, offset, count};
}

auto FTickCountdown::get_const_view(size_type const offset, size_type const count) const noexcept
    -> ConstView {
    return get_view(offset, count);
}
