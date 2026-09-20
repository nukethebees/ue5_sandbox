#pragma once

#include "Math/Vector2D.h"

namespace SandboxUI::Widgets {
inline auto is_finite_vector(FVector2f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y);
}
}
