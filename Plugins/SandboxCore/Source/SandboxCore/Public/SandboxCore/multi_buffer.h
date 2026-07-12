#pragma once

#include <Containers/StaticArray.h>
#include <HAL/Platform.h>
#include <Templates/UnrealTemplate.h>

namespace ml {
// Fixed set of buffers whose previous, current, and next roles rotate on cycle()
template <typename T, int32 N>
    requires (N >= 2)
class MultiBuffer {
  public:
    using value_type = T;
    using index_type = int32;

    static constexpr index_type buffer_count{N};

    MultiBuffer() = default;

    explicit MultiBuffer(TStaticArray<value_type, buffer_count>&& in_buffers)
        : buffers{MoveTemp(in_buffers)} {}

    auto previous() -> value_type& { return buffers[previous_idx]; }
    auto previous() const -> value_type const& { return buffers[previous_idx]; }

    auto current() -> value_type& { return buffers[current_idx]; }
    auto current() const -> value_type const& { return buffers[current_idx]; }

    auto next() -> value_type& { return buffers[next_idx]; }
    auto next() const -> value_type const& { return buffers[next_idx]; }

    void cycle() {
        previous_idx = current_idx;
        current_idx = next_idx;

        ++next_idx;

        if (next_idx == buffer_count) { next_idx = 0; }
    }
  private:
    TStaticArray<value_type, buffer_count> buffers{};
    // Previous and current initially alias, so comparisons before the first
    // cycle are valid and produce zero deltas.
    index_type previous_idx{0};
    index_type current_idx{0};
    index_type next_idx{1};
};
}
