#pragma once
#include "gate.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <stop_token>
#include <thread>

namespace jobserver {
class Server {
  public:
    explicit Server(std::filesystem::path const& directory);
    [[nodiscard]] auto run() -> int;
  private:
    struct Connection {
        void* pipe{};
        ClientId id{};
        LeaseId announced{};
        bool granted{};
        bool closed{};
        bool shutdown{};
        std::deque<nlohmann::json> replies{};
        std::stop_source stop{};
    };
    void serve_client(void* pipe, bool control);
    void respond(Connection& connection);
    void request(Connection& connection, nlohmann::json const& message, bool control);
    Journal journal_;
    GateQueue queue_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::atomic<bool> stopping_{};
    bool draining_{};
    std::stop_source stop_;
    std::size_t handlers_{};
    std::size_t control_handlers_{};
};
}
