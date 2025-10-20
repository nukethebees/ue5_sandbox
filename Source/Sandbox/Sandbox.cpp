// Sandbox.cpp
#include "Sandbox/Sandbox.h"

#include "Sandbox/ui/styles//SandboxStyle.h"

#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FSandboxModule"

void FSandboxModule::StartupModule() {
    FString const shader_dir =
        FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/Sandbox/shaders"));

    AddShaderSourceDirectoryMapping(TEXT("/Project"), shader_dir);

    UE_LOG(LogTemp, Verbose, TEXT("Mapped /Project to: %s"), *shader_dir);

    ml::SandboxStyle::initialize();
}

void FSandboxModule::ShutdownModule() {

    ml::SandboxStyle::shutdown();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_PRIMARY_GAME_MODULE(FSandboxModule, Sandbox, "Sandbox");
