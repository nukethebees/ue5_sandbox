#pragma once

#include <ioj/sim/entity_unique_id.h>

#include <sandbox/core/fixed_array.h>

#include <algorithm>
#include <cassert>
#include <numeric>
#include <span>

namespace ioj::sim {
struct EntityTypeRuns {
    inline static constexpr std::uint32_t capacity{ml::enum_count<EntityType>()};
    ml::FixedArray<EntityType, capacity> types;
    ml::FixedArray<std::uint32_t, capacity> offsets;
    ml::FixedArray<std::uint32_t, capacity> counts;
    std::uint32_t num{};
};

// Inputs must be grouped by type, directly or through the supplied permutation.
inline auto entity_type_runs(std::span<EntityUniqueId const> const ids,
                             std::span<std::uint32_t const> const order = {}) -> EntityTypeRuns {
    assert(order.empty() || order.size() == ids.size());
    assert(std::in_range<std::uint32_t>(ids.size()));
    EntityTypeRuns runs;
    auto const count{static_cast<std::uint32_t>(ids.size())};
    std::uint32_t first{};
    while (first < count) {
        auto const type{ids[order.empty() ? first : order[first]].entity_type()};
        if (std::to_underlying(type) >= EntityTypeRuns::capacity) {
            break;
        }
        assert(runs.num == 0 || type > runs.types[runs.num - 1]);
        auto end{first + 1};
        while (end < count && ids[order.empty() ? end : order[end]].entity_type() == type) {
            ++end;
        }
        runs.types.add(type);
        runs.offsets.add(first);
        runs.counts.add(end - first);
        ++runs.num;
        first = end;
    }
    return runs;
}

inline auto group_entity_ids(std::span<EntityUniqueId const> const ids,
                             std::span<std::uint32_t> const order) -> EntityTypeRuns {
    assert(order.size() == ids.size());
    std::iota(order.begin(), order.end(), 0u);
    std::ranges::sort(order, {}, [&](std::uint32_t const row) { return ids[row]; });
    return entity_type_runs(ids, order);
}
}
