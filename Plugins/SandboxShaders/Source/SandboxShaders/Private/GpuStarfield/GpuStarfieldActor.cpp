#include "SandboxShaders/GpuStarfield/GpuStarfieldActor.h"

#include <ioj/levels/authoring/starfield_reader.h>

#include <Misc/Paths.h>

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
    auto const root{FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"))};
    auto const path{
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), settings_file.FilePath)};
    auto const settings{ioj::levels::authoring::read_starfield_settings_file(
        std::filesystem::path{*path}, std::filesystem::path{*root})};
    if (!settings) {
        auto const error{ioj::levels::format_diagnostics(settings.error())};
        UE_LOG(LogGpuStarfieldSettings, Error, TEXT("%s"), UTF8_TO_TCHAR(error.c_str()));
        return;
    }

    check(starfield_component_);
    settings_ = *settings;
    starfield_component_->apply_settings(settings_, colours);
}
