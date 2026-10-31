#include <ioj/sim/tasks/executor_table.h>

#include <ioj/range_fits.h>
#include <ioj/soa_permutation.h>

#include <algorithm>
#include <cassert>

namespace ioj::sim::tasks {
namespace executor_storage {
inline constexpr EntityTypeSizes offsets{[] {
    EntityTypeSizes result;
    std::uint32_t offset{};
    for (auto const type : EnumTraits<EntityType>::values) {
        result[type] = offset;
        offset += entity_lifetime_capacities[type];
    }
    return result;
}()};

[[nodiscard]] constexpr auto fits_capacity(EntityType const type, std::size_t const count) noexcept
    -> bool {
    return count <= entity_lifetime_capacities[type];
}
}

ExecutorTable::ExecutorTable()
    : current_tasks(entity_identity_capacity)
    , pending_tasks(entity_identity_capacity)
    , current_instructions(entity_identity_capacity) {}

auto ExecutorTable::get_const_view(EntityType const type, std::size_t const count) const
    -> ExecutorConstView {
    assert(executor_storage::fits_capacity(type, count));
    auto const base{executor_storage::offsets[type]};
    return {std::span{current_tasks}.subspan(base, count),
            std::span{pending_tasks}.subspan(base, count),
            std::span{current_instructions}.subspan(base, count)};
}

auto ExecutorTable::get_view(EntityType const type, std::size_t const count) -> ExecutorView {
    assert(executor_storage::fits_capacity(type, count));
    auto const base{executor_storage::offsets[type]};
    return {std::span{current_tasks}.subspan(base, count),
            std::span{pending_tasks}.subspan(base, count),
            std::span{current_instructions}.subspan(base, count)};
}

void ExecutorTable::initialise_rows(EntityType const type,
                                    EntityFrameIndex::value_type const first,
                                    EntityFrameIndex::value_type const count) {
    assert(range_fits(first,
                      count,
                      EntityInstanceHandle::index_layout.range.min,
                      EntityInstanceHandle::index_layout.range.max));
    auto const end{static_cast<std::size_t>(first) + count};
    auto const view{get_view(type, end)};
    std::ranges::fill(view.current_tasks.subspan(first, count), TaskId{});
    std::ranges::fill(view.pending_tasks.subspan(first, count), TaskId{});
    std::ranges::fill(view.current_instructions.subspan(first, count), InstructionHandle{});
}

void ExecutorTable::remove_rows(EntityType const type,
                                std::size_t count,
                                std::span<EntityFrameIndex::value_type const> const rows) {
    auto const view{get_view(type, count)};
    [[maybe_unused]] auto previous{count};
    for (auto const row : rows) {
        assert(row < previous && row < count);
        previous = row;
        auto const last{--count};
        view.current_tasks[row] = view.current_tasks[last];
        view.pending_tasks[row] = view.pending_tasks[last];
        view.current_instructions[row] = view.current_instructions[last];
    }
}

void ExecutorTable::apply_permutation(EntityType const type, std::span<std::int32_t> const order) {
    auto const view{get_view(type, order.size())};
    ioj::apply_permutation(view.current_tasks, order);
    ioj::apply_permutation(view.pending_tasks, order);
    ioj::apply_permutation(view.current_instructions, order);
}
}
