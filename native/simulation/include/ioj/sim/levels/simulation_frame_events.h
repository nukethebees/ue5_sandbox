#pragma once
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_frame_changes.h>
#include <ioj/sim/lasers/laser_hit_details.h>
#include <ioj/sim/levels/frame_event_counts.h>
#include <ioj/sim/player/player_events.h>
#include <ioj/sim/sim_tick.h>

namespace ioj::sim {
// Accumulate fixed-tick occurrences until the next outer advance.
struct SimulationFrameEvents {
    using TickCounts = FrameEventCounts;
    explicit SimulationFrameEvents(std::pmr::memory_resource& memory)
        : ticks{&memory}
        , deaths{memory}
        , laser_impacts{memory}
        , capital_changes{memory}
        , turret_changes{memory}
        , player{&memory} {}

    void clear(SimTick tick) {
        base_tick = tick;
        ticks.clear();
        deaths.reset();
        laser_impacts.reset();
        capital_changes.reset();
        turret_changes.reset();
        player.clear();
    }

    SimTick base_tick{};
    std::pmr::vector<TickCounts> ticks;
    EntityDeathInfo deaths;
    LaserHitDetails laser_impacts;
    EntityFrameChanges capital_changes;
    EntityFrameChanges turret_changes;
    player::PlayerEvents player;
};
}
