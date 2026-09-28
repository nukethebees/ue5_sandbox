#pragma once

#include "journal.hpp"

#include "jobserver/types.hpp"

#include <expected>
#include <functional>
#include <map>

namespace jobserver {
enum class LeaseMode { shared, exclusive };
struct GateClaim {
    std::string name;
    LeaseMode mode{LeaseMode::shared};
};
struct Admission {
    ClientId client;
    CommandId command;
    LeaseId lease;
    std::vector<GateClaim> claims;
    nlohmann::json metadata;
    bool granted{};
    bool started{};
    std::vector<LeaseId> blockers{};
};

// All operations run under the server's state mutex. No process state belongs here.
class GateQueue {
  public:
    explicit GateQueue(Journal& journal);
    auto connect() -> ClientId;
    void disconnect(ClientId client);
    auto acquire(ClientId client, std::vector<GateClaim> claims, nlohmann::json metadata)
        -> std::expected<Admission, Error>;
    auto cancel(ClientId client, LeaseId lease) -> bool;
    auto release(ClientId client, LeaseId lease, int exit_code) -> bool;
    auto started(ClientId client, LeaseId lease, std::uint32_t pid) -> bool;
    [[nodiscard]] auto find(ClientId client) const -> Admission const*;
    [[nodiscard]] auto status() const -> nlohmann::json;
    [[nodiscard]] auto empty() const -> bool { return entries_.empty(); }
  private:
    void admit();
    void record(EventKind kind, Admission const& entry, std::int64_t value = 0);
    void exclusive_event(EventKind kind, Admission const& entry);
    void remove(ClientId client, LeaseId lease, bool cancelled);
    [[nodiscard]] auto conflicts(Admission const& left, Admission const& right) const -> bool;
    Journal& journal_;
    std::uint64_t next_id_;
    std::vector<ClientId> clients_;
    std::vector<Admission> entries_;
    std::map<std::string, GateId> gates_;
};
}
