#pragma once

#include "jobserver/types.hpp"

#include <chrono>
#include <expected>
#include <optional>
#include <string>

namespace jobserver::transport {
[[nodiscard]] auto pipe_name() -> std::wstring const&;

[[nodiscard]] auto read_message(void* handle,
                                std::optional<std::chrono::milliseconds> timeout = std::nullopt)
    -> std::expected<std::string, Error>;
[[nodiscard]] auto write_message(void* handle,
                                 std::string const& message,
                                 std::optional<std::chrono::milliseconds> timeout = std::nullopt)
    -> std::expected<void, Error>;
}
