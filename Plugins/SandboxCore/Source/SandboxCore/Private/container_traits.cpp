#include <SandboxCore/container_traits.h>

#include "Components/InstancedStaticMeshComponent.h"

namespace ml {

auto NumTraits<UInstancedStaticMeshComponent>::num(UInstancedStaticMeshComponent const& value)
    -> int32 {
    return value.GetNumInstances();
}
}
