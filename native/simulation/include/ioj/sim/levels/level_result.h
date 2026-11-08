#pragma once
#include <ioj/levels/team.h>
#include <ioj/sim/levels/terminal_event.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <optional>
namespace ioj::sim {
struct LevelResult {
    TerminalEvent terminal;
    std::optional<Team> winner;
    telemetry::RunEndReason reason;
};
}
