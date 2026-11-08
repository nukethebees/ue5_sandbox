#pragma once
#include <ioj/sim/levels/event_handle.h>
#include <ioj/sim/sim_tick.h>
namespace ioj::sim {
struct TerminalEvent {
    EventHandle event;
    SimTick tick;
};
}
