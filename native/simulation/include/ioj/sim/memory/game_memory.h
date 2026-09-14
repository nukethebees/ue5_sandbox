#pragma once

#include "ioj/sim/memory/game_memory_backing.h"
#include "ioj/sim/memory/game_memory_block.h"
#include "ioj/sim/memory/game_memory_config.h"

#include <native/memory/root_arena.h>

#include <memory_resource>
#include <optional>

namespace ioj::sim {

using GameMemoryAllocationFailure = ml::memory::AllocationFailure;
using GameMemoryStats = ml::memory::Statistics;

class GameMemory {
  public:
    explicit GameMemory(GameMemoryConfig config = {});
    GameMemory(GameMemoryBackingLease lease, GameMemoryConfig config);
    ~GameMemory();
    GameMemory(GameMemory const&) = delete;
    GameMemory(GameMemory&&) = delete;
    auto operator=(GameMemory const&) -> GameMemory& = delete;
    auto operator=(GameMemory&&) -> GameMemory& = delete;

    auto try_acquire_block(std::size_t size_bytes, std::size_t alignment)
        -> std::optional<GameMemoryBlock>;
    auto acquire_block(std::size_t size_bytes, std::size_t alignment) -> GameMemoryBlock;
    auto memory_resource() noexcept -> std::pmr::memory_resource&;
    auto get_stats() const noexcept -> GameMemoryStats;
    auto get_last_allocation_failure() const noexcept -> std::optional<GameMemoryAllocationFailure>;
    auto owns_backing_locally() const noexcept -> bool { return static_cast<bool>(local_backing_); }
    auto backing_address() const noexcept -> std::byte* { return backing_->data(); }
  private:
    std::unique_ptr<GameMemoryBacking> local_backing_{};
    GameMemoryBackingLease external_lease_{};
    GameMemoryBacking* backing_{};
    ml::memory::RootArena root_arena_;
};
} // namespace ioj::sim
