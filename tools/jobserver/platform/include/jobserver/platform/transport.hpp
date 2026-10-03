#pragma once

#include "jobserver/error.hpp"

#include <expected>
#include <filesystem>
#include <functional>
#include <string>

namespace jobserver::platform {
using MessageResult = std::expected<std::string, Error>;
using IoResult = std::expected<void, Error>;

struct ServerReply {
    std::string message;
    bool stop{};
};

[[nodiscard]] auto default_endpoint() -> std::filesystem::path const&;

// Serve local same-user clients sequentially until a reply requests shutdown.
// Deliver the final reply before stopping; log per-connection I/O errors and keep serving.
[[nodiscard]] auto serve(std::filesystem::path const& endpoint,
                         std::function<ServerReply(std::string const&)> const& handle_request)
    -> IoResult;
}
