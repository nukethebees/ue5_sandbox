#pragma once

#include <array>
#include <cstdint>
#include <utility>

namespace ml {
// Fixed set of buffers whose previous, current, and next roles rotate on cycle()
template <typename T, std::int32_t N>
    requires (N >= 2)
class MultiBuffer {
  public:
    using value_type = T;
    using index_type = std::int32_t;

    static constexpr index_type buffer_count{N};

    MultiBuffer() = default;

    explicit MultiBuffer(std::array<value_type, buffer_count>&& in_buffers)
        : buffers{std::move(in_buffers)} {}

    auto previous() -> value_type& { return buffers[previous_idx]; }
    auto previous() const -> value_type const& { return buffers[previous_idx]; }

    auto current() -> value_type& { return buffers[current_idx]; }
    auto current() const -> value_type const& { return buffers[current_idx]; }

    auto next() -> value_type& { return buffers[next_idx]; }
    auto next() const -> value_type const& { return buffers[next_idx]; }

    template <typename TFunc>
    void for_each(TFunc&& func) {
        for (auto& buffer : buffers) {
            func(buffer);
        }
    }

    template <typename TFunc>
    void for_each(TFunc&& func) const {
        for (auto const& buffer : buffers) {
            func(buffer);
        }
    }

    void cycle() {
        previous_idx = current_idx;
        current_idx = next_idx;

        ++next_idx;

        if (next_idx == buffer_count) {
            next_idx = 0;
        }
    }
  private:
    std::array<value_type, buffer_count> buffers{};
    // Previous and current initially alias, so comparisons before the first
    // cycle are valid and produce zero deltas.
    index_type previous_idx{0};
    index_type current_idx{0};
    index_type next_idx{1};
};
}
