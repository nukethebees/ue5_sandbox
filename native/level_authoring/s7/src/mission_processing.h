#pragma once

#include <ioj/levels/authoring/level_inputs.h>
#include <ioj/levels/diagnostics.h>
#include <ioj/levels/mission_definition.h>

namespace ioj::levels::authoring::detail {
auto process_mission(MissionInput input, std::string_view path)
    -> std::expected<MissionDefinition, Diagnostics>;
}
