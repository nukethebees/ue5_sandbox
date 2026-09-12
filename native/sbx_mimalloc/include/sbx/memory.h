#pragma once

#include <cstddef>

namespace sbx::memory {
[[nodiscard]] auto allocate_aligned(std::size_t bytes, std::size_t alignment) noexcept -> void*;
[[nodiscard]] auto reallocate_aligned(void* data, std::size_t bytes, std::size_t alignment) noexcept
    -> void*;
void free(void* data) noexcept;
[[nodiscard]] auto owns(void const* data) noexcept -> bool;
[[nodiscard]] auto version() noexcept -> int;
}
