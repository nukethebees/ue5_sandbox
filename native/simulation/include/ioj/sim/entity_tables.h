#pragma once

#include <ioj/sim/agent_indexes.h>
#include <ioj/sim/health_table.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <functional>
#include <span>

namespace ioj::sim {

template <typename Index>
class EntityComponentIndexBindings {
  public:
    explicit EntityComponentIndexBindings(AgentIndexes const& indexes) noexcept
        : indexes_{indexes} {}

    void bind(EntityType const type,
              std::span<EntityUniqueId const> const owners,
              std::span<Index> const indices) noexcept {
        assert(type < EntityType::COUNT && owners.size() == indices.size());
#ifndef NDEBUG
        for (auto const owner : owners) {
            assert(owner.is_valid() && owner.entity_type() == type);
        }
#endif
        bindings_[static_cast<std::size_t>(type)] = {owners, indices};
    }

    void update(EntityUniqueId const owner, Index const old_index, Index const new_index) const {
        assert(owner.is_valid() && owner.entity_type() < EntityType::COUNT);
        auto const& binding{bindings_[static_cast<std::size_t>(owner.entity_type())]};
        assert(binding.owners.size() == binding.indices.size());

        auto const row{indexes_.find(owner)};
        // Component compaction runs while every owning entity row is still structurally stable
        // and bound, including retained dead entities such as the player.
        assert(row >= 0);
        auto const element{static_cast<std::size_t>(row)};
        if (element >= binding.owners.size() || binding.owners[element] != owner) {
            assert(false);
            return;
        }
        if (binding.indices[element] != old_index) {
            assert(false);
            return;
        }
        binding.indices[element] = new_index;
    }

    template <typename Validate>
    void validate(Validate&& validate_mapping) const {
#ifndef NDEBUG
        for (auto const& binding : bindings_) {
            assert(binding.owners.size() == binding.indices.size());
            auto const count{binding.owners.size()};
            for (std::size_t row{}; row < count; ++row) {
                std::invoke(validate_mapping, binding.indices[row], binding.owners[row]);
            }
        }
#else
        static_cast<void>(validate_mapping);
#endif
    }
  private:
    struct Binding {
        std::span<EntityUniqueId const> owners;
        std::span<Index> indices;
    };

    AgentIndexes const& indexes_;
    std::array<Binding, static_cast<std::size_t>(EntityType::COUNT)> bindings_{};
};

struct EntityTables {
    explicit EntityTables(AgentIndexes const& indexes) noexcept
        : health_indices_{indexes} {}

    void bind_health_indices(EntityType const type,
                             std::span<EntityUniqueId const> const owners,
                             std::span<HealthIndex> const indices) noexcept {
        health_indices_.bind(type, owners, indices);
    }

    void remove_health_rows(std::span<std::int32_t const> const rows,
                            std::span<HealthIndex const> const indices,
                            std::span<EntityUniqueId const> const owners) {
        health.remove_rows(rows, indices, owners, [this](HealthMove const& move) {
            health_indices_.update(move.owner, move.old_index, move.new_index);
        });
    }

    void validate_health_mappings() const {
        health_indices_.validate([this](HealthIndex const index, EntityUniqueId const owner) {
            assert(health.contains(index, owner));
        });
    }

    HealthTable health;
  private:
    EntityComponentIndexBindings<HealthIndex> health_indices_;
};

} // namespace ioj::sim
