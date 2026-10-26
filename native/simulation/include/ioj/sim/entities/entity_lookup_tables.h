#pragma once

#include <ioj/sim/entities/entity_lookup_table.h>
#include <ioj/sim/entities/entity_type_runs.h>
#include <ioj/sim/phase_checks.h>

namespace ioj::sim {
class EntityLookupTables {
  public:
    explicit EntityLookupTables(SimClock const& clock)
        : clock_{clock}
        , tables_{{EntityLookupTable{EntityType::PlayerShip},
                   EntityLookupTable{EntityType::Turret},
                   EntityLookupTable{EntityType::CapitalShip},
                   EntityLookupTable{EntityType::Fighter},
                   EntityLookupTable{EntityType::TubeSpinner}}} {}

    [[nodiscard]] auto for_type(EntityType const type) -> EntityLookupTable& {
        assert(std::to_underlying(type) < enum_count<EntityType>());
        return tables_[type];
    }

    [[nodiscard]] auto clock() const noexcept -> SimClock const& { return clock_; }

    [[nodiscard]] auto for_type(EntityType const type) const -> EntityLookupTable const& {
        assert(std::to_underlying(type) < enum_count<EntityType>());
        return tables_[type];
    }

    // Require ascending type runs, with optional null IDs only at the end.
    // Exclude the null tail from lookup and leave its output handles invalid.
    void lookup_handles(std::span<EntityId const> const ids,
                        std::span<EntityInstanceHandle> const output) const {
        assert(is_lookup_allowed(clock_));
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
        assert(is_lookup_allowed(clock_));
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
    SimClock const& clock_;
    EnumArray<EntityType, EntityLookupTable> tables_;
};
}
