#pragma once

#include "ioj/sim/memory/game_memory.h"

#include <CoreMinimal.h>

DECLARE_DELEGATE_RetVal(std::optional<::ioj::sim::GameMemoryBackingLease>,
                        FAcquireGameMemoryBackingDelegate);

class SPACEGAMESIMULATION_API FGameMemoryBootstrap {
  public:
    static auto acquire_backing_delegate() -> FAcquireGameMemoryBackingDelegate&;
    static auto create_game_memory(::ioj::sim::GameMemoryConfig config = {})
        -> TUniquePtr<::ioj::sim::GameMemory>;
};
