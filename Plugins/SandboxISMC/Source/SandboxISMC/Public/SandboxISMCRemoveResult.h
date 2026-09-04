#pragma once

#include "CoreTypes.h"

struct SANDBOXISMC_API FSandboxISMCRemoveResult {
    bool removed{false};
    int32 moved_from_index{INDEX_NONE};
};
