#pragma once

#include "sandbox/simulation/memory/GameMemoryBacking.h"
#include "sandbox/simulation/memory/GameMemoryBlock.h"
#include "sandbox/simulation/memory/GameMemoryConfig.h"

#include <native/memory/root_arena.h>

#include <memory_resource>
#include <optional>

using FGameMemoryAllocationFailure = ml::memory::AllocationFailure;
using FGameMemoryStats = ml::memory::Statistics;

class FGameMemory {
  public:
    explicit FGameMemory(FGameMemoryConfig config = {});
    FGameMemory(FGameMemoryBackingLease lease, FGameMemoryConfig config);
    ~FGameMemory();
    FGameMemory(FGameMemory const&) = delete;
    FGameMemory(FGameMemory&&) = delete;
    auto operator=(FGameMemory const&) -> FGameMemory& = delete;
    auto operator=(FGameMemory&&) -> FGameMemory& = delete;

    auto try_acquire_block(std::size_t size_bytes, std::size_t alignment)
        -> std::optional<FGameMemoryBlock>;
    auto acquire_block(std::size_t size_bytes, std::size_t alignment) -> FGameMemoryBlock;
    auto memory_resource() noexcept -> std::pmr::memory_resource&;
    auto get_stats() const noexcept -> FGameMemoryStats;
    auto get_last_allocation_failure() const noexcept
        -> std::optional<FGameMemoryAllocationFailure>;
    auto owns_backing_locally() const noexcept -> bool { return static_cast<bool>(local_backing_); }
    auto backing_address() const noexcept -> std::byte* { return backing_->data(); }
  private:
    std::unique_ptr<FGameMemoryBacking> local_backing_{};
    FGameMemoryBackingLease external_lease_{};
    FGameMemoryBacking* backing_{};
    ml::memory::RootArena root_arena_;
};
