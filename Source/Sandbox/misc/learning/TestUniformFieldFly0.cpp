#include "TestUniformFieldFly0.h"

#include <Sandbox/utilities/actor_utils.h>
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "TestUniformField.h"

#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>

ATestUniformFieldFly0::ATestUniformFieldFly0()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATestUniformFieldFly0::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}
void ATestUniformFieldFly0::BeginPlay() {
    Super::BeginPlay();

    // Force a new destination on the first tick
    destination = GetActorLocation();

    if (field == nullptr) {
        WARN_IS_FALSE(LogSandboxLearning, field);
        SetActorTickEnabled(false);
        return;
    }

    if (!ml::actor_is_within(*this, *field, false)) {
        UE_LOG(LogSandboxLearning,
               Error,
               TEXT("%s is not within the vector field"),
               *ml::get_best_display_name(*this));
        SetActorTickEnabled(false);
        return;
    }
}
void ATestUniformFieldFly0::Tick(float dt) {
    Super::Tick(dt);

    if (at_target()) {
        set_new_destination();
    } else {
        move_to_destination();
    }
}
void ATestUniformFieldFly0::EndPlay(EEndPlayReason::Type const reason) {
    Super::EndPlay(reason);
}
void ATestUniformFieldFly0::set_new_destination() {
    // Sample the vector field

    // Use its magnitude and distance to decide where to fly to

    // Do a raycast to the edge of the field so you don't leave it

    // Check if you are outside it

    // Consider adding some randomness

    // Constrain the distance that you can fly
}
void ATestUniformFieldFly0::move_to_destination() {}
bool ATestUniformFieldFly0::at_target() const {
    return FVector::DistSquared(GetActorLocation(), destination) <=
           (acceptance_radius * acceptance_radius);
}
