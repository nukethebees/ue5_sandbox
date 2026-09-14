#pragma once

#include "jobserver/types.hpp"

#include <expected>
#include <string>

namespace jobserver::transport {
[[nodiscard]] auto pipe_name() -> std::wstring const&;

[[nodiscard]] auto read_message(void* handle) -> std::expected<std::string, Error>;
[[nodiscard]] auto write_message(void* handle, std::string const& message)
    -> std::expected<void, Error>;
}
