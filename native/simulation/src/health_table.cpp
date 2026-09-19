#include <ioj/sim/health_table.h>

#include <algorithm>

namespace ioj::sim {

namespace {
[[nodiscard]] auto checked_slot(std::span<EntityUniqueId const> const owners [[maybe_unused]],
                                std::span<HealthIndex const> const indices,
                                std::int32_t const row) -> std::size_t {
    assert(row >= 0 && static_cast<std::size_t>(row) < indices.size());
    auto const index{indices[static_cast<std::size_t>(row)]};
    assert(index.is_valid() && index.raw_value() < owners.size());
    auto const slot{static_cast<std::size_t>(index.raw_value())};
    assert(owners[slot].is_valid());
    return slot;
}
}

auto HealthConstView::health(std::int32_t const row) const -> Health {
    return values_[checked_slot(owners_, indices_, row)];
}
auto HealthConstView::owner(std::int32_t const row) const -> EntityUniqueId {
    return owners_[checked_slot(owners_, indices_, row)];
}
void HealthConstView::copy_to(std::span<Health> const output) const {
    assert(output.size() == indices_.size());
    auto const count{num()};
    for (std::int32_t row{}; row < count; ++row) {
        output[static_cast<std::size_t>(row)] = health(row);
    }
}
auto HealthView::health(std::int32_t const row) const -> Health& {
    return values_[checked_slot(owners_, indices_, row)];
}
auto HealthView::owner(std::int32_t const row) const -> EntityUniqueId {
    return owners_[checked_slot(owners_, indices_, row)];
}
void HealthView::copy_from(std::span<Health const> const input) const {
    assert(input.size() == indices_.size());
    auto const count{num()};
    for (std::int32_t row{}; row < count; ++row) {
        health(row) = input[static_cast<std::size_t>(row)];
    }
}
void HealthView::copy_to(std::span<Health> const output) const {
    assert(output.size() == indices_.size());
    auto const count{num()};
    for (std::int32_t row{}; row < count; ++row) {
        output[static_cast<std::size_t>(row)] = health(row);
    }
}

void HealthTable::reserve(std::size_t const capacity) {
    values_.reserve(capacity);
    owners_.reserve(capacity);
    free_indices_.reserve(capacity);
}
void HealthTable::add(std::span<EntityUniqueId const> const owners,
                      std::span<Health const> const initial_values,
                      std::span<HealthIndex> const output_indices) {
    assert(owners.size() == initial_values.size());
    assert(owners.size() == output_indices.size());
    auto const count{owners.size()};
    for (std::size_t row{}; row < count; ++row) {
        assert(owners[row].is_valid());
        if (free_indices_.empty()) {
            assert(values_.size() < HealthIndex::invalid_value);
            output_indices[row] =
                HealthIndex{static_cast<HealthIndex::storage_type>(values_.size())};
            values_.push_back(initial_values[row]);
            owners_.push_back(owners[row]);
        } else {
            auto const index{free_indices_.back()};
            free_indices_.pop_back();
            auto const index_slot{slot(index)};
            assert(!owners_[index_slot].is_valid());
            output_indices[row] = index;
            values_[index_slot] = initial_values[row];
            owners_[index_slot] = owners[row];
        }
    }
}
void HealthTable::add(std::span<EntityUniqueId const> const owners,
                      Health const initial_value,
                      std::span<HealthIndex> const output_indices) {
    assert(owners.size() == output_indices.size());
    auto const count{owners.size()};
    for (std::size_t row{}; row < count; ++row) {
        if (free_indices_.empty()) {
            assert(owners[row].is_valid() && values_.size() < HealthIndex::invalid_value);
            output_indices[row] =
                HealthIndex{static_cast<HealthIndex::storage_type>(values_.size())};
            values_.push_back(initial_value);
            owners_.push_back(owners[row]);
        } else {
            auto const index{free_indices_.back()};
            free_indices_.pop_back();
            auto const index_slot{slot(index)};
            assert(owners[row].is_valid() && !owners_[index_slot].is_valid());
            output_indices[row] = index;
            values_[index_slot] = initial_value;
            owners_[index_slot] = owners[row];
        }
    }
}
void HealthTable::remove_rows(std::span<std::int32_t const> const rows,
                              std::span<HealthIndex const> const indices,
                              std::span<EntityUniqueId const> const owners [[maybe_unused]]) {
    auto const row_count{rows.size()};
    for (std::size_t row_index{}; row_index < row_count; ++row_index) {
        auto const row{rows[row_index]};
        assert(row >= 0 && static_cast<std::size_t>(row) < indices.size());
        auto const element{static_cast<std::size_t>(row)};
        auto const index{indices[element]};
        auto const index_slot{slot(index)};
        assert(owners_[index_slot] == owners[element]);
        owners_[index_slot] = {};
        values_[index_slot] = {};
        free_indices_.push_back(index);
    }
}
auto HealthTable::get_view(std::span<HealthIndex const> const indices) -> HealthView {
    return {{values_.data(), values_.size()}, {owners_.data(), owners_.size()}, indices};
}
auto HealthTable::get_const_view(std::span<HealthIndex const> const indices) const
    -> HealthConstView {
    return {{values_.data(), values_.size()}, {owners_.data(), owners_.size()}, indices};
}
auto HealthTable::contains(HealthIndex const index, EntityUniqueId const owner) const noexcept
    -> bool {
    return valid_slot(index) && owners_[static_cast<std::size_t>(index.raw_value())] == owner;
}
auto HealthTable::get_health(HealthIndex const index) const -> Health {
    assert(valid_slot(index));
    auto const index_slot{slot(index)};
    return values_[index_slot];
}
auto HealthTable::get_owner(HealthIndex const index) const -> EntityUniqueId {
    assert(valid_slot(index));
    auto const index_slot{slot(index)};
    return owners_[index_slot];
}
auto HealthTable::valid_slot(HealthIndex const index) const noexcept -> bool {
    return index.is_valid() && index.raw_value() < owners_.size() &&
           owners_[static_cast<std::size_t>(index.raw_value())].is_valid();
}
auto HealthTable::slot(HealthIndex const index) const -> std::size_t {
    assert(index.is_valid() && index.raw_value() < values_.size());
    return static_cast<std::size_t>(index.raw_value());
}

} // namespace ioj::sim
