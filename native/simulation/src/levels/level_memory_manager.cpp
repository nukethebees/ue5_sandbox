#include <ioj/sim/levels/level_memory_manager.h>

namespace ioj::sim {
LevelMemoryManager::LevelMemoryManager(std::pmr::memory_resource& memory,
                                       std::size_t const frame_memory_capacity_bytes)
    : game_memory_{memory}
    , frame_memory_{frame_memory_capacity_bytes, game_memory_} {}
}
