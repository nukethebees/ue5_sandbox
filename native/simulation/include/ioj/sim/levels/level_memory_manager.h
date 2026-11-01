#pragma once

#include <ioj/frame_memory_resource.h>
#include <ioj/sim/memory/game_memory.h>

#include <cstddef>

namespace ioj::sim {
class LevelMemoryManager {
  public:
    LevelMemoryManager(std::pmr::memory_resource& memory, std::size_t frame_memory_capacity_bytes);

    auto game_memory() noexcept -> GameMemory& { return game_memory_; }
    auto game_memory() const noexcept -> GameMemory const& { return game_memory_; }
    auto frame_memory() noexcept -> FrameMemoryResource& { return frame_memory_; }
    auto frame_memory() const noexcept -> FrameMemoryResource const& { return frame_memory_; }

  private:
    GameMemory game_memory_;
    FrameMemoryResource frame_memory_;
};
}
