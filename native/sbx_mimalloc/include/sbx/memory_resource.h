#pragma once

#include <memory_resource>

namespace sbx::memory {

// The shared resource remains valid for the lifetime of the module.
[[nodiscard]] auto mimalloc_resource() noexcept -> std::pmr::memory_resource*;

}
