#pragma once

#include <ioj/sim/entity_identity_layout.h>
#include <ioj/sim/entity_instance_handle.h>
#include <ioj/sim/entity_type_runs.h>
#include <ioj/sim/health.h>
#include <ioj/sim/sim_clock.h>

#include <sandbox/core/range_fits.h>

#include <algorithm>
#include <cassert>
#include <memory_resource>
#include <span>
#include <vector>

namespace ioj::sim {
[[nodiscard]] inline auto quantise_health(Health const health, Health const maximum) noexcept
    -> QuantisedHealth {
    assert(health > 0 && maximum > 0);
    // Keep exact quarter boundaries in the lower band and clamp overheal.
    auto const band{(static_cast<std::int64_t>(health) * 4 - 1) / maximum};
    return static_cast<QuantisedHealth>(std::min<std::int64_t>(band, 3));
}

class EntityLookupTable {
  public:
    explicit EntityLookupTable(
        EntityType const type,
        std::pmr::memory_resource* const resource = std::pmr::get_default_resource())
        : type_{type}
        , handles_(entity_lifetime_capacities[type], EntityInstanceHandle{}, resource) {}

    void publish_rows(std::span<EntityUniqueId const> const ids,
                      std::span<Team const> const teams,
                      std::span<Health const> const healths = {},
                      Health const maximum = 1) {
        assert(teams.empty() || teams.size() == ids.size());
        assert(healths.empty() || healths.size() == ids.size());
        assert(ml::range_fits<std::size_t>(0,
                                           ids.size(),
                                           EntityInstanceHandle::index_minimum,
                                           EntityInstanceHandle::index_maximum));
        auto const count{ids.size()};
        row_count_ = static_cast<std::uint32_t>(count);
        [[maybe_unused]] auto const handle_count{handles_.size()};
        for (EntityFrameIndex row{}; row < count; ++row) {
            auto const id{ids[row]};
            assert(id.entity_type() == type_ && id.index() < handle_count);
            auto& handle{handles_[id.index()]};
            if (!healths.empty() && is_dead(healths[row])) {
                handle = {};
                continue;
            }
            auto const team{teams.empty() ? Team::White : teams[row]};
            auto const health_state{healths.empty() ? QuantisedHealth{3}
                                                    : quantise_health(healths[row], maximum)};
            handle = EntityInstanceHandle{row, team, health_state};
        }
    }

    void retire(std::span<EntityUniqueId const> const ids) {
        [[maybe_unused]] auto const handle_count{handles_.size()};
        for (auto const id : ids) {
            assert(id.entity_type() == type_ && id.index() < handle_count);
            handles_[id.index()] = {};
        }
    }

    void retire_rows(std::span<EntityUniqueId const> const ids,
                     std::span<EntityFrameIndex const> const rows) {
        [[maybe_unused]] auto const handle_count{handles_.size()};
        for (auto const row : rows) {
            auto const id{ids[row]};
            assert(id.entity_type() == type_ && id.index() < handle_count);
            handles_[id.index()] = {};
        }
    }

    // Require non-null IDs of this type within its lifetime capacity. Retired IDs return
    // invalid handles; malformed inputs are contract violations, not missing entities.
    // Write one handle per input position, including duplicates.
    void lookup_handles(std::span<EntityUniqueId const> const ids,
                        std::span<EntityInstanceHandle> const output) const {
        assert(output.size() == ids.size());
        auto const count{ids.size()};
        auto const handles{std::span<EntityInstanceHandle const>{handles_}};
        [[maybe_unused]] auto const handle_count{handles.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const id{ids[index]};
            assert(id != EntityUniqueId{} && id.entity_type() == type_ &&
                   id.index() < handle_count);
            output[index] = handles[id.index()];
        }
    }

    [[nodiscard]] auto entries() const noexcept -> std::span<EntityInstanceHandle const> {
        return handles_;
    }
    [[nodiscard]] auto row_count() const noexcept -> std::uint32_t { return row_count_; }
  private:
    [[maybe_unused]] EntityType type_;
    std::pmr::vector<EntityInstanceHandle> handles_;
    std::uint32_t row_count_{};
};

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
        assert(std::to_underlying(type) < ml::enum_count<EntityType>());
        return tables_[type];
    }

    void assert_structural_mutation_allowed() const {
        assert(clock_.permits_structural_mutation());
    }
    void assert_preparation_mutation_allowed() const {
        assert(clock_.permits_preparation_mutation());
    }
    void assert_removal_allowed() const {
        assert(clock_.phase() == SimulationPhase::ResolutionCommit);
    }

    [[nodiscard]] auto permits_lookup() const noexcept -> bool { return clock_.permits_lookup(); }

    [[nodiscard]] auto for_type(EntityType const type) const -> EntityLookupTable const& {
        assert(std::to_underlying(type) < ml::enum_count<EntityType>());
        return tables_[type];
    }

    // Require ascending type runs, with optional null IDs only at the end.
    // Exclude the null tail from lookup and leave its output handles invalid.
    void lookup_handles(std::span<EntityUniqueId const> const ids,
                        std::span<EntityInstanceHandle> const output) const {
        assert(permits_lookup());
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
    auto lookup_handles(std::span<EntityUniqueId const> const ids,
                        std::span<EntityTypeRuns::Offset> const order,
                        std::span<EntityInstanceHandle> const output) const -> EntityTypeRuns {
        assert(permits_lookup());
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
    ml::EnumArray<EntityType, EntityLookupTable> tables_;
};
}
