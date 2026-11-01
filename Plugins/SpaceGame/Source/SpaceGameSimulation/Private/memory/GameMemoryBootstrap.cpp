#include "SpaceGameSimulation/memory/GameMemoryBootstrap.h"

auto FGameMemoryBootstrap::upstream_resource_delegate() -> FGameMemoryUpstreamDelegate& {
    static FGameMemoryUpstreamDelegate delegate;
    return delegate;
}
