#pragma once

#include <ioj/sim/missions/objective_id.h>

#include <span>

namespace ioj::sim {
struct MissionChanges {
    std::span<ObjectiveId const> activated;
    std::span<ObjectiveId const> completed;
};
}
