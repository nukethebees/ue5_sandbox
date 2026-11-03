#pragma once

#include <ioj/frame_memory_resource.h>
#include <ioj/memory/bump_resource.h>
#include <ioj/sim/memory/game_memory.h>

#include <cstddef>

namespace ioj::sim {
class LevelMemoryManager {
  public:
    static constexpr std::size_t default_level_memory_capacity_bytes{1024 * 1024};

    LevelMemoryManager(
        std::pmr::memory_resource& memory,
        std::size_t tick_memory_capacity_bytes,
        std::size_t phase_memory_capacity_bytes,
        std::size_t level_memory_capacity_bytes = default_level_memory_capacity_bytes);

    auto game_memory() noexcept -> GameMemory& { return game_memory_; }
    auto game_memory() const noexcept -> GameMemory const& { return game_memory_; }
    auto level_memory() noexcept -> memory::BumpResource& { return level_memory_; }
    auto tick_memory() noexcept -> FrameMemoryResource& { return tick_memory_; }
    auto tick_memory() const noexcept -> FrameMemoryResource const& { return tick_memory_; }
    auto phase_memory() noexcept -> FrameMemoryResource& { return phase_memory_; }
    auto phase_memory() const noexcept -> FrameMemoryResource const& { return phase_memory_; }

  private:
    GameMemory game_memory_;
    memory::BumpResource level_memory_;
    FrameMemoryResource tick_memory_;
    FrameMemoryResource phase_memory_;
};
}
