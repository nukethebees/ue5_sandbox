// Sandbox.cpp
#include "Sandbox.h"

#if WITH_EDITOR
#include "EdGraphUtilities.h"
#include "Sandbox/USFLoaderNodeFactory.h"
#endif

#define LOCTEXT_NAMESPACE "FSandboxModule"

void FSandboxModule::StartupModule() {
    FString const shader_dir =
        FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/Sandbox/shaders"));

    AddShaderSourceDirectoryMapping(TEXT("/Project"), shader_dir);

    UE_LOG(LogTemp, Warning, TEXT("Mapped /Project to: %s"), *shader_dir);

#if WITH_EDITOR
    // Only register editor functionality when in editor
    if (GIsEditor) {
        // Create and register our custom graph node factory
        USFLoaderNodeFactory = MakeShareable(new FUSFLoaderNodeFactory());
        FEdGraphUtilities::RegisterVisualNodeFactory(USFLoaderNodeFactory);
    }
#endif
}

void FSandboxModule::ShutdownModule() {
    // Cleanup not usually needed for shader mappings

#if WITH_EDITOR
    // Unregister our factory on shutdown
    if (USFLoaderNodeFactory.IsValid()) {
        FEdGraphUtilities::UnregisterVisualNodeFactory(USFLoaderNodeFactory);
        USFLoaderNodeFactory.Reset();
    }
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_PRIMARY_GAME_MODULE(FSandboxModule, Sandbox, "Sandbox");
