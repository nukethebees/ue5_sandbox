#pragma once
#include <ioj/levels/mission_definition.h>

#include <span>

namespace ioj::levels {
// Validate structure independently of simulation compilation. Entity IDs must be declared.
auto validate_mission(MissionDefinition const& mission, std::span<EntityId const> entities)
    -> std::vector<std::string>;
}
