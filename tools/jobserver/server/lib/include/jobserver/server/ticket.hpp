#pragma once

#include "jobserver/ticket_id.hpp"

#include <string>

namespace jobserver::server {
enum class Mode { shared, exclusive };
enum class State { queued, ready, running, done, cancelled };

struct Ticket {
    TicketId id;
    Mode mode;
    std::string name;
    State state{State::queued};
};
}
