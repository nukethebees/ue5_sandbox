#pragma once

#include "CoreMinimal.h"

struct FTriggerResult {
    bool enable_ticking{false};
};

enum class ETriggerOccurred : uint8 { no, yes };
