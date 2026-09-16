#pragma once

#include <ioj/sim/entity_identity_layout.h>
#include <ioj/sim/entity_unique_id.h>
#include <ioj/sim/sim_clock.h>
#include <sandbox/core/enum_array.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <span>
#include <vector>

namespace ioj::sim {

// Allocation and ID offsets last for the level lifetime. Retire IDs before removal
// and bind surviving rows after layout changes, before any subsequent lookup.
class AgentIndexes {
  public:
    inline static constexpr std::uint32_t invalid_index{std::numeric_limits<std::uint32_t>::max()};

    explicit AgentIndexes(
        SimClock const& clock,
        std::pmr::memory_resource* const resource = std::pmr::get_default_resource())
        : clock_{clock}
        , local_indexes_(entity_identity_capacity, invalid_index, resource) {}

    void reset() {
        assert_preparation_mutation_allowed();
        std::ranges::fill(local_indexes_, invalid_index);
    }

    void assert_structural_mutation_allowed() const noexcept {
        assert(clock_.permits_structural_mutation());
    }

    void assert_preparation_mutation_allowed() const noexcept {
        assert(clock_.permits_preparation_mutation());
    }

    void assert_removal_allowed() const noexcept {
        assert(clock_.phase == SimulationPhase::ResolutionCommit);
    }

    void retire(EntityUniqueId const id) noexcept {
        assert_structural_mutation_allowed();
        if (is_entity_identity_offset(id)) {
            local_indexes_[id.index()] = invalid_index;
        }
    }

    void bind([[maybe_unused]] EntityType const type, std::span<EntityUniqueId const> const ids) {
        assert_structural_mutation_allowed();
        assert(type < EntityType::COUNT);
        auto const count{ids.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const id{ids[index]};
            assert(is_entity_identity_offset(id) && id.entity_type() == type);
            local_indexes_[id.index()] = static_cast<std::uint32_t>(index);
        }
    }

    [[nodiscard]] auto find(EntityUniqueId const id) const noexcept -> std::int32_t {
        if (!is_entity_identity_offset(id)) {
            return -1;
        }
        auto const index{local_indexes_[id.index()]};
        return index == invalid_index ? -1 : static_cast<std::int32_t>(index);
    }

    [[nodiscard]] auto group(EntityType const type) const noexcept
        -> std::span<std::uint32_t const> {
        assert(type < EntityType::COUNT);
        return std::span{local_indexes_}.subspan(entity_identity_offsets[type],
                                                 entity_lifetime_capacities[type]);
    }
  private:
    SimClock const& clock_;
    std::pmr::vector<std::uint32_t> local_indexes_;
};
}
