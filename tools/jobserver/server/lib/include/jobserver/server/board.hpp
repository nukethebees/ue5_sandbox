#pragma once
#include "jobserver/error.hpp"
#include "jobserver/server/ticket.hpp"

#include <nlohmann/json.hpp>

#include <expected>
#include <string>
#include <vector>

namespace jobserver::server {
using TicketResult = std::expected<Ticket, Error>;

auto ticket_json(Ticket const& ticket) -> nlohmann::json;

class JobsBoard {
  public:
    auto request(Mode mode, std::string name) -> TicketResult;
    auto transition(TicketId id, State next) -> TicketResult;
    auto check(TicketId id) const -> TicketResult;
    auto status() const -> nlohmann::json;
    auto empty() const -> bool { return tickets_.empty(); }
  private:
    void ready();
    TicketId next_id_{1};
    std::vector<Ticket> tickets_;
};
}
