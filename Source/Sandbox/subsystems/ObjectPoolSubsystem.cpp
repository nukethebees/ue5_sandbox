#include "Sandbox/subsystems/ObjectPoolSubsystem.h"

void UObjectPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystem::Initialize"))

    Super::Initialize(Collection);

    core_.initialize_pools(GetWorld());
}
