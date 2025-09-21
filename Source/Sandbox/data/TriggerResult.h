#pragma once

#include "CoreMinimal.h"

struct FTriggerResult {
    bool enable_ticking{false};
    // Extensible for future features: bool success, FString debug_info, etc.
};