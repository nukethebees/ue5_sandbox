#pragma once
#include "jobserver/types.hpp"

#include <expected>

namespace jobserver {
// Human diagnostics and daemon lifecycle only. No admission API.
class Client {
  public:
    static auto status() -> std::expected<std::string, Error>;
    static auto trace() -> std::expected<std::string, Error>;
    static auto ping() -> std::expected<void, Error>;
    static auto shutdown() -> std::expected<void, Error>;
    static auto start_daemon() -> std::expected<void, Error>;
};
}
