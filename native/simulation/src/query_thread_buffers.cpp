#include "ioj/sim/query_thread_buffers.h"

#include <ioj/sim/entity_queries.h>

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace ioj::sim {
void QueryThreadBuffers::gather_candidates(EntityTables const& tables) {
    auto const count{candidates.size()};
    candidate_order.resize(count);
    candidate_teams.resize(count);
    candidate_alive.resize(count);
    candidate_xs.resize(count);
    candidate_ys.resize(count);
    candidate_zs.resize(count);
    gather_entities(tables,
                    candidates,
                    candidate_order,
                    {.locations = {candidate_xs, candidate_ys, candidate_zs},
                     .teams = candidate_teams,
                     .alive = candidate_alive});
}

void QueryThreadBuffers::ensure_entity_stamp_count(std::uint32_t const entity_count) {
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
} // namespace ioj::sim
