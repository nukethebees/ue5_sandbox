#include "WarpComponent.h"

UWarpComponent::UWarpComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UWarpComponent::warp_to_location(AActor* parent, FVector const& target_location) {
    if (!parent) {
        return;
    }

    auto const current_location{parent->GetActorLocation()};
    auto offset{target_location - current_location};

    if (offset.Size() > max_warp_distance) {
        return;
    }

    auto const final_location{current_location + offset};
    parent->SetActorLocation(final_location);
}
