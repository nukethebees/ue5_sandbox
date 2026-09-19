#include <ioj/sim/health_table.h>

#include <algorithm>

namespace ioj::sim {

namespace {
[[nodiscard]] auto checked_slot(std::span<EntityUniqueId const> const owners [[maybe_unused]],
                                std::span<HealthIndex const> const indices,
                                std::span<EntityUniqueId const> const expected_owners
                                    [[maybe_unused]],
                                std::int32_t const row) -> std::size_t {
    assert(row >= 0 && static_cast<std::size_t>(row) < indices.size());
    auto const index{indices[static_cast<std::size_t>(row)]};
    assert(index.is_valid() && index.raw_value() < owners.size());
    auto const slot{static_cast<std::size_t>(index.raw_value())};
    assert(owners[slot].is_valid());
    assert(expected_owners.size() == indices.size());
    assert(owners[slot] == expected_owners[static_cast<std::size_t>(row)]);
    return slot;
}
}

auto HealthConstView::health(std::int32_t const row) const -> Health {
    return values_[checked_slot(owners_, indices_, expected_owners_, row)];
}
auto HealthConstView::owner(std::int32_t const row) const -> EntityUniqueId {
    return owners_[checked_slot(owners_, indices_, expected_owners_, row)];
}
void HealthConstView::copy_to(std::span<Health> const output) const {
    assert(output.size() == indices_.size());
    auto const count{num()};
    for (std::int32_t row{}; row < count; ++row) {
        output[static_cast<std::size_t>(row)] = health(row);
    }
}
auto HealthView::health(std::int32_t const row) const -> Health& {
    return values_[checked_slot(owners_, indices_, expected_owners_, row)];
}
auto HealthView::owner(std::int32_t const row) const -> EntityUniqueId {
    return owners_[checked_slot(owners_, indices_, expected_owners_, row)];
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
}
void HealthTable::add(std::span<EntityUniqueId const> const owners,
                      std::span<Health const> const initial_values,
                      std::span<HealthIndex> const output_indices) {
    assert(owners.size() == initial_values.size());
    assert(owners.size() == output_indices.size());
    auto const count{owners.size()};
    for (std::size_t row{}; row < count; ++row) {
        assert(owners[row].is_valid());
        assert(values_.size() < HealthIndex::invalid_value);
        output_indices[row] = HealthIndex{static_cast<HealthIndex::storage_type>(values_.size())};
        values_.push_back(initial_values[row]);
        owners_.push_back(owners[row]);
    }
}
void HealthTable::add(std::span<EntityUniqueId const> const owners,
                      Health const initial_value,
                      std::span<HealthIndex> const output_indices) {
    assert(owners.size() == output_indices.size());
    auto const count{owners.size()};
    for (std::size_t row{}; row < count; ++row) {
        assert(owners[row].is_valid() && values_.size() < HealthIndex::invalid_value);
        output_indices[row] = HealthIndex{static_cast<HealthIndex::storage_type>(values_.size())};
        values_.push_back(initial_value);
        owners_.push_back(owners[row]);
    }
}
auto HealthTable::remove(HealthIndex const index, EntityUniqueId const owner)
    -> std::optional<HealthMove> {
    auto const removed_slot{slot(index)};
    assert(owners_[removed_slot] == owner);
#ifdef NDEBUG
    static_cast<void>(owner);
#endif

    auto const final_slot{values_.size() - 1};
    std::optional<HealthMove> move;
    if (removed_slot != final_slot) {
        auto const moved_owner{owners_[final_slot]};
        auto const old_index{HealthIndex{static_cast<HealthIndex::storage_type>(final_slot)}};
        values_[removed_slot] = values_[final_slot];
        owners_[removed_slot] = moved_owner;
        move = HealthMove{moved_owner, old_index, index};
    }

    values_.pop_back();
    owners_.pop_back();
    return move;
}
auto HealthTable::get_view(std::span<HealthIndex const> const indices,
                           std::span<EntityUniqueId const> const owners) -> HealthView {
    validate_owners(indices, owners);
    return {{values_.data(), values_.size()}, {owners_.data(), owners_.size()}, indices, owners};
}
auto HealthTable::get_const_view(std::span<HealthIndex const> const indices,
                                 std::span<EntityUniqueId const> const owners) const
    -> HealthConstView {
    validate_owners(indices, owners);
    return {{values_.data(), values_.size()}, {owners_.data(), owners_.size()}, indices, owners};
}
auto HealthTable::contains(HealthIndex const index, EntityUniqueId const owner) const noexcept
    -> bool {
    return valid_slot(index) && owners_[static_cast<std::size_t>(index.raw_value())] == owner;
}
auto HealthTable::get_health(HealthIndex const index, EntityUniqueId const owner) const -> Health {
    assert(contains(index, owner));
    return values_[slot(index)];
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
void HealthTable::validate_owners(std::span<HealthIndex const> const indices,
                                  std::span<EntityUniqueId const> const owners) const {
    assert(indices.size() == owners.size());
#ifndef NDEBUG
    auto const count{indices.size()};
    for (std::size_t row{}; row < count; ++row) {
        assert(contains(indices[row], owners[row]));
    }
#else
    static_cast<void>(indices);
    static_cast<void>(owners);
#endif
}
auto HealthTable::slot(HealthIndex const index) const -> std::size_t {
    assert(index.is_valid() && index.raw_value() < values_.size());
    return static_cast<std::size_t>(index.raw_value());
}

} // namespace ioj::sim
