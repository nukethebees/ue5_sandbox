#pragma once

#include <CoreMinimal.h>

#include "sandbox/simulation/memory/GameMemory.h"

DECLARE_DELEGATE_RetVal(std::optional<FGameMemoryBackingLease>, FAcquireGameMemoryBackingDelegate);

class SPACEGAMESIMULATION_API FGameMemoryBootstrap {
  public:
    static auto acquire_backing_delegate() -> FAcquireGameMemoryBackingDelegate&;
    static auto create_game_memory(FGameMemoryConfig config = {}) -> TUniquePtr<FGameMemory>;
};
