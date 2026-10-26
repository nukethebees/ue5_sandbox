#pragma once

#include <ioj/range_fits.h>
#include <ioj/sim/entities/entity_identity_layout.h>
#include <ioj/sim/entities/entity_instance_handle.h>
#include <ioj/sim/entities/health.h>
#include <ioj/sim/entities/quantise_health.h>

#include <algorithm>
#include <cassert>
#include <memory_resource>
#include <span>
#include <vector>

namespace ioj::sim {

[[nodiscard]] inline auto valid_lookup_id(EntityId const id,
                                          EntityType const type,
                                          std::size_t const handle_count) noexcept -> bool {
    return id != EntityId{} && id.entity_type() == type && id.index() < handle_count;
}

class EntityLookupTable {
  public:
    explicit EntityLookupTable(
        EntityType const type,
        std::pmr::memory_resource* const resource = std::pmr::get_default_resource())
        : type_{type}
        , handles_(entity_lifetime_capacities[type], EntityInstanceHandle{}, resource) {}

    void publish_rows(std::span<EntityId const> const ids,
                      std::span<Team const> const teams,
                      std::span<Health const> const healths = {},
                      Health const maximum = Health{1}) {
        assert(teams.empty() || teams.size() == ids.size());
        assert(healths.empty() || healths.size() == ids.size());
        assert(range_fits<std::size_t>(0,
                                       ids.size(),
                                       EntityInstanceHandle::index_layout.range.min,
                                       EntityInstanceHandle::index_layout.range.max));
        auto const count{ids.size()};
        row_count_ = static_cast<std::uint32_t>(count);
        [[maybe_unused]] auto const handle_count{handles_.size()};
        for (EntityFrameIndex::value_type row{}; row < count; ++row) {
            auto const id{ids[row]};
            assert(valid_lookup_id(id, type_, handle_count));
            auto& handle{handles_[id.index()]};
            if (!healths.empty() && is_dead(healths[row])) {
                handle = {};
                continue;
            }
            auto const team{teams.empty() ? Team::White : teams[row]};
            auto const health_state{healths.empty() ? QuantisedHealth::value_type{3}
                                                    : quantise_health(healths[row], maximum)};
            handle = EntityInstanceHandle{row, team, health_state};
        }
    }

    void retire(std::span<EntityId const> const ids) {
        [[maybe_unused]] auto const handle_count{handles_.size()};
        for (auto const id : ids) {
            assert(valid_lookup_id(id, type_, handle_count));
            handles_[id.index()] = {};
        }
    }

    void retire_rows(std::span<EntityId const> const ids,
                     std::span<EntityFrameIndex::value_type const> const rows) {
        [[maybe_unused]] auto const handle_count{handles_.size()};
        for (auto const row : rows) {
            auto const id{ids[row]};
            assert(valid_lookup_id(id, type_, handle_count));
            handles_[id.index()] = {};
        }
    }

    // Require non-null IDs of this type within its lifetime capacity. Retired IDs return
    // invalid handles; malformed inputs are contract violations, not missing entities.
    // Write one handle per input position, including duplicates.
    void lookup_handles(std::span<EntityId const> const ids,
                        std::span<EntityInstanceHandle> const output) const {
        assert(output.size() == ids.size());
        auto const count{ids.size()};
        auto const handles{std::span<EntityInstanceHandle const>{handles_}};
        [[maybe_unused]] auto const handle_count{handles.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const id{ids[index]};
            assert(valid_lookup_id(id, type_, handle_count));
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

}
