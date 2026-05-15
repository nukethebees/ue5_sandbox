#include "WarpVolume.h"

#include <Components/BoxComponent.h>
#include <Components/SceneComponent.h>

AWarpVolume::AWarpVolume()
    : box{CreateDefaultSubobject<UBoxComponent>(TEXT("box"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    box->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

auto AWarpVolume::get_box() const -> UBoxComponent* {
    return box;
}
