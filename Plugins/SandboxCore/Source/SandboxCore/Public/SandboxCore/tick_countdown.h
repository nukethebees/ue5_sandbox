#pragma once

#include "CoreMinimal.h"


struct SANDBOXCORE_API FTickCountdown {
    using size_type = int32;
    using counter_type = int16;
    using View = TArrayView<counter_type>;
    using ConstView = TConstArrayView<counter_type>;

    FTickCountdown() = default;
    FTickCountdown(size_type const count, counter_type const initial_tick_value);

    void tick() noexcept;

    [[nodiscard]] static auto is_ready(counter_type const value) noexcept -> bool;
    [[nodiscard]] auto try_consume(size_type const index) noexcept -> bool;
    void consume(size_type const index) noexcept;
    void consume() noexcept;

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
