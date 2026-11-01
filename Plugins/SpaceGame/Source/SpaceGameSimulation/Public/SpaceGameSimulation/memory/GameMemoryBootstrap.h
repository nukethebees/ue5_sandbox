#pragma once

#include <CoreMinimal.h>

namespace ioj::memory {
class BumpResource;
}

DECLARE_DELEGATE_RetVal(ioj::memory::BumpResource&, FGameMemoryUpstreamDelegate);

class SPACEGAMESIMULATION_API FGameMemoryBootstrap {
  public:
    static auto upstream_resource_delegate() -> FGameMemoryUpstreamDelegate&;
};
