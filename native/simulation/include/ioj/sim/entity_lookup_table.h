#pragma once

#include <ioj/sim/entity_identity_layout.h>
#include <ioj/sim/entity_instance_handle.h>
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

    void resolve(std::span<EntityUniqueId const> const ids,
                 std::span<EntityInstanceHandle> const output) const {
        assert(output.size() == ids.size());
        auto const count{ids.size()};
        auto const handles{std::span<EntityInstanceHandle const>{handles_}};
        auto const handle_count{handles.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const id{ids[index]};
            assert(!id.is_valid() || id.entity_type() == type_);
            output[index] = id.is_valid() && id.index() < handle_count ? handles[id.index()]
                                                                       : EntityInstanceHandle{};
        }
    }

    [[nodiscard]] auto entries() const noexcept -> std::span<EntityInstanceHandle const> {
        return handles_;
    }
  private:
    EntityType type_;
    std::pmr::vector<EntityInstanceHandle> handles_;
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

    // Resolve each contiguous type run into the matching caller-owned output slice.
    void resolve(std::span<EntityUniqueId const> const ids,
                 std::span<EntityInstanceHandle> const output) const {
        assert(permits_lookup());
        assert(output.size() == ids.size());
        assert(std::ranges::is_sorted(ids, {}, &EntityUniqueId::entity_type));

        auto const count{ids.size()};
        std::size_t first{};
        while (first < count) {
            auto const type{ids[first].entity_type()};
            auto end{first + 1};
            while (end < count && ids[end].entity_type() == type) {
                ++end;
            }

            auto const destination{output.subspan(first, end - first)};
            if (std::to_underlying(type) < ml::enum_count<EntityType>()) {
                for_type(type).resolve(ids.subspan(first, end - first), destination);
            } else {
                std::ranges::fill(destination, EntityInstanceHandle{});
            }
            first = end;
        }
    }
  private:
    SimClock const& clock_;
    ml::EnumArray<EntityType, EntityLookupTable> tables_;
};
}
