#pragma once

#include <ioj/memory/allocation.h>

#include <cstddef>
#include <memory_resource>

namespace ioj::memory {
// Own one fixed allocation. Individual deallocations do not reclaim space.
class BoundedResource final : public std::pmr::memory_resource {
  public:
    explicit BoundedResource(
        std::size_t capacity_bytes,
        std::pmr::memory_resource& upstream = *std::pmr::get_default_resource());
    ~BoundedResource() override;
    BoundedResource(BoundedResource const&) = delete;
    BoundedResource(BoundedResource&&) = delete;
    auto operator=(BoundedResource const&) -> BoundedResource& = delete;
    auto operator=(BoundedResource&&) -> BoundedResource& = delete;

    [[nodiscard]] auto try_allocate(std::size_t bytes, std::size_t alignment) noexcept -> void*;
    // Release all consumer storage before rewinding this resource.
    void reset() noexcept { offset_ = 0; }
    auto capacity_bytes() const noexcept -> std::size_t { return buffer_.size_bytes; }
    auto used_bytes() const noexcept -> std::size_t { return offset_; }

  private:
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override;
    void do_deallocate(void*, std::size_t, std::size_t) override {}
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override {
        return this == &other;
    }

    std::pmr::memory_resource& upstream_;
    Allocation buffer_;
    std::size_t offset_{};
};
}
