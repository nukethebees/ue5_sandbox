#include "ioj/sim/memory/game_memory.h"

#include <ioj/diagnostics.h>

#include <format>

namespace ioj::sim {

GameMemory::GameMemory(memory::Config const config, std::pmr::memory_resource& upstream)
    : backing_{config.root_capacity_bytes, upstream}
    , root_arena_{backing_.data(), backing_.capacity_bytes()} {}

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
        fatal_error(
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
