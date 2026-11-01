#pragma once

#include <ioj/levels/objective_outcome.h>
#include <ioj/sim/missions/objective_id.h>

namespace ioj::sim {
struct ActivationNotification {
    ObjectiveId dependent{};
    ObjectiveOutcome outcome{};
};
}
