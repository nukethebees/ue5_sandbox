#pragma once

#include "CoreMinimal.h"


class FTickCountdown;

class SANDBOXCORE_API FTickCountdownView {
  public:
    using size_type = int32;
    using counter_type = int16;

    FTickCountdownView() = default;

    [[nodiscard]] auto num() const noexcept -> size_type;
    [[nodiscard]] auto operator[](size_type const index) const noexcept -> counter_type;
    [[nodiscard]] auto try_consume(size_type const index) noexcept -> bool;
  private:
    friend class FTickCountdown;

    FTickCountdownView(FTickCountdown& countdown,
                       size_type const offset,
                       size_type const length) noexcept;

    FTickCountdown* countdown_{nullptr};
    size_type offset_{0};
    size_type length_{0};
};

class SANDBOXCORE_API FTickCountdownConstView {
  public:
    using size_type = int32;
    using counter_type = int16;

    FTickCountdownConstView() = default;

    [[nodiscard]] auto num() const noexcept -> size_type;
    [[nodiscard]] auto operator[](size_type const index) const noexcept -> counter_type;
  private:
    friend class FTickCountdown;

    FTickCountdownConstView(FTickCountdown const& countdown,
                            size_type const offset,
                            size_type const length) noexcept;

    FTickCountdown const* countdown_{nullptr};
    size_type offset_{0};
    size_type length_{0};
};

class SANDBOXCORE_API FTickCountdown {
  public:
    using size_type = int32;
    using counter_type = int16;
    using View = FTickCountdownView;
    using ConstView = FTickCountdownConstView;

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
    [[nodiscard]] auto tick_value() const noexcept -> counter_type;
    void set_tick_value(counter_type const value) noexcept;
    [[nodiscard]] auto counters() noexcept -> TArrayView<counter_type>;
    [[nodiscard]] auto counters() const noexcept -> TConstArrayView<counter_type>;

    [[nodiscard]] auto get_view() noexcept -> View;
    [[nodiscard]] auto get_view() const noexcept -> ConstView;
    [[nodiscard]] auto get_view(size_type const offset, size_type const count) noexcept -> View;
    [[nodiscard]] auto get_view(size_type const offset,
                                size_type const count) const noexcept -> ConstView;
    [[nodiscard]] auto get_const_view(size_type const offset,
                                      size_type const count) const noexcept
        -> ConstView;
  private:
    counter_type tick_value_{0};
    TArray<counter_type> counters_;
};
