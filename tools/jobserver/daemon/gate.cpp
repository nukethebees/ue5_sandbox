#include "gate.hpp"

#include <algorithm>

namespace jobserver {
GateQueue::GateQueue(Journal& journal)
    : journal_{journal} {}
auto GateQueue::connect() -> ClientId {
    auto const id{ClientId{next_id_++}};
    clients_.push_back(id);
    journal_.append("connected", id);
    return id;
}
void GateQueue::disconnect(ClientId const client) {
    static_cast<void>(release(client));
    std::erase(clients_, client);
    journal_.append("disconnected", client);
}
auto GateQueue::request(ClientId const client, Mode const mode, std::string name)
    -> std::expected<void, Error> {
    if (std::ranges::find(clients_, client) == clients_.end()) {
        return std::unexpected(Error{"unknown_client", "No scheduling connection"});
    }
    if (find(client)) {
        return std::unexpected(Error{"ticket_exists", "This connection already owns a ticket"});
    }
    if (name.empty() || name.size() > 1024) {
        return std::unexpected(Error{"invalid_name", "Ticket name must contain 1 to 1024 bytes"});
    }

    tickets_.push_back({client, mode, std::move(name)});
    journal_.append("requested", client, tickets_.back().name);
    admit();
    return {};
}
auto GateQueue::release(ClientId const client) -> bool {
    auto const found{std::ranges::find(tickets_, client, &Ticket::client)};
    if (found == tickets_.end()) {
        return false;
    }
    journal_.append("released", client, found->name);
    tickets_.erase(found);
    admit();
    return true;
}
auto GateQueue::find(ClientId const client) const -> Ticket const* {
    auto const found{std::ranges::find(tickets_, client, &Ticket::client)};
    return found == tickets_.end() ? nullptr : &*found;
}
void GateQueue::admit() {
    bool earlier_shared{};
    for (auto& ticket : tickets_) {
        if (ticket.mode == Mode::exclusive && earlier_shared) {
            break;
        }
        if (!ticket.granted) {
            ticket.granted = true;
            journal_.append("granted", ticket.client, ticket.name);
        }
        if (ticket.mode == Mode::exclusive) {
            break;
        }
        earlier_shared = true;
    }
}
auto GateQueue::status() const -> nlohmann::json {
    auto tickets = nlohmann::json::array();
    for (auto const& ticket : tickets_) {
        tickets.push_back({{"client", ticket.client.value},
                           {"name", ticket.name},
                           {"mode", ticket.mode == Mode::shared ? "shared" : "exclusive"},
                           {"state", ticket.granted ? "granted" : "queued"}});
    }
    return {{"type", "status"}, {"tickets", tickets}, {"clients", clients_.size()}};
}
}
