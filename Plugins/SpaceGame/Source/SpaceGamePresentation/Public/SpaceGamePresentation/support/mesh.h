#pragma once

#include <CoreMinimal.h>

class UStaticMeshComponent;
class UStaticMesh;
class UPrimitiveComponent;

namespace ml {
auto get_static_mesh(UPrimitiveComponent const* component) -> UStaticMesh const*;
}
