#include "Sandbox/players/ShipDamageResult.h"

bool FShipDamageResult::was_killed() const {
    return result_type == EDamageResult::Killed;
}
