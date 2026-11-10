#pragma once
#include <ioj/levels/team.h>
#include <ioj/sim/levels/run_end_reason.h>
#include <ioj/sim/levels/terminal_event.h>

#include <optional>
namespace ioj::sim {
struct LevelResult {
    TerminalEvent terminal;
    std::optional<Team> winner;
    RunEndReason reason;
};
}
