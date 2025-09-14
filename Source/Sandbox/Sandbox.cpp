// Sandbox.cpp
#include "Sandbox.h"

#define LOCTEXT_NAMESPACE "FSandboxModule"

void FSandboxModule::StartupModule() {
    FString const shader_dir =
        FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/Sandbox/shaders"));

    AddShaderSourceDirectoryMapping(TEXT("/Project"), shader_dir);

    UE_LOG(LogTemp, Warning, TEXT("Mapped /Project to: %s"), *shader_dir);
}

void FSandboxModule::ShutdownModule() {
    // Cleanup not usually needed for shader mappings
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_PRIMARY_GAME_MODULE(FSandboxModule, Sandbox, "Sandbox");
