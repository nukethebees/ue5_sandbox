#pragma once

#include <ioj/levels/mission_types.h>
#include <ioj/sim/missions/objective_id.h>

namespace ioj::sim {
struct ActivationNotification {
    ObjectiveId dependent{};
    ObjectiveOutcome outcome{};
};
}
