#pragma once

#include <ioj/sim/entities/entity_lookup_table.h>
#include <ioj/sim/entities/entity_type_runs.h>

namespace ioj::sim {
// Resolve stable entity IDs to current row handles.
class EntityLookupTables {
  public:
    explicit EntityLookupTables(
        std::pmr::memory_resource& memory = *std::pmr::get_default_resource())
        : tables_{{EntityLookupTable{EntityType::PlayerShip, &memory},
                   EntityLookupTable{EntityType::Turret, &memory},
                   EntityLookupTable{EntityType::CapitalShip, &memory},
                   EntityLookupTable{EntityType::Fighter, &memory},
                   EntityLookupTable{EntityType::TubeSpinner, &memory}}} {}

    [[nodiscard]] auto for_type(EntityType const type) -> EntityLookupTable& {
        assert(is_valid(type));
        return tables_[type];
    }

    [[nodiscard]] auto for_type(EntityType const type) const -> EntityLookupTable const& {
        assert(is_valid(type));
        return tables_[type];
    }

    // Require ascending type runs, with optional null IDs only at the end.
    // Exclude the null tail from lookup and leave its output handles invalid.
    void lookup_handles(std::span<EntityId const> const ids,
                        std::span<EntityInstanceHandle> const output) const {
        assert(output.size() == ids.size());

        std::ranges::fill(output, EntityInstanceHandle{});
        auto const runs{entity_type_runs(ids)};
        for (std::uint32_t run{}; run < runs.num; ++run) {
            auto const offset{runs.offsets[run]};
            auto const count{runs.counts[run]};
            for_type(runs.types[run])
                .lookup_handles(ids.subspan(offset, count), output.subspan(offset, count));
        }
    }

    // Group unordered IDs using caller-owned scratch; preserve input/output correspondence.
    // Null IDs are allowed here and excluded from runs. Non-null IDs must satisfy the
    // per-type lookup contract above. Retired IDs remain ordinary missing results.
    auto lookup_handles(std::span<EntityId const> const ids,
                        std::span<EntityTypeRuns::Offset> const order,
                        std::span<EntityInstanceHandle> const output) const -> EntityTypeRuns {
        assert(output.size() == ids.size());
        std::ranges::fill(output, EntityInstanceHandle{});
        auto runs{group_entity_ids(ids, order)};
        for (std::uint32_t run{}; run < runs.num; ++run) {
            auto const handles{for_type(runs.types[run]).entries()};
            [[maybe_unused]] auto const handle_count{handles.size()};
            auto const end{runs.end(run)};
            // Full-ID sorting makes the final index the largest in this populated run.
            assert(ids[order[end - 1]].index() < handle_count);
            for (auto index{runs.offsets[run]}; index < end; ++index) {
                auto const row{order[index]};
                auto const offset{ids[row].index()};
                output[row] = handles[offset];
            }
        }
        return runs;
    }
  private:
    EnumArray<EntityLookupTable, EntityType> tables_;
};
}
