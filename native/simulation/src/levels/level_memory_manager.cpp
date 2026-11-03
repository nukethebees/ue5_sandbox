#include <ioj/sim/levels/level_memory_manager.h>

namespace ioj::sim {
LevelMemoryManager::LevelMemoryManager(std::pmr::memory_resource& memory,
                                       std::size_t const tick_memory_capacity_bytes,
                                       std::size_t const phase_memory_capacity_bytes,
                                       std::size_t const level_memory_capacity_bytes)
    : game_memory_{memory}
    , level_memory_{level_memory_capacity_bytes, game_memory_}
    , tick_memory_{tick_memory_capacity_bytes, game_memory_}
    , phase_memory_{phase_memory_capacity_bytes, game_memory_} {}
}
