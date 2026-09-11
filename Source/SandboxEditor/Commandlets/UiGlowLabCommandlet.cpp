#include "SandboxEditor/Commandlets/UiGlowLabCommandlet.h"

#include "SandboxEditor/slate/UiGlowLab.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHIGlobals.h"

UUiGlowLabCommandlet::UUiGlowLabCommandlet() {
    IsClient = true;
    IsEditor = true;
    LogToConsole = true;
}

int32 UUiGlowLabCommandlet::Main(FString const& params) {
    if (GUsingNullRHI) {
        UE_LOG(LogTemp, Error, TEXT("UiGlowLab requires a real graphics RHI."));
        return 1;
    }
    FString output_directory{FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UiGlowLab"))};
    FParse::Value(*params, TEXT("Output="), output_directory);
    bool const initialise_slate{!FSlateApplication::IsInitialized()};
    if (initialise_slate) {
        auto& renderer_module{
            FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer"))};
        FSlateApplication::InitializeAsStandaloneApplication(
            renderer_module.CreateSlateRHIRenderer());
    }
    bool const success{ml::ui::glow_lab::capture(output_directory)};
    if (initialise_slate) {
        FSlateApplication::Shutdown();
    }
    return success ? 0 : 1;
}
