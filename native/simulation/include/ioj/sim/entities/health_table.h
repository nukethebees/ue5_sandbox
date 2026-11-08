#pragma once

#include <ioj/sim/entities/entity_identity_layout.h>
#include <ioj/sim/entities/entity_table_operations.h>
#include <ioj/sim/entities/health.h>
#include <ioj/subspan.h>

#include <cassert>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <utility>
#include <vector>

namespace ioj::sim {
namespace health_storage {
struct Layout {
    EntityTypeSizes capacities{};
    EntityTypeSizes offsets{};
    std::uint32_t capacity{};
};
inline constexpr auto layout{[] {
    Layout result;
    for (auto const type : EnumTraits<EntityType>::values) {
        result.capacities[type] = is_damageable(type) ? entity_lifetime_capacities[type] : 0;
        result.offsets[type] = result.capacity;
        result.capacity += result.capacities[type];
    }
    return result;
}()};

[[nodiscard]] inline auto index(EntityId const id,
                                [[maybe_unused]] EntityType const type,
                                [[maybe_unused]] std::size_t const capacity) noexcept
    -> std::size_t {
    assert(id.is_valid() && id.entity_type() == type && id.index() < capacity);
    return id.index();
}
}

class HealthConstView {
  public:
    HealthConstView() = default;
    explicit HealthConstView(EntityType const type, std::span<Health const> const values) noexcept
        : type_{type}
        , values_{values} {}

    [[nodiscard]] auto is_empty() const noexcept -> bool { return values_.empty(); }
    [[nodiscard]] auto values() const noexcept -> std::span<Health const> { return values_; }
    [[nodiscard]] auto health(EntityId const id) const -> Health {
        return values_[health_storage::index(id, type_, values_.size())];
    }
    void copy_to(std::span<EntityId const> const ids, std::span<Health> const output) const {
        assert(output.size() == ids.size());
        auto const count{ids.size()};
        for (std::size_t index{}; index < count; ++index) {
            output[index] = health(ids[index]);
        }
    }
  private:
    EntityType type_{};
    std::span<Health const> values_;
};

class HealthView {
  public:
    HealthView() = default;
    explicit HealthView(EntityType const type, std::span<Health> const values) noexcept
        : type_{type}
        , values_{values} {}

    [[nodiscard]] auto is_empty() const noexcept -> bool { return values_.empty(); }
    [[nodiscard]] auto health(EntityId const id) const -> Health {
        return values_[health_storage::index(id, type_, values_.size())];
    }
    void set_health(EntityId const id, Health const value) const {
        values_[health_storage::index(id, type_, values_.size())] = value;
    }
    void copy_from(std::span<EntityId const> const ids, std::span<Health const> const input) const {
        assert(input.size() == ids.size());
        auto const count{ids.size()};
        for (std::size_t index{}; index < count; ++index) {
            set_health(ids[index], input[index]);
        }
    }
    void copy_to(std::span<EntityId const> const ids, std::span<Health> const output) const {
        HealthConstView{type_, values_}.copy_to(ids, output);
    }
  private:
    EntityType type_{};
    std::span<Health> values_;
};

class HealthTable : public EntityTableOperations {
  public:
    using view_type = HealthView;
    using const_view_type = HealthConstView;

    explicit HealthTable(std::pmr::memory_resource& memory = *std::pmr::get_default_resource())
        : values_(health_storage::layout.capacity, &memory) {}

    [[nodiscard]] auto health(EntityId const id) const -> Health {
        return values_[storage_index(id)];
    }
    void set_health(EntityId const id, Health const value) { values_[storage_index(id)] = value; }

    template <EntityType Type>
    void initialise_entities(std::span<EntityId const> const ids,
                             std::span<Health const> const initial_values) {
        view<Type>().copy_from(ids, initial_values);
    }

    template <EntityType Type>
    void initialise_entities(std::span<EntityId const> const ids, Health const initial_value) {
        auto const view{this->view<Type>()};
        for (auto const id : ids) {
            view.set_health(id, initial_value);
        }
    }
  private:
    friend struct EntityTableOperations;

    template <typename Self, typename Func>
    decltype(auto) with_columns(this Self& self, EntityType const type, Func&& func) {
        auto const subspan{
            Subspan{health_storage::layout.offsets[type], health_storage::layout.capacities[type]}};
        return std::forward<Func>(func)(subspan(self.values_));
    }

    [[nodiscard]] static constexpr auto is_supported_type(EntityType const type) noexcept -> bool {
        return is_damageable(type);
    }

    [[nodiscard]] static auto storage_index(EntityId const id) noexcept -> std::size_t {
        assert(is_entity_identity_offset(id) && is_damageable(id.entity_type()));
        return health_storage::layout.offsets[id.entity_type()] + id.index();
    }

    std::pmr::vector<Health> values_;
};
}
