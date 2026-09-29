#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

namespace jobserver {
using TicketId = std::uint64_t;
enum class Mode { shared, exclusive };
enum class State { queued, ready, running, done, cancelled };
struct Ticket {
    TicketId id;
    Mode mode;
    std::string name;
    State state{State::queued};
};
struct Error {
    std::string code;
    std::string message;
};
[[nodiscard]] auto path_to_utf8(std::filesystem::path const& value) -> std::string;
}
