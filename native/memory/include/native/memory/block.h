#pragma once

#include <cstddef>
#include <memory_resource>

namespace ioj::memory {
class Block {
  public:
    Block() = default;
    Block(std::pmr::memory_resource& resource, std::size_t size_bytes, std::size_t alignment);
    ~Block();
    Block(Block const&) = delete;
    auto operator=(Block const&) -> Block& = delete;
    Block(Block&& other) noexcept;
    auto operator=(Block&& other) noexcept -> Block&;

    auto data() const noexcept -> std::byte* { return data_; }
    auto size_bytes() const noexcept -> std::size_t { return size_bytes_; }
    auto alignment() const noexcept -> std::size_t { return alignment_; }
    explicit operator bool() const noexcept { return data_ != nullptr; }
  private:
    void reset();

    std::pmr::memory_resource* owner_{};
    std::byte* data_{};
    std::size_t size_bytes_{};
    std::size_t alignment_{};
};
}
