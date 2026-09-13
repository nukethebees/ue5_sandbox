#include "sandbox/simulation/memory/GameMemory.h"
#include <cassert>
#include <format>
#include <sandbox/core/diagnostics.h>

namespace game_memory_detail {
auto checked_capacity(FGameMemoryBacking& backing, FGameMemoryConfig const config) -> std::size_t {
    if (backing.capacity_bytes() < config.root_capacity_bytes) {
        ml::fatal_error(std::format("Game-memory backing has {} bytes; {} bytes requested",
                                    backing.capacity_bytes(),
                                    config.root_capacity_bytes));
    }
    return config.root_capacity_bytes;
}
}

FGameMemory::FGameMemory(FGameMemoryConfig const config)
    : local_backing_{FGameMemoryBacking::create(config.root_capacity_bytes)}
    , backing_{local_backing_.get()}
    , root_arena_{backing_->data(), game_memory_detail::checked_capacity(*backing_, config)} {
    assert(static_cast<bool>(local_backing_));
    assert(!external_lease_);
    assert(backing_ != nullptr);
}

FGameMemory::FGameMemory(FGameMemoryBackingLease lease, FGameMemoryConfig const config)
    : external_lease_{std::move(lease)}
    , backing_{&external_lease_.get()}
    , root_arena_{backing_->data(), game_memory_detail::checked_capacity(*backing_, config)} {
    assert(!static_cast<bool>(local_backing_));
    assert(external_lease_);
    assert(backing_ != nullptr);
}

FGameMemory::~FGameMemory() = default;

auto FGameMemory::try_acquire_block(std::size_t const size_bytes, std::size_t const alignment)
    -> std::optional<FGameMemoryBlock> {
    auto block{root_arena_.try_acquire_block(size_bytes, alignment)};
    if (!block) {
        return std::nullopt;
    }
    return FGameMemoryBlock{std::move(block)};
}

auto FGameMemory::acquire_block(std::size_t const size_bytes, std::size_t const alignment)
    -> FGameMemoryBlock {
    auto block{try_acquire_block(size_bytes, alignment)};
    if (!block) {
        auto const stats{root_arena_.get_statistics()};
        ml::fatal_error(
            std::format("Game memory exhausted: requested={} alignment={} claimed={} capacity={}",
                        size_bytes,
                        alignment,
                        stats.claimed_bytes,
                        stats.total_capacity_bytes));
    }
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
