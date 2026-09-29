#pragma once
#include "jobserver/types.hpp"

#include <nlohmann/json.hpp>

#include <expected>
#include <vector>

namespace jobserver {
auto ticket_json(Ticket const& ticket) -> nlohmann::json;

class JobsBoard {
  public:
    auto request(Mode mode, std::string name) -> std::expected<Ticket, Error>;
    auto transition(TicketId id, State next) -> std::expected<Ticket, Error>;
    auto check(TicketId id) const -> std::expected<Ticket, Error>;
    auto status() const -> nlohmann::json;
    auto empty() const -> bool { return tickets_.empty(); }
  private:
    void ready();
    TicketId next_id_{1};
    std::vector<Ticket> tickets_;
};
}
