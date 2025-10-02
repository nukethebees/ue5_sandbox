#pragma once

#include <optional>

#include "AI/Navigation/NavigationTypes.h"
#include "CoreMinimal.h"

class AActor;

namespace ml {
auto get_random_nav_point(AActor* actor, float radius) -> std::optional<FNavLocation>;
}
