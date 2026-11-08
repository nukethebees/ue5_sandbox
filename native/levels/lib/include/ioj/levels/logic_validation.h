#pragma once
#include <ioj/levels/diagnostics.h>
#include <ioj/levels/entity_spawn_definition.h>
#include <ioj/levels/logic_definition.h>

#include <span>
namespace ioj::levels {
auto validate_logic(LogicDefinition const& logic, std::span<EntitySpawnDefinition const> entities)
    -> Diagnostics;
}
