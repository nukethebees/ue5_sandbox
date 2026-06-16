#include "TestFlyBase.h"

#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/health/ShipHealthComponent.h"
#include "Sandbox/players/ShipDamageResult.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Engine/HitResult.h"
#include "Engine/World.h"

ATestFlyBase::ATestFlyBase()
    : main_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("main_mesh"))}
    , main_collision{CreateDefaultSubobject<UBoxComponent>(TEXT("main_collision"))}
    , health{CreateDefaultSubobject<UShipHealthComponent>(TEXT("health"))} {
    RootComponent = main_collision;

    main_mesh->SetupAttachment(RootComponent);

    main_collision->SetCollisionResponseToChannel(ml::collision::projectile,
                                                  ECollisionResponse::ECR_Block);

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
    if (world) { world->LineTraceSingleByObjectType(hit, start, end, obj_params, params); }

    return hit;
}
void ATestFlyBase::configure_material() {
    material_state.initialise_mesh_material(this, *main_mesh, 0);
}

// IDamageableShip
auto ATestFlyBase::apply_damage(ShipDamageContext context) -> FShipDamageResult {
    health->apply_damage(context.damage);
    if (health->is_dead()) {
        Destroy();
        return {EDamageResult::ActorKilled};
    }

    return {EDamageResult::Damaged};
}
auto ATestFlyBase::get_on_killed_delegate() -> FOnKilled& {
    return on_killed;
}
