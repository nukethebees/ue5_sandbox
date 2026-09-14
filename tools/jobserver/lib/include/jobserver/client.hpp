#pragma once

#include "jobserver/types.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <string>

namespace jobserver {
using OutputCallback = std::function<void(std::string const& stream, std::string const& text)>;

class Lease {
  public:
    Lease() = default;
    Lease(Lease&&) noexcept;
    auto operator=(Lease&&) noexcept -> Lease&;
    Lease(Lease const&) = delete;
    auto operator=(Lease const&) -> Lease& = delete;
    ~Lease();

    [[nodiscard]] auto id() const -> std::string const&;
    auto release() -> std::expected<void, Error>;
  private:
    friend class Client;
    explicit Lease(void* handle, std::string id);
    void* handle_{};
    std::string id_;
};

class Client {
  public:
    [[nodiscard]] static auto acquire(AcquireRequest const& request) -> std::expected<Lease, Error>;
    [[nodiscard]] static auto run(SubmitRequest const& request, OutputCallback output)
        -> std::expected<int, Error>;
    [[nodiscard]] static auto status(bool include_history = false)
        -> std::expected<std::string, Error>;
    [[nodiscard]] static auto ping() -> std::expected<void, Error>;
    [[nodiscard]] static auto cancel(std::string const& id, bool kill)
        -> std::expected<void, Error>;
    [[nodiscard]] static auto shutdown() -> std::expected<void, Error>;
    [[nodiscard]] static auto start_daemon() -> std::expected<void, Error>;
};
}
