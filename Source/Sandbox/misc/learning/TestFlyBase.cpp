#include "TestFlyBase.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

ATestFlyBase::ATestFlyBase()
    : main_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("main_mesh"))}
    , main_collision{CreateDefaultSubobject<UBoxComponent>(TEXT("main_collision"))} {
    RootComponent = main_collision;

    main_mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
}
