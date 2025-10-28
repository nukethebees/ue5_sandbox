#include "Sandbox/players/common/actor_components/ActorDescriptionScannerComponent.h"

#include "Engine/World.h"

#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/ui/hud/widgets/umg/ItemDescriptionHUDWidget.h"
#include "Sandbox/ui/utilities/ui.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UActorDescriptionScannerComponent::UActorDescriptionScannerComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UActorDescriptionScannerComponent::BeginPlay() {
    Super::BeginPlay();
}

void UActorDescriptionScannerComponent::perform_raycast(APlayerController const& pc,
                                                        FVector position,
                                                        FRotator rotation) {
    constexpr auto logger{NestedLogger<"perform_raycast">()};

    TRY_INIT_PTR(world, GetWorld());

    auto const direction{rotation.Vector()};
    auto const start{position};
    auto const end{position + direction * raycast_distance};

    FHitResult hit_result;
    FCollisionQueryParams query_params;
    query_params.AddIgnoredActor(GetOwner());

    bool const hit{
        world->LineTraceSingleByChannel(hit_result, start, end, ECC_Visibility, query_params)};

    AActor* hit_actor{hit ? hit_result.GetActor() : nullptr};

    // Check if hit actor implements IDescribable
    FText description{};

    // Only update if actor changed
    bool const actor_changed{hit_actor != last_seen_actor.Get()};

    if (!actor_changed) {
        return;
    }

    if (hit_actor) {
        if (auto* describable{Cast<IDescribable>(hit_actor)}) {
            description = describable->get_description();
            update_outline(pc, *hit_actor);
        } else {
            on_target_screen_bounds_cleared.ExecuteIfBound();
        }
    } else {
        on_target_screen_bounds_cleared.ExecuteIfBound();
    }

    last_seen_actor = hit_actor;
    on_description_update.ExecuteIfBound(description);
}

void UActorDescriptionScannerComponent::set_hud_widget(UItemDescriptionHUDWidget* widget) {
    hud_widget = widget;
}
void UActorDescriptionScannerComponent::update_outline(APlayerController const& pc,
                                                       AActor const& actor) {
    auto const corners{ml::get_actor_corners(actor)};
    on_target_screen_bounds_update.ExecuteIfBound(corners);
}
