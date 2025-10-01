#include "Sandbox/subsystems/ObjectPoolSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

void UObjectPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystem::Initialize"))

    Super::Initialize(Collection);

    TRY_INIT_PTR(world, GetWorld());

    core_.initialize_pools(*world);
}
