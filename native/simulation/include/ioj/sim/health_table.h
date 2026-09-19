#pragma once

#include <ioj/sim/entity_unique_id.h>
#include <ioj/sim/health.h>

#include <cassert>
#include <compare>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <vector>

namespace ioj::sim {

struct HealthIndex {
    using storage_type = std::uint32_t;

    inline static constexpr storage_type invalid_value{std::numeric_limits<storage_type>::max()};

    constexpr HealthIndex() noexcept = default;
    explicit constexpr HealthIndex(storage_type const value) noexcept
        : value_{value} {}

    [[nodiscard]] constexpr auto raw_value() const noexcept -> storage_type { return value_; }
    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {
        return value_ != invalid_value;
    }
    [[nodiscard]] constexpr auto operator<=>(HealthIndex const&) const noexcept = default;
  private:
    storage_type value_{invalid_value};
};
static_assert(sizeof(HealthIndex) == sizeof(HealthIndex::storage_type));
static_assert(std::is_trivially_copyable_v<HealthIndex>);
static_assert(std::is_standard_layout_v<HealthIndex>);

class HealthTable;

class HealthConstView {
  public:
    HealthConstView() = default;

    [[nodiscard]] auto num() const noexcept -> std::int32_t {
        return static_cast<std::int32_t>(indices_.size());
    }
    [[nodiscard]] auto is_empty() const noexcept -> bool { return indices_.empty(); }
    [[nodiscard]] auto indices() const noexcept -> std::span<HealthIndex const> { return indices_; }
    [[nodiscard]] auto health(std::int32_t row) const -> Health;
    [[nodiscard]] auto owner(std::int32_t row) const -> EntityUniqueId;
    void copy_to(std::span<Health> output) const;
  private:
    friend class HealthTable;

    HealthConstView(std::span<Health const> values,
                    std::span<EntityUniqueId const> owners,
                    std::span<HealthIndex const> indices) noexcept
        : values_{values}
        , owners_{owners}
        , indices_{indices} {}

    std::span<Health const> values_{};
    std::span<EntityUniqueId const> owners_{};
    std::span<HealthIndex const> indices_{};
};

class HealthView {
  public:
    HealthView() = default;

    [[nodiscard]] auto num() const noexcept -> std::int32_t {
        return static_cast<std::int32_t>(indices_.size());
    }
    [[nodiscard]] auto is_empty() const noexcept -> bool { return indices_.empty(); }
    [[nodiscard]] auto indices() const noexcept -> std::span<HealthIndex const> { return indices_; }
    [[nodiscard]] auto health(std::int32_t row) const -> Health&;
    [[nodiscard]] auto owner(std::int32_t row) const -> EntityUniqueId;
    void copy_from(std::span<Health const> input) const;
    void copy_to(std::span<Health> output) const;
  private:
    friend class HealthTable;

    HealthView(std::span<Health> values,
               std::span<EntityUniqueId const> owners,
               std::span<HealthIndex const> indices) noexcept
        : values_{values}
        , owners_{owners}
        , indices_{indices} {}

    std::span<Health> values_{};
    std::span<EntityUniqueId const> owners_{};
    std::span<HealthIndex const> indices_{};
};

// Health slots are stable until removed. A free-list avoids component relocation while
// entity SOAs swap-remove and fighters reorder; owners make a later compacting policy possible.
class HealthTable {
  public:
    void reserve(std::size_t capacity);

    void add(std::span<EntityUniqueId const> owners,
             std::span<Health const> initial_values,
             std::span<HealthIndex> output_indices);
    void add(std::span<EntityUniqueId const> owners,
             Health initial_value,
             std::span<HealthIndex> output_indices);
    void remove_rows(std::span<std::int32_t const> rows,
                     std::span<HealthIndex const> indices,
                     std::span<EntityUniqueId const> owners);

    [[nodiscard]] auto get_view(std::span<HealthIndex const> indices) -> HealthView;
    [[nodiscard]] auto get_const_view(std::span<HealthIndex const> indices) const
        -> HealthConstView;
    [[nodiscard]] auto contains(HealthIndex index, EntityUniqueId owner) const noexcept -> bool;
    [[nodiscard]] auto get_health(HealthIndex index) const -> Health;
    [[nodiscard]] auto get_owner(HealthIndex index) const -> EntityUniqueId;
    [[nodiscard]] auto num_slots() const noexcept -> std::int32_t {
        return static_cast<std::int32_t>(values_.size());
    }
  private:
    [[nodiscard]] auto valid_slot(HealthIndex index) const noexcept -> bool;
    [[nodiscard]] auto slot(HealthIndex index) const -> std::size_t;

    std::vector<Health> values_{};
    std::vector<EntityUniqueId> owners_{};
    std::vector<HealthIndex> free_indices_{};
};

} // namespace ioj::sim
