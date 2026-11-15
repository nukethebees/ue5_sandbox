// Sandbox.cpp
#include "Sandbox/Sandbox.h"

#include "SandboxGameShared/ui/SandboxStyle.h"

#define LOCTEXT_NAMESPACE "FSandboxModule"

void FSandboxModule::StartupModule() {
    ioj::SandboxStyle::initialize();
}

void FSandboxModule::ShutdownModule() {

    ioj::SandboxStyle::shutdown();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_PRIMARY_GAME_MODULE(FSandboxModule, Sandbox, "Sandbox");
