#include "TestFlyBase.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Engine/HitResult.h"
#include "Engine/World.h"

ATestFlyBase::ATestFlyBase()
    : main_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("main_mesh"))}
    , main_collision{CreateDefaultSubobject<UBoxComponent>(TEXT("main_collision"))} {
    RootComponent = main_collision;

    main_mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
}

void ATestFlyBase::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    configure_material();
}
void ATestFlyBase::BeginPlay() {
    Super::BeginPlay();

    configure_material();
}

auto ATestFlyBase::check_if_hit(FVector start, FVector end) -> FHitResult {
    FHitResult hit{};
    FCollisionQueryParams params{};
    FCollisionObjectQueryParams obj_params{};
    params.AddIgnoredActor(this);

    auto* world{GetWorld()};
    if (world) {
        world->LineTraceSingleByObjectType(hit, start, end, obj_params, params);
    }

    return hit;
}
void ATestFlyBase::configure_material() {
    material_state.initialise_mesh_material(this, *main_mesh, 0);
}
