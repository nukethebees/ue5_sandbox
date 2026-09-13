#include "SandboxNative/SandboxNative.h"

#if defined(SANDBOX_WITH_TRACY)
#include <tracy/Tracy.hpp>
#endif

void FSandboxNativeModule::StartupModule() {
#if defined(SANDBOX_WITH_TRACY)
    tracy::StartupProfiler();
#endif
}
void FSandboxNativeModule::ShutdownModule() {
#if defined(SANDBOX_WITH_TRACY)
    if (TracyIsStarted) {
        tracy::ShutdownProfiler();
    }
#endif
}

IMPLEMENT_MODULE(FSandboxNativeModule, SandboxNative)
