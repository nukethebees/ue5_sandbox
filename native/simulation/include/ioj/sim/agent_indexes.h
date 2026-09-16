#pragma once

#include <ioj/sim/entity_unique_id.h>
#include <ioj/sim/sim_clock.h>
#include <sandbox/core/enum_array.h>

#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {

// Previous-tick lookup remains valid until structural mutation begins. Rebind all
// owner views after layout changes, before any subsequent lookup.
class AgentIndexes {
  public:
    explicit AgentIndexes(SimClock const& clock) noexcept
        : clock_{clock} {}

    void assert_structural_mutation_allowed() const noexcept {
        assert(clock_.permits_structural_mutation());
    }

    void retire(EntityUniqueId const id) noexcept {
        assert_structural_mutation_allowed();
        if (id.is_valid() && id.index() < local_indexes_.size()) {
            local_indexes_[id.index()] = -1;
        }
    }

    void bind(EntityType const type, std::span<EntityUniqueId const> const ids) {
        assert_structural_mutation_allowed();
        assert(type < EntityType::COUNT);
        owner_ids_[type] = ids;
        auto const count{ids.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const id{ids[index]};
            assert(id.is_valid() && id.entity_type() == type);
            if (id.index() >= local_indexes_.size()) {
                local_indexes_.resize(static_cast<std::size_t>(id.index()) + 1, -1);
            }
            local_indexes_[id.index()] = static_cast<std::int32_t>(index);
        }
    }

    [[nodiscard]] auto find(EntityUniqueId const id) const noexcept -> std::int32_t {
        if (!id.is_valid() || id.index() >= local_indexes_.size()) {
            return -1;
        }
        auto const index{local_indexes_[id.index()]};
        auto const ids{owner_ids_[id.entity_type()]};
        if (index < 0 || static_cast<std::size_t>(index) >= ids.size() || ids[index] != id) {
            return -1;
        }
        return index;
    }
  private:
    SimClock const& clock_;
    std::vector<std::int32_t> local_indexes_;
    ml::EnumArray<EntityType,
                  std::span<EntityUniqueId const>,
                  static_cast<std::size_t>(EntityType::COUNT)>
        owner_ids_{};
};
}
