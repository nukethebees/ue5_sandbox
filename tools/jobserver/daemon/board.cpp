#include "board.hpp"

#include <algorithm>

namespace jobserver {
auto ticket_json(Ticket const& ticket) -> nlohmann::json {
    std::string_view state;
    switch (ticket.state) {
        case State::queued:
            state = "Queued";
            break;
        case State::ready:
            state = "Ready";
            break;
        case State::running:
            state = "Running";
            break;
        case State::done:
            state = "Done";
            break;
        case State::cancelled:
            state = "Cancelled";
            break;
    }
    return {{"id", ticket.id},
            {"mode", ticket.mode == Mode::shared ? "shared" : "exclusive"},
            {"state", state},
            {"name", ticket.name}};
}

auto JobsBoard::request(Mode const mode, std::string name) -> std::expected<Ticket, Error> {
    if (name.empty()) {
        return std::unexpected(Error{"invalid_name", "A descriptive name is required"});
    }
    tickets_.push_back({next_id_++, mode, std::move(name)});
    ready();
    return tickets_.back();
}
auto JobsBoard::check(TicketId const id) const -> std::expected<Ticket, Error> {
    auto const found{std::ranges::find(tickets_, id, &Ticket::id)};
    if (found == tickets_.end()) {
        return std::unexpected(Error{"unknown_ticket", "No active ticket #" + std::to_string(id)});
    }
    return *found;
}
auto JobsBoard::transition(TicketId const id, State const next) -> std::expected<Ticket, Error> {
    auto const found{std::ranges::find(tickets_, id, &Ticket::id)};
    if (found == tickets_.end()) {
        return std::unexpected(Error{"unknown_ticket", "No active ticket #" + std::to_string(id)});
    }
    auto const allowed{(next == State::running && found->state == State::ready) ||
                       (next == State::done && found->state == State::running) ||
                       (next == State::cancelled &&
                        (found->state == State::queued || found->state == State::ready))};
    if (!allowed) {
        return std::unexpected(
            Error{"invalid_transition",
                  "Start requires Ready; end requires Running; cancel requires Queued or Ready"});
    }

    found->state = next;
    auto const result{*found};
    if (next == State::done || next == State::cancelled) {
        tickets_.erase(found);
        ready();
    }
    return result;
}
void JobsBoard::ready() {
    for (auto& ticket : tickets_) {
        if (ticket.mode == Mode::exclusive && &ticket != &tickets_.front()) {
            break;
        }
        if (ticket.state == State::queued) {
            ticket.state = State::ready;
        }
        if (ticket.mode == Mode::exclusive) {
            break;
        }
    }
}
auto JobsBoard::status() const -> nlohmann::json {
    auto tickets = nlohmann::json::array();
    for (auto const& ticket : tickets_) {
        tickets.push_back(ticket_json(ticket));
    }
    return {{"type", "status"}, {"tickets", tickets}};
}
}
