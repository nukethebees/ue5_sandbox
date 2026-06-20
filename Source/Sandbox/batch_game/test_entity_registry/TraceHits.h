#pragma once

#include <Containers/AllowShrinking.h>
#include <Containers/Array.h>
#include <HAL/Platform.h>

class AActor;
class UActorComponent;

struct TraceHits {
    TArray<AActor*> actors;
    TArray<UActorComponent*> actor_components;
    TArray<int32> hit_items;

    void reset();
    auto num() const -> int32;
    void
        remove_at_swap(int32 const index, int32 const count, EAllowShrinking const allow_shrinking);

    void validate_array_sizes() const;
};
