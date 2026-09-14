#pragma once

#include "ioj/sim/entity_registry_query.h"
#include "ioj/sim/vectors3f.h"

#include <cstdint>
#include <span>

namespace ioj::sim {
// Returns the first invalid input offset, or -1 when every handle was refreshable.
[[nodiscard]] auto refresh_registry_handles(EntityRegistryQueryView registry,
                                            std::span<RegistryEntityHandle> handles) noexcept
    -> std::int32_t;
} // namespace ioj::sim
