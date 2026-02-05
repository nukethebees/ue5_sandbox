#include "Sandbox/environment/interactive/ChangeLevelActor.h"

#include "Kismet/GameplayStatics.h"

#include "Components/BoxComponent.h"

AChangeLevelActor::AChangeLevelActor()
    : collision_box{CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"))} {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));

    collision_box->SetupAttachment(RootComponent);
}

void AChangeLevelActor::BeginPlay() {
    Super::BeginPlay();

    collision_box->OnComponentBeginOverlap.AddDynamic(this, &AChangeLevelActor::on_overlap_begin);
}

void AChangeLevelActor::on_overlap_begin(UPrimitiveComponent* overlapped_component,
                                         AActor* other_actor,
                                         UPrimitiveComponent* other_component,
                                         int32 other_body_index,
                                         bool from_sweep,
                                         FHitResult const& sweep_result) {

    UGameplayStatics::OpenLevel(this, FName(*level_to_load.GetAssetName()));
}
