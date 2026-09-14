#pragma once

#include "jobserver/types.hpp"

#include <expected>
#include <filesystem>
#include <functional>
#include <string>

#include <cstdint>

namespace jobserver {
struct RecoveryAssessment {
    bool responsive{};
    bool authority_valid{};
    bool process_running{};
    bool recoverable{};
    std::uint32_t process_id{};
    std::uint64_t creation_time{};
    std::filesystem::path executable;
    std::string reason;
};

[[nodiscard]] auto publish_authority() -> std::expected<void, Error>;
void clear_authority() noexcept;
[[nodiscard]] auto check_recovery_authority(std::function<bool()> const& is_responsive)
    -> std::expected<RecoveryAssessment, Error>;
[[nodiscard]] auto force_recover_authority(std::function<bool()> const& is_responsive)
    -> std::expected<void, Error>;
}
