#pragma once

#include <native/memory/statistics.h>

#include <memory_resource>

namespace ioj::sim {

class GameMemory final : public std::pmr::memory_resource {
  public:
    explicit GameMemory(std::pmr::memory_resource& upstream);
    ~GameMemory() override;

    GameMemory(GameMemory const&) = delete;
    GameMemory(GameMemory&&) = delete;
    auto operator=(GameMemory const&) -> GameMemory& = delete;
    auto operator=(GameMemory&&) -> GameMemory& = delete;

    auto stats() const noexcept -> memory::Statistics { return stats_; }

  private:
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override;
    void do_deallocate(void* allocation, std::size_t bytes, std::size_t alignment) override;
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override;

    std::pmr::memory_resource& upstream_;
    memory::Statistics stats_;
};
}
