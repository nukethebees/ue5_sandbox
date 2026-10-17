#pragma once

#include "ioj/sim/memory/game_memory.h"

#include <CoreMinimal.h>

DECLARE_DELEGATE_RetVal_OneParam(std::pmr::memory_resource*,
                                 FGameMemoryUpstreamDelegate,
                                 std::size_t);

class SPACEGAMESIMULATION_API FGameMemoryBootstrap {
  public:
    static auto upstream_resource_delegate() -> FGameMemoryUpstreamDelegate&;
    static auto create_game_memory(ioj::memory::Config config = {})
        -> TUniquePtr<::ioj::sim::GameMemory>;
};
