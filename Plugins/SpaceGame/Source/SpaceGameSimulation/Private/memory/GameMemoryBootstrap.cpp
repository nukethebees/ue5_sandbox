#include "SpaceGameSimulation/memory/GameMemoryBootstrap.h"

auto FGameMemoryBootstrap::upstream_resource_delegate() -> FGameMemoryUpstreamDelegate& {
    static FGameMemoryUpstreamDelegate delegate;
    return delegate;
}

auto FGameMemoryBootstrap::create_game_memory(ioj::memory::Config config)
    -> TUniquePtr<::ioj::sim::GameMemory> {
    auto& delegate{upstream_resource_delegate()};
    if (delegate.IsBound()) {
        if (auto* const upstream{delegate.Execute(config.root_capacity_bytes)}) {
            return MakeUnique<::ioj::sim::GameMemory>(config, *upstream);
        }
    }

    return MakeUnique<::ioj::sim::GameMemory>(config);
}
