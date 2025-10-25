#include "Sandbox/players/common/actor_components/ActorDescriptionScannerComponent.h"

#include "Engine/World.h"

#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/ui/hud/widgets/umg/ItemDescriptionHUDWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UActorDescriptionScannerComponent::UActorDescriptionScannerComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UActorDescriptionScannerComponent::BeginPlay() {
    Super::BeginPlay();
}

void UActorDescriptionScannerComponent::perform_raycast(FVector position, FRotator rotation) {
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
    bool has_description{false};

    if (hit) {
        if (hit_actor) {
            if (auto* describable{Cast<IDescribable>(hit_actor)}) {
                description = describable->get_description();
                has_description = !description.IsEmpty();
            }
        } else {
            logger.log_verbose(TEXT("hit_actor is nullptr."));
        }
    }

    // Only update if actor changed
    bool const actor_changed{hit_actor != last_seen_actor.Get()};

    if (!actor_changed) {
        return;
    }

    last_seen_actor = hit_actor;
    on_description_update.ExecuteIfBound(has_description ? description : FText::GetEmpty());
}

void UActorDescriptionScannerComponent::set_hud_widget(UItemDescriptionHUDWidget* widget) {
    hud_widget = widget;
}
