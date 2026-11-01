#pragma once

#include <ioj/frame_memory_resource.h>
#include <native/memory/block.h>

#include <cstddef>

namespace ioj::sim {
class LevelMemoryManager {
  public:
    LevelMemoryManager(std::pmr::memory_resource& memory, std::size_t frame_memory_capacity_bytes);

    auto get_frame_memory() noexcept -> FrameMemoryResource& { return frame_memory_; }
    auto get_frame_memory() const noexcept -> FrameMemoryResource const& { return frame_memory_; }

  private:
    memory::Block frame_memory_block_;
    FrameMemoryResource frame_memory_;
};
}
