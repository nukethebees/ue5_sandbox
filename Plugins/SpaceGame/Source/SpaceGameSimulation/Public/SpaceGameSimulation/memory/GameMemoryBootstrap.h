#pragma once

#include "SpaceGameSimulation/memory/GameMemory.h"

DECLARE_DELEGATE_RetVal(TOptional<FGameMemoryBackingLease>, FAcquireGameMemoryBackingDelegate);

class SPACEGAMESIMULATION_API FGameMemoryBootstrap {
  public:
    static auto acquire_backing_delegate() -> FAcquireGameMemoryBackingDelegate&;
    static auto create_game_memory(FGameMemoryConfig config = {}) -> TUniquePtr<FGameMemory>;
};
