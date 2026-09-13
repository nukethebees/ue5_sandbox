#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

namespace ml {
template <std::signed_integral Counter>
class TickCountdownView {
  public:
    using counter_type = std::remove_const_t<Counter>;

    TickCountdownView() = default;
    TickCountdownView(std::span<Counter> const counters, counter_type const restart_value) noexcept
        : counters_{counters}
        , restart_value_{restart_value} {
        assert(restart_value_ >= 0);
    }

    [[nodiscard]] auto num() const noexcept -> std::size_t { return counters_.size(); }

    [[nodiscard]] auto operator[](std::size_t const index) const noexcept -> counter_type {
        assert(index < num());
        return counters_[index];
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

        restart_counter(index);
        return true;
    }

    void restart_counter(std::size_t const index) const noexcept
        requires (!std::is_const_v<Counter>)
    {
        set_counter(index, restart_value_);
    }

    void set_counter(std::size_t const index, counter_type const value) const noexcept
        requires (!std::is_const_v<Counter>)
    {
        assert(index < num());
        assert(value >= 0);
        counters_[index] = value;
    }

    void zero_counter(std::size_t const index) const noexcept
        requires (!std::is_const_v<Counter>)
    {
        set_counter(index, 0);
    }
  private:
    std::span<Counter> counters_;
    counter_type restart_value_{};
};
}
