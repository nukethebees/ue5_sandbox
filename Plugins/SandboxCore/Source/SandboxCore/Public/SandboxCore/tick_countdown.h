#pragma once

#include "CoreMinimal.h"

#include <type_traits>


template <bool is_const>
struct TTickCountdownMixin {};

template <>
struct TTickCountdownMixin<false> {
    void tick(this auto& self) noexcept {
        for (auto& counter : self.counters) {
            --counter;
        }
    }

    [[nodiscard]] auto try_consume(this auto& self, int32 const index) noexcept -> bool {
        auto& counter{self.counters[index]};
        if (counter > 0) {
            return false;
        }

        self.reset(index);
        return true;
    }

    void reset(this auto& self, int32 const index) noexcept {
        self.counters[index] = self.tick_value;
    }

    void consume(this auto& self, int32 const index) noexcept {
        (void)self.try_consume(index);
    }

    void consume(this auto& self) noexcept {
        for (auto& counter : self.counters) {
            if (counter <= 0) {
                counter = self.tick_value;
            }
        }
    }
};

template <bool is_const>
struct TTickCountdownView : public TTickCountdownMixin<is_const> {
    using size_type = int32;
    using counter_type = int16;
    using element_type = std::conditional_t<is_const, counter_type const, counter_type>;
    using CountersView = TArrayView<element_type>;

    TTickCountdownView() = default;
    TTickCountdownView(counter_type const new_tick_value, CountersView const new_counters)
        : tick_value{new_tick_value}, counters{new_counters} {}

    [[nodiscard]] auto Num() const noexcept -> size_type { return counters.Num(); }
    [[nodiscard]] auto num() const noexcept -> size_type { return counters.Num(); }

    auto operator[](size_type const index) noexcept -> element_type& { return counters[index]; }
    auto operator[](size_type const index) const noexcept -> element_type const& {
        return counters[index];
    }

    counter_type tick_value{0};
    CountersView counters;
};


struct SANDBOXCORE_API FTickCountdown : public TTickCountdownMixin<false> {
    using size_type = int32;
    using counter_type = int16;
    using View = TTickCountdownView<false>;
    using ConstView = TTickCountdownView<true>;

    FTickCountdown() = default;
    FTickCountdown(size_type const count, counter_type const initial_tick_value);

    [[nodiscard]] static auto is_ready(counter_type const value) noexcept -> bool;

    using TTickCountdownMixin<false>::reset;
    void reset();
    void reserve(size_type const count);
    void add_zeroed(size_type const count);
    void add_defaulted(size_type const count);
    void add_uninitialised(size_type const count);
    void remove_at_swap(size_type const index,
                        size_type const count,
                        EAllowShrinking const allow_shrinking);
    void set_num(size_type const count, EAllowShrinking const allow_shrinking);
    void copy_element(size_type const dst_index,
                      FTickCountdown const& src,
                      size_type const src_index);
    [[nodiscard]] auto num() const noexcept -> size_type;
    [[nodiscard]] auto get_view() noexcept -> View;
    [[nodiscard]] auto get_view() const noexcept -> ConstView;
    [[nodiscard]] auto get_view(size_type const offset, size_type const count) noexcept -> View;
    [[nodiscard]] auto get_view(size_type const offset, size_type const count) const noexcept
        -> ConstView;
    [[nodiscard]] auto get_const_view(size_type const offset, size_type const count) const noexcept
        -> ConstView;

    counter_type tick_value{0};
    TArray<counter_type> counters;
};
