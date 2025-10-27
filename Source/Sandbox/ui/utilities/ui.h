#pragma once

#include "CoreMinimal.h"

#include "Sandbox/ui/data/ScreenBounds.h"

class AActor;
class APlayerController;

namespace ml {
int32 scale_font_to_umg(int32 slate_dpi);
auto get_screen_bounds(APlayerController const& pc, AActor const& actor) -> FScreenBounds;
}
