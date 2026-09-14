#pragma once

#include "jobserver/types.hpp"

#include <expected>
#include <functional>

namespace jobserver {
[[nodiscard]] auto publish_authority() -> std::expected<void, Error>;
void clear_authority() noexcept;
[[nodiscard]] auto force_recover_authority(std::function<bool()> const& is_responsive)
    -> std::expected<void, Error>;
}
