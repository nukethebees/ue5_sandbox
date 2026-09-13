#include "sandbox/simulation/query_thread_buffers.h"

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace ml::simulation {
void QueryThreadBuffers::ensure_entity_stamp_count(std::int32_t const entity_count) {
    assert(entity_count >= 0);
    auto const required_count{static_cast<std::size_t>(entity_count)};
    if (range_query_entity_stamps.size() < required_count) {
        range_query_entity_stamps.resize(required_count);
    }
}

auto QueryThreadBuffers::advance_range_query_stamp() noexcept -> std::uint32_t {
    ++range_query_stamp;
    if (range_query_stamp == 0) {
        std::ranges::fill(range_query_entity_stamps, std::uint32_t{});
        range_query_stamp = 1;
    }
    return range_query_stamp;
}
} // namespace ml::simulation
