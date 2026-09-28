#pragma once
#include "jobserver/authority.hpp"
#include "jobserver/types.hpp"

#include <nlohmann/json.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <stop_token>

namespace jobserver {
struct Grant {
    ClientId client;
    CommandId command;
    LeaseId lease;
};
class Session {
  public:
    static auto connect() -> std::expected<std::unique_ptr<Session>, Error>;
    ~Session();
    Session(Session const&) = delete;
    auto operator=(Session const&) -> Session& = delete;
    [[nodiscard]] auto id() const -> ClientId;
    [[nodiscard]] auto lost_event() const -> void*;
    [[nodiscard]] auto failure() const -> Error;
    auto acquire(std::vector<GateClaim> const& gates,
                 nlohmann::json const& metadata,
                 std::stop_token stop = {},
                 std::function<void(nlohmann::json const&)> const& state = {})
        -> std::expected<Grant, Error>;
    auto started(Grant const& grant, std::uint32_t pid) -> std::expected<void, Error>;
    auto release(Grant const& grant, int exit_code) -> std::expected<void, Error>;
  private:
    struct State;
    explicit Session(std::unique_ptr<State> state);
    auto send(nlohmann::json const& message) -> std::expected<void, Error>;
    auto receive() -> std::expected<nlohmann::json, Error>;
    std::unique_ptr<State> state_;
};
class Client {
  public:
    static auto status() -> std::expected<std::string, Error>;
    static auto trace(nlohmann::json filters = nlohmann::json::object())
        -> std::expected<std::string, Error>;
    static auto ping() -> std::expected<void, Error>;
    static auto shutdown() -> std::expected<void, Error>;
    static auto start_daemon() -> std::expected<void, Error>;
    static auto check_daemon_recovery() -> std::expected<RecoveryAssessment, Error>;
    static auto force_recover_daemon() -> std::expected<void, Error>;
};
}
