#pragma once

#include "native/memory/block.h"
#include "native/memory/statistics.h"

#include <cstddef>
#include <memory>
#include <memory_resource>

namespace ml::memory {
class RootArena {
  public:
    RootArena(std::byte* backing, std::size_t capacity_bytes);
    ~RootArena();
    RootArena(RootArena const&) = delete;
    RootArena(RootArena&&) = delete;
    auto operator=(RootArena const&) -> RootArena& = delete;
    auto operator=(RootArena&&) -> RootArena& = delete;

    auto try_acquire_block(std::size_t size_bytes, std::size_t alignment) -> Block;
    auto memory_resource() noexcept -> std::pmr::memory_resource&;
    auto get_statistics() const noexcept -> Statistics;
    auto get_last_allocation_failure() const noexcept -> AllocationFailure const*;
    auto backing_address() const noexcept -> std::byte*;
  private:
    friend class Block;

    struct Impl;

    void release_block(std::byte* data, std::size_t size_bytes);

    std::unique_ptr<Impl> impl_;
};
}
