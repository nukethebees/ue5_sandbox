#include "TestEntityOwnerId.h"

bool TestEntityOwnerId::is_valid() const {
    return id != ThisClass::NULL_ID;
}
