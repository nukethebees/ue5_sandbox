#include "SandboxShaders/GpuStarfield/GpuStarfieldActor.h"

#if WITH_EDITOR
#include <ioj/levels/authoring/definition_reader.h>

#include <Engine/World.h>
#include <Misc/Paths.h>

DEFINE_LOG_CATEGORY_STATIC(LogGpuStarfieldSettings, Log, All);
#endif

namespace ioj {
AGpuStarfieldActor::AGpuStarfieldActor() {
    PrimaryActorTick.bCanEverTick = false;

    starfield_component_ = CreateDefaultSubobject<UGpuStarfieldComponent>(TEXT("GpuStarfield"));
    SetRootComponent(starfield_component_);
}

#if WITH_EDITOR
void AGpuStarfieldActor::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    if (GetWorld() && !GetWorld()->IsGameWorld()) {
        load_preview_settings();
    }
}

void AGpuStarfieldActor::load_preview_settings() {
    auto const root{FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"))};
    auto const path{
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), settings_file.FilePath)};
    auto const settings{
        levels::authoring::DefinitionReader{std::filesystem::path{*root}}.read_settings_file(
            std::filesystem::path{*path})};
    if (!settings) {
        auto const error{levels::format_diagnostics(settings.error())};
        UE_LOG(LogGpuStarfieldSettings, Error, TEXT("%s"), UTF8_TO_TCHAR(error.c_str()));
        return;
    }

    set_starfield_settings(settings->starfield);
}

#endif

void AGpuStarfieldActor::set_starfield_settings(starfield::Settings const& settings) {
    starfield_component_->apply_settings(settings, colours);
}

} // namespace ioj
