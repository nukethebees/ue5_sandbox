// Copyright Epic Games, Inc. All Rights Reserved.

#include "SandboxCore/SandboxCore.h"

#if defined(IOJ_WITH_TRACY)
#include <tracy/Tracy.hpp>
#endif

void FSandboxCoreModule::StartupModule() {
#if defined(IOJ_WITH_TRACY)
    tracy::StartupProfiler();
#endif
}

void FSandboxCoreModule::ShutdownModule() {
#if defined(IOJ_WITH_TRACY)
    if (TracyIsStarted) {
        tracy::ShutdownProfiler();
    }
#endif
}

IMPLEMENT_MODULE(FSandboxCoreModule, SandboxCore)
