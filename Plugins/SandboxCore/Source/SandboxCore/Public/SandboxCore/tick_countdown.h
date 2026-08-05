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
    void consume(size_type const index) noexcept;
    void consume() noexcept;

    void reset();
    void reserve(size_type const count);
    [[nodiscard]] auto num() const noexcept -> size_type;
    [[nodiscard]] auto get_view() noexcept -> View;
    [[nodiscard]] auto get_view() const noexcept -> ConstView;

    counter_type tick_value{0};
    TArray<counter_type> counters;
};
