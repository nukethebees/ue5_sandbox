#include <SandboxCoreEngine/container_traits.h>

#include "Components/InstancedStaticMeshComponent.h"

namespace ioj {

auto NumTraits<UInstancedStaticMeshComponent>::num(UInstancedStaticMeshComponent const& value)
    -> int32 {
    return value.GetNumInstances();
}
}
