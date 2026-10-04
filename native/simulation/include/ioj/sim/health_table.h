#pragma once

#include <ioj/sim/entity_identity_layout.h>
#include <ioj/sim/entity_instance_handle.h>
#include <ioj/sim/health.h>

#include <sandbox/core/range_fits.h>
#include <sandbox/core/soa_permutation.h>

#include <algorithm>
#include <cassert>
#include <span>
#include <vector>

namespace ioj::sim {
inline constexpr auto has_health(EntityType const type) -> bool {
    return type == EntityType::PlayerShip || type == EntityType::CapitalShip ||
           type == EntityType::Fighter || type == EntityType::Turret;
}
namespace health_storage {
struct Layout {
    EntityTypeSizes capacities{};
    EntityTypeSizes offsets{};
    std::uint32_t capacity{};
};
inline constexpr auto layout{[] {
    Layout result;
    for (auto const type : ml::EnumTraits<EntityType>::values) {
        result.capacities[type] = has_health(type) ? entity_lifetime_capacities[type] : 0;
        result.offsets[type] = result.capacity;
        result.capacity += result.capacities[type];
    }
    return result;
}()};
}

class HealthConstView {
  public:
    HealthConstView() = default;
    explicit HealthConstView(std::span<Health const> const values) noexcept
        : values_{values} {}
    [[nodiscard]] auto num() const noexcept -> std::uint32_t {
        return static_cast<std::uint32_t>(values_.size());
    }
    [[nodiscard]] auto is_empty() const noexcept -> bool { return values_.empty(); }
    [[nodiscard]] auto values() const noexcept -> std::span<Health const> { return values_; }
    [[nodiscard]] auto health(EntityFrameIndex const row) const -> Health {
        assert(row < values_.size());
        return values_[row];
    }
    void copy_to(std::span<Health> const output) const {
        assert(output.size() == values_.size());
        std::ranges::copy(values_, output.begin());
    }
  private:
    std::span<Health const> values_;
};

class HealthView {
  public:
    HealthView() = default;
    [[nodiscard]] auto num() const noexcept -> std::uint32_t {
        return static_cast<std::uint32_t>(values_.size());
    }
    [[nodiscard]] auto is_empty() const noexcept -> bool { return values_.empty(); }
    [[nodiscard]] auto health(EntityFrameIndex const row) const -> Health {
        assert(row < values_.size());
        return values_[row];
    }
    void set_health(EntityFrameIndex const row, Health const value) const {
        assert(row < values_.size());
        values_[row] = value;
    }
    void copy_from(std::span<Health const> const input) const {
        assert(input.size() == values_.size());
        std::ranges::copy(input, values_.begin());
    }
    void copy_to(std::span<Health> const output) const { HealthConstView{values_}.copy_to(output); }
  private:
    friend class HealthTable;
    explicit HealthView(std::span<Health> values)
        : values_{values} {}
    std::span<Health> values_;
};

class HealthTable {
  public:
    HealthTable()
        : values_(health_storage::layout.capacity) {}

    [[nodiscard]] auto get_const_view(EntityType const type, std::size_t const count) const
        -> HealthConstView {
        assert(has_health(type) && count <= health_storage::layout.capacities[type]);
        return HealthConstView{
            std::span{values_}.subspan(health_storage::layout.offsets[type], count)};
    }

    template <EntityType Type>
    [[nodiscard]] auto get_const_view(std::size_t const count) const -> HealthConstView {
        static_assert(has_health(Type));
        assert(count <= health_storage::layout.capacities[Type]);
        constexpr auto base{health_storage::layout.offsets[Type]};
        return HealthConstView{std::span{values_}.subspan(base, count)};
    }

    template <EntityType Type>
    [[nodiscard]] auto get_view(std::size_t const count) -> HealthView {
        static_assert(has_health(Type));
        assert(count <= health_storage::layout.capacities[Type]);
        constexpr auto base{health_storage::layout.offsets[Type]};
        return HealthView{std::span{values_}.subspan(base, count)};
    }

    template <EntityType Type>
    void initialise_rows(EntityFrameIndex const first,
                         std::span<Health const> const initial_values) {
        [[maybe_unused]] auto const count{static_cast<EntityFrameIndex>(initial_values.size())};
        assert(ml::range_fits(first,
                              count,
                              EntityInstanceHandle::index_minimum,
                              EntityInstanceHandle::index_maximum));
        assert(first + count <= health_storage::layout.capacities[Type]);
        constexpr auto base{health_storage::layout.offsets[Type]};
        std::ranges::copy(initial_values, values_.begin() + base + first);
    }

    template <EntityType Type>
    void initialise_rows(EntityFrameIndex const first,
                         EntityFrameIndex const count,
                         Health const initial_value) {
        assert(ml::range_fits(first,
                              count,
                              EntityInstanceHandle::index_minimum,
                              EntityInstanceHandle::index_maximum));
        assert(first + count <= health_storage::layout.capacities[Type]);
        constexpr auto base{health_storage::layout.offsets[Type]};
        std::fill_n(values_.begin() + base + first, count, initial_value);
    }

    template <EntityType Type>
    void remove_rows(std::size_t count, std::span<EntityFrameIndex const> const rows) {
        assert(count <= health_storage::layout.capacities[Type]);
        constexpr auto base{health_storage::layout.offsets[Type]};
        [[maybe_unused]] auto previous{count};
        for (auto const row : rows) {
            assert(row < previous && row < count);
            previous = row;
            values_[base + row] = values_[base + --count];
        }
    }

    template <EntityType Type>
    void apply_permutation(std::span<std::int32_t> const order) {
        assert(order.size() <= health_storage::layout.capacities[Type]);
        constexpr auto base{health_storage::layout.offsets[Type]};
        ml::apply_permutation(std::span{values_}.subspan(base, order.size()), order);
    }
  private:
    std::vector<Health> values_;
};
}
