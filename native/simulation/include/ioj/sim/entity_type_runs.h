#pragma once

#include <ioj/sim/entity_unique_id.h>

#include <sandbox/core/fixed_array.h>

#include <algorithm>
#include <cassert>
#include <numeric>
#include <span>

namespace ioj::sim {
struct EntityTypeRuns {
    using Offset = std::uint32_t;
    using Count = std::uint32_t;
    inline static constexpr std::uint32_t capacity{ml::enum_count<EntityType>()};
    ml::FixedArray<EntityType, capacity> types;
    ml::FixedArray<Offset, capacity> offsets;
    ml::FixedArray<Count, capacity> counts;
    Count num{};

    auto end(Count const run) const -> Offset { return offsets[run] + counts[run]; }
};

namespace entity_type_run_detail {
inline auto scan(EntityTypeRuns::Count const count, auto id_at) -> EntityTypeRuns {
    EntityTypeRuns runs;
    EntityTypeRuns::Offset first{};
    while (first < count) {
        auto const id{id_at(first)};
        if (id == EntityUniqueId{}) {
            break;
        }
        auto const type{id.entity_type()};
        assert(std::to_underlying(type) < EntityTypeRuns::capacity);
        assert(runs.num == 0 || type > runs.types[runs.num - 1]);
        auto end{first + 1};
        while (end < count && id_at(end).entity_type() == type) {
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
}

// IDs must be sorted by type, with empty IDs last.
inline auto entity_type_runs(std::span<EntityUniqueId const> const ids) -> EntityTypeRuns {
    assert(std::in_range<EntityTypeRuns::Count>(ids.size()));
    return entity_type_run_detail::scan(static_cast<EntityTypeRuns::Count>(ids.size()),
                                        [ids](EntityTypeRuns::Offset row) { return ids[row]; });
}

// The permutation must sort IDs by type, with empty IDs last.
inline auto entity_type_runs(std::span<EntityUniqueId const> const ids,
                             std::span<EntityTypeRuns::Offset const> const order)
    -> EntityTypeRuns {
    assert(order.size() == ids.size());
    assert(std::in_range<EntityTypeRuns::Count>(ids.size()));
    return entity_type_run_detail::scan(
        static_cast<EntityTypeRuns::Count>(ids.size()),
        [ids, order](EntityTypeRuns::Offset row) { return ids[order[row]]; });
}

inline auto group_entity_ids(std::span<EntityUniqueId const> const ids,
                             std::span<EntityTypeRuns::Offset> const order) -> EntityTypeRuns {
    assert(order.size() == ids.size());
    std::iota(order.begin(), order.end(), EntityTypeRuns::Offset{});
    std::ranges::sort(order, {}, [&](EntityTypeRuns::Offset const row) { return ids[row]; });
    return entity_type_runs(ids, order);
}
}
