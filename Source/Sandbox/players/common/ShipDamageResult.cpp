#include "Sandbox/players/common/ShipDamageResult.h"

auto FShipDamageResult::was_killed() const {
    return result_type == EDamageResult::Killed;
}
