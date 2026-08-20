#pragma once

#include <optional>

#include "CoreMinimal.h"
#include "AI/Navigation/NavigationTypes.h"

class AActor;

namespace ml {
auto get_random_nav_point(AActor& actor, float radius) -> std::optional<FNavLocation>;
}
