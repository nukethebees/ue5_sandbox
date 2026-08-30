#pragma once

#include <Math/Box.h>

class UStaticMesh;

namespace ml {
auto SGCOLLISION_API get_aabb(UStaticMesh const& mesh) -> FBox;
}
