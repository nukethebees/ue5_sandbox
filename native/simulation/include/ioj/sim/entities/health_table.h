#pragma once

#include <ioj/range_fits.h>
#include <ioj/sim/entities/entity_identity_layout.h>
#include <ioj/sim/entities/entity_instance_handle.h>
#include <ioj/sim/entities/health.h>
#include <ioj/soa_permutation.h>
#include <ioj/subspan.h>

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
    for (auto const type : EnumTraits<EntityType>::values) {
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
    [[nodiscard]] auto health(EntityFrameIndex::value_type const row) const -> Health {
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
    [[nodiscard]] auto health(EntityFrameIndex::value_type const row) const -> Health {
        assert(row < values_.size());
        return values_[row];
    }
    void set_health(EntityFrameIndex::value_type const row, Health const value) const {
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
        assert(has_health(type) && fits_capacity(type, count));
        auto const subspan{Subspan{health_storage::layout.offsets[type], count}};
        return HealthConstView{subspan(values_)};
    }

    template <EntityType Type>
    [[nodiscard]] auto get_const_view(std::size_t const count) const -> HealthConstView {
        static_assert(has_health(Type));
        assert(fits_capacity(Type, count));
        auto const subspan{Subspan{health_storage::layout.offsets[Type], count}};
        return HealthConstView{subspan(values_)};
    }

    template <EntityType Type>
    [[nodiscard]] auto get_view(std::size_t const count) -> HealthView {
        static_assert(has_health(Type));
        assert(fits_capacity(Type, count));
        auto const subspan{Subspan{health_storage::layout.offsets[Type], count}};
        return HealthView{subspan(values_)};
    }

    template <EntityType Type>
    void initialise_rows(EntityFrameIndex::value_type const first,
                         std::span<Health const> const initial_values) {
        [[maybe_unused]] auto const count{
            static_cast<EntityFrameIndex::value_type>(initial_values.size())};
        assert(fits_index_range(first, count));
        assert(fits_capacity(Type, first + count));
        auto const subspan{Subspan{health_storage::layout.offsets[Type] + first, count}};
        std::ranges::copy(initial_values, subspan(values_).begin());
    }

    template <EntityType Type>
    void initialise_rows(EntityFrameIndex::value_type const first,
                         EntityFrameIndex::value_type const count,
                         Health const initial_value) {
        assert(fits_index_range(first, count));
        assert(fits_capacity(Type, first + count));
        auto const subspan{Subspan{health_storage::layout.offsets[Type] + first, count}};
        std::ranges::fill(subspan(values_), initial_value);
    }

    template <EntityType Type>
    void remove_rows(std::size_t count, std::span<EntityFrameIndex::value_type const> const rows) {
        assert(fits_capacity(Type, count));
        auto const values{Subspan{health_storage::layout.offsets[Type], count}(values_)};
        [[maybe_unused]] auto previous{count};
        for (auto const row : rows) {
            assert(row < previous && row < count);
            previous = row;
            values[row] = values[--count];
        }
    }

    template <EntityType Type>
    void apply_permutation(std::span<std::int32_t> const order) {
        assert(fits_capacity(Type, order.size()));
        auto const subspan{Subspan{health_storage::layout.offsets[Type], order.size()}};
        ioj::apply_permutation(subspan(values_), order);
    }
  private:
    [[nodiscard]] static constexpr auto fits_capacity(EntityType const type,
                                                      std::size_t const count) noexcept -> bool {
        return count <= health_storage::layout.capacities[type];
    }

    [[nodiscard]] static constexpr auto
        fits_index_range(EntityFrameIndex::value_type const first,
                         EntityFrameIndex::value_type const count) noexcept -> bool {
        return range_fits(first,
                          count,
                          EntityInstanceHandle::index_layout.range.min,
                          EntityInstanceHandle::index_layout.range.max);
    }

    std::vector<Health> values_;
};
}
