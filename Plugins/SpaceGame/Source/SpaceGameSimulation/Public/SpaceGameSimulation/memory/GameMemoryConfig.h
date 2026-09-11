#pragma once

#include "CoreMinimal.h"

struct FGameMemoryConfig {
    inline static constexpr SIZE_T default_root_capacity_bytes{SIZE_T{1} << 30};

    SIZE_T root_capacity_bytes{default_root_capacity_bytes};
};
