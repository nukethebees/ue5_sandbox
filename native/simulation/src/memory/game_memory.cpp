#include <ioj/sim/memory/game_memory.h>

#include <algorithm>
#include <cassert>

namespace ioj::sim {

GameMemory::GameMemory(std::pmr::memory_resource& upstream)
    : upstream_{upstream} {}
GameMemory::~GameMemory() {
    assert(stats_.live_allocation_count == 0);
}
auto GameMemory::stats() const noexcept -> memory::Statistics {
    std::scoped_lock lock{mutex_};
    return stats_;
}
auto GameMemory::do_allocate(std::size_t const bytes, std::size_t const alignment) -> void* {
    std::scoped_lock lock{mutex_};
    auto* const allocation{upstream_.allocate(bytes, alignment)};
    stats_.live_bytes += bytes;
    ++stats_.live_allocation_count;
    stats_.peak_live_bytes = std::max(stats_.peak_live_bytes, stats_.live_bytes);
    stats_.peak_live_allocation_count =
        std::max(stats_.peak_live_allocation_count, stats_.live_allocation_count);
    return allocation;
}
void GameMemory::do_deallocate(void* const allocation,
                               std::size_t const bytes,
                               std::size_t const alignment) {
    std::scoped_lock lock{mutex_};
    upstream_.deallocate(allocation, bytes, alignment);
    stats_.live_bytes -= bytes;
    --stats_.live_allocation_count;
}
auto GameMemory::do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool {
    return this == &other;
}
}
