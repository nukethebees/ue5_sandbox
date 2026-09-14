#include "SpaceGameSimulation/memory/GameMemoryBootstrap.h"

auto FGameMemoryBootstrap::acquire_backing_delegate() -> FAcquireGameMemoryBackingDelegate& {
    static FAcquireGameMemoryBackingDelegate delegate;
    return delegate;
}

auto FGameMemoryBootstrap::create_game_memory(::ioj::sim::GameMemoryConfig config)
    -> TUniquePtr<::ioj::sim::GameMemory> {
    auto& delegate{acquire_backing_delegate()};
    if (delegate.IsBound()) {
        auto lease{delegate.Execute()};
        if (lease.has_value() && lease->get().capacity_bytes() >= config.root_capacity_bytes) {
            return MakeUnique<::ioj::sim::GameMemory>(MoveTemp(lease.value()), config);
        }
    }

    return MakeUnique<::ioj::sim::GameMemory>(config);
}
