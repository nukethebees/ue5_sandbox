// Copyright Epic Games, Inc. All Rights Reserved.

#include "SandboxCore/SandboxCore.h"

#if defined(SANDBOX_WITH_TRACY)
#include <tracy/Tracy.hpp>
#endif

void FSandboxCoreModule::StartupModule() {
#if defined(SANDBOX_WITH_TRACY)
    tracy::StartupProfiler();
#endif
}

void FSandboxCoreModule::ShutdownModule() {
#if defined(SANDBOX_WITH_TRACY)
    if (TracyIsStarted) {
        tracy::ShutdownProfiler();
    }
#endif
}

IMPLEMENT_MODULE(FSandboxCoreModule, SandboxCore)
