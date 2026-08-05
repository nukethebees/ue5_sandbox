#include "Sandbox/environment/effects/ShipPostProcessing.h"

#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"

AShipPostProcessing::AShipPostProcessing()
    : post_process_volume{CreateDefaultSubobject<UPostProcessComponent>(TEXT("pp"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    post_process_volume->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
}

void AShipPostProcessing::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (post_process_volume) {
        set_up_post_processing(*post_process_volume);
    }
}
void AShipPostProcessing::set_up_post_processing(UPostProcessComponent& pp) {
    pp.bUnbound = true;
    auto& s{pp.Settings};

    // Global intensity
    s.bOverride_BloomIntensity = true;
    s.BloomIntensity = 1.f;

    s.bOverride_BloomMethod = true;
    s.BloomMethod = EBloomMethod::BM_SOG;

    s.bOverride_BloomGaussianIntensity = true;
    s.BloomGaussianIntensity = 2.f;

    s.bOverride_BloomThreshold = true;
    s.BloomThreshold = 5.f;
}
