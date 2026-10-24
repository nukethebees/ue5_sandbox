#pragma once

#include <ioj/levels/mission_definition.h>
#include <ioj/sim/missions/compiled_mission.h>
#include <ioj/sim/sim_clock.h>

#include <expected>
#include <span>

namespace ioj::sim {
auto compile_mission(levels::MissionDefinition const& definition,
                     std::span<levels::EntityId const> entities,
                     SimClock const& clock)
    -> std::expected<CompiledMission, std::vector<std::string>>;
}
