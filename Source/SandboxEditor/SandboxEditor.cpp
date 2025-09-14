#include "SandboxEditor/SandboxEditor.h"

void FSandboxEditorModule::StartupModule() {
    UE_LOG(LogTemp, Warning, TEXT("SandboxEditor module starting up!"));
    // No factory registration needed - MaterialExpressions control their own UI
}

void FSandboxEditorModule::ShutdownModule() {
    // No cleanup needed
}

IMPLEMENT_MODULE(FSandboxEditorModule, SandboxEditor)
