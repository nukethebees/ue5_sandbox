#pragma once

#include <CoreMinimal.h>

class UStaticMeshComponent;
class UStaticMesh;
class UPrimitiveComponent;

namespace ioj {
auto get_static_mesh(UPrimitiveComponent const* component) -> UStaticMesh const*;
}
