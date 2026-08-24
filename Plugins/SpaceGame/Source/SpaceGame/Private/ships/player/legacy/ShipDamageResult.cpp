#include "SpaceGame/ships/player/legacy/ShipDamageResult.h"

bool FShipDamageResult::was_killed() const {
    return result_type == EDamageResult::ActorKilled;
}
