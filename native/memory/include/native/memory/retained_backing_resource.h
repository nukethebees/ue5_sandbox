#pragma once

#include <native/memory/backing.h>

namespace ioj::memory {

// Retain one allocation between borrowers. Serialize access and outlive every borrower.
class RetainedBackingResource final : public std::pmr::memory_resource {
  public:
    explicit RetainedBackingResource(std::size_t capacity_bytes);
    ~RetainedBackingResource() override;

    auto can_allocate(std::size_t bytes, std::size_t alignment) const noexcept -> bool;
    auto is_in_use() const noexcept -> bool { return active_bytes_ != 0; }
    auto data() const noexcept -> std::byte* { return backing_.data(); }
  private:
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override;
    void do_deallocate(void* allocation, std::size_t bytes, std::size_t alignment) override;
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override;

    Backing backing_;
    std::size_t active_bytes_{};
    std::size_t active_alignment_{};
};
}
