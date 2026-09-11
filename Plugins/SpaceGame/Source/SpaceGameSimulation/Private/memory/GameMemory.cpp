#include "SpaceGameSimulation/memory/GameMemory.h"

namespace game_memory_detail {
auto checked_capacity(FGameMemoryBacking& backing, FGameMemoryConfig const config) -> SIZE_T {
    checkf(backing.capacity_bytes() >= config.root_capacity_bytes,
           TEXT("The game-memory backing has %llu bytes but %llu bytes were requested."),
           static_cast<uint64>(backing.capacity_bytes()),
           static_cast<uint64>(config.root_capacity_bytes));
    return config.root_capacity_bytes;
}
}

FGameMemory::FGameMemory(FGameMemoryConfig const config)
    : local_backing_{FGameMemoryBacking::create(config.root_capacity_bytes)}
    , backing_{local_backing_.Get()}
    , root_arena_{backing_->data(), game_memory_detail::checked_capacity(*backing_, config)} {
    check(local_backing_.IsValid());
    check(!external_lease_);
    check(backing_ != nullptr);
}

FGameMemory::FGameMemory(FGameMemoryBackingLease lease, FGameMemoryConfig const config)
    : external_lease_{MoveTemp(lease)}
    , backing_{&external_lease_.get()}
    , root_arena_{backing_->data(), game_memory_detail::checked_capacity(*backing_, config)} {
    check(!local_backing_.IsValid());
    check(external_lease_);
    check(backing_ != nullptr);
}

FGameMemory::~FGameMemory() = default;

auto FGameMemory::try_acquire_block(SIZE_T const size_bytes, SIZE_T const alignment)
    -> std::optional<FGameMemoryBlock> {
    auto block{root_arena_.try_acquire_block(size_bytes, alignment)};
    if (!block) {
        return std::nullopt;
    }
    return FGameMemoryBlock{std::move(block)};
}

auto FGameMemory::acquire_block(SIZE_T const size_bytes, SIZE_T const alignment)
    -> FGameMemoryBlock {
    auto block{try_acquire_block(size_bytes, alignment)};
    auto const stats{root_arena_.get_statistics()};
    checkf(block.has_value(),
           TEXT("Game memory exhausted: requested=%llu alignment=%llu claimed=%llu capacity=%llu"),
           static_cast<uint64>(size_bytes),
           static_cast<uint64>(alignment),
           static_cast<uint64>(stats.claimed_bytes),
           static_cast<uint64>(stats.total_capacity_bytes));
    return std::move(*block);
}

auto FGameMemory::memory_resource() noexcept -> std::pmr::memory_resource& {
    return root_arena_.memory_resource();
}

auto FGameMemory::get_stats() const noexcept -> FGameMemoryStats {
    return root_arena_.get_statistics();
}

auto FGameMemory::get_last_allocation_failure() const noexcept
    -> std::optional<FGameMemoryAllocationFailure> {
    auto const* const failure{root_arena_.get_last_allocation_failure()};
    return failure != nullptr ? std::optional<FGameMemoryAllocationFailure>{*failure}
                              : std::nullopt;
}
