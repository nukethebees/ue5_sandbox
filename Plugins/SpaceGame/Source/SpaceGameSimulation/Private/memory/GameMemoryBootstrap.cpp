#include "SpaceGameSimulation/memory/GameMemoryBootstrap.h"

auto FGameMemoryBootstrap::acquire_backing_delegate() -> FAcquireGameMemoryBackingDelegate& {
    static FAcquireGameMemoryBackingDelegate delegate;
    return delegate;
}

auto FGameMemoryBootstrap::create_game_memory(FGameMemoryConfig config) -> TUniquePtr<FGameMemory> {
    auto& delegate{acquire_backing_delegate()};
    if (delegate.IsBound()) {
        auto lease{delegate.Execute()};
        if (lease.IsSet() && lease->get().capacity_bytes() >= config.root_capacity_bytes) {
            return MakeUnique<FGameMemory>(MoveTemp(lease.GetValue()), config);
        }
    }

    return MakeUnique<FGameMemory>(config);
}
