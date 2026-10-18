#include "ShooterGame/players/blackboard_utils.hpp"

namespace ioj {
bool is_valid_key(UBlackboardComponent& bb, FName const& name) {
    return bb.GetKeyID(name) != FBlackboard::InvalidKey;
}
}
