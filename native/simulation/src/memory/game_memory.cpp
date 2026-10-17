#include "ioj/sim/memory/game_memory.h"

#include <ioj/diagnostics.h>

#include <cassert>
#include <format>

namespace ioj::sim {

namespace game_memory_detail {
auto checked_capacity(memory::Backing& backing, memory::Config const config) -> std::size_t {
    if (backing.capacity_bytes() < config.root_capacity_bytes) {
        ml::fatal_error(std::format("Game-memory backing has {} bytes; {} bytes requested",
                                    backing.capacity_bytes(),
                                    config.root_capacity_bytes));
    }
    return config.root_capacity_bytes;
}
}

GameMemory::GameMemory(memory::Config const config)
    : local_backing_{memory::Backing::create(config.root_capacity_bytes)}
    , backing_{local_backing_.get()}
    , root_arena_{backing_->data(), game_memory_detail::checked_capacity(*backing_, config)} {
    assert(static_cast<bool>(local_backing_));
    assert(!external_lease_);
    assert(backing_ != nullptr);
}

GameMemory::GameMemory(memory::BackingLease lease, memory::Config const config)
    : external_lease_{std::move(lease)}
    , backing_{&external_lease_.get()}
    , root_arena_{backing_->data(), game_memory_detail::checked_capacity(*backing_, config)} {
    assert(!static_cast<bool>(local_backing_));
    assert(external_lease_);
    assert(backing_ != nullptr);
}

GameMemory::~GameMemory() = default;

auto GameMemory::try_acquire_block(std::size_t const size_bytes, std::size_t const alignment)
    -> std::optional<memory::Block> {
    auto block{root_arena_.try_acquire_block(size_bytes, alignment)};
    if (!block) {
        return std::nullopt;
    }
    return memory::Block{std::move(block)};
}

auto GameMemory::acquire_block(std::size_t const size_bytes, std::size_t const alignment)
    -> memory::Block {
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

auto GameMemory::memory_resource() noexcept -> std::pmr::memory_resource& {
    return root_arena_.memory_resource();
}

auto GameMemory::get_stats() const noexcept -> memory::Statistics {
    return root_arena_.get_statistics();
}

auto GameMemory::get_last_allocation_failure() const noexcept
    -> std::optional<memory::AllocationFailure> {
    auto const* const failure{root_arena_.get_last_allocation_failure()};
    return failure != nullptr ? std::optional<memory::AllocationFailure>{*failure} : std::nullopt;
}
} // namespace ioj::sim
