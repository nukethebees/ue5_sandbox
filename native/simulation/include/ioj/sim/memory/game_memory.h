#pragma once

#include "native/memory/backing.h"
#include "native/memory/block.h"
#include "native/memory/config.h"
#include <native/memory/root_arena.h>

#include <memory_resource>
#include <optional>

namespace ioj::sim {

class GameMemory {
  public:
    explicit GameMemory(memory::Config config = {});
    GameMemory(memory::BackingLease lease, memory::Config config);
    ~GameMemory();
    GameMemory(GameMemory const&) = delete;
    GameMemory(GameMemory&&) = delete;
    auto operator=(GameMemory const&) -> GameMemory& = delete;
    auto operator=(GameMemory&&) -> GameMemory& = delete;

    auto try_acquire_block(std::size_t size_bytes, std::size_t alignment)
        -> std::optional<memory::Block>;
    auto acquire_block(std::size_t size_bytes, std::size_t alignment) -> memory::Block;
    auto memory_resource() noexcept -> std::pmr::memory_resource&;
    auto get_stats() const noexcept -> memory::Statistics;
    auto get_last_allocation_failure() const noexcept -> std::optional<memory::AllocationFailure>;
    auto owns_backing_locally() const noexcept -> bool { return static_cast<bool>(local_backing_); }
    auto backing_address() const noexcept -> std::byte* { return backing_->data(); }
  private:
    std::unique_ptr<memory::Backing> local_backing_{};
    memory::BackingLease external_lease_{};
    memory::Backing* backing_{};
    memory::RootArena root_arena_;
};
} // namespace ioj::sim
