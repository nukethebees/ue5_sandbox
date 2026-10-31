#include <ioj/sim/tasks/executor_table.h>

#include <ioj/range_fits.h>
#include <ioj/subspan.h>

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

ExecutorTable::ExecutorTable(std::pmr::memory_resource& resource)
    : current_tasks(entity_identity_capacity, &resource)
    , pending_tasks(entity_identity_capacity, &resource)
    , current_instructions(entity_identity_capacity, &resource) {}

auto ExecutorTable::subspan_for(EntityType const type, std::size_t const count) -> Subspan {
    assert(executor_storage::fits_capacity(type, count));
    return Subspan{executor_storage::offsets[type], count};
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
    auto const subspan{Subspan{first, count}};
    std::ranges::fill(subspan(view.current_tasks), TaskId{});
    std::ranges::fill(subspan(view.pending_tasks), TaskId{});
    std::ranges::fill(subspan(view.current_instructions), InstructionHandle{});
}

void ExecutorTable::remove_rows(EntityType const type,
                                std::size_t const count,
                                std::span<EntityFrameIndex::value_type const> const rows) {
    remove_rows_impl(type, count, rows);
}

void ExecutorTable::apply_permutation(EntityType const type, std::span<std::int32_t> const order) {
    apply_permutation_impl(type, order);
}
}
