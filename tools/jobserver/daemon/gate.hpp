#pragma once
#include "journal.hpp"

#include "jobserver/types.hpp"

#include <expected>
#include <vector>

namespace jobserver {
struct Ticket {
    ClientId client;
    Mode mode;
    std::string name;
    bool granted{};
};

// The server serializes all queue operations under its mutex.
class GateQueue {
  public:
    explicit GateQueue(Journal& journal);
    auto connect() -> ClientId;
    void disconnect(ClientId client);
    auto request(ClientId client, Mode mode, std::string name) -> std::expected<void, Error>;
    auto release(ClientId client) -> bool;
    [[nodiscard]] auto find(ClientId client) const -> Ticket const*;
    [[nodiscard]] auto status() const -> nlohmann::json;
    [[nodiscard]] auto empty() const -> bool { return tickets_.empty(); }
  private:
    void admit();
    Journal& journal_;
    std::uint64_t next_id_{1};
    std::vector<ClientId> clients_;
    std::vector<Ticket> tickets_;
};
}
