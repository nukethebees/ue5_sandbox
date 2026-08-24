#include "SpaceGame/entities/TestEntityOwnerId.h"

bool TestEntityOwnerId::is_valid() const {
    return id != ThisClass::NULL_ID;
}
