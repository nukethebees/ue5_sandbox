#pragma once

#include "jobserver/types.hpp"

#include <chrono>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>

namespace jobserver::transport {
[[nodiscard]] auto user_sid() -> std::wstring const&;
[[nodiscard]] auto pipe_name() -> std::wstring const&;

[[nodiscard]] auto read_message(void* handle,
                                std::optional<std::chrono::milliseconds> timeout = std::nullopt,
                                std::chrono::milliseconds frame_timeout = std::chrono::seconds{5},
                                std::stop_token stop = {}) -> std::expected<std::string, Error>;
[[nodiscard]] auto write_message(void* handle,
                                 std::string const& message,
                                 std::optional<std::chrono::milliseconds> timeout = std::nullopt,
                                 std::stop_token stop = {}) -> std::expected<void, Error>;
}
