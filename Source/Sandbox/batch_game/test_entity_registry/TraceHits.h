#pragma once

#include <Containers/AllowShrinking.h>
#include <Containers/Array.h>
#include <HAL/Platform.h>

#include <SandboxCore/soa_array_mixin.h>

class AActor;
class UActorComponent;

struct TraceHits
    : public ml::FSoAViewMixin
    , ml::FSoAArrayMixin {
    TArray<AActor*> actors;
    TArray<UActorComponent*> actor_components;
    TArray<int32> hit_items;

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.actors, self.actor_components, self.hit_items);
    }
};
