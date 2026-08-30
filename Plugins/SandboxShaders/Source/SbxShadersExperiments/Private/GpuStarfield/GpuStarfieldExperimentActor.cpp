#include "SbxShadersExperiments/GpuStarfield/GpuStarfieldExperimentActor.h"

AGpuStarfieldExperimentActor::AGpuStarfieldExperimentActor() {
    PrimaryActorTick.bCanEverTick = false;

    starfield_component_ = CreateDefaultSubobject<UGpuStarfieldComponent>(TEXT("GpuStarfield"));
    SetRootComponent(starfield_component_);
}

void AGpuStarfieldExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    apply_settings();
}

void AGpuStarfieldExperimentActor::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    apply_settings();
}

void AGpuStarfieldExperimentActor::apply_settings() {
    if (IsValid(starfield_component_)) {
        starfield_component_->apply_settings(settings);
    }
}
