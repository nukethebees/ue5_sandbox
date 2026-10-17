#pragma once

#include <cstddef>
#include <memory_resource>

namespace ioj::memory {

class Backing {
  public:
    static constexpr std::size_t alignment{64};

    // Keep upstream alive until this allocation is destroyed.
    explicit Backing(std::size_t capacity_bytes,
                     std::pmr::memory_resource& upstream = *std::pmr::new_delete_resource());

    ~Backing();
    Backing(Backing const&) = delete;
    Backing(Backing&&) = delete;
    auto operator=(Backing const&) -> Backing& = delete;
    auto operator=(Backing&&) -> Backing& = delete;

    [[nodiscard]] auto data() const noexcept -> std::byte* { return data_; }
    [[nodiscard]] auto capacity_bytes() const noexcept -> std::size_t { return capacity_bytes_; }
  private:
    std::pmr::memory_resource& upstream_;
    std::byte* data_{};
    std::size_t capacity_bytes_{};
};
}
