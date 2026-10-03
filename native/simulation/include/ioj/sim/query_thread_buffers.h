#pragma once

#include "ioj/sim/line_traces.h"
#include "ioj/sim/trace_hits.h"
#include <ioj/sim/team.h>
#include <ioj/sim/vectors3f.h>

#include <cstdint>
#include <memory_resource>
#include <vector>

namespace ioj::sim {
struct EntityTables;
struct QueryThreadBuffers {
    explicit QueryThreadBuffers(
        std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : line_traces{resource}
        , trace_hits{resource}
        , range_query_entity_stamps{resource}
        , candidates{resource}
        , candidate_order{resource}
        , candidate_teams{resource}
        , candidate_alive{resource}
        , candidate_xs{resource}
        , candidate_ys{resource}
        , candidate_zs{resource} {}

    void ensure_entity_stamp_count(std::uint32_t entity_count);
    [[nodiscard]] auto advance_range_query_stamp() noexcept -> std::uint32_t;
    void gather_candidates(EntityTables const& tables);
    [[nodiscard]] auto candidate_locations() const -> Vectors3fConstView {
        return {candidate_xs, candidate_ys, candidate_zs};
    }

    LineTraces line_traces;
    TraceHits trace_hits;
    std::pmr::vector<std::uint32_t> range_query_entity_stamps;
    std::uint32_t range_query_stamp{};
    std::pmr::vector<EntityUniqueId> candidates;
    std::pmr::vector<std::uint32_t> candidate_order;
    std::pmr::vector<Team> candidate_teams;
    std::pmr::vector<std::uint8_t> candidate_alive;
    std::pmr::vector<float> candidate_xs;
    std::pmr::vector<float> candidate_ys;
    std::pmr::vector<float> candidate_zs;
};
} // namespace ioj::sim
