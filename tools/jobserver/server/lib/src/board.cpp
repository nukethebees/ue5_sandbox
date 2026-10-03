#include "jobserver/server/board.hpp"

#include <algorithm>
#include <format>

namespace jobserver::server {
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
            {"name", ticket.name},
            {"owner", ticket.owner},
            {"worktree", ticket.worktree}};
}

auto JobsBoard::request(Mode const mode, std::string name, std::string owner, std::string worktree)
    -> TicketResult {
    if (name.empty()) {
        return TicketResult{std::unexpect, "invalid_name", "A descriptive name is required"};
    }
    if (owner.empty() || worktree.empty()) {
        return TicketResult{std::unexpect, "invalid_owner", "An owner and worktree are required"};
    }

    tickets_.push_back(
        {next_id_++, mode, std::move(name), State::queued, std::move(owner), std::move(worktree)});
    ready();
    return TicketResult{std::in_place, tickets_.back()};
}
auto JobsBoard::check(TicketId const id) const -> TicketResult {
    auto const found{std::ranges::find(tickets_, id, &Ticket::id)};
    if (found == tickets_.end()) {
        return TicketResult{
            std::unexpect, "unknown_ticket", std::format("No active ticket #{}", id)};
    }
    return TicketResult{std::in_place, *found};
}
auto JobsBoard::transition(TicketId const id, State const next) -> TicketResult {
    auto const found{std::ranges::find(tickets_, id, &Ticket::id)};
    if (found == tickets_.end()) {
        return TicketResult{
            std::unexpect, "unknown_ticket", std::format("No active ticket #{}", id)};
    }
    auto const allowed{(next == State::running && found->state == State::ready) ||
                       (next == State::done && found->state == State::running) ||
                       (next == State::cancelled &&
                        (found->state == State::queued || found->state == State::ready))};
    if (!allowed) {
        return TicketResult{
            std::unexpect,
            "invalid_transition",
            "Start requires Ready; end requires Running; cancel requires Queued or Ready. Use 'coj "
            "jobs clear ID' for manual board-only removal in any state"};
    }

    found->state = next;
    TicketResult result{std::in_place, *found};
    if (next == State::done || next == State::cancelled) {
        tickets_.erase(found);
        ready();
    }
    return result;
}
auto JobsBoard::clear(TicketId const id) -> TicketResult {
    auto const found{std::ranges::find(tickets_, id, &Ticket::id)};
    if (found == tickets_.end()) {
        return TicketResult{
            std::unexpect, "unknown_ticket", std::format("No active ticket #{}", id)};
    }

    TicketResult result{std::in_place, *found};
    tickets_.erase(found);
    ready();
    return result;
}
auto JobsBoard::clear_owner(std::string const& owner, std::optional<std::string> const& worktree)
    -> nlohmann::json {
    auto removed = nlohmann::json::array();
    std::erase_if(tickets_, [&](Ticket const& ticket) {
        if (ticket.owner != owner || (worktree && ticket.worktree != *worktree)) {
            return false;
        }
        removed.push_back(ticket_json(ticket));
        return true;
    });
    ready();
    return removed;
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
