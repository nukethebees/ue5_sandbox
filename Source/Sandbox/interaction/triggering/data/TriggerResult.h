#pragma once

#include "CoreMinimal.h"

struct FTriggerResult {
    FTriggerResult() = default;
    FTriggerResult(bool enable_ticking)
        : enable_ticking(enable_ticking) {}

    bool enable_ticking{false};
};

enum class ETriggerOccurred : uint8 { no, yes };
