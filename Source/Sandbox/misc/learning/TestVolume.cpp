#include "TestVolume.h"

#include <Components/BoxComponent.h>
#include <Components/SceneComponent.h>

ATestVolume::ATestVolume()
    : box{CreateDefaultSubobject<UBoxComponent>(TEXT("box"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    box->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

auto ATestVolume::get_box() const -> UBoxComponent* {
    return box;
}
