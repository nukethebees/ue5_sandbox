#include "SandboxShaders/GpuStarfield/GpuStarfieldActor.h"

#include "NativeSettings/settings_reader.h"
#include <ioj/gfx/starfield_settings_reader.h>

DEFINE_LOG_CATEGORY_STATIC(LogGpuStarfieldSettings, Log, All);

AGpuStarfieldActor::AGpuStarfieldActor() {
    PrimaryActorTick.bCanEverTick = false;

    starfield_component_ = CreateDefaultSubobject<UGpuStarfieldComponent>(TEXT("GpuStarfield"));
    SetRootComponent(starfield_component_);
}

void AGpuStarfieldActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    apply_settings();
}

void AGpuStarfieldActor::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    apply_settings();
}

void AGpuStarfieldActor::apply_settings() {
    auto const assignments{ioj::settings::read_file(settings_file.FilePath)};
    if (!assignments) {
        UE_LOG(LogGpuStarfieldSettings, Error, TEXT("%s"), *assignments.error());
        return;
    }

    auto const settings{ioj::gfx::read_starfield_settings(*assignments)};
    if (!settings) {
        UE_LOG(LogGpuStarfieldSettings, Error, TEXT("%s"), UTF8_TO_TCHAR(settings.error().c_str()));
        return;
    }

    check(starfield_component_);
    settings_ = *settings;
    starfield_component_->apply_settings(settings_, colours);
}
