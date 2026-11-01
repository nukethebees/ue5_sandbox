#pragma once

#include <native/memory/block.h>

#include <cstddef>
#include <memory_resource>

namespace ioj::memory {

class BumpResource final : public std::pmr::memory_resource {
  public:
    static constexpr std::size_t default_capacity_bytes{std::size_t{1} << 30};
    static constexpr std::size_t backing_alignment{64};

    explicit BumpResource(std::size_t capacity_bytes = default_capacity_bytes);
    BumpResource(std::size_t capacity_bytes, std::pmr::memory_resource& upstream);

    BumpResource(BumpResource const&) = delete;
    BumpResource(BumpResource&&) = delete;
    auto operator=(BumpResource const&) -> BumpResource& = delete;
    auto operator=(BumpResource&&) -> BumpResource& = delete;

    // Destroy all consumers before rewinding the buffer or destroying this resource.
    void reset() noexcept { offset_ = 0; }
    auto capacity_bytes() const noexcept -> std::size_t { return buffer_.size_bytes(); }
    auto used_bytes() const noexcept -> std::size_t { return offset_; }

  private:
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override;
    void do_deallocate(void* allocation, std::size_t bytes, std::size_t alignment) override;
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override;

    std::pmr::memory_resource& upstream_;
    Block buffer_;
    std::size_t offset_{};
};
}
