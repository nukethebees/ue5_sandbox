#pragma once

#include <SandboxCore/container_traits.h>

class UInstancedStaticMeshComponent;

namespace ioj {
template <>
struct SANDBOXCOREENGINE_API NumTraits<UInstancedStaticMeshComponent> {
    static auto num(UInstancedStaticMeshComponent const& value) -> int32;
};
}
