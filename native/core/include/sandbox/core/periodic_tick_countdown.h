#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

namespace ml {
template <std::signed_integral Counter>
class PeriodicTickCountdownView {
  public:
    using counter_type = std::remove_const_t<Counter>;

    PeriodicTickCountdownView() = default;
    PeriodicTickCountdownView(std::span<Counter> const remaining_ticks,
                              std::span<counter_type const> const periods) noexcept
        : remaining_ticks_{remaining_ticks}
        , periods_{periods} {
        assert(remaining_ticks_.size() == periods_.size());
    }

    [[nodiscard]] auto num() const noexcept -> std::size_t { return remaining_ticks_.size(); }

    [[nodiscard]] auto operator[](std::size_t const index) const noexcept -> counter_type {
        assert(index < num());
        return remaining_ticks_[index];
    }

    [[nodiscard]] auto is_ready(std::size_t const index) const noexcept -> bool {
        return (*this)[index] <= 0;
    }

    [[nodiscard]] auto try_consume(std::size_t const index) const noexcept -> bool
        requires (!std::is_const_v<Counter>)
    {
        if (!is_ready(index)) {
            return false;
        }

        remaining_ticks_[index] = periods_[index];
        return true;
    }
  private:
    std::span<Counter> remaining_ticks_;
    std::span<counter_type const> periods_;
};
}
