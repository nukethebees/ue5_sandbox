#include "SGLegacy/combat/ShipDamageResult.h"

bool FShipDamageResult::was_killed() const {
    return result_type == EDamageResult::ActorKilled;
}
