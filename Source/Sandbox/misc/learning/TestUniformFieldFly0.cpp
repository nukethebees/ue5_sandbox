#include "TestUniformFieldFly0.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "TestUniformField.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

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

    WARN_IF_EXPR_ELSE(field == nullptr) {
        field->on_field_post_construction.AddUObject(this, &ThisClass::on_field_post_construction);
    }
}
void ATestUniformFieldFly0::Tick(float dt) {
    Super::Tick(dt);
}
void ATestUniformFieldFly0::EndPlay(EEndPlayReason::Type const reason) {
    WARN_IF_EXPR_ELSE(field == nullptr) {
        field->on_field_post_construction.RemoveAll(this);
    }

    Super::EndPlay(reason);
}

void ATestUniformFieldFly0::on_field_post_construction(ATestUniformField& field_) {
    // Sample the first point
}
