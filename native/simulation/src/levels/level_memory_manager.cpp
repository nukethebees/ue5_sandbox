#include <ioj/sim/levels/level_memory_manager.h>

#include <ioj/sim/memory/game_memory.h>

#include <span>

namespace ioj::sim {
LevelMemoryManager::LevelMemoryManager(GameMemory& game_memory,
                                       std::size_t const frame_memory_capacity_bytes)
    : frame_memory_block_{game_memory.acquire_block(frame_memory_capacity_bytes,
                                                    FrameMemoryResource::backing_alignment)}
    , frame_memory_{
          std::span<std::byte>{frame_memory_block_.data(), frame_memory_block_.size_bytes()}} {}
}
