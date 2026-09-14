#pragma once

#include "ioj/sim/entity_history.h"
#include "ioj/sim/entity_types.h"

#include <cstdint>
#include <expected>
#include <span>

namespace ioj::sim {
enum class UniqueIdLookupError : std::uint8_t {
    InvalidHandle,
    MissingStaleHandle,
};

[[nodiscard]] constexpr auto is_valid_unique_id(EntityUniqueId const id,
                                                std::int32_t const issued_count) noexcept -> bool {
    return id.id >= 0 && id.id < issued_count;
}

[[nodiscard]] auto find_entity_unique_id(std::span<std::int32_t const> generations,
                                         std::span<EntityUniqueId const> current_unique_ids,
                                         EntityHistoryColumnsConstView history,
                                         RegistryEntityHandle handle) noexcept
    -> std::expected<EntityUniqueId, UniqueIdLookupError>;
} // namespace ioj::sim
